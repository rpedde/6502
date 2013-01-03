/*
 * Copyright (C) 2009 Ron Pedde (ron@pedde.com)
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

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "debug.h"
#include "compiler.h"
#include "parser.h"
#include "getopt.h"

extern FILE *yyin;
extern int yyparse(void*);

int parse_failure = 0;
uint16_t compiler_offset = 0x8000;

#define OPCODE_NOTFOUND 0x0100

typedef struct opdata_list_t_struct {
    struct opdata_list_t_struct *next;
    opdata_t *data;
} opdata_list_t;

opdata_list_t list = { NULL, NULL };
opdata_list_t *list_tailp = &list;

extern void set_offset(uint16_t offset) {
    compiler_offset = offset;
}

void add_opdata(opdata_t *pnew) {
    opdata_list_t *pnewitem;

    pnewitem = (opdata_list_t *)malloc(sizeof(opdata_list_t));
    if(!pnewitem) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    pnewitem->data = pnew;
    list_tailp->next = pnewitem;
    list_tailp = pnewitem;
}

uint16_t opcode_lookup(opdata_t *op, int line, int fatal) {
    opcode_t *ptable = (opcode_t*)&cpu_opcode_map;
    int index = 0;

    DPRINTF(DBG_DEBUG, " - Looking up opcode '%s' (%s)\n",
            cpu_opcode_mnemonics[op->opcode_family],
            cpu_addressing_mode[op->addressing_mode]);

    while(index < 256 && (op->opcode_family != ptable->opcode_family ||
                          op->addressing_mode != ptable->addressing_mode)) {
        index++;
        ptable++;
    }

    if(index == 256) {
        if(fatal) {
            DPRINTF(DBG_ERROR, "Can't lookup indexing mode '%s' for '%s' on line %d\n",
                    cpu_addressing_mode[op->addressing_mode],
                    cpu_opcode_mnemonics[op->opcode_family],line);
            exit(EXIT_FAILURE);
        } else {
            return OPCODE_NOTFOUND;
        }
    }

    return (uint16_t)index;
}

/*
 * We now should have all symbols in the symbol table, now
 * sanity-check the indexing modes, and solidify all the addressing
 * modes
 */
