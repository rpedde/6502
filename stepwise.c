/*
 *
 */

#include "emulator.h"
#include "stepwise.h"
#include "debug.h"
#include "6502.h"
#include "memory.h"

#include <sys/stat.h>

#define DEFAULT_DEBUG_FIFO "/tmp/debug";
#define VERSION "0.1"

static int step_cmd_fd = -1;
static int step_rsp_fd = -1;

#define STEP_BAD_REG "Bad register specified"
#define STEP_BAD_FILE "Cannot open file"

void step_return(uint8_t result, uint16_t len, uint8_t *data) {
    dbg_response_t response;

    response.response_status = result;
    response.extra_len = len;

    write(step_rsp_fd, (char*)&response, sizeof(response));
    DPRINTF(DBG_DEBUG, "Returning response: %s\n", result ? "Error" : "Success");
    if(len) {
        DPRINTF(DBG_DEBUG, "Returning %d bytes of extra data\n", len);
        write(step_rsp_fd, (char*) data, response.extra_len);
    }
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
        DPRINTF(DBG_DEBUG, "Read %d bytes\n", bytesread);
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

    switch(cmd->cmd) {
    case CMD_NOP:
        step_return(RESPONSE_OK, 0, NULL);
        break;

    case CMD_VER:
        step_return(RESPONSE_OK,strlen(version)+1,(uint8_t*)version);
        break;

    case CMD_REGS:
        step_return(RESPONSE_OK,sizeof(cpu_t),(uint8_t*)&cpu_state);
        break;

    case CMD_READMEM:
        start = cmd->param1;
        len = cmd->param2;

        DPRINTF(DBG_DEBUG,"Attemping to read $%04x bytes\n", len);

        memory = (uint8_t*)malloc(len);
        if(!memory) {
            perror("malloc");
            exit(EXIT_FAILURE);
        }

        for(current = start; (current >= start) &&  (current-start < len); current++) {
            memory[current-start] = memory_read(current);
        }

        step_return(RESPONSE_OK, len, memory);
        free(memory);
        break;

    case CMD_WRITEMEM:
        start = cmd->param1;
        len = cmd->extra_len;

        for(current = start; current < len; current++) {
            memory_write(current, data[current-start]);
        }

        step_return(RESPONSE_OK, 0, NULL);
        break;

    case CMD_LOAD:
        if(memory_load(cmd->param1, (char*)data, 0) != E_MEM_SUCCESS) {
            step_return(RESPONSE_ERROR, strlen(STEP_BAD_FILE)+1, (uint8_t*)STEP_BAD_FILE);
        } else {
            step_return(RESPONSE_OK, 0, NULL);
        }
        break;

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
            step_return(RESPONSE_ERROR, strlen(STEP_BAD_REG) + 1,
                        (uint8_t*)STEP_BAD_REG);
            return;
        }

        step_return(RESPONSE_OK, 0, NULL);
        break;

    case CMD_NEXT:
        cpu_execute();
        step_return(RESPONSE_OK, 0, NULL);
        break;

    default:
        DPRINTF(DBG_ERROR, "Unknown command from debugger: %d\n", cmd->cmd);
        exit(EXIT_FAILURE);
    }
}


void stepwise_debugger(char *fifo) {
    struct stat sb;
    dbg_command_t cmd;
    uint8_t *data = NULL;
    ssize_t bytes_read;
    char *fifo_path;

    if(!fifo)
        fifo = DEFAULT_DEBUG_FIFO;

    fifo_path = malloc(strlen(fifo) + 5);
    strcpy(fifo_path, fifo);
    strcat(fifo_path, "-cmd");

    if((stat(fifo_path,&sb) == -1) && (errno == ENOENT)) {
        /* make it */
        DPRINTF(DBG_DEBUG,"Creating fifo: %s\n",fifo_path);
        mkfifo(fifo_path, 0600);
    } else {
        if(!(sb.st_mode & S_IFIFO)) {
            DPRINTF(DBG_FATAL, "File '%s' exists, and is not a fifo\n", fifo);
            exit(EXIT_FAILURE);
        }
    }

    DPRINTF(DBG_DEBUG,"Opening cmd fifo\n");
    step_cmd_fd = open(fifo_path, O_RDWR);
    if(step_cmd_fd == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    strcpy(fifo_path, fifo);
    strcat(fifo_path, "-rsp");

    if((stat(fifo_path,&sb) == -1) && (errno == ENOENT)) {
        /* make it */
        DPRINTF(DBG_DEBUG,"Creating fifo: %s\n",fifo_path);
        mkfifo(fifo_path, 0600);
    } else {
        if(!(sb.st_mode & S_IFIFO)) {
            DPRINTF(DBG_FATAL, "File '%s' exists, and is not a fifo\n", fifo);
            exit(EXIT_FAILURE);
        }
    }

    DPRINTF(DBG_DEBUG,"Opening rsp fifo\n");
    step_rsp_fd = open(fifo_path, O_RDWR);
    free(fifo_path);

    DPRINTF(DBG_DEBUG, "Waiting for input\n");

    while(1) {

        bytes_read = readblock(step_cmd_fd, (char*) &cmd, sizeof(cmd));
        if(bytes_read != sizeof(cmd)) {
            DPRINTF(DBG_FATAL, "Bad read.  Expecting %d, read %d\n",
                    sizeof(cmd), bytes_read);
            exit(EXIT_FAILURE);
        }

        DPRINTF(DBG_DEBUG, "Received command: %02x\n",cmd.cmd);

        if(cmd.cmd == CMD_STOP) {
            DPRINTF(DBG_INFO, "Exiting emulator at debugger request\n");
            return;
        }

        if(data) free(data);
        data = NULL;

        if(cmd.extra_len) {
            DPRINTF(DBG_DEBUG, "Reading %d bytes of extra info\n", cmd.extra_len);
            data = (uint8_t*)malloc(cmd.extra_len);
            if(!data) {
                perror("malloc");
                exit(EXIT_FAILURE);
            }

            if(readblock(step_cmd_fd,data,cmd.extra_len) != cmd.extra_len) {
                DPRINTF(DBG_FATAL,"Short read from remote debugger\n");
                exit(EXIT_FAILURE);
            }
        }

        DPRINTF(DBG_DEBUG, "Evaluating command\n");
        step_eval(&cmd, data);
        DPRINTF(DBG_DEBUG, "Waiting for input\n");
    }

    close(step_cmd_fd);
    close(step_rsp_fd);
}
