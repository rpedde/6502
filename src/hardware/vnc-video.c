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
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

#include <rfb/rfb.h>

#include "hardware.h"
#include "hw-common.h"

#define CHARMAP_WIDTH  8
#define CHARMAP_HEIGHT 10
#define VIDEO_MEMORY_SIZE 4096
#define VIDEO_CHARMAP_SIZE 4096  /* 2560, actually.. 8x10 */


/**
 * This impements the same naive video card as the
 * generic video driver, except using vnc as an
 * output mechanism rather than sdl
 */
hw_reg_t *init(hw_config_t *config, hw_callbacks_t *callbacks);
uint8_t vnc_memop(hw_reg_t *hw, uint16_t addr, uint8_t memop, uint8_t data);

typedef struct color_t {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} color_t;

static color_t video_colors[] = {
    { 0xff, 0xff, 0xff },   // 0: white
    { 0xff, 0x00, 0x00 },   // 1: red
    { 0x00, 0xff, 0x00 },   // 2: green
    { 0x00, 0x00, 0xff },   // 3: blue
    { 0x00, 0xff, 0xff },   // 4: cyan
    { 0xff, 0x00, 0xff },   // 5: magenta
    { 0xff, 0xff, 0x00 },   // 6: yellow
    { 0xa0, 0xa0, 0xa0 },   // 7: light gray
    { 0x60, 0x60, 0x60 },   // 8: dark gray
    { 0x7f, 0x00, 0x00 },   // 9: dark red
    { 0x00, 0x7f, 0x00 },   // A: dark green
    { 0x00, 0x00, 0x7f },   // B: dark blue
    { 0x00, 0x7f, 0x7f },   // C: dark cyan
    { 0xff, 0x00, 0x7f },   // D: dark magenta
    { 0x7f, 0x7f, 0x00 },   // E: dark yellow
    { 0x00, 0x00, 0x00 }    // F: black
};

typedef struct vnc_state_t {
    uint8_t mode_register;  /* 4095 */
    uint8_t color_register; /* 4094 */

    uint8_t *video_memory;
    uint8_t *charmap_memory;
    uint8_t *texture_memory;

    /* frame-buffer stuff */
    rfbScreenInfoPtr screen;
    pthread_t event_tid;

    pthread_mutex_t state_lock;
    int dirty;
} vnc_state_t;

static hw_callbacks_t *hardware_callbacks;

static void lock_state(vnc_state_t *state);
static void unlock_state(vnc_state_t *state);
static void *rfb_proc(void *arg);
static void vnc_error_log(const char *format, ...);
static void vnc_info_log(const char *format, ...);
static void set_value_at_position(vnc_state_t *state,
                                  int x, int y, uint8_t value);
static void update_screen(vnc_state_t *state);