void pass2(void) {
    opdata_list_t *pcurrent = list.next;
    value_t *psym;
    value_t *operand = NULL;

    DPRINTF(DBG_INFO, "Pass 2: Symbol resolution/addressing mode determination\n");

    /* FIXME: Fix data here, minus address fixups */

    while(pcurrent) {
        if (pcurrent->data->type == TYPE_INSTRUCTION) {
            if(pcurrent->data->value)
                operand = y_evaluate_val(pcurrent->data->value, pcurrent->data->line, 0);

            switch(pcurrent->data->addressing_mode) {
            case CPU_ADDR_MODE_IMPLICIT:
            case CPU_ADDR_MODE_ACCUMULATOR:
                /* Some assemblers seem to allow Accumulator type indexing modes to be
                 * specified as implicit.  Try swapping them if the specified mode is
                 * invalid.
                 */
                if(opcode_lookup(pcurrent->data, pcurrent->data->line, 0) == OPCODE_NOTFOUND) {
                    if(pcurrent->data->addressing_mode == CPU_ADDR_MODE_IMPLICIT)
                        pcurrent->data->addressing_mode = CPU_ADDR_MODE_ACCUMULATOR;
                    else
                        pcurrent->data->addressing_mode = CPU_ADDR_MODE_IMPLICIT;
                }
                break;
            case CPU_ADDR_MODE_IMMEDIATE:
            case CPU_ADDR_MODE_ZPAGE:
            case CPU_ADDR_MODE_ZPAGE_X:
            case CPU_ADDR_MODE_ZPAGE_Y:
            case CPU_ADDR_MODE_IND_X:
            case CPU_ADDR_MODE_IND_Y:
                /* verify byte-size operand */
                if(operand->type == Y_TYPE_WORD) {
                    DPRINTF(DBG_ERROR, "Line %d: Error:  Addressing mode requires BYTE, operand is WORD\n",
                            pcurrent->data->line);
                    exit(EXIT_FAILURE);
                }
                break;

            case CPU_ADDR_MODE_RELATIVE:
                /* this can either be a direct BYTE value, or an address -- we'll verify symbols,
                 * but do bounds checking in fixups pass */
                break;

            case CPU_ADDR_MODE_ABSOLUTE:
            case CPU_ADDR_MODE_ABSOLUTE_X:
            case CPU_ADDR_MODE_ABSOLUTE_Y:
            case CPU_ADDR_MODE_INDIRECT:
                /* verify word-size operand */
                if(operand->type == Y_TYPE_BYTE) {
                    DPRINTF(DBG_ERROR, "Line %d: Error:  Addressing mode requires WORD, operand is BYTE\n",
                            pcurrent->data->line);
                    exit(EXIT_FAILURE);
                }
                break;

            case CPU_ADDR_MODE_UNKNOWN:
            case CPU_ADDR_MODE_UNKNOWN_X:
            case CPU_ADDR_MODE_UNKNOWN_Y:
                /* these are either ZPAGE (if byte), or ABSOLUTE (if word) */
                /* we'll massage the operand type if:
                 *
                 * - it's a byte, but there exists no ZPAGE instruction
                 * We might do this at some later point, too...
                 * - it's a word, there exists a ZPAGE indexing mode, and value < 265
                 */
                if(operand->type == Y_TYPE_BYTE) {
                    pcurrent->data->addressing_mode = CPU_ADDR_MODE_ZPAGE +
                        (pcurrent->data->addressing_mode - CPU_ADDR_MODE_UNKNOWN);

                    if(opcode_lookup(pcurrent->data, pcurrent->data->line, 0) == OPCODE_NOTFOUND) {
                        /* *must* be an absolute that should be promoted */
                        DPRINTF(DBG_WARN,"Line %d: Warning: Extending presumed ZPAGE operand to ABSOLUTE\n",
                                pcurrent->data->line);
                        operand->type = Y_TYPE_WORD;
                        operand->word = operand->byte;
                        pcurrent->data->promote = 1;
                    }
                    pcurrent->data->addressing_mode = CPU_ADDR_MODE_UNKNOWN +
                        (pcurrent->data->addressing_mode - CPU_ADDR_MODE_ZPAGE);

                }

                if(operand->type == Y_TYPE_WORD) {
                    pcurrent->data->addressing_mode = CPU_ADDR_MODE_ABSOLUTE +
                        (pcurrent->data->addressing_mode - CPU_ADDR_MODE_UNKNOWN);
                    pcurrent->data->len = 3;
                } else {
                    pcurrent->data->addressing_mode = CPU_ADDR_MODE_ZPAGE +
                        (pcurrent->data->addressing_mode - CPU_ADDR_MODE_UNKNOWN);
                    pcurrent->data->len = 2;
                }
                break;
            default:
                DPRINTF(DBG_ERROR, "Line %d: Error:  Unknown addressing mode\n",
                        pcurrent->data->line);
            }

            pcurrent->data->offset = compiler_offset;
            compiler_offset += pcurrent->data->len;
        } else if (pcurrent->data->type == TYPE_LABEL) {
            psym = y_lookup_symbol(pcurrent->data->value->label);
            if(!psym) {
                DPRINTF(DBG_ERROR, "Line %d: Internal Error:  Cannot find symbol '%s'\n",
                        pcurrent->data->line,
                        pcurrent->data->value->label);
                exit(EXIT_FAILURE);
            }

            if(psym->type != Y_TYPE_WORD) {
                DPRINTF(DBG_ERROR, "Line: %d: Internal Error:  Expected symbol '%s' to be a WORD\n",
                        pcurrent->data->line,
                        pcurrent->data->value->label);
                exit(EXIT_FAILURE);
            }

            DPRINTF(DBG_DEBUG, " - Updating symbol '%s' to %04x\n",
                    pcurrent->data->value->label,
                    compiler_offset);

            psym->word = compiler_offset;
        } else if (pcurrent->data->type == TYPE_OFFSET) {
            compiler_offset = pcurrent->data->value->word;
        } else if (pcurrent->data->type == TYPE_DATA) {
            pcurrent->data->offset = compiler_offset;
            compiler_offset += pcurrent->data->len;
        } else {
            DPRINTF(DBG_ERROR, "Line %d: Error: Unknown instruction type\n",
                    pcurrent->data->line);
        }

        pcurrent = pcurrent->next;
    }
}

