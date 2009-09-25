/*
 *
 */

#ifndef _STEPWISE_H_
#define _STEPWISE_H_


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

/* Terminate the emulator
 */
#define CMD_STOP     0xFF



typedef struct dbg_command_t_struct {
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

typedef struct dbg_response_t_struct {
    uint8_t response_status;
    uint16_t extra_len;
} dbg_response_t;

extern void stepwise_debugger(char *fifo);

#endif /* _STEPWISE_H_ */
