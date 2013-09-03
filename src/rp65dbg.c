/* -*- mode: c -*-
 *
 * Copyright (C) 2012, 2013 Ron Pedde <ron@pedde.com>
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

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <fcntl.h>
#include <limits.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/time.h>

#include "emulator.h"
#include "libtui.h"
#include "stepwise.h"
#include "6502.h"
#include "debuginfo.h"
#include "redblack.h"

#define _INCLUDE_OPCODE_MAP
#include "opcodes.h"

#define CMD_HEIGHT 10
#define REGISTER_WIDTH 25
#define REGISTER_HEIGHT 10

#define DEFAULT_FIFO "/tmp/debug"
#define DEFAULT_EMU_PATH "./rp65emu"

int stepif_cmd_fd = -1;
int stepif_rsp_fd = -1;
int stepif_asy_fd = -1;
int stepif_debug_threshold = 2;
int stepif_running = 0;
int stepif_emu_pid = 0;

#define DISPLAY_MODE_DUMP    0
#define DISPLAY_MODE_DISASM  1
#define DISPLAY_MODE_WATCH   2

#define PIPE_READ_FD   0
#define PIPE_WRITE_FD  1

cpu_t stepif_state;
int stepif_display_mode;
int stepif_display_track;
int stepif_radix = 16;

uint16_t stepif_disassemble_addr;
uint16_t stepif_dump_addr;
uint16_t stepif_watch_addr;
uint16_t stepif_remote_caps = 0;

int stepif_follow_on_run=1;

#define D_FATAL 0
#define D_ERROR 1
#define D_WARN  2
#define D_INFO  3
#define D_DEBUG 4

window_t *pscreen;
window_t *pregisters;
window_t *pcommand;
window_t *pdisplay;
window_t *pstack;

char xlat[256];

struct rbtree *stepif_breakpoints = NULL;
struct rbtree *stepif_watches = NULL;

#define TOK_QUIT        0
#define TOK_VERSION     1
#define TOK_DUMP        2
#define TOK_DISASM      3
#define TOK_LOAD        4
#define TOK_SET         5
#define TOK_NEXT        6
#define TOK_BREAK       7
#define TOK_RUN         8
#define TOK_FOLLOW      9
#define TOK_DI         10
#define TOK_WATCH      11
#define TOK_RADIX      12
#define TOK_UNKNOWN   100
#define TOK_AMBIGUOUS 101

char *tokens[] = {
    "quit",
    "version",
    "dump",
    "disasm",
    "load",
    "set",
    "next",
    "break",
    "run",
    "follow",
    "di",
    "watch",
    "radix",
    NULL
};

typedef struct emu_hardware_t {
    char *serial_path;
    char *vnc_port;
} emu_hardware_t;

emu_hardware_t hwinfo = { NULL, NULL };

int stepif_command(dbg_command_t *command, uint8_t *out_data, dbg_response_t *retval, uint8_t **in_data);

/**
 * malloc and exit on error
 */
void *error_malloc(ssize_t size) {
    void *retval;

    retval = malloc(size);
    if(!retval) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    return retval;
}

/**
 * watch_add
 *
 * add a watch to the rb tree
 */
void watch_add(uint16_t addr) {
    uint16_t *paddr = error_malloc(sizeof(uint16_t));
    *paddr = addr;
    rbsearch((void*)paddr, stepif_watches);
}

/**
 * watch_remove
 *
 * delete a watch from the rb tree
 */
void watch_remove(uint16_t addr) {
    rbdelete((void*)&addr, stepif_watches);
}

/**
 * watch_set
 */
int watch_is_set(uint16_t addr) {
    if(rbfind((void*)&addr, stepif_watches))
        return 1;
    return 0;
}

/**
 * comparison function for redblack trees for address lookups
 * (breakpoint and watches)
 */
int addr_compare(const void *a, const void *b, const void *config) {
    uint16_t a1 = *(uint16_t *)a;
    uint16_t a2 = *(uint16_t *)b;

    if(a1 == a2)
        return 0;

    if(a1 < a2)
        return -1;

    return 1;
}

int stepif_simple_command(uint8_t cmd, uint16_t param1,
                          uint16_t param2, uint8_t *data,
                          uint16_t len) {
    dbg_command_t command;
    dbg_response_t response;

    command.cmd = cmd;
    command.param1 = param1;
    command.param2 = param2;
    if(data) {
        command.extra_len = len;
    } else {
        command.extra_len = 0;
    }

    return stepif_command(&command, data, &response, NULL);
}

void breakpoint_add(uint16_t addr) {
    uint16_t *paddr = error_malloc(sizeof(uint16_t));

    *paddr = addr;
    rbsearch((void*)paddr, stepif_breakpoints);

    /* if we have emu support for offloaded breakpoints,
       we need to push the breakpoint add to the
       emulator */
    if(stepif_remote_caps & CAP_BP)
        stepif_simple_command(CMD_BP, PARAM_BP_SET, addr, NULL, 0);
}

void breakpoint_remove(uint16_t addr) {
    rbdelete((void*)&addr, stepif_breakpoints);

    if(stepif_remote_caps & CAP_BP)
        stepif_simple_command(CMD_BP, PARAM_BP_DEL, addr, NULL, 0);
}

int breakpoint_is_set(uint16_t addr) {
    if(rbfind((void*)&addr, stepif_breakpoints))
        return 1;
    return 0;
}

/**
 * see if a file is writable, using select
 */
int is_writable(char *path) {
    int fd;
    fd_set writeset;
    int retval;
    struct timeval timeout;

    fd = open(path, O_WRONLY | O_NONBLOCK);

    /* this is really all I probably need */
    if(fd < 0)
        return 0;

    FD_ZERO(&writeset);
    FD_SET(fd, &writeset);

    timeout.tv_sec = 0;
    timeout.tv_usec = 0;

    while(((retval = select(FD_SETSIZE, NULL, &writeset, NULL, &timeout)) == -1) &&
          ((errno == EAGAIN) || (errno == EINTR)));

    if(retval == -1) {
        perror("select");
        close(fd);
        return 0;
    }

    retval = FD_ISSET(fd, &writeset);
    close(fd);

    return retval;
}

