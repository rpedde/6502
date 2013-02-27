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
#include <pthread.h>

#include <SDL.h>

#include "hardware.h"
#include "hw-common.h"

#if SDL_VERSION_ATLEAST(2,0,0)
#define SDL2
#endif

#define CHARMAP_WIDTH  8
#define CHARMAP_HEIGHT 10
#define VIDEO_MEMORY_SIZE 4096
#define VIDEO_CHARMAP_SIZE 4096  /* 2560, actually.. 8x10 */

/*
 * This emualates a really bad VGA-resolution video card
 *
 * Resolution is 640x480 internally, and dependant on video modes
 *
 * Video mode 0: Mono text, 80x48 with charmaps of 8x10
 * Video mode 1: Mono Graphic 160x120
 *
 * Video memory: 8k
 * Bitmapped character memory: 2560 bytes internal to video card
 *
 * Mode 0
 * Video:
 *   0-1919: video memory
 *   4094: color
 *        High 4: foreground
 *        Low  4: background
 *   4095: mode register (0/1)
 *
 * Mode 1
 * Video:
 *   0-1919: video memory
 *   1920-3839: color
 *         High 4: foreground color
 *         Low 4: background color
 *                Color map:
 *                   0000 (0): White
 *                   0001 (1): Red
 *                   0010 (2): Green
 *                   0011 (3): Blue
 *                   0100 (4): Cyan
 *                   0101 (5): Magenta
 *                   0110 (6): Yellow
 *                   0111 (7): light gray
 *                   ...     : Dark versions of above
 *                   1111 (F): Black
 *   4095: Mode register (0/1)
 */

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

typedef struct video_state_t {
    uint8_t mode_register;  /* 4095 */
    uint8_t color_register; /* 4094 */
    int dirty;
    uint8_t *video_memory;
    uint8_t *charmap_memory;
    pthread_t event_tid;
    pthread_mutex_t state_lock;

    uint8_t mode;
    uint8_t bg_color;
    uint8_t fg_color;

#ifdef SDL2
    uint8_t *texture_memory;
    SDL_Window *window;
    SDL_Renderer *renderer;
    SDL_Texture *texture;
#else
    SDL_Surface *screen;
#endif
} video_state_t;

hw_callbacks_t *hardware_callbacks;

hw_reg_t *init(hw_config_t *config, hw_callbacks_t *callbacks);
uint8_t video_memop(hw_reg_t *hw, uint16_t addr, uint8_t memop, uint8_t data);
static uint8_t video_eventloop(void *arg, int blocking);
static void lock_state(video_state_t *state);
static void unlock_state(video_state_t *state);
static void set_value_at_position(video_state_t *state, int x,
                                  int y, uint8_t value);

