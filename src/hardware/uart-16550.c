/*
 * Copyright (C) 2012 Ron Pedde <ron@pedde.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

#include "hardware.h"
#include "hw-common.h"
#include "uart-16550.h"

#define UART_MAX_BUFFER 16

hw_reg_t *init(hw_config_t *config, hw_callbacks_t *callbacks);
static uint8_t uart_memop(hw_reg_t *hw, uint16_t addr, uint8_t memop, uint8_t data);
static void *listener_proc(void *arg);

typedef struct uart_state_t {
    uint8_t RBR; /* register buffer receiver (r/o) */
    uint8_t IER; /* interrupt enable register */
    uint8_t IIR; /* interrupt identity register (r/o) */
    uint8_t FCR; /* fifo control register (w/o) */
    uint8_t LCR; /* line control register */
    uint8_t MCR; /* modem control register */
    uint8_t LSR; /* line status register */
    uint8_t MSR; /* modem status register */
    uint8_t SCR; /* scratch register */
    uint8_t DLL; /* divisor latch (lsb) */
    uint8_t DLM; /* divisor latch (msb) */

    int pty;
    int head_buffer_pos;
    int tail_buffer_pos;

    int buffer[UART_MAX_BUFFER];
    pthread_t listener_tid;
    pthread_mutex_t state_lock;
} uart_state_t;

static void lock_state(uart_state_t *state);
static void unlock_state(uart_state_t *state);
static void receive_byte(uart_state_t *state, uint8_t byte);
static void recalculate_irq(uart_state_t *state);

static hw_callbacks_t *hardware_callbacks;

hw_reg_t *init(hw_config_t *config, hw_callbacks_t *callbacks) {
    hw_reg_t *uart_reg;
    uint16_t start;
    uint16_t end;
    uart_state_t *state;
    int ret;
    int raw;
    struct termios pty_termios;

    hardware_callbacks = callbacks;

    uart_reg = malloc(sizeof(hw_reg_t) + sizeof(mem_remap_t));
    if(!uart_reg) {
        perror("malloc");
        exit(1);
    }

    memset(uart_reg, 0, sizeof(uart_reg));

    if(!config_get_uint16(config, "mem_start", &start))
        return NULL;

    if(!config_get_bool(config, "raw", &raw))
        raw = 1;

    end = start + 8;

    uart_reg->hw_family = HW_FAMILY_SERIAL;
    uart_reg->memop = uart_memop;
    uart_reg->remapped_regions = 1;

    uart_reg->remap[0].mem_start = start;
    uart_reg->remap[0].mem_end = end;
    uart_reg->remap[0].readable = 1;
    uart_reg->remap[0].writable = 1;

    state = malloc(sizeof(uart_state_t));
    if(!state) {
        perror("malloc");
        exit(1);
    }

    memset(state, 0, sizeof(uart_state_t));

    /* init the state */
    uart_reg->state = state;

    /* set up the pty */
    state->pty = posix_openpt(O_RDWR);
    if(state->pty < 0) {
        perror("posix_openpt");
        return NULL;
    }

    ret = grantpt(state->pty);
    if(ret) {
        perror("grantpt");
        DEBUG("grantpt: %d", ret);
        return NULL;
    }

    ret = unlockpt(state->pty);
    if(ret) {
        perror("unlockpt");
        return NULL;
    }

    if(raw) {
        tcgetattr(state->pty, &pty_termios);
        cfmakeraw(&pty_termios);
        tcsetattr(state->pty, TCSANOW, &pty_termios);
    }

    INFO("Opened pty for 16550 uart at %s", ptsname(state->pty));
    NOTIFY("Opened pty for 16550 uart at %s", ptsname(state->pty));

    /* now, start up an async listener thread */
    if(pthread_mutex_init(&state->state_lock, NULL) < 0) {
        perror("pthread_mutex_init");
        return NULL;
    }

    if(pthread_create(&state->listener_tid, NULL, listener_proc, state) < 0) {
        perror("pthread_create");
        return NULL;
    }

    return uart_reg;
}

/**
 * we aren't honoring irq yet
 * caller must be holding state lock.
 *
 * @param state uart state
 */
void recalculate_irq(uart_state_t *state) {
    /* nothing to see here */
}

/**
 * Lock the state blob when producing or consuming.  It either
 * succeeds, or we exit.
 *
 * @param state uart state to be locked
 */
void lock_state(uart_state_t *state) {
    int res;

    if((res = pthread_mutex_lock(&state->state_lock))) {
        perror("pthread_mutex_lock");
        exit(EXIT_FAILURE);
    }
}

/**
 * and conversely, unlock the state blob.  Again, if this does
 * not succeed, bad things are afoot and we crash.
 *
 * @param state uart state to be unlocked
 */

void unlock_state(uart_state_t *state) {
    int res;

    if((res = pthread_mutex_unlock(&state->state_lock))) {
        perror("pthread_mutex_unlock");
        exit(EXIT_FAILURE);
    }
}