void stepif_debug(int level, char *format, ...) {
    va_list args;
    if(level > stepif_debug_threshold)
        return;

    tui_putstring(pcommand, " ");
    va_start(args, format);
    tui_putva(pcommand, format, args);
    va_end(args);
}

/**
 * make a fd blocking or non-blocking
 */
void fd_blocking(int fd, int blocking) {
    int flags = fcntl(fd, F_GETFL, 0);
    flags = blocking ? flags & ~O_NONBLOCK : flags | O_NONBLOCK;
    fcntl(fd, F_SETFL, flags);
}

/**
 * read an async message
 */
int stepif_process_message(int fd) {
    dbg_command_t cmd;
    ssize_t bytes_read;
    uint8_t *extra_data = NULL;

    bytes_read = read(fd, (char *)&cmd, sizeof(dbg_command_t));
    if(bytes_read != sizeof(dbg_command_t)) {
        stepif_debug(D_DEBUG, "Expecting to read %d bytes, read %d\n",
                     sizeof(dbg_command_t), bytes_read);
        return RESPONSE_ERROR;
    }

    if(cmd.extra_len) {
        extra_data = malloc(cmd.extra_len);
        if(!extra_data) {
            perror("malloc");
            exit(EXIT_FAILURE);
        }

        bytes_read = read(fd, (char*)extra_data, cmd.extra_len);
        if(bytes_read != cmd.extra_len) {
            stepif_debug(D_DEBUG, "Expecting to read %d bytes, read %d\n",
                         cmd.extra_len, bytes_read);
            return RESPONSE_ERROR;
        }
    }

    switch(cmd.cmd) {
    case ASYNC_NOTIFICATION:
        tui_putstring(pcommand, " %s\n", extra_data);
        break;
    case ASYNC_HWNOTIFY:
        switch(cmd.param1) {
        case HW_TYPE_SERIAL: /* serial */
            hwinfo.serial_path = strdup((char*)extra_data);
            break;
        case HW_TYPE_VNC: /* vnc */
            hwinfo.vnc_port = strdup((char*)extra_data);
            break;
        default: /* ?? */
            break;
        }

        tui_refresh(tui_getstatusbar());
        break;
    default:
        stepif_debug(D_DEBUG, "Bad async command: %d\n",
                     cmd.cmd);
        if(extra_data)
            free(extra_data);
        return RESPONSE_ERROR;
        break;
    }

    if(extra_data)
        free(extra_data);

    return RESPONSE_OK;
}

/**
 * eval a string, using current radix, or looking it up
 * as a symbol
 */
int stepif_eval(char *string, uint16_t *value) {
    char *endptr;

    assert(string);

    if(!string)
        return 0;

    if(string[0] == '$') {
        /* force radix 16 */
        *value = strtol(&string[1], NULL, 16);
        return 1;
    }

    if(string[0] == '%') {
        /* force 2 */
        *value = strtol(&string[1], NULL, 2);
        return 1;
    }

    if(string[0] == '#') {
        *value = strtol(&string[1], NULL, 10);
        return 1;
    }

    *value = strtol(string, &endptr, stepif_radix);
    if(*endptr) {
        /* must be a symbol? */
        return debuginfo_lookup_symbol(string, value);
    }
    return 1;
}


int get_token(char *string) {
    int token = 0;
    int found = 0;
    int count = 0;

    while(tokens[token]) {
        if(strcasecmp(string, tokens[token]) == 0)
            return token;
        if(strncasecmp(string, tokens[token], strlen(string)) == 0) {
            found = token;
            count++;
        }
        token++;
    }

    if(count == 1)
        return found;

    if(count > 1)
        return TOK_AMBIGUOUS;

    return TOK_UNKNOWN;
}

int stepif_command(dbg_command_t *command, uint8_t *out_data, dbg_response_t *retval, uint8_t **in_data) {
    ssize_t bytes_read;
    ssize_t bytes_written;

    stepif_debug(D_DEBUG, "Writing command of type %02x (size %d)\n", command->cmd, sizeof(dbg_command_t));
    bytes_written = write(stepif_cmd_fd, (char *)command, sizeof(dbg_command_t));

    if(command->extra_len) {
        bytes_written += write(stepif_cmd_fd, (char *)out_data, command->extra_len);
    }

    stepif_debug(D_DEBUG, "Wrote %d bytes\n", bytes_written);

    /* read the result and return it */
    bytes_read = read(stepif_rsp_fd, (char *)retval, sizeof(dbg_response_t));
    if(bytes_read != sizeof(dbg_response_t)) {
        stepif_debug(D_DEBUG, "Expecting to read %d bytes, read %d\n",
                     sizeof(dbg_response_t), bytes_read);
        *in_data = (uint8_t*)"Bad read";
        return RESPONSE_ERROR;
    }

    stepif_debug(D_DEBUG, "Read result: %s, extra_len: %d (size %d)\n",
                 retval->response_status ? "Failure" : "Success",
                 retval->extra_len, bytes_read);

    if(retval->response_status == RESPONSE_OK && retval->extra_len) {
        *in_data = (uint8_t*)malloc(retval->extra_len);
        if(!*in_data) {
            perror("malloc");
            exit(EXIT_FAILURE);
        }

        stepif_debug(D_DEBUG, "Attempting to read %d bytes\n", retval->extra_len);
        read(stepif_rsp_fd, *in_data, retval->extra_len);
    }

    return retval->response_status;
}

/**
 * split a string on delimiter boundaries, filling
 * a string-pointer array.
 *
 * The user must free both the first element in the array,
 * and the array itself.
 *
 * @param s string to split
 * @param delimiters boundaries to split on
 * @param argvp an argv array to be filled
 * @returns number of tokens
 */
