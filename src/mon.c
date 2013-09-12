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
#include <termios.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "6502.h"
#include "stepwise.h"
#include "debug.h"
#include "redblack.h"

#define DEFAULT_DEBUG_FIFO "/tmp/debug";
#define VERSION "0.1"

static int step_cmd_fd = -1;
static int step_rsp_fd = -1;
static int step_asy_fd = -1;
static int step_dbg_fd = -1;

static int regs_valid = 0;

/* static struct rbtree *step_bp = NULL; */
/* static int step_run = 0; */

#define STEP_BAD_REG "Bad register specified"
#define STEP_BAD_FILE "Cannot open file"

cpu_t cpu_state;

#define MON_CMD_PING       0
#define MON_CMD_GETREG     1
#define MON_CMD_GETMEM     2

uint8_t *mon_request(uint8_t *data, int len, int resp_len) {
    uint8_t *result = NULL;
    uint8_t ir;
    uint8_t *current;
    ssize_t bytes_read;

    DEBUG("Sending %d bytes (monitor cmd $%02X)", len, data[0]);
    if(write(step_dbg_fd, data, len) != len) {
        FATAL("Error writing to debug fd: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    DEBUG("Reading intermediate result");
    if((read(step_dbg_fd, &ir, 1) != 1) || (ir == 0)) {
        FATAL("Error reading ir result: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }
    DEBUG("IR success");

    if(resp_len) {
        DEBUG("Allocating %d bytes of reponse data", resp_len);
        if((result = (uint8_t*)malloc(resp_len + 1)) == NULL) {
            FATAL("Malloc");
            exit(EXIT_FAILURE);
        }

        memset(result, 0, resp_len + 1);

        DEBUG("Reading %d bytes of reponse data", resp_len);

        current = result;

        while(resp_len) {
            bytes_read = read(step_dbg_fd, current, resp_len);
            if(bytes_read > 0) {
                current += bytes_read;
                resp_len -= bytes_read;

                DEBUG("Read %d bytes", bytes_read);
            } else {
                FATAL("Error reading from debug fd: %s", strerror(errno));
                exit(EXIT_FAILURE);
            }
        }
    }

    DEBUG("Returning result");
    return result;
}

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

void get_regs(void) {
    uint8_t *reply_data;

    if(!regs_valid) {
        reply_data = mon_request((uint8_t*)"\1", 1, 7);
        cpu_state.a = reply_data[0];
        cpu_state.x = reply_data[1];
        cpu_state.y = reply_data[2];
        cpu_state.sp = reply_data[3];
        cpu_state.p = reply_data[4];
        cpu_state.ip = reply_data[6] << 8 | reply_data[5];
        free(reply_data);

        regs_valid = 1;
    }
}

void set_regs(void) {
    uint8_t cpu_data[8];
    uint8_t *reply_data;

    cpu_data[0] = 3;
    cpu_data[1] = cpu_state.a;
    cpu_data[2] = cpu_state.x;
    cpu_data[3] = cpu_state.y;
    cpu_data[4] = cpu_state.sp;
    cpu_data[5] = cpu_state.p;
    cpu_data[6] = (uint8_t)(cpu_state.ip & 0xff);
    cpu_data[7] = (uint8_t)(cpu_state.ip >> 8);

    reply_data = mon_request((uint8_t*)cpu_data, 8, 1);

    free(reply_data);
}

void step_eval(dbg_command_t *cmd, uint8_t *data) {
    char *version = VERSION;
    uint8_t *reply_data, *memory;
    uint16_t start, len;

    switch(cmd->cmd) {
    case CMD_NOP:
        DEBUG("Debugger request: CMD_NOP");
        step_return(RESPONSE_OK, 0, 0, NULL);
        break;

    case CMD_VER:
        DEBUG("Debugger request: CMD_VER");
        step_return(RESPONSE_OK, 0, strlen(version)+1,(uint8_t*)version);
        break;

    case CMD_REGS:
        DEBUG("Debugger request: CMD_REGS");
        get_regs();
        step_return(RESPONSE_OK, 0, sizeof(cpu_t),(uint8_t*)&cpu_state);
        break;

    case CMD_READMEM:
        DEBUG("Debugger request: CMD_READMEM");
        start = cmd->param1;
        len = cmd->param2;

        uint8_t req_data[4];
        uint8_t requested_len;
        uint8_t *pmemory;
        int len_left = len;

        DEBUG("Attemping to read $%04x bytes from $%04x", len, start);

        memory = (uint8_t*)malloc(len);
        if(!memory) {
            perror("malloc");
            exit(EXIT_FAILURE);
        }

        pmemory = memory;

        /* we break this up into multiple blocks of FF or less */
        while(len_left) {
            if(len_left > 255) {
                requested_len = 255;
            } else {
                requested_len = len_left;
            }

            req_data[0] = 2;
            req_data[1] = (uint8_t)(start & 0xff);
            req_data[2] = (uint8_t)(start >> 8);
            req_data[3] = (uint8_t)requested_len;

            DEBUG("READ_MEM Request:  %02X %02X %02X",
                  (uint8_t)(start & 0xFF),
                  (uint8_t)(start >> 8),
                  (uint8_t)requested_len);

            reply_data = mon_request(req_data, 4, requested_len);

            len_left -= requested_len;
            memcpy(pmemory, reply_data, requested_len);
            pmemory += requested_len;
            start += requested_len;

            free(reply_data);
        }

        step_return(RESPONSE_OK, 0, len, memory);
        free(memory);
        break;

    case CMD_WRITEMEM:
        start = cmd->param1;
        len = cmd->extra_len;

        DEBUG("Attempting to write $%04x bytes to $%04x", len, start);

        /* for(current = 0; current < len; current++) { */
        /*     memory_write(current + start, data[current]); */
        /* } */

        step_return(RESPONSE_OK, 0, 0, NULL);
        break;

    case CMD_SET:
        get_regs();

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

        set_regs();

        step_return(RESPONSE_OK, 0, 0, NULL);
        break;

    case CMD_NEXT:
        /* cpu_execute(); */
        step_return(RESPONSE_OK, 0, sizeof(cpu_t),(uint8_t*)&cpu_state);
        break;

    case CMD_CAPS:
        step_return(RESPONSE_OK, CAP_BP | CAP_RUN, 0, NULL);
        break;

    case CMD_BP:
        /* if(!paddr) { */
        /*     perror("malloc"); */
        /*     exit(EXIT_FAILURE); */
        /* } */
        /* *paddr = cmd->param2; */

        /* switch(cmd->param1) { */
        /* case PARAM_BP_SET: */
        /*     rbsearch((void*)paddr, step_bp); */
        /*     break; */
        /* case PARAM_BP_DEL: */
        /*     rbdelete((void*)paddr, step_bp); */
        /*     break; */
        /* } */
        step_return(RESPONSE_OK, 0, 0, NULL);
        break;

    case CMD_RUN:
        /* step_run = 1; */
        step_return(RESPONSE_OK, 0, 0, NULL);
        break;

    case CMD_STOP:
        /* step_run = 0; */
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
    close(step_asy_fd);
}

void usage(void) {
    printf("rp65mon <options>\n\n");
    printf("Valid Options:\n");
    printf("-d <debuglevel>      debug level (0-5, 5 most verbose)\n");
    printf("-b <path>            base fifo path\n");
    printf("-p <path>            path to serial device\n");
}



/**
 * main
 */
int main(int argc, char *argv[]) {
    int option;
    char *path = NULL;
    char *base_path = DEFAULT_DEBUG_FIFO;
    int debuglevel = DBG_WARN;
    struct termios pty_termios;

    while((option = getopt(argc, argv, "d:p:b:")) != -1) {
        switch(option) {
        case 'd':
            debuglevel = atoi(optarg);
            break;
        case 'p':
            path = optarg;
            break;
        case 'b':
            base_path = optarg;
            break;
        default:
            usage();
            exit(EXIT_FAILURE);
            break;
        }
    }

    if(!path) {
        usage();
        exit(EXIT_FAILURE);
    }

    debug_level(debuglevel);

    DEBUG("Opening monitor fd");
    if((step_dbg_fd = open(path, O_RDWR)) == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    DEBUG("Termioing monitor fd");
    tcgetattr(step_dbg_fd, &pty_termios);
    cfmakeraw(&pty_termios);
    tcsetattr(step_dbg_fd, TCSANOW, &pty_termios);

    DEBUG("Initializing debug fifos");
    step_init(base_path);

    DEBUG("Testing connection to monitor");
    mon_request((uint8_t*)"\0", 1, 1);
    DEBUG("Connection established");

    stepwise_debugger();
}