void pass3(void) {
    opdata_list_t *pcurrent = list.next;
    uint16_t effective_addr;
    uint16_t target_addr;
    int offset;
    value_t *operand = NULL;

    DPRINTF(DBG_INFO, "Pass 3:  Finalizing opcode/operand data.\n");
    while(pcurrent) {
        if(pcurrent->data->type == TYPE_INSTRUCTION) {
            /* we really shouldn't be evaluating this twice, sanity
             * checks should be here, and pass 2 should be strictly
             * addressing and label fixups */

            if(pcurrent->data->value) {
                operand = y_evaluate_val(pcurrent->data->value, pcurrent->data->line, pcurrent->data->offset);
                if(pcurrent->data->promote) {
                    operand->type = Y_TYPE_WORD;
                    operand->word = operand->byte;
                }
            }

            pcurrent->data->opcode = (uint8_t)opcode_lookup(pcurrent->data, pcurrent->data->line, 1);

            /* FIX UP RELATIVE ADDRESSING HERE */
            if(pcurrent->data->addressing_mode == CPU_ADDR_MODE_RELATIVE) {
                effective_addr = pcurrent->data->offset + 2;

                if(operand->type == Y_TYPE_WORD) {
                    target_addr = operand->word;
                    pcurrent->data->value->type = Y_TYPE_BYTE;

                    offset = target_addr - effective_addr;
                    if(offset < -128 || offset > 127) {
                        DPRINTF(DBG_ERROR, "Line %d: Error:  Branch out of range\n",
                                pcurrent->data->line);
                        exit(EXIT_FAILURE);
                    }

                    if(offset < 0) {
                        pcurrent->data->value->byte = abs(offset) ^ 0xFF;
                        pcurrent->data->value->byte += 1;
                    } else {
                        pcurrent->data->value->byte = offset;
                    }
                    DPRINTF(DBG_DEBUG, " - Line %d: Fixed up branch to $%02x\n",
                            pcurrent->data->line,
                            (uint16_t)pcurrent->data->value->byte);
                } else {
                    pcurrent->data->type = Y_TYPE_BYTE;
                    pcurrent->data->value->byte = operand->byte;
                }
            } else {
                /* finalize the rest */
                pcurrent->data->value = operand;
            }
        }
        pcurrent = pcurrent->next;
    }
}

