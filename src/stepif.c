#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

#include "emulator.h"
#include "libtui.h"
#include "stepwise.h"
#include "6502.h"

#define _INCLUDE_OPCODE_MAP
#include "opcodes.h"

#define CMD_HEIGHT 10
#define REGISTER_WIDTH 25

#define DEFAULT_FIFO "/tmp/debug"

int stepif_cmd_fd = -1;
int stepif_rsp_fd = -1;
int stepif_debug_threshold = 2;
int stepif_running = 0;

#define DISPLAY_MODE_DUMP    0
#define DISPLAY_MODE_DISASM  1
#define DISPLAY_MODE_UNKNOWN 2

cpu_t stepif_state;
int stepif_display_mode;
int stepif_display_track;
uint16_t stepif_display_addr;
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

char xlat[256];

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
    NULL
};

typedef struct breakpoint_list_t {
    uint16_t breakpoint;
    struct breakpoint_list_t *pnext;
} breakpoint_list_t;

breakpoint_list_t breakpoint_list;

int breakpoint_add(uint16_t addr) {
    breakpoint_list_t *pnew;

    pnew = malloc(sizeof(breakpoint_list_t));
    if(!pnew) {
        perror("malloc");
        exit(1);
    }

    pnew->breakpoint = addr;
    pnew->pnext = breakpoint_list.pnext;
    breakpoint_list.pnext = pnew;

    return 1;
}

int breakpoint_remove(uint16_t addr) {
    breakpoint_list_t *last, *current;

    last = &breakpoint_list;
    current = last->pnext;

    while(current) {
        if (current->breakpoint == addr) {
            last->pnext = current->pnext;
            free(current);
            return 1;
        }
        last = current;
        current = current->pnext;
    }

    return 0;
}

int breakpoint_is_set(uint16_t addr) {
    breakpoint_list_t *current = breakpoint_list.pnext;

    while(current) {
        if (current->breakpoint == addr)
            return 1;
        current = current->pnext;
    }
    return 0;
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


void stepif_debug(int level, char *format, ...) {
    va_list args;
    if(level > stepif_debug_threshold)
        return;

    va_start(args, format);
    tui_putva(pcommand, format, args);
    va_end(args);
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

void process_command(char *cmd) {
    char **argv;
    int argc;
    dbg_command_t command;
    dbg_response_t response;
    uint8_t *data = NULL;
    int result;
    int token;
    unsigned int temp;
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
        if(sscanf(argv[1], "$%x", &temp) != 1) {
            stepif_debug(D_ERROR, "Bad address format (should be $xxxx)\n");
            break;
        }

        stepif_display_addr = (uint16_t)temp;
        stepif_display_mode = DISPLAY_MODE_DUMP;
        tui_refresh(pdisplay);
        break;

    case TOK_DISASM:
        if(argc != 2) {
            stepif_debug(D_ERROR, "Usage: disasm <$addr>\n");
            break;
        }
        if(sscanf(argv[1], "$%x", &temp) != 1) {
            stepif_debug(D_ERROR, "Bad address format (should be $xxxx)\n");
            break;
        }

        stepif_display_addr = (uint16_t)temp;
        stepif_display_mode = DISPLAY_MODE_DISASM;
        tui_refresh(pdisplay);
        break;

    case TOK_LOAD:
        if(argc != 3) {
            stepif_debug(D_ERROR, "Usage: load <file> <$addr>\n");
            break;
        }

        if(sscanf(argv[2], "$%x", &temp) != 1) {
            stepif_debug(D_ERROR, "Bad address format (should be $xxxx)\n");
            break;
        }

        command.cmd = CMD_LOAD;
        command.param1 = (uint16_t)temp;
        command.extra_len = strlen(argv[1]) + 1;

        if((result = stepif_command(&command, (uint8_t*)argv[1], &response, &data)) == RESPONSE_OK) {
            tui_putstring(pcommand, "Loaded\n",data);
        } else {
            tui_putstring(pcommand, "Error loading: %s\n",data);
        }
        break;

    case TOK_SET:
        if(argc != 3) {
            stepif_debug(D_ERROR, "Usage: SET <reg> <$value>");
            break;
        }

        if(sscanf(argv[2], "$%x", &temp) != 1) {
            stepif_debug(D_ERROR, "Bad address format (should be $xxxx)\n");
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
            tui_putstring(pcommand, "Set\n",data);
        } else {
            tui_putstring(pcommand, "Error setting: %s\n",data);
        }
        tui_refresh(pregisters);

        if((stepif_display_track) && (command.param1 == PARAM_IP) && (stepif_display_mode == DISPLAY_MODE_DISASM)) {
            stepif_display_addr = command.param2;
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
                tui_putstring(pcommand, "Processor stalled\n");
            }
        } else {
            stall_count = 0;
        }

        if ((stepif_follow_on_run) || (!stepif_running)) {
            tui_refresh(pregisters);
            if((stepif_display_mode == DISPLAY_MODE_DISASM) && (stepif_display_track)) {
                stepif_display_addr = stepif_state.ip;
                tui_refresh(pdisplay);
            }
        }

        if (breakpoint_is_set(stepif_state.ip) && stepif_running) {
            stepif_running = 0;
            tui_putstring(pcommand, "Breakpoint $%04x reached\n", stepif_state.ip);
            tui_refresh(pregisters);
            if((stepif_display_mode == DISPLAY_MODE_DISASM) && (stepif_display_track)) {
                stepif_display_addr = stepif_state.ip;
                tui_refresh(pdisplay);
            }
        }
        break;

    case TOK_BREAK:
        if(argc != 2) {
            stepif_debug(D_ERROR, "Usage: break <$addr>\n");
            break;
        }

        if(sscanf(argv[1], "$%x", &temp) != 1) {
            stepif_debug(D_ERROR, "Bad address format (should be $xxxx)\n");
            break;
        }

        if (breakpoint_is_set((uint16_t)temp)) {
            breakpoint_remove((uint16_t)temp);
            tui_putstring(pcommand, "Breakpoint unset\n");
        } else {
            breakpoint_add((uint16_t)temp);
            tui_putstring(pcommand, "Breakpoint set\n");
        }

        if((stepif_display_track) && (stepif_display_mode == DISPLAY_MODE_DISASM)) {
            tui_refresh(pdisplay);
        }
        break;

    case TOK_RUN:
        stepif_running = 1;
        /* turn off blocking getch so we can get chars when free-running */
        tui_window_nodelay(pcommand);
        tui_putstring(pcommand, "Free-running: <ENTER> to stop\n");
        break;

    case TOK_FOLLOW:
        stepif_follow_on_run = !stepif_follow_on_run;
        tui_putstring(pcommand, "Follow mode is now %s\n",
                      stepif_follow_on_run ? "on" : "off");
        break;

    case TOK_AMBIGUOUS:
        tui_putstring(pcommand, "Ambiguous command\n");
        break;

    case TOK_UNKNOWN:
    default:
        tui_putstring(pcommand, "Unknown command\n");
        break;
    }

    if(data) {
        free(data);
        data = NULL;
    }

    util_dispose_split(argv);
}