hw_reg_t *init(hw_config_t *config, hw_callbacks_t *callbacks) {
    hw_reg_t *vnc_reg;
    uint16_t start;
    uint16_t end;
    vnc_state_t *state;
    char *video_rom;

    hardware_callbacks = callbacks;

    vnc_reg = malloc(sizeof(hw_reg_t) + sizeof(mem_remap_t));
    if(!vnc_reg) {
        perror("malloc");
        exit(1);
    }

    memset(vnc_reg, 0, sizeof(vnc_reg));

    if(!config_get_uint16(config, "mem_start", &start))
        return NULL;

    if(!config_get_uint16(config, "mem_end", &end))
        return NULL;

    if(!(video_rom = config_get(config, "video_rom")))
        return NULL;

    vnc_reg->hw_family = HW_FAMILY_VIDEO;
    vnc_reg->memop = vnc_memop;
    vnc_reg->remapped_regions = 1;

    vnc_reg->remap[0].mem_start = start;
    vnc_reg->remap[0].mem_end = end;
    vnc_reg->remap[0].readable = 1;
    vnc_reg->remap[0].writable = 1;

    state = malloc(sizeof(vnc_state_t));
    if(!state) {
        perror("malloc");
        exit(1);
    }

    /* init the state */
    state->video_memory = (uint8_t*)malloc(VIDEO_MEMORY_SIZE);
    state->charmap_memory = (uint8_t*)malloc(VIDEO_CHARMAP_SIZE);

    if((!state->video_memory) || (!state->charmap_memory)) {
        if(state->video_memory)
            free(state->video_memory);

        if(state->charmap_memory)
            free(state->charmap_memory);

        free(state);
        FATAL("malloc");
        exit(1);
    }

    if(video_rom) {
        FILE *fd = fopen(video_rom, "r");
        if(!fd) {
            FATAL("fopen");
            exit(1);
        }

        fread(state->charmap_memory, 1, 2560, fd);
        fclose(fd);
    }

    if(pthread_mutex_init(&state->state_lock, NULL) < 0) {
        FATAL("pthread_mutex_init: %s", strerror(errno));
        return NULL;
    }

    state->mode_register = 0;
    state->color_register = 0x0F;  /* gray on black */
    state->dirty = 0;

    /* init the fb */
    rfbLog = (rfbLogProc)vnc_info_log;
    rfbErr = (rfbLogProc)vnc_error_log;

    state->screen = rfbGetScreen(0, NULL, 640, 480, 8, 3, 4);
    if(!state->screen) {
        FATAL("Could not alloc rfb screen");
        exit(EXIT_FAILURE);
    }

    state->screen->desktopName = "6502 Video";

    state->screen->frameBuffer=malloc(640*480*4);

    state->screen->autoPort = TRUE;
    rfbInitServer(state->screen);

    asprintf(&vnc_reg->descr, "port %d", state->screen->port);

    NOTIFY("Starting vnc server on %s", vnc_reg->descr);
    INFO("Starting vnc server on %s", vnc_reg->descr);

    /* and we'll kick off the event loop */
    if(pthread_create(&state->event_tid, NULL, rfb_proc, state) < 0) {
        perror("pthread_create");
        return NULL;
    }

    vnc_reg->state = state;
    return vnc_reg;
}

/**
 * update entire screen.  This should be done
 * with state locked
 */
static void update_screen(vnc_state_t *state) {
    int x, y, pos=0;
    static uint8_t counter = 0xE0;

    for(y = 0; y < 24; y++) {
        for(x = 0; x < 80; x++) {
            set_value_at_position(state, x, y, state->video_memory[pos]);
            pos++;
        }
    }

    rfbMarkRectAsModified(state->screen, 0, 0, 640, 480);
}


/**
 * set display at a position to a particular value.  this
 * should probably be done as a consequence of setting
 * video memory.
 *
 * this should be done with state locked.
 *
 * @param state video state blob
 * @param x x position to set
 * @param y y position to set
 * @param value byte index into video cargen memory to update
 *              position to
 */
static void set_value_at_position(vnc_state_t *state,
                                  int x, int y, uint8_t value) {
    uint8_t *texture;
    uint8_t *dest;
    uint8_t rack, offset;

    color_t *fg;
    color_t *bg;

    int stride = 640 * 4;  /* 4 bpp */

    texture = &state->charmap_memory[value * CHARMAP_HEIGHT];
    dest = (uint8_t*)&state->screen->frameBuffer[(stride * CHARMAP_HEIGHT * 2 * y) + (x * 4 * CHARMAP_WIDTH)];

    bg = &video_colors[state->color_register & 0x0f];
    fg = &video_colors[(state->color_register & 0xf0) >> 4];

    for(offset = 0; offset < CHARMAP_HEIGHT; offset++) {
        for(rack = 0x80; rack > 0; rack >>=1) {
            if((*(texture + offset)) & rack) {
                memcpy(dest, fg, 3);
                memcpy(dest + stride, fg, 3);
            } else {
                memcpy(dest, bg, 3);
                memcpy(dest + stride, bg, 3);
            }
            dest += 4;
        }
        dest += (2*stride) - (4 * CHARMAP_WIDTH);
    }
}


