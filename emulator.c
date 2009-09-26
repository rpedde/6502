#include "emulator.h"

#include <libconfig.h>

#include "memory.h"
#include "6502.h"
#include "stepwise.h"

config_t main_config;

#define DEFAULT_CONFIG_FILE "emulator.conf"

int load_memory(void) {
    config_setting_t *pmemory;
    config_setting_t *pblock;
    int memory_items = 0;
    const char *file;
    uint16_t addr;
    int is_rom=0;

    DPRINTF(DBG_INFO, "Loading memory\n");
    pmemory = config_lookup(&main_config, "memory");
    if(!pmemory) {
        DPRINTF(DBG_INFO,"No memory blocks to laod\n");
        return TRUE;
    }

    while(1) {
        pblock = config_setting_get_elem(pmemory, memory_items);
        memory_items++;
        if(!pblock)
            break;
        file = config_setting_get_string_elem(pblock,0);
        is_rom = config_setting_get_int_elem(pblock,1);
        addr = (uint16_t) config_setting_get_int_elem(pblock,2);

        DPRINTF(DBG_INFO, "Loading '%s' into %s memory at $%04x\n", file, is_rom ? "ROM" : "RAM", addr);

        if(memory_load(addr, file, is_rom) != E_MEM_SUCCESS) {
            DPRINTF(DBG_ERROR, "Error loading memory\n");
            return FALSE;
        }
    }

    DPRINTF(DBG_INFO, "Memory loaded\n");
    return TRUE;
}

int main(int argc, char *argv[]) {
    char *configfile = DEFAULT_CONFIG_FILE;
    int option;
    int step = 0;

    while((option = getopt(argc, argv, "d:sc:")) != -1) {
        switch(option) {
        case 'd':
            debug_level(atoi(optarg));
            break;

        case 'c':
            configfile = optarg;
            break;

        case 's':
            step = 1;
            break;

        default:
            fprintf(stderr,"Srsly?");
            exit(EXIT_FAILURE);
        }
    }

    config_init(&main_config);
    if(config_read_file(&main_config, configfile) != CONFIG_TRUE) {
        DPRINTF(DBG_FATAL,"Can't read config file (%s, line %d): %s\n",
                configfile, config_error_line(&main_config),
                config_error_text(&main_config));
        exit(EXIT_FAILURE);
    }


    memory_init();
    if(!load_memory())
        exit(EXIT_FAILURE);

    debug_level(DBG_DEBUG);
    cpu_init();

    if(step) {
        stepwise_debugger(NULL);
    } else {
    }

    exit(EXIT_SUCCESS);
}