hw_reg_t *init(hw_config_t *config, hw_callbacks_t *callbacks) {
    hw_reg_t *video_reg;
    uint16_t start;
    uint16_t end;
    video_state_t *state;
    char *video_rom;

    hardware_callbacks = callbacks;

    video_reg = malloc(sizeof(hw_reg_t) + sizeof(mem_remap_t));
    if(!video_reg) {
        perror("malloc");
        exit(1);
    }

    memset(video_reg, 0, sizeof(video_reg));

    if(!config_get_uint16(config, "mem_start", &start))
        return NULL;

    if(!config_get_uint16(config, "mem_end", &end))
        return NULL;

    if(!(video_rom = config_get(config, "video_rom")))
        return NULL;

    video_reg->hw_family = HW_FAMILY_IO;
    video_reg->memop = video_memop;
    video_reg->eventloop = video_eventloop;
    video_reg->remapped_regions = 1;

    video_reg->remap[0].mem_start = start;
    video_reg->remap[0].mem_end = end;
    video_reg->remap[0].readable = 1;
    video_reg->remap[0].writable = 1;

    state = malloc(sizeof(video_state_t));
    if(!state) {
        FATAL("malloc");
        exit(1);
    }

    memset(state, 0, sizeof(video_state_t));

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

    INFO("Initializing SDL");

    if(SDL_Init(SDL_INIT_VIDEO) < 0) {
        ERROR("sdl error: %s", SDL_GetError());
        exit(1);
    }

#ifdef SDL2
    uint8_t *dest;
    int offset;
    uint8_t rack;

    INFO("Creating window");
    state->window = SDL_CreateWindow("6502 Video", SDL_WINDOWPOS_CENTERED,
                                     SDL_WINDOWPOS_CENTERED, 640, 480,
                                     SDL_WINDOW_SHOWN);
    if(state->window == NULL) {
        ERROR("SDL Error: %s", SDL_GetError());
        exit(EXIT_FAILURE);
    }

    INFO("Creating renderer");
    state->renderer = SDL_CreateRenderer(state->window, -1, 0);
    if(state->renderer == NULL) {
        ERROR("SDL Error: %s", SDL_GetError());
        exit(EXIT_FAILURE);
    }

    state->texture_memory = (uint8_t *)malloc(
        CHARMAP_WIDTH * CHARMAP_HEIGHT * 256 * 2);

    if(!state->texture_memory) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    INFO("Calculating textures in ARGB1555");
    dest = state->texture_memory;
    for(offset = 0; offset < CHARMAP_HEIGHT * 256; offset++) {
        for(rack=0x80; rack > 0; rack >>= 1) {
            if(state->charmap_memory[offset] & rack) {
                *dest++ = 0xFF;
                *dest++ = 0xFF;
            } else {
                *dest++ = 0x00;
                *dest++ = 0x00;
            }
        }
    }

    INFO("Creating texture");
    state->texture = SDL_CreateTexture(state->renderer,
                                       SDL_PIXELFORMAT_ARGB1555,
                                       SDL_TEXTUREACCESS_STATIC,
                                       CHARMAP_WIDTH,
                                       CHARMAP_HEIGHT * 256);

    if(!state->texture) {
        ERROR("SDL_CreateTexture: %s", SDL_GetError());
        exit(EXIT_FAILURE);
    }

    INFO("Updating texture");
    if(SDL_UpdateTexture(state->texture, NULL, state->texture_memory, CHARMAP_WIDTH * 2) < 0) {
        ERROR("SDL_UpdateTexture: %s", SDL_GetError());
        exit(EXIT_FAILURE);
    }


    INFO("Clearing renderer");
    SDL_SetRenderDrawColor(state->renderer, 0, 0, 0, 255);
    SDL_RenderClear(state->renderer);
    SDL_RenderPresent(state->renderer);
#else
    state->screen = SDL_SetVideoMode(640, 480, 32, SDL_SWSURFACE);
#endif

    video_reg->state = state;

    state->dirty = 0;
    state->mode_register = 0;
    state->color_register = 0x0F;

    return video_reg;
}

/**
 * Perform a memory operation on a registered memory address
 *
 * @param addr address of memory operation
 * @param memop a memop constant (@see MEMOP_READ, @see MEMOP_WRITE)
 * @param data byte to write (on MEMOP_WRITE)
 * @return data item (on MEMOP_READ)
 */
