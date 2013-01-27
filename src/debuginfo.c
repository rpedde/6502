/*
 * Copyright (C) 2013 Ron Pedde <ron@pedde.com>
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

#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>

#include "redblack.h"
#include "debuginfo.h"

typedef struct debug_info_t {
    uint16_t address;
    FILE *fp;
    long offset;
} debug_info_t;

typedef struct debug_info_symtable_t {
    char *label;
    uint16_t address;
} debug_info_symtable_t;

typedef struct debuginfo_fh_t {
    char *file;
    FILE *fp;
    uint32_t current_line;
    long offset;
    struct debuginfo_fh_t *next;
} debuginfo_fh_t;

debuginfo_fh_t debuginfo_fh = { NULL, NULL, 0, 0, NULL };

static struct rbtree *debug_rb = NULL;
static struct rbtree *debug_symbol_rb = NULL;
static struct rbtree *debug_symbol_reverse_rb = NULL;

static int debuginfo_compare(const void *a, const void *b, const void *config);
static int debuginfo_symbol_compare(const void *a, const void *b, const void *config);
static int debuginfo_symbol_addr(const void *a, const void *b, const void *config);

static debuginfo_fh_t *debuginfo_fh_find(char *file);
static int debuginfo_add_symtable(FILE *fin);
static int debuginfo_add_opaddr(FILE *fin);

static int debuginfo_compare(const void *a, const void *b, const void *config) {
    debug_info_t *ap = (debug_info_t*)a;
    debug_info_t *bp = (debug_info_t*)b;

    if(ap->address == bp->address)
        return 0;

    if(ap->address < bp->address)
        return -1;

    return 1;
}

static int debuginfo_symbol_compare(const void *a, const void *b, const void *config) {
    debug_info_symtable_t *ap = (debug_info_symtable_t *)a;
    debug_info_symtable_t *bp = (debug_info_symtable_t *)b;

    return strcasecmp(ap->label, bp->label);
}

static int debuginfo_symbol_addr(const void *a, const void *b, const void *config) {
    debug_info_symtable_t *ap = (debug_info_symtable_t *)a;
    debug_info_symtable_t *bp = (debug_info_symtable_t *)b;

    if(ap->address == bp->address)
        return 0;

    if(ap->address < bp->address)
        return -1;

    return 1;
}


debuginfo_fh_t *debuginfo_fh_find(char *file) {
    debuginfo_fh_t *pinfo = debuginfo_fh.next;
    while(pinfo) {
        if(strcmp(file, pinfo->file) == 0)
            return pinfo;

        pinfo = pinfo->next;
    }

    pinfo = (debuginfo_fh_t *)malloc(sizeof(debuginfo_fh_t));
    if(!pinfo) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    pinfo->file = strdup(file);
    pinfo->fp = fopen(file, "r");
    if(!pinfo->fp) {
        free(pinfo);
        return NULL;
    }

    pinfo->offset = 0;
    pinfo->current_line = 1;

    pinfo->next = debuginfo_fh.next;
    debuginfo_fh.next = pinfo;

    return pinfo;
}


/**
 * debuginfo_getline
 *
 * given an address, return the text of that line
 * @param addr memory location to look for
 * @param buffer where ot drop the line data
 * @param len size of buffer
 * @returns true if address in map, false otherwise
 */
int debuginfo_getline(uint16_t addr, char *buffer, int len) {
    debug_info_t di;
    debug_info_t *pdi = NULL;

    di.address = addr;
    pdi = (debug_info_t *)rbfind((void*)&di, debug_rb);

    if(!pdi)
        return 0;

    if(pdi->fp) {
        fseek(pdi->fp, pdi->offset, SEEK_SET);
        fgets(buffer, len, pdi->fp);
        return 1;
    }

    return 0;
}

/**
 * debuginfo_load
 *
 * load a debug file generated by rp65a.  this walks through
 * the debug file and loads all the addresses, filenames and line
 * numbers, populating a red/black tree on the address, keeping
 * a filehandle and ftell position of the start of that line.
 *
 * @param file file to load
 * @return true on succes, false otherwise
 */
int debuginfo_load(char *file) {
    FILE *fin;
    uint32_t magic;
    uint16_t record_type;

    fin = fopen(file, "r");
    if(!fin)
        return 0;

    if((fread(&magic, 1, sizeof(uint32_t), fin) != sizeof(uint32_t))) {
        fclose(fin);
        return 0;
    }

    if(magic != 0xdeadbeef) {
        fclose(fin);
        return 0;
    }


    while(1) {
        if((fread(&record_type, 1, sizeof(uint16_t), fin) != sizeof(uint16_t)))
            break;

        if(record_type == 0) {
            if(!debuginfo_add_opaddr(fin))
                break;
        } else if(record_type == 1) {
            if(!debuginfo_add_symtable(fin))
                break;
        } else {
            fprintf(stderr, "unknown debug symbol type");
            exit(EXIT_FAILURE);
        }
    }
    fclose(fin);

    return 1;
}

