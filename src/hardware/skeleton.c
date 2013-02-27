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

#include "hardware.h"
#include "hw-common.h"

hw_reg_t *init(hw_config_t *config, hw_callbacks_t *callbacks);
uint8_t skeleton_memop(hw_reg_t *hw, uint16_t addr, uint8_t memop, uint8_t data);

typedef struct skeleton_state_t {
} skeleton_state_t;

static hw_callbacks_t *hardware_callbacks;

hw_reg_t *init(hw_config_t *config, hw_callbacks_t *callbacks) {
    hw_reg_t *skeleton_reg;
    uint16_t start;
    uint16_t end;
    skeleton_state_t *state;

    hardware_callbacks = callbacks;

    skeleton_reg = malloc(sizeof(hw_reg_t) + sizeof(mem_remap_t));
    if(!skeleton_reg) {
        perror("malloc");
        exit(1);
    }

    memset(skeleton_reg, 0, sizeof(skeleton_reg));

    if(!config_get_uint16(config, "mem_start", &start))
        return NULL;

    if(!config_get_uint16(config, "mem_end", &end))
        return NULL;

    skeleton_reg->hw_family = HW_FAMILY_IO;
    skeleton_reg->memop = skeleton_memop;
    skeleton_reg->remapped_regions = 1;

    skeleton_reg->remap[0].mem_start = start;
    skeleton_reg->remap[0].mem_end = end;
    skeleton_reg->remap[0].readable = 1;
    skeleton_reg->remap[0].writable = 1;

    int size = end - start + 1;

    state = malloc(sizeof(skeleton_state_t));
    if(!state) {
        perror("malloc");
        exit(1);
    }

    /* init the state */

    skeleton_reg->state = state;
    return skeleton_reg;
}

/**
 * Perform a memory operation on a registered memory address
 *
 * @param addr address of memory operation
 * @param memop a memop constant (@see MEMOP_READ, @see MEMOP_WRITE)
 * @param data byte to write (on MEMOP_WRITE)
 * @return data item (on MEMOP_READ)
 */
uint8_t skeleton_memop(hw_reg_t *hw, uint16_t addr, uint8_t memop, uint8_t data) {
    skeleton_state_t *skeleton_state = (skeleton_state_t*)(hw->state);

    /* if(memop == MEMOP_READ) { */
    /*     return mem_state->mem[addr - hw->remap[0].mem_start]; */
    /* } */

    /* if(memop == MEMOP_WRITE) { */
    /*     mem_state->mem[addr - hw->remap[0].mem_start] = data; */
    /*     return 1; */
    /* } */

    return 0;
}
