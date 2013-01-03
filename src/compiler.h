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

#ifndef _COMPILER_H_
#define _COMPILER_H_

#include "opcodes.h"


#define TYPE_INSTRUCTION 0x01
#define TYPE_DATA        0x02
#define TYPE_LABEL       0x03
#define TYPE_OFFSET      0x04


#define Y_TYPE_BYTE  0
#define Y_TYPE_WORD  1
#define Y_TYPE_LABEL 2
#define Y_TYPE_ARITH 3


typedef struct value_t_struct {
    int type;
    uint8_t byte;
    uint16_t word;
    char *label;
    struct value_t_struct *left;
    struct value_t_struct *right;
} value_t;

typedef struct opdata_t_struct {
    int type;
    uint8_t opcode_family;   /* INSTRUCTION */
    uint8_t opcode;          /* INSTRUCTION */
    uint8_t addressing_mode; /* INSTRUCTION */
    value_t *value;          /* INSTRUCTION */
    int needs_fixup;         /* INSTRUCTION */
    int len;                 /* ALL */
    int promote;
    int demote;
    int offset;
    int line;
} opdata_t;

typedef struct symtable_t_struct {
    char *label;
    value_t *value;
    struct symtable_t_struct *next;
} symtable_t;
extern symtable_t symtable;

#define CPU_ADDR_MODE_UNKNOWN    0x80
#define CPU_ADDR_MODE_UNKNOWN_X  0x81
#define CPU_ADDR_MODE_UNKNOWN_Y  0x82

extern uint16_t compiler_offset;     /* current compiler offset */
extern void add_opdata(opdata_t*);
extern value_t *y_lookup_symbol(char *label);
extern value_t *y_evaluate_val(value_t *value, int line, uint16_t addr);
extern void y_add_symtable(char *label, value_t *value);

#endif /* _COMPILER_H_ */
