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
#include <pthread.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

#include "hardware.h"
#include "hw-common.h"
#include "uart-16550.h"

hw_reg_t *init(hw_config_t *config);
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
    pthread_t listener_tid;
    pthread_mutex_t state_lock;
} uart_state_t;

static void lock_state(uart_state_t *state);
static void unlock_state(uart_state_t *state);
static void receive_byte(uart_state_t *state, uint8_t byte);

hw_reg_t *init(hw_config_t *config) {
    hw_reg_t *uart_reg;
    uint16_t start;
    uint16_t end;
    uart_state_t *state;
    int ret;

    uart_reg = malloc(sizeof(hw_reg_t) + sizeof(mem_remap_t));
    if(!uart_reg) {
        perror("malloc");
        exit(1);
    }

    memset(uart_reg, 0, sizeof(uart_reg));

    if(!config_get_uint16(config, "mem_start", &start))
        return NULL;

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
        fprintf(stderr, "grantpt: %d\n", ret);
        return NULL;
    }

    ret = unlockpt(state->pty);
    if(ret) {
        perror("unlockpt");
        return NULL;
    }

    fprintf(stderr, "Opened pty for 16550 uart at %s\n", ptsname(state->pty));

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
 * lock the state blob when producing or consuming
 */
void lock_state(uart_state_t *state) {
    int res;

    if((res = pthread_mutex_lock(&state->state_lock))) {
        perror("pthread_mutex_lock");
        exit(EXIT_FAILURE);
    }
}

/**
 * and conversely, unlock the state blob.
 */

void unlock_state(uart_state_t *state) {
    int res;

    if((res = pthread_mutex_unlock(&state->state_lock))) {
        perror("pthread_mutex_unlock");
        exit(EXIT_FAILURE);
    }
}

/**
 * do the needful when we receive a new async byte
 */
void receive_byte(uart_state_t *state, uint8_t byte) {
    lock_state(state);
    /* throw it away! */
    unlock_state(state);
}


/**
 * async listener for pty
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
            fprintf(stderr, "EOF on pty\n");
            exit(EXIT_FAILURE);
        }

        receive_byte(state, byte);
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

    switch(addr_offset) {
    case 0: /* RBR, THR, DLL */
        if(dlab) {
            if(read)
                return state->DLL;
            state->DLL = data;
        } else {
            if(read)
                return state->RBR;

            /* write to THR -- drop the byte out the pts */
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
        if(read)
            return state->LSR;
        state->LSR = data;
        break;

    case 6:
        if(read)
            return state->MSR;
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