int util_split(char *s, char *delimiters, char ***argvp) {
    int i;
    int numtokens;
    const char *snew;
    char *t;
    char *tokptr;
    char *tmp;
    char *fix_src, *fix_dst;

    if ((s == NULL) || (delimiters == NULL) || (argvp == NULL))
        return -1;
    *argvp = NULL;
    snew = s + strspn(s, delimiters);
    if ((t = malloc(strlen(snew) + 1)) == NULL)
        return -1;

    strcpy(t, snew);
    numtokens = 1;
    tokptr = NULL;
    tmp = t;

    tmp = s;
    while(*tmp) {
        if(strchr(delimiters,*tmp) && (*(tmp+1) == *tmp)) {
            tmp += 2;
        } else if(strchr(delimiters,*tmp)) {
            numtokens++;
            tmp++;
        } else {
            tmp++;
        }
    }

    if ((*argvp = malloc((numtokens + 1)*sizeof(char *))) == NULL) {
        free(t);
        return -1;
    }

    if (numtokens == 0)
        free(t);
    else {
        tokptr = t;
        tmp = t;
        for (i = 0; i < numtokens; i++) {
            while(*tmp) {
                if(strchr(delimiters,*tmp) && (*(tmp+1) != *tmp))
                    break;
                if(strchr(delimiters,*tmp)) {
                    tmp += 2;
                } else {
                    tmp++;
                }
            }
            *tmp = '\0';
            tmp++;
            (*argvp)[i] = tokptr;

            fix_src = fix_dst = tokptr;
            while(*fix_src) {
                if(strchr(delimiters,*fix_src) && (*(fix_src+1) == *fix_src)) {
                    fix_src++;
                }
                *fix_dst++ = *fix_src++;
            }
            *fix_dst = '\0';

            tokptr = tmp;
        }
    }

    *((*argvp) + numtokens) = NULL;
    return numtokens;
}

/**
 * dispose of the argv set that was created in util_split
 *
 * @param argv string array to delete
 */
void util_dispose_split(char **argv) {
    if(!argv)
        return;

    if(argv[0])
        free(argv[0]);

    free(argv);
}

/**
 * determine if a string is a valid opcode.
 * @see fixup_line
 *
 * @param str string to test for opcodeness
 */
int is_opcode(char *str) {
    opcode_info_t *p = cpu_opcode_info;
    while(p->mnemonic) {
        if(strcasecmp(p->mnemonic, str) == 0)
            return 1;
        p++;
    }

    return 0;
}

/**
 * try to straighten out crazy formatting and tab
 * stuff in source code.
 *
 * @param line data to fix.  will return a mallocd string,
 * caller must free.
 */
int fixup_line(uint16_t addr, char **line) {
    char *newline;
    char *start;
    char *token;
    char *label = NULL;
    char *comment = NULL;

    int line_size = 80;
    char *src, *dst;
    int last_space = 0;

    src = dst = *line;

    while(*src && (*src == ' ' || *src == '\t'))
        src++;

    while(*src) {
        if(*src == '\t' || *src == ' ')
            last_space = 1;
        else if (*src == ';') {
            *dst++ = 0;
            comment = dst;
        } else if (*src == '\n' || *src == '\r') {
            /* nothing */
        } else {
            if(last_space) {
                *dst++ = ' ';
                last_space = 0;
            }
            *dst = *src;
            dst++;
        }
        src++;
    }
    *dst = '\0';

    newline = (char*)error_malloc(line_size);
    memset(newline, 0, line_size);

    start = *line;
    token = strsep(&start, "\t ");
    if(!token) {
        *line = newline;
        return 0;
    }

    if(debuginfo_lookup_addr(addr, &label)) {
        sprintf(newline, "%-14s", label);
        if(is_opcode(token)) {
            strcat(newline, token);
        } else {
            if((strcasecmp(label, token) == 0) ||
               ((strlen(token) == strlen(label) + 1) &&
                (strncasecmp(label, token, strlen(token)) == 0))) {
                /* token is the label... ignore it */
            } else {
                /* token is something else... .db directive, etc */
                snprintf(&newline[strlen(newline)], line_size - strlen(newline), "%s", token);
            }
        }
    } else {
        sprintf(newline, "              %s", token);
    }

    while((token = strsep(&start, " ")) != NULL) {
        strncat(newline, " ", line_size - strlen(newline));
        strncat(newline, token, line_size - strlen(newline));
    }

    /* append the comments at 28 */
    while(strlen(newline) < 35) {
        strcat(newline, " ");
    }

    /* snprintf(&newline[strlen(newline)], line_size - strlen(newline), "%*s", (int)30 - strlen(newline), ""); */
    if(comment) {
        strncat(newline, ";", line_size - strlen(newline));
        strncat(newline, comment, line_size - strlen(newline));
    }

    *line = newline;
    return 1;
}

/**
 * process a command from user input
 *
 * @param cmd entire command line
 */