void display_update(void) {
    int result;
    dbg_command_t command;
    dbg_response_t response;
    uint8_t *data;
    uint8_t line;
    int pos;
    static int last_display_type = DISPLAY_MODE_UNKNOWN;

    memset((void*)&command, 0, sizeof(command));
    memset((void*)&response, 0, sizeof(response));

    command.cmd = CMD_READMEM;
    command.param1 = stepif_display_addr;
    if(16 * (pdisplay->height - 2) > 0xffff)
        command.param2 = 0xffff - command.param1;
    else
        command.param2 = 16 * (pdisplay->height - 2);

    stepif_debug(D_DEBUG, "Asking for $%04x bytes\n", command.param2);

    result = stepif_command(&command, NULL, &response, &data);
    if(response.response_status == RESPONSE_ERROR)
        return;

    if(stepif_display_mode == DISPLAY_MODE_DUMP) {
        tui_setpos(pdisplay, 0, 1);
        for(line = 0; line < (pdisplay->height - 2); line++) {
            if(stepif_display_addr + (line * 16) <= 0xffff) {
                tui_putstring(pdisplay, " %04x: ", stepif_display_addr + (line * 16));
                for(pos = 0; pos < 16; pos++) {
                    if((line*16 + pos)  < command.param2) {
                        tui_putstring(pdisplay, "%02x %s", data[(line * 16) + pos], pos == 7 ? " " : "");
                    } else {
                        tui_putstring(pdisplay, "   %s", pos == 7 ? " " : "");
                    }
                }
                tui_putstring(pdisplay, "  ");

                for(pos = 0; pos < 16; pos++) {
                    if((line*16 + pos)  < command.param2) {
                        tui_putstring(pdisplay, "%c", xlat[data[(line * 16) + pos]]);
                    }
                }
            }

            tui_putstring(pdisplay,"\n");
        }
    } else if(stepif_display_mode == DISPLAY_MODE_DISASM) {
        uint8_t opcode;
        uint8_t b;
        uint16_t w;
        uint16_t rel;
        int len;

        opcode_t *popcode;

        tui_setpos(pdisplay, 0, 1);
        line = 0;
        pos = stepif_display_addr;

        while(line < (pdisplay->height - 2)) {
            if(pos > 0xffff) {
                tui_putstring(pdisplay, "\n");
                line++;
                continue;
            }

            if(breakpoint_is_set(pos))
                tui_putstring(pdisplay, " *");
            else
                tui_putstring(pdisplay, "  ");

            if(stepif_state.ip == pos)
                tui_putstring(pdisplay, "=> ");
            else
                tui_putstring(pdisplay, "   ");

            tui_putstring(pdisplay, "%04X: ", pos);
            opcode = data[pos - stepif_display_addr];
            b = data[(pos - stepif_display_addr) + 1];
            w = data[(pos - stepif_display_addr) + 2] << 8 | b;

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
                break;
            case CPU_ADDR_MODE_ACCUMULATOR:
                tui_putstring(pdisplay,"A");
                break;
            case CPU_ADDR_MODE_IMMEDIATE:
                tui_putstring(pdisplay, "#$%02x", b);
                break;
            case CPU_ADDR_MODE_RELATIVE:
                tui_putstring(pdisplay, "$%02x", rel);
                break;
            case CPU_ADDR_MODE_ABSOLUTE:
                tui_putstring(pdisplay, "$%04x", w);
                break;
            case CPU_ADDR_MODE_ABSOLUTE_X:
                tui_putstring(pdisplay, "$%04x,X", w);
                break;
            case CPU_ADDR_MODE_ABSOLUTE_Y:
                tui_putstring(pdisplay, "$%04x,Y", w);
                break;
            case CPU_ADDR_MODE_ZPAGE:
                tui_putstring(pdisplay, "$%02x", b);
                break;
            case CPU_ADDR_MODE_ZPAGE_X:
                tui_putstring(pdisplay, "$%02x,X", b);
                break;
            case CPU_ADDR_MODE_ZPAGE_Y:
                tui_putstring(pdisplay, "$%02x,Y", b);
                break;
            case CPU_ADDR_MODE_INDIRECT:
                tui_putstring(pdisplay, "($%04x)", w);
                break;
            case CPU_ADDR_MODE_IND_X:
                tui_putstring(pdisplay, "($%02x,X)", b);
                break;
            case CPU_ADDR_MODE_IND_Y:
                tui_putstring(pdisplay, "($%02x),Y", b);
                break;
            }

            pos += len;
            if(popcode->opcode_undocumented)
                tui_putstring(pdisplay, " (!!)");
            tui_putstring(pdisplay, "\n");
            line++;
        }


    }
    last_display_type = stepif_display_mode;
    free(data);
}

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
    tui_putstring(pregisters, " %-4s $%04x\n\n", "IP:", cpu_state->ip);
    tui_putstring(pregisters, " %s $%02x", "Flags:", cpu_state->p);

