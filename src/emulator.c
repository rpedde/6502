/*
 * Copyright (C) 2009-2103 Ron Pedde (ron@pedde.com)
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
#include <stdio.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <libconfig.h>
#include <getopt.h>
#include <pthread.h>

#include "emulator.h"
#include "debug.h"
#include "memory.h"
#include "6502.h"
#include "stepwise.h"
#include "hardware.h"

config_t main_config;
static void *stepwise_proc(void *arg);

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

    INFO("Loading memory");
    pmemory = config_lookup(&main_config, "memory");
    if(!pmemory) {
        INFO("No memory blocks to laod");
        return TRUE;
    }

    while(1) {
        pblock = config_setting_get_elem(pmemory, memory_items);
        memory_items++;
        if(!pblock)
            break;

        name = config_setting_name(pblock);

        INFO("Found memory element: %s", name);

        if(!config_setting_lookup_string(pblock, "module", &module)) {
            FATAL("Missing module entry for memory item %s", name);
            exit(1);
        }

        INFO("  Module name: %s", module);
        args = config_setting_get_member(pblock, "args");

        if (!args) {
            hw_config = malloc(sizeof(hw_config_t));
            if (!hw_config) {
                FATAL("malloc");
                exit(1);
            }

            arg_items = 0;
        } else {
            arg_items = config_setting_length(args);
            INFO("  %d arg items", arg_items);
            hw_config = malloc(sizeof(hw_config_t) +
                               (arg_items * sizeof(hw_config_item_t)));
            if(!hw_config) {
                FATAL("malloc");
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
                FATAL("Bad arg key in %s", name);
                exit(1);
            }
            const char *value = NULL;
            value = config_setting_get_string(arg);

            if(!value) {
                FATAL("Bad arg value for %s in %s",
                      key, name);
                exit(1);
            }

            hw_config->item[x].key = key;
            hw_config->item[x].value = value;

            INFO("    %s => %s", key, value);
        }

        /* now, let's load it */
        memory_load(module, hw_config);

        if (hw_config) {
            free(hw_config);
            hw_config = NULL;
        }
    }

    INFO("Memory loaded");
    return TRUE;
}

int main(int argc, char *argv[]) {
    char *configfile = DEFAULT_CONFIG_FILE;
    int option;
    int step = 0;
    int debuglevel = 2;
    pthread_t run_tid;
    int running=1;

    while((option = getopt(argc, argv, "d:sc:")) != -1) {
        switch(option) {
        case 'd':
            debuglevel = atoi(optarg);
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

    debug_level(debuglevel);

    config_init(&main_config);
    if(config_read_file(&main_config, configfile) != CONFIG_TRUE) {
        FATAL("Can't read config file (%s, line %d): %s",
              configfile, config_error_line(&main_config),
              config_error_text(&main_config));
        exit(EXIT_FAILURE);
    }


    if(step)
        step_init(NULL);

    memory_init();
    if(!load_memory())
        exit(EXIT_FAILURE);

    cpu_init();

    if(step) {
        if(pthread_create(&run_tid, NULL, stepwise_proc, &running) < 0) {
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
    } else {
    }

    /* here, we should run the event loop required by any drivers.
     * really, this means the SDL video driver.  for osx, all
     * event polling/pumping has to be done in the main thread,
     * not background threads.  that's kind of a fail.
     */
    while(running) {
        memory_run_eventloop();
    }

    exit(EXIT_SUCCESS);
}


/**
 * stepwise_proc - this is the thread that runs the cpu
 * in the background
 *
 * @args arg: unused
 */
static void *stepwise_proc(void *arg) {
    int *running = (int*) arg;

    *running = 1;
    stepwise_debugger();
    *running = 0;
}
