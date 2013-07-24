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

#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "stepwise.h"
#include "debug.h"
#include "6502.h"
#include "memory.h"
#include "redblack.h"

#define DEFAULT_DEBUG_FIFO "/tmp/debug";
#define VERSION "0.1"

static int step_cmd_fd = -1;
static int step_rsp_fd = -1;
static int step_asy_fd = -1;

static struct rbtree *step_bp = NULL;
static int step_run = 0;

#define STEP_BAD_REG "Bad register specified"
#define STEP_BAD_FILE "Cannot open file"


void step_return(uint8_t result, uint16_t retval,
                 uint16_t len, uint8_t *data) {
    dbg_response_t response;

    response.response_status = result;
    response.response_value = retval;
    response.extra_len = len;

    write(step_rsp_fd, (char*)&response, sizeof(response));
    DEBUG("Returning response: %s", result ? "Error" : "Success");
    if(len) {
        DEBUG("Returning %d bytes of extra data", len);
        write(step_rsp_fd, (char*) data, response.extra_len);
    }
}

void step_send_async(uint8_t message, uint16_t param1,
                     uint16_t param2, uint16_t len,
                     uint8_t *data) {
    dbg_command_t cmd;

    cmd.cmd = message;
    cmd.param1 = param1;
    cmd.param2 = param2;
    cmd.extra_len = len;
    DEBUG("Sending async cmd %d", cmd.cmd);

    write(step_asy_fd, (char*)&cmd, sizeof(cmd));
    if(len) {
        DEBUG("Writing %d bytes of extra data", len);
        write(step_asy_fd, (char*)data, cmd.extra_len);
    }
}

/*
 * FIXME(rp): this should probably be serialized
 */
void stepwise_notification(char *format, ...) {
    char *buffer = NULL;

    va_list args;
    va_start(args, format);
    vasprintf(&buffer, format, args);
    va_end(args);

    /* send it out! */
    step_send_async(ASYNC_NOTIFICATION, 0, 0, strlen(buffer) + 1,
                    (uint8_t*) buffer);

    if(buffer)
        free(buffer);
}

ssize_t readblock(int fd, void *buf, size_t size) {
    char *bufp;
    ssize_t bytesread;
    size_t bytestoread;
    size_t totalbytes;

    for (bufp = buf, bytestoread = size, totalbytes = 0;
         bytestoread > 0;
         bufp += bytesread, bytestoread -= bytesread) {
        bytesread = read(fd, bufp, bytestoread);
        DEBUG("Read %d bytes", bytesread);
        if ((bytesread == 0) && (totalbytes == 0))
            return 0;
        if (bytesread == 0) {
            errno = EINVAL;
            return -1;
        }
        if ((bytesread) == -1 && (errno != EINTR))
            return -1;
        if (bytesread == -1)
            bytesread = 0;
        totalbytes += bytesread;
    }
    return totalbytes;
}