/**
 * dump a vncserver lib error line
 *
 * this is kind of broken, as we don't get original
 * line number, but meh.
 */
void vnc_error_log(const char *format, ...) {
    va_list args;
    char *out;

    va_start(args, format);
    vasprintf(&out, format, args);
    va_end(args);

    while(strlen(out) && (out[strlen(out) - 1] == '\n' ||
                          out[strlen(out) - 1] == '\r'))
        out[strlen(out) - 1] = '\0';

    ERROR("%s", out);

    free(out);

}

/**
 * dump a vncserver lib info line
 *
 * this is kind of broken, as we don't get original
 * line number, but meh.
 */
void vnc_info_log(const char *format, ...) {
    va_list args;
    char *out;

    va_start(args, format);
    vasprintf(&out, format, args);
    va_end(args);

    while(strlen(out) && (out[strlen(out) - 1] == '\n' ||
                          out[strlen(out) - 1] == '\r'))
        out[strlen(out) - 1] = '\0';

    INFO("%s", out);

    free(out);
}

/**
 * Perform a memory operation on a registered memory address
 *
 * @param addr address of memory operation
 * @param memop a memop constant (@see MEMOP_READ, @see MEMOP_WRITE)
 * @param data byte to write (on MEMOP_WRITE)
 * @return data item (on MEMOP_READ)
 */
uint8_t vnc_memop(hw_reg_t *hw, uint16_t addr, uint8_t memop, uint8_t data) {
    vnc_state_t *state = (vnc_state_t*)(hw->state);
    int offset;
    int x, y;

    offset = addr - hw->remap[0].mem_start;

    if(memop == MEMOP_READ) {
        switch(state->mode_register) {
        case 0:
            if(offset < 80 * 24) {
                return state->video_memory[offset];
            }
            if(offset == 4095) {
                return state->mode_register;
            }
            if(offset == 4094) {
                return state->color_register;
            }
            return 0;
        default:
            return 0;
        }
    }

    if(memop == MEMOP_WRITE) {
        DEBUG("memop write: $%02x to $%04x", data, addr);
        lock_state(state);
        switch(state->mode_register) {
        case 0:
            if(offset < (80 * 24)) {
                DEBUG(" -- valid to video memory");
                state->video_memory[offset] = data;

                x = offset % 80;
                y = offset / 80;

                set_value_at_position(state, x, y, data);
                rfbMarkRectAsModified(state->screen, x * CHARMAP_WIDTH,
                                      y * CHARMAP_HEIGHT * 2,
                                      (x + 1) * CHARMAP_WIDTH,
                                      (y + 1) * CHARMAP_HEIGHT * 2);

            }
            if(offset == 4095) {
                DEBUG(" -- valid to mode register");
                state->mode_register = data;
                update_screen(state);
            }
            if(offset == 4094) {
                DEBUG(" -- valid to color register");
                state->color_register = data;
                update_screen(state);
            }
            break;
        default:
            break;
        }
        unlock_state(state);
        return 1;
    }

    return 0;
}

/**
 * Lock the state blob when producing or consuming.  It either
 * succeeds, or we exit.
 *
 * @param state video state to be locked
 */
void lock_state(vnc_state_t *state) {
    int res;

    if((res = pthread_mutex_lock(&state->state_lock))) {
        FATAL("pthread_mutex_lock: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
}

/**
 * and conversely, unlock the state blob.  Again, if this does
 * not succeed, bad things are afoot and we crash.
 *
 * @param state video state to be unlocked
 */

static void unlock_state(vnc_state_t *state) {
    int res;

    if((res = pthread_mutex_unlock(&state->state_lock))) {
        FATAL("pthread_mutex_unlock: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
}

/**
 *
 */
void *rfb_proc(void *arg) {
    vnc_state_t *state = (vnc_state_t*)arg;

    while(rfbIsActive(state->screen)) {
        /* INFO("Tick"); */
        rfbProcessEvents(state->screen, 1000);
    }

    INFO("rfb event loop terminating");
    return(0);
}