int debuginfo_lookup_addr(uint16_t addr, char **symbol) {
    debug_info_symtable_t lookup;
    debug_info_symtable_t *val = NULL;

    lookup.address = addr;

    val = (debug_info_symtable_t *)rbfind((void*)&lookup, debug_symbol_reverse_rb);

    if(val) {
        *symbol = val->label;
        return 1;
    }

    return 0;
}

int debuginfo_lookup_symbol(char *symbol, uint16_t *value) {
    debug_info_symtable_t lookup;
    debug_info_symtable_t *val = NULL;

    lookup.label = symbol;

    val = (debug_info_symtable_t *)rbfind((void*)&lookup, debug_symbol_rb);

    if(val) {
        *value = val->address;
        return 1;
    }
    return 0;
}


int debuginfo_add_symtable(FILE *fin) {
    uint16_t symsize;
    uint16_t value;
    char *label;
    debug_info_symtable_t *pnew;

    if((fread(&symsize, 1, sizeof(uint16_t), fin) != sizeof(uint16_t)))
        return 0;

    label = (char*)malloc(symsize);
    if(!label) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    if((fread(label, 1, symsize, fin) != symsize)) {
        free(label);
        return 0;
    }

    if((fread(&value, 1, sizeof(uint16_t), fin) != sizeof(uint16_t))) {
        free(label);
        return 0;
    }

    pnew = (debug_info_symtable_t *)malloc(sizeof(debug_info_symtable_t));
    if(!pnew) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    pnew->label = label;
    pnew->address = value;

    rbsearch(pnew, debug_symbol_reverse_rb);
    rbsearch(pnew, debug_symbol_rb);

    return 1;
}

int debuginfo_add_opaddr(FILE *fin) {
    uint16_t addr;
    uint32_t line;
    uint16_t flen;
    char filename[PATH_MAX];

    debuginfo_fh_t *pinfo;
    char *str;

    struct debug_info_t *pdebug;
    const struct debug_info_t *val;

    if((fread(&addr, 1, sizeof(uint16_t), fin) != sizeof(uint16_t)))
        return 0;

    if((fread(&line, 1, sizeof(uint32_t), fin) != sizeof(uint32_t)))
        return 0;

    if((fread(&flen, 1, sizeof(uint16_t), fin) != sizeof(uint16_t)))
        return 0;

    if((fread(filename, 1, flen, fin) != flen))
        return 0;

    /* we have an entry, make a rb for it */
    pinfo = debuginfo_fh_find(filename);

    if(pinfo) {
        if(line < pinfo->current_line) {
            pinfo->current_line = 1;
            pinfo->offset = 0;
        }

        fseek(pinfo->fp, pinfo->offset, SEEK_SET);

        while(pinfo->current_line < line) {
            str = fgets(filename, sizeof(filename), pinfo->fp);
            if(!str) {
                pinfo->current_line = 1;
                pinfo->offset = 0;
                break;
            }

            if(filename[strlen(filename) - 1] == 0x0a) {
                /* newline! */
                pinfo->current_line++;
                pinfo->offset = ftell(pinfo->fp);
            }
        }

        if(pinfo->current_line == line) {
            /* we have everything.  do it */
            pdebug = (debug_info_t *)malloc(sizeof(debug_info_t));
            if(!pdebug) {
                perror("malloc");
                exit(EXIT_FAILURE);
            }

            pdebug->address = addr;
            pdebug->fp = pinfo->fp;
            pdebug->offset = pinfo->offset;

            /* printf("addr $%04x: line: %d offset: %lu\n", addr, pinfo->current_line, pinfo->offset); */

            val = (debug_info_t *)rbsearch(pdebug, debug_rb);
            if(val != pdebug) {
                rbdelete((void*)val, debug_rb);
                rbsearch((void*)pdebug, debug_rb);
            }
        }
    }
    return 1;
}


/**
 * set up the redblack tree
 *
 * @return true on success, false otherwise
 */
int debuginfo_init(void) {
    if ((debug_rb=rbinit(debuginfo_compare, NULL)) == NULL) {
        return 0;
    }

    if((debug_symbol_rb=rbinit(debuginfo_symbol_compare, NULL)) == NULL) {
        return 0;
    }

    if((debug_symbol_reverse_rb = rbinit(debuginfo_symbol_addr, NULL)) == NULL) {
        return 0;
    }
    return 1;
}

/**
 * turn down the redblack tree
 *
 * @return true on success, false otherwise
 */
int debuginfo_deinit(void) {
    return 1;
}