void step_eval(dbg_command_t *cmd, uint8_t *data) {
    char *version = VERSION;
    uint8_t *memory;
    uint16_t start, len, current;
    uint16_t *paddr = malloc(sizeof(uint16_t));

    switch(cmd->cmd) {
    case CMD_NOP:
        step_return(RESPONSE_OK, 0, 0, NULL);
        break;

    case CMD_VER:
        step_return(RESPONSE_OK, 0, strlen(version)+1,(uint8_t*)version);
        break;

    case CMD_REGS:
        step_return(RESPONSE_OK, 0, sizeof(cpu_t),(uint8_t*)&cpu_state);
        break;

    case CMD_READMEM:
        start = cmd->param1;
        len = cmd->param2;

        DEBUG("Attemping to read $%04x bytes from $%04x", len, start);

        memory = (uint8_t*)malloc(len);
        if(!memory) {
            perror("malloc");
            exit(EXIT_FAILURE);
        }

        for(current = 0; current < len; current++) {
            memory[current] = memory_read(current + start);
        }

        step_return(RESPONSE_OK, 0, len, memory);
        free(memory);
        break;

    case CMD_WRITEMEM:
        start = cmd->param1;
        len = cmd->extra_len;

        DEBUG("Attempting to write $%04x bytes to $%04x", len, start);

        for(current = 0; current < len; current++) {
            memory_write(current + start, data[current]);
        }

        step_return(RESPONSE_OK, 0, 0, NULL);
        break;

        /* FIXME: rewrite by loading the file and bytewise writing it */

    /* case CMD_LOAD: */
    /*     if(memory_load(cmd->param1, (char*)data, 0) != E_MEM_SUCCESS) { */
    /*         step_return(RESPONSE_ERROR, strlen(STEP_BAD_FILE)+1, (uint8_t*)STEP_BAD_FILE); */
    /*     } else { */
    /*         step_return(RESPONSE_OK, 0, NULL); */
    /*     } */
    /*     break; */

    case CMD_SET:
        switch(cmd->param1) {
        case PARAM_A:
            cpu_state.a = cmd->param2;
            break;
        case PARAM_X:
            cpu_state.x = cmd->param2;
            break;
        case PARAM_Y:
            cpu_state.y = cmd->param2;
            break;
        case PARAM_P:
            cpu_state.p = cmd->param2;
            break;
        case PARAM_SP:
            cpu_state.sp = cmd->param2;
            break;
        case PARAM_IP:
            cpu_state.ip = cmd->param2;
            break;
        default:
            step_return(RESPONSE_ERROR, 0, strlen(STEP_BAD_REG) + 1,
                        (uint8_t*)STEP_BAD_REG);
            return;
        }

        step_return(RESPONSE_OK, 0, 0, NULL);
        break;

    case CMD_NEXT:
        cpu_execute();
        step_return(RESPONSE_OK, 0, sizeof(cpu_t),(uint8_t*)&cpu_state);
        break;

    case CMD_CAPS:
        step_return(RESPONSE_OK, CAP_BP | CAP_RUN, 0, NULL);
        break;

    case CMD_BP:
        if(!paddr) {
            perror("malloc");
            exit(EXIT_FAILURE);
        }
        *paddr = cmd->param2;

        switch(cmd->param1) {
        case PARAM_BP_SET:
            rbsearch((void*)paddr, step_bp);
            break;
        case PARAM_BP_DEL:
            rbdelete((void*)paddr, step_bp);
            break;
        }
        step_return(RESPONSE_OK, 0, 0, NULL);
        break;

    case CMD_RUN:
        step_run = 1;
        step_return(RESPONSE_OK, 0, 0, NULL);
        break;

    case CMD_STOP:
        step_run = 0;
        step_return(RESPONSE_OK, 0, 0, NULL);
        break;

    default:
        ERROR("Unknown command from debugger: %d", cmd->cmd);
        exit(EXIT_FAILURE);
    }
}

int make_fifo(char *base, char *extension) {
    char *file;
    struct stat sb;
    int fd = -1;

    if(!base)
        base = DEFAULT_DEBUG_FIFO;

    file = malloc(strlen(base) + 5);
    if(!file) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    strcpy(file, base);
    strcat(file, extension);

    if((stat(file, &sb) == -1) && (errno == ENOENT)) {
        /* make it */
        DEBUG("Creating fifo: %s", file);
        mkfifo(file, 0600);
    } else {
        if(!(sb.st_mode & S_IFIFO)) {
            FATAL("File '%s' exists, and is not a fifo", file);
            exit(EXIT_FAILURE);
        }
    }

    DEBUG("Opening cmd fifo");
    fd = open(file, O_RDWR);
    if(fd == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    free(file);

    return fd;
}

void step_init(char *fifo) {
    DEBUG("Initializing fds");

    step_cmd_fd = make_fifo(fifo, "-cmd");
    step_rsp_fd = make_fifo(fifo, "-rsp");
    step_asy_fd = make_fifo(fifo, "-asy");
}


void stepwise_debugger(void) {
    dbg_command_t cmd;
    uint8_t *data = NULL;
    ssize_t bytes_read;

    DEBUG("Waiting for input");

    while(1) {
        bytes_read = readblock(step_cmd_fd, (char*) &cmd, sizeof(cmd));
        if(bytes_read != sizeof(cmd)) {
            FATAL("Bad read.  Expecting %d, read %d",
                  sizeof(cmd), bytes_read);
            exit(EXIT_FAILURE);
        }

        DEBUG("Received command: %02x",cmd.cmd);

        if(cmd.cmd == CMD_STOP) {
            INFO("Exiting emulator at debugger request");
            step_return(RESPONSE_OK, 0, 0, NULL);
            return;
        }

        if(data) free(data);
        data = NULL;

        if(cmd.extra_len) {
            DEBUG("Reading %d bytes of extra info", cmd.extra_len);
            data = (uint8_t*)malloc(cmd.extra_len);
            if(!data) {
                perror("malloc");
                exit(EXIT_FAILURE);
            }

            if(readblock(step_cmd_fd,data,cmd.extra_len) != cmd.extra_len) {
                FATAL("Short read from remote debugger");
                exit(EXIT_FAILURE);
            }
        }

        DEBUG("Evaluating command");
        step_eval(&cmd, data);

        if(data) {
            free(data);
            data = NULL;
        }
        DEBUG("Waiting for input");
    }

    close(step_cmd_fd);
    close(step_rsp_fd);
}
