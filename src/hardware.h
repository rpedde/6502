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

#ifndef _HARDWARE_H_
#define _HARDWARE_H_

#include <stdint.h>

#define HW_FAMILY_VIDEO  0x01
#define HW_FAMILY_IO     0x02
#define HW_FAMILY_SERIAL 0x04
#define HW_FAMILY_MEMORY 0x08
#define HW_FAMILY_OTHER  0x10

#define MEMOP_READ       0
#define MEMOP_WRITE      1

typedef struct mem_remap_t {
    uint16_t mem_start;
    uint16_t mem_end;
    uint8_t readable;
    uint8_t writable;
} mem_remap_t;

typedef struct hw_reg_t {
    int hw_family;
    uint8_t (*memop)(struct hw_reg_t *, uint16_t, uint8_t, uint8_t);
    uint8_t (*eventloop)(void *, int);
    int irq_asserted;
    int nmi_asserted;
    void *state;
    int remapped_regions;
    mem_remap_t remap[];
} hw_reg_t;

typedef struct hw_config_item_t {
    const char *key;
    const char *value;
} hw_config_item_t;

typedef struct hw_config_t {
    int config_items;
    hw_config_item_t item[];
} hw_config_t;

typedef struct hw_callbacks_t {
    void (*hw_logger)(int, char *, ...);
    void (*hw_notify)(char *, ...);
    void (*irq_change)(void);
    void (*nmi_change)(void);
} hw_callbacks_t;


#endif /* _HARDWARE_H_ */