/*     tui_putstring(pregisters, "  NVBDIZC\n"); */
    tui_putstring(pregisters, "  (%c%c%c%c%c%c%c)\n",
                  cpu_state->p & FLAG_N ? 'N' : 'n',
                  cpu_state->p & FLAG_V ? 'V' : 'v',
                  cpu_state->p & FLAG_B ? 'B' : 'b',
                  cpu_state->p & FLAG_D ? 'D' : 'd',
                  cpu_state->p & FLAG_I ? 'I' : 'i',
                  cpu_state->p & FLAG_Z ? 'Z' : 'z',
                  cpu_state->p & FLAG_C ? 'C' : 'c');

    memcpy((void*)&stepif_state, (void*)cpu_state, sizeof(cpu_t));
    free(cpu_state);
}

int main(int argc, char *argv[]) {
    char buffer[40];
    char *fifo;
    char *fifo_path;
    int pos;
    int step_char;

    breakpoint_list.pnext = NULL;
    stepif_display_mode = DISPLAY_MODE_DUMP;
    stepif_display_addr = 0x8000;
    stepif_display_track = 1;

    memset((void*)xlat, '.', sizeof(xlat));
    for(pos = ' '; pos <= '~'; pos++)
        xlat[pos] = pos;

    fifo = DEFAULT_FIFO;

    fifo_path = malloc(strlen(fifo) + 5);
    strcpy(fifo_path, fifo);
    strcat(fifo_path, "-rsp");

    stepif_rsp_fd = open(fifo_path, O_RDWR);
    if(stepif_rsp_fd == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    strcpy(fifo_path, fifo);
    strcat(fifo_path, "-cmd");

    stepif_cmd_fd = open(fifo_path, O_RDWR);
    if(stepif_cmd_fd == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    free(fifo_path);

    pscreen = tui_init(NULL, 1);
    pregisters = tui_window(pscreen->width - REGISTER_WIDTH, 0, REGISTER_WIDTH, pscreen->height - (CMD_HEIGHT + 1), 1, "Registers", COLORSET_GREEN);
    pcommand = tui_window(0, pscreen->height - (CMD_HEIGHT + 1), pscreen->width, CMD_HEIGHT, 1, "Command", COLORSET_DEFAULT);
    pdisplay = tui_window(0, 0, pscreen->width - REGISTER_WIDTH, pscreen->height - (CMD_HEIGHT+1), 1, "6502 Explorer", COLORSET_BLUE);

    tui_window_callback(pregisters, register_update);
    tui_window_callback(pdisplay, display_update);

    tui_inputwindow(pcommand);

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
            tui_putstring(pcommand, "> ");
            tui_getstring(pcommand, buffer, sizeof(buffer));
            if(strcmp(buffer,"quit") == 0)
                break;

            process_command(buffer);
        }
    }

    exit(EXIT_SUCCESS);
}
