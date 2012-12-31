#include "emulator.h"

#include <libconfig.h>

#include "memory.h"
#include "6502.h"
#include "stepwise.h"
#include "hardware.h"

config_t main_config;

#define DEFAULT_CONFIG_FILE "emulator.conf"

int load_memory(void) {
    config_setting_t *pmemory;
    config_setting_t *pblock;
    config_setting_t *args;
    config_setting_t *arg;

    int memory_items = 0;
    int arg_items = 0;
    char *name;
    const char *module;

    hw_config_t *hw_config = NULL;

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

        name = config_setting_name(pblock);

        DPRINTF(DBG_INFO, "Found memory element: %s\n", name);

        if(!config_setting_lookup_string(pblock, "module", &module)) {
            DPRINTF(DBG_FATAL,
                    "Missing module entry for memory item %s", name);
            exit(1);
        }

        DPRINTF(DBG_INFO, "  Module name: %s\n", module);
        args = config_setting_get_member(pblock, "args");

        if (!args) {
            hw_config = malloc(sizeof(hw_config_t));
            if (!hw_config) {
                DPRINTF(DBG_FATAL, "malloc");
                exit(1);
            }

            arg_items = 0;
        } else {
            arg_items = config_setting_length(args);
            DPRINTF(DBG_INFO, "  %d arg items\n", arg_items);
            hw_config = malloc(sizeof(hw_config_t) +
                               (arg_items * sizeof(hw_config_item_t)));
            if(!hw_config) {
                DPRINTF(DBG_FATAL, "malloc");
                exit(1);
            }
        }

        /* now walk the args and put them in a
           config struct */
        hw_config->config_items = arg_items;
        for(int x = 0; x < arg_items; x++) {
            arg = config_setting_get_elem(args, x);
            const char *key = config_setting_name(arg);
            if(!key) {
                DPRINTF(DBG_FATAL, "Bad arg key in %s", name);
                exit(1);
            }
            const char *value = NULL;
            value = config_setting_get_string(arg);

            if(!value) {
                DPRINTF(DBG_FATAL, "Bad arg value for %s in %s",
                        key, name);
                exit(1);
            }

            hw_config->item[x].key = key;
            hw_config->item[x].value = value;

            DPRINTF(DBG_INFO, "    %s => %s\n", key, value);
        }

        /* now, let's load it */
        memory_load(module, hw_config);

        if (hw_config) {
            free(hw_config);
            hw_config = NULL;
        }
    }

    DPRINTF(DBG_INFO, "Memory loaded\n");
    exit(1);
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