void process_command(char *cmd) {
    char **argv;
    int argc;
    dbg_command_t command;
    dbg_response_t response;
    uint8_t *data = NULL;
    int result;
    int token;
    uint16_t temp;
    uint16_t old_ip = stepif_state.ip;
    static int stall_count = 0;

    memset((void*)&command, 0, sizeof(command));
    memset((void*)&response, 0, sizeof(response));

    argc = util_split(cmd, " \t", &argv);

    if(!argc) {
        util_dispose_split(argv);
        return;
    }

    stepif_debug(D_DEBUG, "Processing command\n");
    token = get_token(argv[0]);

    switch(token) {
    case TOK_VERSION:
        command.cmd = CMD_VER;
        if((result = stepif_command(&command, NULL, &response, &data)) == RESPONSE_OK) {
            stepif_debug(D_DEBUG, "Response: %s\n",data);
        }
        break;

    case TOK_DUMP:
        if(argc != 2) {
            stepif_debug(D_ERROR, "Usage: dump <$addr>\n");
            break;
        }

        if(!stepif_eval(argv[1], &temp)) {
            stepif_debug(D_ERROR, "Invalid dump address\n");
            break;
        }

        stepif_dump_addr = (uint16_t)temp;
        stepif_display_mode = DISPLAY_MODE_DUMP;
        tui_refresh(pdisplay);
        break;

    case TOK_DISASM:
        if(argc != 2) {
            stepif_debug(D_ERROR, "Usage: disasm <$addr>\n");
            break;
        }

        if(!stepif_eval(argv[1], &temp)) {
            stepif_debug(D_ERROR, "Invalid address\n");
            break;
        }

        stepif_disassemble_addr = (uint16_t)temp;
        stepif_display_mode = DISPLAY_MODE_DISASM;
        tui_refresh(pdisplay);
        break;

    case TOK_LOAD:
        if(argc != 3) {
            stepif_debug(D_ERROR, "Usage: load <file> <$addr>\n");
            break;
        }

        if(!stepif_eval(argv[2], &temp)) {
            stepif_debug(D_ERROR, "Invalid address\n");
            break;
        }

        FILE *infile;
        size_t bytes_read;
        uint8_t *extra_data=NULL;

        infile = fopen(argv[1], "r");
        if(!infile) {
            tui_putstring(pcommand, " Error loading: %s\n", strerror(errno));
        } else {
            data = malloc(1024);

            while((bytes_read = fread(data, 1, 256, infile)) > 0) {
                command.cmd = CMD_WRITEMEM;
                command.param1 = temp;
                command.extra_len = bytes_read;
                stepif_command(&command, data, &response, &extra_data);
                temp+= bytes_read;
                if(extra_data) {
                    free(extra_data);
                    extra_data = NULL;
                }
            }
            fclose(infile);
            tui_putstring(pcommand, " Loaded file.\n");
        }

        /* check and see if we have a .dbg file for this */
        char *basename = strdup(argv[1]);
        char *dot;
        if((dot=strrchr(basename, '.'))) {
            *dot = '\0';
            asprintf(&dot, "%s.%s", basename, "dbg");
            if(debuginfo_load(dot)) {
                tui_putstring(pcommand, " Loaded debug symbols\n");
            } else {
                tui_putstring(pcommand, " No debug symbols found\n");
            }

            free(dot);
        }
        free(basename);

        if(stepif_display_mode == DISPLAY_MODE_DISASM)
            tui_refresh(pdisplay);
        break;

    case TOK_SET:
        if(argc != 3) {
            stepif_debug(D_ERROR, "Usage: SET <reg> <$value>");
            break;
        }

        if(!stepif_eval(argv[2], &temp)) {
            stepif_debug(D_ERROR, "Invalid value\n");
            break;
        }

        command.cmd = CMD_SET;
        command.param2 = temp;

        if(strcasecmp(argv[1],"a") == 0) {
            command.param1 = PARAM_A;
        } else if(strcasecmp(argv[1],"x") == 0) {
            command.param1 = PARAM_X;
        } else if(strcasecmp(argv[1],"y") == 0) {
            command.param1 = PARAM_Y;
        } else if(strcasecmp(argv[1],"P") == 0) {
            command.param1 = PARAM_P;
        } else if(strcasecmp(argv[1],"sp") == 0) {
            command.param1 = PARAM_SP;
        } else if(strcasecmp(argv[1],"IP") == 0) {
            command.param1 = PARAM_IP;
        } else {
            stepif_debug(D_ERROR, "Bad register");
            break;
        }

        if((result = stepif_command(&command, NULL, &response, &data)) == RESPONSE_OK) {
            tui_putstring(pcommand, " Set\n",data);
        } else {
            tui_putstring(pcommand, " Error setting: %s\n",data);
        }
        tui_refresh(pregisters);
        if(command.param1 == PARAM_SP)
            tui_refresh(pstack);

        if((stepif_display_track) && (command.param1 == PARAM_IP) && (stepif_display_mode == DISPLAY_MODE_DISASM)) {
            stepif_disassemble_addr = command.param2;
            tui_refresh(pdisplay);
        }
        break;

    case TOK_NEXT:
        old_ip = stepif_state.ip;

        command.cmd = CMD_NEXT;
        stepif_command(&command, NULL, &response, &data);
        memcpy((void*)&stepif_state, (void*)data, sizeof(cpu_t));

        if ((stepif_state.ip == old_ip) && (stepif_running)) {
            stall_count++;
            if (stall_count > 10) {
                stepif_running = 0;
                tui_putstring(pcommand, " Processor stalled\n");
            }
        } else {
            stall_count = 0;
        }

        if ((stepif_follow_on_run) || (!stepif_running)) {
            tui_refresh(pregisters);
            tui_refresh(pstack);
            if((stepif_display_mode == DISPLAY_MODE_DISASM) && (stepif_display_track)) {
                stepif_disassemble_addr = stepif_state.ip;
                tui_refresh(pdisplay);
            }
        }

        if (breakpoint_is_set(stepif_state.ip) && stepif_running) {
            stepif_running = 0;
            tui_putstring(pcommand, " Breakpoint $%04x reached\n", stepif_state.ip);
            tui_refresh(pregisters);
            tui_refresh(pstack);
            if((stepif_display_mode == DISPLAY_MODE_DISASM) && (stepif_display_track)) {
                stepif_disassemble_addr = stepif_state.ip;
                tui_refresh(pdisplay);
            }
        }
        break;

    case TOK_BREAK:
        /* allow break with no arg to break on current line */
        if(argc > 2) {
            stepif_debug(D_ERROR, "Usage: break [<$addr>]\n");
            break;
        }

        if(argc == 1) {
            temp = stepif_state.ip;
        } else {
            if(!stepif_eval(argv[1], &temp)) {
                stepif_debug(D_ERROR, "Invalid addr\n");
                break;
            }
        }

        if (breakpoint_is_set((uint16_t)temp)) {
            breakpoint_remove((uint16_t)temp);
            tui_putstring(pcommand, " Breakpoint unset\n");
        } else {
            breakpoint_add((uint16_t)temp);
            tui_putstring(pcommand, " Breakpoint set\n");
        }

        if((stepif_display_track) && (stepif_display_mode == DISPLAY_MODE_DISASM)) {
            tui_refresh(pdisplay);
        }
        break;

    case TOK_RUN:
        if(stepif_remote_caps & CAP_RUN) {
        }

        stepif_running = 1;
        /* turn off blocking getch so we can get chars when free-running */
        tui_window_nodelay(pcommand);
        tui_putstring(pcommand, " Free-running: <ENTER> to stop\n");
        break;

    case TOK_FOLLOW:
        stepif_follow_on_run = !stepif_follow_on_run;
        tui_putstring(pcommand, " Follow mode is now %s\n",
                      stepif_follow_on_run ? "on" : "off");
        break;

    case TOK_DI:
        if (argc != 2) {
            stepif_debug(D_ERROR, "Usage: di <filename>\n");
            break;
        }

        if(!debuginfo_load(argv[1])) {
            tui_putstring(pcommand, " Error loading debug symbols\n");
            break;
        }

        tui_putstring(pcommand, " Loaded.\n");

        if(stepif_display_mode == DISPLAY_MODE_DISASM)
            tui_refresh(pdisplay);
        break;

    case TOK_WATCH:
        if (argc != 2) {
            stepif_debug(D_ERROR, "Usage: watch <addr>\n");
            break;
        }

        if(!stepif_eval(argv[1], &temp)) {
            stepif_debug(D_ERROR, "Invalid addr\n");
            break;
        }

        if (watch_is_set((uint16_t)temp)) {
            watch_remove((uint16_t)temp);
            tui_putstring(pcommand, " Watch unset\n");
        } else {
            watch_add((uint16_t)temp);
            tui_putstring(pcommand, " Watch set\n");
        }

        if((stepif_display_mode == DISPLAY_MODE_DUMP) ||
           (stepif_display_mode == DISPLAY_MODE_WATCH))
            tui_refresh(pdisplay);
        break;

    case TOK_RADIX:
        if(argc != 2) {
            stepif_debug(D_ERROR, "Usage: radix <base>, where base is 10 or 16\n");
            break;
        }

        if(strcmp(argv[1], "16") == 0)
            stepif_radix = 16;
        else if(strcmp(argv[1], "10") == 0)
            stepif_radix = 10;
        else
            stepif_debug(D_ERROR, "Radix must be 10 or 16\n");
        break;

    case TOK_AMBIGUOUS:
        tui_putstring(pcommand, " Ambiguous command\n");
        break;

    case TOK_UNKNOWN:
    default:
        tui_putstring(pcommand, " Unknown command\n");
        break;
    }

    if(data) {
        free(data);
        data = NULL;
    }

    util_dispose_split(argv);
}