/**
 * do the needful when we receive a new async byte.  This
 * should update the fifo, recalculate whether or not we
 * should be holding irq low, etc.
 *
 * @param state uart state (unlocked)
 * @param byte data byte received
 */
void receive_byte(uart_state_t *state, uint8_t byte) {
    lock_state(state);

    if(((state->head_buffer_pos + 1) % UART_MAX_BUFFER) == state->tail_buffer_pos) {
        /* we just dropped a char */
        state->LSR = state->LSR | LSR_OE;
        WARN("just dropped byte");
    } else {
        state->buffer[state->head_buffer_pos] = byte;
        state->head_buffer_pos = (state->head_buffer_pos + 1) % UART_MAX_BUFFER;
        /* mark rx available */
        state->LSR = state->LSR | LSR_DR;
    }

    recalculate_irq(state);

    unlock_state(state);
}


/**
 * Async listener for pty.  This listens on the pty and
 * any received bytes get passed to the receive_byte function.
 *
 * @param arg a void* cast state blob
 */
void *listener_proc(void *arg) {
    uart_state_t *state = (uart_state_t*)arg;
    int res;
    uint8_t byte;

    while(1) {
        res = read(state->pty, &byte, sizeof(byte));
        if(res < 0) {
            perror("read");
            exit(EXIT_FAILURE);
        }

        if(res == 0) {
            /* file closed out from end of us */
            WARN("EOF on pty");
            exit(EXIT_FAILURE);
        }

        receive_byte(state, byte);
        DEBUG("got byte $%02x", byte);
    }
}


/**
 * Perform a memory operation on a registered memory address
 *
 * @param addr address of memory operation
 * @param memop a memop constant (@see MEMOP_READ, @see MEMOP_WRITE)
 * @param data byte to write (on MEMOP_WRITE)
 * @return data item (on MEMOP_READ)
 */
uint8_t uart_memop(hw_reg_t *hw, uint16_t addr, uint8_t memop, uint8_t data) {
    uart_state_t *state = (uart_state_t*)(hw->state);
    uint16_t addr_offset = addr - hw->remap[0].mem_start;
    int read = (memop == MEMOP_READ);
    int dlab = (state->LCR & LCR_DLAB);
    uint8_t retval;

    switch(addr_offset) {
    case 0: /* RBR, THR, DLL */
        if(dlab) {
            if(read)
                return state->DLL;
            state->DLL = data;
        } else {
            if(read) {
                /* pull something out of the receive buffer */
                lock_state(state);
                if(state->head_buffer_pos == state->tail_buffer_pos) {
                    /* nothing in the buffer */
                    retval = 0;
                    DEBUG("read on empty fifo");
                } else {
                    retval = state->buffer[state->tail_buffer_pos];
                    state->tail_buffer_pos = (state->tail_buffer_pos + 1) % UART_MAX_BUFFER;
                    if(state->tail_buffer_pos == state->head_buffer_pos) {
                        /* fifo is empty */
                        state->LSR &= ~LSR_DR;
                        recalculate_irq(state);
                    }
                }

                unlock_state(state);
                return retval;
            }

            /* write to THR -- drop the byte out the pts */
            DEBUG("Writing byte %02X to pty", data);

            write(state->pty, &data, 1);
        }
        break;

    case 1: /* IER, DLM */
        if(dlab) {
            if(read)
                return state->DLM;
            state->DLM = data;
        } else {
            if(read)
                return state->IER;
            state->IER = data;
        }
        break;

    case 2: /* IIR, FCR */
        if(read)
            return state->IIR;
        state->FCR = data;
        break;

    case 3:
        if(read)
            return state->LCR;
        state->LCR = data;
        break;

    case 4:
        if(read)
            return state->MCR;
        state->MCR = data;
        break;

    case 5:
        if(read) {
            /* error flags reset on read */
            lock_state(state);
            retval = state->LSR;
            state->LSR &= ~(LSR_OE | LSR_PE | LSR_FE | LSR_BI | LSR_ERF);
            unlock_state(state);
            return retval;
        }
        state->LSR = data;
        break;

    case 6:
        if(read) {
            lock_state(state);
            retval = state->MSR;
            /* we clear the delta states on msr read */
            state->MSR &= ~(MSR_DCTS | MSR_DDSR | MSR_DDCD | MSR_TERI);
            unlock_state(state);
            return retval;

        }
        state->MSR = data;
        break;

    case 7:
        if(read)
            return state->SCR;
        state->SCR = data;
        break;
    }


    /* if(memop == MEMOP_READ) { */
    /*     return mem_state->mem[addr - hw->remap[0].mem_start]; */
    /* } */

    /* if(memop == MEMOP_WRITE) { */
    /*     mem_state->mem[addr - hw->remap[0].mem_start] = data; */
    /*     return 1; */
    /* } */

    return 0;
}
