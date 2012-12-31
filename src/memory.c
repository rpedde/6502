/*
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <dlfcn.h>

#include "debug.h"
#include "emulator.h"
#include "memory.h"

typedef struct memory_list_t {
    hw_reg_t *hw_reg;
    struct memory_list_t *pnext;
} memory_list_t;

memory_list_t memory_list;

typedef struct module_list_t {
    char *module_name;
    hw_reg_t *(*init)(hw_config_t *);
    void *handle;
    struct module_list_t *pnext;
} module_list_t;

module_list_t memory_modules;

module_list_t *get_load_module(const char *module) {
    module_list_t *pentry = memory_modules.pnext;

    while(pentry) {
        if(strcmp(pentry->module_name, module) == 0)
            break;

        pentry = pentry->pnext;
    }

    if(!pentry) {
        /* we have to load the module */
        pentry = malloc(sizeof(module_list_t));
        if(!pentry) {
            perror("malloc");
            exit(1);
        }

        pentry->module_name = strdup(module);
        pentry->handle = dlopen(module, RTLD_LAZY | RTLD_LOCAL);
        if(!pentry->handle) {
            perror(module);
            exit(1);
        }

        pentry->init = dlsym(pentry->handle, "init");
        if(!pentry->init) {
            exit(1);
        }

        pentry->pnext = memory_modules.pnext;
        memory_modules.pnext = pentry;
    }

    return pentry;
}


int memory_init(void) {
    memory_list.pnext = NULL;
    memory_modules.pnext = NULL;

    /* now we are free to load any memory modules from disk */
    return E_MEM_SUCCESS;
}

void memory_deinit(void) {
    memory_list_t *current = memory_list.pnext;

    while(current) {
        /* unload all the modules */
        current = current->pnext;
    }
}

uint8_t memory_read(uint16_t addr) {
    memory_list_t *current = memory_list.pnext;

    while(current) {
        /* find the module associated with
           this memory range */
        for(int x=0; x < current->hw_reg->remapped_regions; x++) {
            if((current->hw_reg->remap[x].mem_start <= addr) &&
               (current->hw_reg->remap[x].mem_end >= addr) &&
               (current->hw_reg->remap[x].readable == 1)) {
                return current->hw_reg->memop(current->hw_reg, addr,
                                              MEMOP_READ, 0);
            }
        }

        current = current->pnext;
    }

    EPRINTF(DBG_ERROR, "No readable memory at addr %x\n", addr);
    return 0;
}

void memory_write(uint16_t addr, uint8_t value) {
    memory_list_t *current = memory_list.pnext;

    while(current) {
        /* find the module associated with
           this memory range */
        for(int x=0; x < current->hw_reg->remapped_regions; x++) {
            if((current->hw_reg->remap[x].mem_start <= addr) &&
               (current->hw_reg->remap[x].mem_end >= addr) &&
               (current->hw_reg->remap[x].writable == 1)) {
                current->hw_reg->memop(current->hw_reg, addr,
                                       MEMOP_WRITE, value);
                return;
            }
        }

        current = current->pnext;
    }
    EPRINTF(DBG_ERROR, "No writable memory at addr %x\n", addr);
}

int memory_load(const char *module, hw_config_t *config) {
    memory_list_t *modentry = NULL;
    module_list_t *pmodule = NULL;

    DPRINTF(DBG_DEBUG, "Loading module %s\n", module);

    modentry = malloc(sizeof(memory_list_t));
    if(!modentry) {
        EPRINTF(DBG_FATAL, "malloc");
        exit(1);
    }

    memset(modentry, 0x00, sizeof(modentry));
    pmodule = get_load_module(module);

    modentry->hw_reg = pmodule->init(config);

    if(modentry->hw_reg == NULL) {
        EPRINTF(DBG_FATAL, "Module %s init failed", module);
        exit(1);
    }

    DPRINTF(DBG_DEBUG, "Loaded module at 0x%x - 0x%x.  Read: %d, Write: %d\n",
            modentry->hw_reg->remap[0].mem_start,
            modentry->hw_reg->remap[0].mem_end,
            modentry->hw_reg->remap[0].readable,
            modentry->hw_reg->remap[0].writable);

    modentry->pnext = memory_list.pnext;
    memory_list.pnext = modentry;

    return E_MEM_SUCCESS;
}