void display_update_dump(void) {
    int result;
    dbg_command_t command;
    dbg_response_t response;
    uint8_t *data;
    uint8_t line;
    int pos;

    if(stepif_display_mode != DISPLAY_MODE_DUMP)
        return;

    memset((void*)&command, 0, sizeof(command));
    memset((void*)&response, 0, sizeof(response));

    command.cmd = CMD_READMEM;
    command.param1 = stepif_dump_addr;

    if(16 * (pdisplay->height - 2) > 0xffff)
        command.param2 = 0xffff - command.param1;
    else
        command.param2 = 16 * (pdisplay->height - 2);

    stepif_debug(D_DEBUG, "Asking for $%04x bytes\n", command.param2);

    result = stepif_command(&command, NULL, &response, &data);
    if(response.response_status == RESPONSE_ERROR)
        return;

    tui_setpos(pdisplay, 0, 1);
    for(line = 0; line < (pdisplay->height - 2); line++) {
        if(stepif_dump_addr + (line * 16) <= 0xffff) {
            tui_putstring(pdisplay, "     %04x: ", stepif_dump_addr + (line * 16));
            for(pos = 0; pos < 16; pos++) {
                if((line*16 + pos)  < command.param2) {
                    if(watch_is_set(line * 16 + pos + stepif_dump_addr))
                        tui_setcolor(pdisplay, 1);
                    tui_putstring(pdisplay, "%02x", data[(line * 16) + pos]);
                    tui_resetcolor(pdisplay);
                    tui_putstring(pdisplay, " %s", pos == 7 ? " ":"");
                } else {
                    tui_putstring(pdisplay, "   %s", pos == 7 ? " " : "");
                }
            }
            tui_putstring(pdisplay, "  ");

            for(pos = 0; pos < 16; pos++) {
                if(watch_is_set(line * 16 + pos + stepif_dump_addr))
                    tui_setcolor(pdisplay, 1);

                if((line*16 + pos)  < command.param2) {
                    tui_putstring(pdisplay, "%c", xlat[data[(line * 16) + pos]]);
                tui_resetcolor(pdisplay);
                }
            }
        }

        tui_putstring(pdisplay,"\n");
    }

    free(data);
}


/**
 * update the watch display
 */
void display_update_watch(void) {
    int result;
    dbg_command_t command;
    dbg_response_t response;
    uint8_t *data;
    uint8_t line;
    uint16_t addr;
    const void *addrp;
    uint8_t value;

    if(stepif_display_mode != DISPLAY_MODE_WATCH)
        return;

    addr = stepif_watch_addr;

    line = 0;

    tui_setpos(pdisplay, 0, 1);

    while(line < pdisplay->height - 2) {
        line++;
        addrp = rblookup(RB_LUGTEQ, (void*)&addr, stepif_watches);
        if(addrp) {
            addr = *(uint16_t*)addrp;

            /* this is terrible */

            memset((void*)&command, 0, sizeof(command));
            memset((void*)&response, 0, sizeof(response));

            command.cmd = CMD_READMEM;
            command.param1 = addr;
            command.param2 = 1;

            stepif_debug(D_DEBUG, "Asking for 1 byte at $%04x\n", addr);

            result = stepif_command(&command, NULL, &response, &data);
            if(response.response_status == RESPONSE_ERROR)
                return;

            value = data[0];
            free(data);

            tui_putstring(pdisplay, "     %04x: %02x\n", addr, value);
            addr++;
        } else {
            tui_putstring(pdisplay, "\n");
        }
    }

}


/**
 * display_update
 *
 * update the main display window
 */
