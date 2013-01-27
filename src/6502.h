/*
 * Copyright (C) 2009-2013 Ron Pedde (ron@pedde.com)
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

#ifndef _6502_H_
#define _6502_H_

extern void cpu_init(void);
extern void cpu_deinit(void);
extern uint8_t cpu_execute(void);

typedef struct cpu_t_struct {
    uint8_t p;
    uint8_t a;
    uint8_t x;
    uint8_t y;
    uint16_t ip;
    uint8_t sp;
    uint8_t irq;
} cpu_t;

extern cpu_t cpu_state;

#define FLAG_N  0x80
#define FLAG_V  0x40
#define FLAG_UNUSED 0x20
#define FLAG_B  0x10
#define FLAG_D  0x08
#define FLAG_I  0x04
#define FLAG_Z  0x02
#define FLAG_C  0x01

#define FLAG_IRQ 0x01
#define FLAG_NMI 0x02

#endif /* _6502_H_ */
