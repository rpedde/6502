/*
 * Copyright (C) 2009 Ron Pedde (ron@pedde.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _MEMORY_H_
#define _MEMORY_H_

#include "hardware.h"

/* The actual reading and writing of RAM/ROM should be done by
   a driver, as this is hardware, not cpu dependent */

#define E_MEM_SUCCESS 0
#define E_MEM_ALLOC   1
#define E_MEM_FOPEN   2

extern int memory_init(void);
extern void memory_deinit(void);

extern uint8_t memory_read(uint16_t addr);
extern void memory_write(uint16_t addr, uint8_t data);
extern int memory_load(const char *name, const char *module, hw_config_t *config);
extern void memory_run_eventloop(void);

#endif /* _MEMORY_H_ */