void display_update_disasm(void) {
    int result;
    dbg_command_t command;
    dbg_response_t response;
    uint8_t *data = NULL;
    uint8_t line;
    int pos;

    if(stepif_display_mode != DISPLAY_MODE_DISASM)
        return;

    memset((void*)&command, 0, sizeof(command));
    memset((void*)&response, 0, sizeof(response));

    command.cmd = CMD_READMEM;
    command.param1 = stepif_disassemble_addr;

    if(3 * (pdisplay->height - 2) > 0xffff)
        command.param2 = 0xffff - command.param1;
    else
        command.param2 = 3 * (pdisplay->height - 2);

    stepif_debug(D_DEBUG, "Asking for $%04x bytes\n", command.param2);

    result = stepif_command(&command, NULL, &response, &data);
    if(response.response_status == RESPONSE_ERROR)
        return;

    uint8_t opcode;
    uint8_t b;
    uint16_t w;
    uint16_t rel;
    int len;

    char linebuffer[80];

    opcode_t *popcode;

    tui_setpos(pdisplay, 0, 1);
    line = 0;
    pos = stepif_disassemble_addr;

    while(line < (pdisplay->height - 2)) {
        if(pos > 0xffff) {
            tui_putstring(pdisplay, "\n");
            line++;
            continue;
        }

        if(breakpoint_is_set(pos)) {
            tui_setcolor(pdisplay, 1);
            tui_putstring(pdisplay, " *");
        } else {
            tui_putstring(pdisplay, "  ");
        }

        if(stepif_state.ip == pos)
            tui_putstring(pdisplay, "=> ");
        else
            tui_putstring(pdisplay, "   ");

        tui_putstring(pdisplay, "%04X: ", pos);
        opcode = data[pos - stepif_disassemble_addr];
        b = data[(pos - stepif_disassemble_addr) + 1];
        w = data[(pos - stepif_disassemble_addr) + 2] << 8 | b;

        popcode = &cpu_opcode_map[opcode];
        len = cpu_addressing_mode_length[popcode->addressing_mode];

        rel = b & 0x80 ? pos - ((b + len -1) ^ 0xFF) : pos + b + len;

        if(len == 1)
            tui_putstring(pdisplay, "%02x         ", opcode);
        else if (len == 2)
            tui_putstring(pdisplay, "%02x %02x      ", opcode, b);
        else
            tui_putstring(pdisplay, "%02x %02x %02x   ", opcode, b, w >> 8);

        tui_putstring(pdisplay, "%s ", cpu_opcode_mnemonics[popcode->opcode_family]);

        switch(popcode->addressing_mode) {
        case CPU_ADDR_MODE_IMPLICIT:
            tui_putstring(pdisplay,"          ");
            break;
        case CPU_ADDR_MODE_ACCUMULATOR:
            tui_putstring(pdisplay,"A         ");
            break;
        case CPU_ADDR_MODE_IMMEDIATE:
            tui_putstring(pdisplay, "#$%02x      ", b);
            break;
        case CPU_ADDR_MODE_RELATIVE:
            tui_putstring(pdisplay, "$%04x     ", rel);
            break;
        case CPU_ADDR_MODE_ABSOLUTE:
            tui_putstring(pdisplay, "$%04x     ", w);
            break;
        case CPU_ADDR_MODE_ABSOLUTE_X:
            tui_putstring(pdisplay, "$%04x,X   ", w);
            break;
        case CPU_ADDR_MODE_ABSOLUTE_Y:
            tui_putstring(pdisplay, "$%04x,Y   ", w);
            break;
        case CPU_ADDR_MODE_ZPAGE:
            tui_putstring(pdisplay, "$%02x       ", b);
            break;
        case CPU_ADDR_MODE_ZPAGE_X:
            tui_putstring(pdisplay, "$%02x,X     ", b);
            break;
        case CPU_ADDR_MODE_ZPAGE_Y:
            tui_putstring(pdisplay, "$%02x,Y     ", b);
            break;
        case CPU_ADDR_MODE_INDIRECT:
            tui_putstring(pdisplay, "($%04x)   ", w);
            break;
        case CPU_ADDR_MODE_IND_X:
            tui_putstring(pdisplay, "($%02x,X)   ", b);
            break;
        case CPU_ADDR_MODE_IND_Y:
            tui_putstring(pdisplay, "($%02x),Y   ", b);
            break;
        }

        if(popcode->opcode_undocumented)
            tui_putstring(pdisplay, " ?? ");
        else
            tui_putstring(pdisplay, "    ");

        if(debuginfo_getline(pos, linebuffer, sizeof(linebuffer))) {
            tui_setcolor(pdisplay, 6);
            char *linedata = linebuffer;
            fixup_line(pos, &linedata);

            /* line is 36 characters already.... trim the debug line */
            if((36 + strlen(linedata)) > (pdisplay->width - 2)) {
                int max_len = (pdisplay->width - 2) - 36;
                if(max_len > 0) {
                    linedata[max_len] = 0;
                }
            }

            tui_putstring(pdisplay, linedata);
            free(linedata);
            tui_resetcolor(pdisplay);
        }

        pos += len;
        tui_putstring(pdisplay, "\n");
        line++;
        tui_resetcolor(pdisplay);
    }

    if(data)
        free(data);
}

void display_update(void) {
    if(stepif_display_mode == DISPLAY_MODE_DUMP) {
        display_update_dump();
    } else if(stepif_display_mode == DISPLAY_MODE_DISASM) {
        display_update_disasm();
    } else if(stepif_display_mode == DISPLAY_MODE_WATCH) {
        display_update_watch();
    }
}

/**
 * statusbar_update
 *
 * update the status bar.  This will contain
 * hardware details we are interested in
 * (serial port, etc)
 */
void statusbar_update(void) {
    window_t *statusbar = tui_getstatusbar();

    tui_setpos(statusbar, 0, 0);
    /* tui_cleareol(statusbar); */

    tui_putstring(statusbar, " Ready | ");
    if (hwinfo.serial_path) {
        tui_putstring(statusbar, "Com: %s | ", hwinfo.serial_path);
    }

    if(hwinfo.vnc_port) {
        tui_putstring(statusbar, "VNC: %s | ", hwinfo.vnc_port);
    }
}

/**
 * update the register window
 *
 */
void register_update(void) {
    int result;
    dbg_command_t command;
    dbg_response_t response;
    cpu_t *cpu_state;


    memset((void*)&command, 0, sizeof(command));
    memset((void*)&response, 0, sizeof(response));

    command.cmd = CMD_REGS;

    result = stepif_command(&command, NULL, &response, (uint8_t**)&cpu_state);
    if(response.response_status == RESPONSE_ERROR)
        return;

    tui_setpos(pregisters,0,1);
    tui_putstring(pregisters, " %-4s $%02x    ", "A:", cpu_state->a);
    tui_putstring(pregisters, " %-4s $%02x\n", "X:", cpu_state->x);
    tui_putstring(pregisters, " %-4s $%02x    ", "Y:", cpu_state->y);
    tui_putstring(pregisters, " %-4s $%02x\n", "SP:", cpu_state->sp);
    tui_putstring(pregisters, " %-4s $%04x  ", "IP:", cpu_state->ip);
    tui_putstring(pregisters, " %-4s $%02x\n\n", "P:", cpu_state->p);

    /* tui_putstring(pregisters, " %s $%02x", "Flags:", cpu_state->p); */

    tui_putstring(pregisters, "  Flags: NV-BDIZC\n");
    tui_putstring(pregisters, "         %c%c-%c%c%c%c%c\n",
                  cpu_state->p & FLAG_N ? '1' : '0',
                  cpu_state->p & FLAG_V ? '1' : '0',
                  cpu_state->p & FLAG_B ? '1' : '0',
                  cpu_state->p & FLAG_D ? '1' : '0',
                  cpu_state->p & FLAG_I ? '1' : '0',
                  cpu_state->p & FLAG_Z ? '1' : '0',
                  cpu_state->p & FLAG_C ? '1' : '0');

    memcpy((void*)&stepif_state, (void*)cpu_state, sizeof(cpu_t));
    free(cpu_state);
}


