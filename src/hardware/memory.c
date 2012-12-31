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

hw_reg_t *init(hw_config_t *config);
uint8_t mem_memop(hw_reg_t *hw, uint16_t addr, uint8_t memop, uint8_t data);

typedef struct mem_state_t {
    uint8_t *mem;
} mem_state_t;

hw_reg_t *init(hw_config_t *config) {
    hw_reg_t *mem_reg;
    uint16_t start;
    uint16_t end;
    mem_state_t *state;
    char *is_rom;
    char *backing_file;
    uint8_t writable = 1;

    mem_reg = malloc(sizeof(hw_reg_t) + sizeof(mem_remap_t));
    if(!mem_reg) {
        perror("malloc");
        exit(1);
    }

    memset(mem_reg, 0, sizeof(mem_reg));

    if(!config_get_uint16(config, "mem_start", &start))
        return NULL;

    if(!config_get_uint16(config, "mem_end", &end))
        return NULL;

    is_rom = config_get(config, "is_rom");
    if(is_rom) {
        if((strcasecmp(is_rom, "true") == 0) ||
           (strcasecmp(is_rom, "1") == 0) ||
           (strcasecmp(is_rom, "yes") == 0)) {
            writable = 0;
        }
    }

    backing_file = config_get(config, "backing_file");

    mem_reg->hw_family = HW_FAMILY_MEMORY;
    mem_reg->memop = mem_memop;
    mem_reg->remapped_regions = 1;

    mem_reg->remap[0].mem_start = start;
    mem_reg->remap[0].mem_end = end;
    mem_reg->remap[0].readable = 1;
    mem_reg->remap[0].writable = writable;

    int size = end - start + 1;
    state = malloc(sizeof(mem_state_t));
    if(!state) {
        perror("malloc");
        exit(1);
    }

    state->mem = malloc(size);
    if(!state->mem) {
        perror("malloc");
        exit(1);
    }

    if(backing_file) {
        /* FIXME: check file size is same as mem size */
        FILE *fd = fopen(backing_file, "r");
        if(!fd) {
            perror("fopen");
            exit(1);
        }

        fread(state->mem, 1, size, fd);
        fclose(fd);
    }

    mem_reg->state = state;
    return mem_reg;
}

/**
 * Perform a memory operation on a registered memory address
 *
 * @param addr address of memory operation
 * @param memop a memop constant (@see MEMOP_READ, @see MEMOP_WRITE)
 * @param data byte to write (on MEMOP_WRITE)
 * @return data item (on MEMOP_READ)
 */
uint8_t mem_memop(hw_reg_t *hw, uint16_t addr, uint8_t memop, uint8_t data) {
    mem_state_t *mem_state = (mem_state_t*)(hw->state);

    if(memop == MEMOP_READ) {
        return mem_state->mem[addr - hw->remap[0].mem_start];
    }

    if(memop == MEMOP_WRITE) {
        mem_state->mem[addr - hw->remap[0].mem_start] = data;
        return 1;
    }

    return 0;
}