uint8_t video_memop(hw_reg_t *hw, uint16_t addr, uint8_t memop, uint8_t data) {
    video_state_t *state = (video_state_t*)(hw->state);
    int offset;
    SDL_Event event;

    offset = addr - hw->remap[0].mem_start;

    if(memop == MEMOP_READ) {
        switch(state->mode) {
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
        switch(state->mode) {
        case 0:
            if(offset < (80 * 24)) {
                DEBUG(" -- valid to video memory");
                state->video_memory[offset] = data;
                state->dirty=1;
            }
            if(offset == 4095) {
                DEBUG(" -- valid to mode register");
                state->mode_register = data;
                state->dirty=1;
            }
            if(offset == 4094) {
                DEBUG(" -- valid to color register");
                state->color_register = data;
                state->dirty=1;
            }

            if(state->dirty) {
                DEBUG("Pushing SDL event");
                event.type = SDL_USEREVENT;
                event.user.code = 0;
                SDL_PushEvent(&event);
            }
        default:
            break;
        }
        unlock_state(state);
        return 1;
    }

    return 0;
}

/**
 * set display at a position to a particular value.  this
 * should probably be done as a consequence of setting
 * video memory.
 *
 * @param state video state blob
 * @param x x position to set
 * @param y y position to set
 * @param value byte index into video cargen memory to update
 *              position to
 */
static void set_value_at_position(video_state_t *state,
                                  int x, int y, uint8_t value) {
    SDL_Rect srect, drect;

    srect.x = 0;
    srect.w = CHARMAP_WIDTH;
    srect.y = CHARMAP_HEIGHT * value;
    srect.h = CHARMAP_HEIGHT;

    drect.x = CHARMAP_WIDTH * x;
    drect.y = CHARMAP_HEIGHT * (y*2);
    drect.w = CHARMAP_WIDTH;;
    drect.h = CHARMAP_HEIGHT * 2;

    if(SDL_RenderCopy(state->renderer, state->texture,
                      &srect, &drect) < 0) {
        ERROR("SDL_RenderCopy: %s", SDL_GetError());
        exit(EXIT_FAILURE);
    }
}


/**
 * async SDL event proc
 *
 * @param arg a void* cast state blob
 */
static uint8_t video_eventloop(void *arg, int blocking) {
    video_state_t *state = (video_state_t*)arg;
    int result = 0;
    SDL_Event e;
    color_t *bg_color, *fg_color;

    if(blocking)
        result = SDL_WaitEvent(&e);
    else
        result = SDL_PollEvent(&e);

    if(result) {
        if(e.type == SDL_QUIT) {
            INFO("Got SDL Quit event");
            SDL_Quit();
            exit(EXIT_SUCCESS);
        }

        if(e.type == SDL_KEYDOWN) {
            INFO("Got Keydown");
        }

        if(e.type == SDL_MOUSEBUTTONDOWN) {
            INFO("Got Mousedown");
        }

        if(e.type == SDL_USEREVENT) {
            INFO("Got userevent");
        }

        lock_state(state);

        if(state->dirty) {
            INFO("refreshing display");
            switch(state->mode_register) {
            case 0:
                bg_color = &video_colors[state->color_register & 0x0f];
                fg_color = &video_colors[(state->color_register & 0xf0) >> 4];

                SDL_SetRenderDrawColor(state->renderer, bg_color->r,
                                       bg_color->g, bg_color->b, 0xff);
                SDL_RenderClear(state->renderer);
                SDL_SetTextureColorMod(state->texture, fg_color->r,
                                       fg_color->g, fg_color->b);

                INFO("Sweeping screen");
                int x;
                int y;
                int pos=0;

                for(y = 0; y < 24; y++) {
                    for(x = 0; x < 80; x++) {
                        set_value_at_position(state, x, y, state->video_memory[pos]);
                        pos++;
                    }
                }

                SDL_RenderPresent(state->renderer);
                break;
            default:
                INFO("Bad video mode on refresh");
                break;
            }

            state->dirty = 0;

        }
        unlock_state(state);

        /* SDL_RenderClear(state->renderer); */
        /* SDL_RenderPresent(state->renderer); */
    }
}


/**
 * Lock the state blob when producing or consuming.  It either
 * succeeds, or we exit.
 *
 * @param state video state to be locked
 */
void lock_state(video_state_t *state) {
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

static void unlock_state(video_state_t *state) {
    int res;

    if((res = pthread_mutex_unlock(&state->state_lock))) {
        FATAL("pthread_mutex_unlock: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
}