/**
 * update the stack window
 *
 */
void stack_update(void) {
    int result;
    dbg_command_t command;
    dbg_response_t response;
    cpu_t *cpu_state;
    uint8_t *data;

    uint16_t startaddr;

    memset((void*)&command, 0, sizeof(command));
    memset((void*)&response, 0, sizeof(response));

    command.cmd = CMD_REGS;

    result = stepif_command(&command, NULL, &response, (uint8_t**)&cpu_state);
    if(response.response_status == RESPONSE_ERROR)
        return;

    /* now, get the stack memory */
    memset((void*)&command, 0, sizeof(command));
    memset((void*)&response, 0, sizeof(response));

    command.cmd = CMD_READMEM;
    command.param1 = 0x0100;
    command.param2 = 256;

    result = stepif_command(&command, NULL, &response, &data);
    if(response.response_status == RESPONSE_ERROR)
        return;

    tui_setpos(pregisters, 0, 1);
    startaddr = 0x01ff;

    if((255 - cpu_state->sp) > (pstack->height / 2)) {
        startaddr = 0x100 + cpu_state->sp + ((pstack->height - 2) / 2);
        if (startaddr < (0xff + (pstack->height - 2)))
            startaddr = 0xff + (pstack->height - 2);

        /* if((startaddr + (pstack->height / 2)) > pstack->height) */
        /*     startaddr = 0x100 + pstack->height; */
    }

    tui_setpos(pstack,0,1);
    for(uint16_t offset = 0; offset < (pstack->height - 2); offset++) {
        uint16_t addr = startaddr - offset;

        if((addr - 0x100) == cpu_state->sp)
            tui_putstring(pstack, " => ");
        else
            tui_putstring(pstack, "    ");

        tui_putstring(pstack, "$%04x: $%02x\n", addr,
                      data[addr - 0x100]);

    }

    free(cpu_state);
    free(data);
}

void fkey_callback(int inchar) {
    if(inchar == KEY_F(1))
        stepif_display_mode = DISPLAY_MODE_DISASM;
    else if(inchar == KEY_F(2))
        stepif_display_mode = DISPLAY_MODE_DUMP;
    else if(inchar == 9) {
        switch(stepif_display_mode) {
        case DISPLAY_MODE_DUMP:
            stepif_display_mode = DISPLAY_MODE_DISASM;
            break;
        case DISPLAY_MODE_DISASM:
            stepif_display_mode = DISPLAY_MODE_WATCH;
            break;
        case DISPLAY_MODE_WATCH:
            stepif_display_mode = DISPLAY_MODE_DUMP;
            break;
        }
    }

    /* should really handle page down and page up too */

    tui_refresh(pdisplay);
}


void usage(char *a0) {
    printf("Usage: %s [-c configfile] [-e]\n\n", a0);
    printf("  -c <configfile>  configfile to use for the emulator\n");
    printf("  -e               control the emulator as part of debugger\n");
}

/**
 * start up the emulator, returning pid
 * of the child process.  If it cannot
 * be started, returns a pid of 0
 *
 * @param configfile for the emulator (-c option)
 * @returns pid on success, 0 otherwise
 */
