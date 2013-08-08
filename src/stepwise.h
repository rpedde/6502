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

#ifndef _STEPWISE_H_
#define _STEPWISE_H_

#include <stdint.h>

/* NOP relies with one byte of magic data:
 * 0xEA
 */
#define CMD_NOP      0x00

/* VER responds with a zero-terminated string
 * describing version
 */
#define CMD_VER      0x01

/* REGS responds with a cpu_state structure, see
 * 6502.h
 */
#define CMD_REGS     0x02

/* READMEM requires param1 to be memory block to start
 * read at, and param 2 the length to read.  Returns
 * the raw data.
 */
#define CMD_READMEM  0x03

/* WRITEMEM writes a block of memory, starting at
 * param1, for param2 bytes.
 */
#define CMD_WRITEMEM 0x04

/* LOAD loads a binary object.  Param1 is address
 * to load at, while extra data is filename (zero terminated)
 */
#define CMD_LOAD     0x05

/* Load a register specified in param1 with the value specified
 * in param2
 */
#define CMD_SET      0x06

#define PARAM_A  0x01
#define PARAM_X  0x02
#define PARAM_Y  0x03
#define PARAM_P  0x04
#define PARAM_SP 0x05
#define PARAM_IP 0x06

/* execute next step
 */
#define CMD_NEXT 0x07

/* Get capabilities
 */
#define CMD_CAPS 0x08

#define CAP_BP    0x01
#define CAP_WATCH 0x02
#define CAP_RUN   0x04

/* Add and remove breakpoints
 */
#define CMD_BP   0x09

#define PARAM_BP_SET 0x01
#define PARAM_BP_DEL 0x02

/* free-run until breakpoint (or cmd_step)
 */
#define CMD_RUN  0x0A

/* stop free-running, and go back to
   stepwise execution */
#define CMD_STEP 0x0B

/* Terminate the emulator
 */
#define CMD_STOP     0xFF

typedef struct __attribute__((packed)) dbg_command_t {
    uint8_t cmd;
    uint16_t param1;
    uint16_t param2;
    uint16_t extra_len;
} dbg_command_t;


/* Reponse data is specified in request comments.  An error
 * response will return an error string as the extra
 * data
 */
#define RESPONSE_OK    0x00
#define RESPONSE_ERROR 0x01

typedef struct __attribute__((packed)) dbg_response_t {
    uint8_t response_status;
    uint16_t response_value;
    uint16_t extra_len;
} dbg_response_t;

/* reponse is literal string notification from emulator to
 * whatever is driving the emulator.  This is typically information
 * that must be passed -- vnc port, what pty, etc.
 *
 * param1 - unused
 * param2 - unused
 * extra_len - size of message (zero terminated string)
 * extra_data - string message
 */
#define ASYNC_NOTIFICATION 0x00


extern void stepwise_debugger(void);
extern void stepwise_notification(char *format, ...);
extern void step_init(char *fifo);


#endif /* _STEPWISE_H_ */