void pass4(void) {
    FILE *map, *bin;
    opdata_list_t *pcurrent = list.next;
    int len;
    symtable_t *psym;

    if(!(map = fopen("out.map","w"))) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    if(!(bin = fopen("out.bin","w"))) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    DPRINTF(DBG_INFO, "Pass 4: Writing output.\n");
    fprintf(map, "Symbol Table:\n=======================================================\n\n");

    psym = symtable.next;
    while(psym) {
        len = fprintf(map, "%s ", psym->label);
        fprintf(map, "%*c", 40 - len, ' ');
        if(psym->value->type == Y_TYPE_BYTE)
            fprintf(map, "$%02x", psym->value->byte);
        else if (psym->value->type == Y_TYPE_WORD)
            fprintf(map, "$%04x", psym->value->word);
        else {
            DPRINTF(DBG_ERROR, "Bad Y-type\n");
            exit(EXIT_FAILURE);
        }
        fprintf(map,"\n");
        psym = psym->next;
    }

    fprintf(map, "\n\nOutput:\n=======================================================\n\n");

    while(pcurrent) {
        if(pcurrent->data->type == TYPE_INSTRUCTION) {
            len = 0;
            len = fprintf(map,"%04x: %02x ", pcurrent->data->offset,
                          pcurrent->data->opcode);

            if(pcurrent->data->len == 2)
                len += fprintf(map,"%02x ", pcurrent->data->value->byte);
            else if(pcurrent->data->len == 3)
                len += fprintf(map,"%02x %02x", pcurrent->data->value->word & 0x00ff,
                               (pcurrent->data->value->word >> 8) & 0x00ff);

            fprintf(map, "%*c", 20 - len, ' ');
            fprintf(map, "%s ", cpu_opcode_mnemonics[pcurrent->data->opcode_family]);

            switch(pcurrent->data->addressing_mode) {
            case CPU_ADDR_MODE_IMPLICIT:
                break;
            case CPU_ADDR_MODE_ACCUMULATOR:
                fprintf(map, "A");
                break;
            case CPU_ADDR_MODE_IMMEDIATE:
                fprintf(map, "#$%02x", pcurrent->data->value->byte);
                break;
            case CPU_ADDR_MODE_RELATIVE:
                fprintf(map, "$%02x", pcurrent->data->value->byte);
                break;
            case CPU_ADDR_MODE_ABSOLUTE:
                fprintf(map, "$%04x", pcurrent->data->value->word);
                break;
            case CPU_ADDR_MODE_ABSOLUTE_X:
                fprintf(map, "$%04x,X", pcurrent->data->value->word);
                break;
            case CPU_ADDR_MODE_ABSOLUTE_Y:
                fprintf(map, "$%04x,Y", pcurrent->data->value->word);
                break;
            case CPU_ADDR_MODE_ZPAGE:
                fprintf(map, "$%02x", pcurrent->data->value->byte);
                break;
            case CPU_ADDR_MODE_ZPAGE_X:
                fprintf(map, "$%02x,X", pcurrent->data->value->byte);
                break;
            case CPU_ADDR_MODE_ZPAGE_Y:
                fprintf(map, "$%02x,Y", pcurrent->data->value->byte);
                break;
            case CPU_ADDR_MODE_INDIRECT:
                fprintf(map, "($%04x)", pcurrent->data->value->word);
                break;
            case CPU_ADDR_MODE_IND_X:
                fprintf(map, "($%04x,X)", pcurrent->data->value->word);
                break;
            case CPU_ADDR_MODE_IND_Y:
                fprintf(map, "($%04x),Y)", pcurrent->data->value->word);
                break;
            default:
                DPRINTF(DBG_ERROR,"Unknown addressing mode in map file\n");
                exit(EXIT_FAILURE);
            }

            fprintf(map, "\n");

            fprintf(bin, "%c", pcurrent->data->opcode);
            if(pcurrent->data->len == 2)
                fprintf(bin, "%c", pcurrent->data->value->byte);
            else if(pcurrent->data->len == 3)
                fprintf(bin, "%c%c", pcurrent->data->value->word & 0x00ff,
                        (pcurrent->data->value->word >> 8) & 0x00ff);
        } else if(pcurrent->data->type == TYPE_DATA) {
            len = 0;
            len = fprintf(map,"%04x: ", pcurrent->data->offset);
            if(pcurrent->data->value->type == Y_TYPE_BYTE) {
                len += fprintf(map, "%02x", pcurrent->data->value->byte);
                fprintf(map, "%*c", 20 - len, ' ');
                fprintf(map, ".data %02x", pcurrent->data->value->byte);
                fprintf(bin,"%c", pcurrent->data->value->byte);
            } else {
                DPRINTF(DBG_ERROR, "Bad Y-type in static data\n");
                exit(EXIT_FAILURE);
            }

            fprintf(map, "\n");
        }

        pcurrent = pcurrent->next;
    }

    fclose(bin);
    fclose(map);
}

int main(int argc, char *argv[]) {
    int result;
    FILE *fin;
    int option;
    value_t star_symbol = { Y_TYPE_WORD, 0, 0, "*", NULL, NULL };

    debug_level(2);

    while((option = getopt(argc, argv, "d:")) != -1) {
        switch(option) {
        case 'd':
            debug_level(atoi(optarg));
            break;
        default:
            fprintf(stderr,"Srsly?");
            exit(EXIT_FAILURE);
        }
    }

    if(optind + 1 != argc) {
        fprintf(stderr,"Must specify filename as first non-option\n");
        exit(EXIT_FAILURE);
    }

    fin = fopen(argv[optind], "r");
    if(!fin) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    /* add a dummy symtable entry */
    y_add_symtable("*", &star_symbol);

    DPRINTF(DBG_INFO, "Pass 1:  Parsing.\n");
    yyin = fin;
    result = yyparse(NULL);
    fclose(fin);

    if(parse_failure) {
        DPRINTF(DBG_ERROR, "Error parsing file.  Aborting\n");
        exit(EXIT_FAILURE);
    }

    pass2();
    pass3();
    pass4();

    DPRINTF(DBG_INFO, "Done.\n");
    exit(EXIT_SUCCESS);
}