pid_t start_emu(char *configfile, char *cmd_path, char *emu_path) {
    pid_t pid;
    int pipe_fd[2];

    char *args[] = { "rp65emu",
                     "-c",
                     configfile,
                     "-s",
                     NULL };

    if (pipe(pipe_fd) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    /* fork and dup and whatnot */
    pid = fork();
    if(!pid) { /* parent */
        for(int x=0; x < 3; x++) {
            close(x);
        }

        close(pipe_fd[PIPE_READ_FD]);
        dup2(pipe_fd[PIPE_WRITE_FD], 1);
        dup2(pipe_fd[PIPE_WRITE_FD], 2);

        /* stdin/out/etc duped, let's exec! */
        if(execvp(emu_path, args) == -1) {
            perror("execvp");
            exit(EXIT_FAILURE);
        }

        /* shouldn't get here */
    }

    /* wait for the command socket to become writable.
     * monitor startup to see if we get a tty notification
     */
    close(pipe_fd[PIPE_WRITE_FD]);

    for(int x = 0; x < 20; x++) {
        if(is_writable(cmd_path))
            return pid;

        usleep(100000);
    }

    if(kill(pid, 0) != -1) {
        /* child is running, cmd fifo isn't ready */
        kill(pid, SIGTERM);
    }

    fprintf(stderr, "command buffer (%s) did not become writable\n", cmd_path);
    exit(EXIT_FAILURE);
}

/**
 * exit callback
 */
void stepif_exit_callback(void) {
    int stat;

    if(stepif_emu_pid) {
        if(kill(stepif_emu_pid, 0) != -1)
            kill(stepif_emu_pid, SIGTERM);
        waitpid(stepif_emu_pid, &stat, 0);
    }
}

int stepif_open_fifo(char *base_path, char *extension) {
    char *fifo_path = NULL;
    int fd;

    fifo_path = malloc(strlen(base_path) + strlen(extension) + 1);
    if(!fifo_path) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    strcpy(fifo_path, base_path);
    strcat(fifo_path, extension);

    fd = open(fifo_path, O_RDWR);
    free(fifo_path);

    if(fd < 0) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    return fd;
}


/**
 * main
 */
int main(int argc, char *argv[]) {
    char buffer[80];
    char *emu_path = DEFAULT_EMU_PATH;
    char *base_path = DEFAULT_FIFO;
    char *startup_script = NULL;
    int pos;
    int step_char;
    int result;
    dbg_command_t command;
    dbg_response_t response;

    int control_emu = 0;
    char *config_file = NULL;
    int option;

    while((option = getopt(argc, argv, "c:e:s:b:")) != -1) {
        switch(option) {
        case 'c':
            config_file = optarg;
            control_emu = 1;
            break;

        case 'e':
            emu_path = optarg;
            control_emu = 1;
            break;

        case 's':
            startup_script = optarg;
            break;

        case 'b':
            base_path = optarg;
            break;

        default:
            usage(argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    if (control_emu && !config_file) {
        fprintf(stderr, "must specify config file when starting emu\n");
        exit(EXIT_FAILURE);
    }

    stepif_watches = rbinit(addr_compare, NULL);
    stepif_breakpoints = rbinit(addr_compare, NULL);

    if((!stepif_watches) || (!stepif_breakpoints)) {
        fprintf(stderr, "error initializing watches\n");
        exit(EXIT_FAILURE);
    }
    debuginfo_init();

    stepif_display_mode = DISPLAY_MODE_DUMP;

    stepif_disassemble_addr = 0x8000;
    stepif_dump_addr = 0x8000;
    stepif_watch_addr = 0;

    stepif_display_track = 1;

    memset((void*)xlat, '.', sizeof(xlat));
    for(pos = ' '; pos <= '~'; pos++)
        xlat[pos] = pos;

    char *fifo_path;
    fifo_path = malloc(strlen(base_path) + 5);
    if(!fifo_path) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    strcpy(fifo_path, base_path);
    strcat(fifo_path, "-cmd");

    if(!is_writable(fifo_path)) {
        if(!control_emu) {
            fprintf(stderr, "Emulator not running.\n");
            exit(EXIT_FAILURE);
        } else {
            stepif_emu_pid = start_emu(config_file, fifo_path, emu_path);
            if(!stepif_emu_pid) {
                fprintf(stderr, "could not start emulator\n");
                exit(EXIT_FAILURE);
            }
        }
    }

    free(fifo_path);

    stepif_cmd_fd = stepif_open_fifo(base_path, "-cmd");
    stepif_rsp_fd = stepif_open_fifo(base_path, "-rsp");
    stepif_asy_fd = stepif_open_fifo(base_path, "-asy");

    pscreen = tui_init(NULL, 1);
    tui_set_exit_callback(stepif_exit_callback);

    pregisters = tui_window(pscreen->width - REGISTER_WIDTH, 0, REGISTER_WIDTH, REGISTER_HEIGHT, 1, "Registers", COLORSET_GREEN);
    pstack = tui_window(pscreen->width - REGISTER_WIDTH, REGISTER_HEIGHT, REGISTER_WIDTH, pscreen->height - (CMD_HEIGHT + 1) - REGISTER_HEIGHT, 1, "Stack", COLORSET_GREEN);
    pcommand = tui_window(0, pscreen->height - (CMD_HEIGHT + 1), pscreen->width, CMD_HEIGHT, 1, "Command", COLORSET_DEFAULT);
    pdisplay = tui_window(0, 0, pscreen->width - REGISTER_WIDTH, pscreen->height - (CMD_HEIGHT+1), 1, "6502 Explorer", COLORSET_BLUE);

    tui_window_callback(pregisters, register_update);
    tui_window_callback(pstack, stack_update);
    tui_window_callback(pdisplay, display_update);
    tui_window_callback(tui_getstatusbar(), statusbar_update);

    tui_inputwindow(pcommand);

    /* get capabilities first */
    memset((void*)&command, 0, sizeof(command));
    memset((void*)&response, 0, sizeof(response));

    command.cmd = CMD_CAPS;

    result = stepif_command(&command, NULL, &response, NULL);
    if(response.response_status == RESPONSE_ERROR) {
        fprintf(stderr, "Error: cannot get remote caps\n");
        exit(EXIT_FAILURE);
    }

    stepif_remote_caps = response.response_value;

    tui_putstring(pcommand, " Remote caps: %04X\n", stepif_remote_caps);

    /* this is kind of lame, but let's walk through and read off all
     * the async events */
    fd_blocking(stepif_asy_fd, 0);
    while(stepif_process_message(stepif_asy_fd) == RESPONSE_OK);
    fd_blocking(stepif_asy_fd, 1);

    tui_putstring(pcommand, " refreshing statusbar...");
    tui_refresh(tui_getstatusbar());
    tui_putstring(pcommand, " done\n");

    /* run the startup script */
    if(startup_script) {
        FILE *ss = fopen(startup_script, "r");
        if(!ss) {
            tui_putstring(pcommand, " Error reading opening startup script\n");
        } else {
            while(fgets(buffer, sizeof(buffer), ss)) {
                while(buffer[strlen(buffer) - 1] == '\n')
                    buffer[strlen(buffer) - 1] = '\0';
                tui_putstring(pcommand, " > %s\n", buffer);
                process_command(buffer);
            }
            fclose(ss);
        }
    }

    /* start running the step loop */
    while(1) {
        if(stepif_running) {
            step_char = tui_getch(pcommand);
            switch(step_char) {
            case KEY_ENTER:
            case 0x0a:
                stepif_running = 0;
                tui_window_delay(pcommand);
                break;
            case ERR:
                break;
            /* default: */
            /*     sprintf(buffer, "%02x: %02d: %c\n", step_char, step_char, step_char); */
            /*     tui_putstring(pcommand, buffer); */
            /*     break; */
            }
            process_command("next");
        } else {
            /* make sure we are in delay mode */
            tui_window_delay(pcommand);
            tui_putstring(pcommand, " > ");
            tui_getstring(pcommand, buffer, sizeof(buffer), fkey_callback);
            if(strcmp(buffer,"quit") == 0)
                break;

            process_command(buffer);
        }
    }

    memset((void*)&command, 0, sizeof(command));
    memset((void*)&response, 0, sizeof(response));

    command.cmd = CMD_STOP;

    result = stepif_command(&command, NULL, &response, NULL);

    /* atexit will reap the child */
    exit(EXIT_SUCCESS);
}
