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

#define MIN_SPLIT_GAP 32
#define HEX_BYTES_PER_LINE 16

uint16_t min_split_gap = MIN_SPLIT_GAP;
uint8_t skip_data_byte = 0xea;
uint8_t hex_bytes_per_line = HEX_BYTES_PER_LINE;

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

typedef struct hex_line_t_struct {
    uint16_t start_addr;
    uint8_t current_byte_count;
    uint8_t bytes[16];
} hex_line_t;

extern void set_offset(uint16_t offset) {
    compiler_offset = offset;
}

void hexline_flush(FILE *fh, hex_line_t *hexline) {
    uint8_t checksum = 0;
    uint8_t byte;

    if(!hexline->current_byte_count)
        return;

    fprintf(fh, ":");
    fprintf(fh, "%02X", hexline->current_byte_count);
    checksum += hexline->current_byte_count;
    fprintf(fh, "%04X", hexline->start_addr);
    checksum += (hexline->start_addr & 0xFF00) >> 8;
    checksum += (hexline->start_addr) & 0xFF;
    fprintf(fh, "00");
    for(byte = 0; byte < hexline->current_byte_count; byte++) {
        fprintf(fh, "%02X", hexline->bytes[byte]);
        checksum += (hexline->bytes[byte]);
    }

    /* twos compliment */
    checksum  ^= 0xFF;
    checksum += 1;

    fprintf(fh, "%02X", checksum);
    fprintf(fh, "\n");
}

void add_hexline_byte(FILE *fh, hex_line_t *hexline, uint16_t addr, uint8_t byte) {
    if((hexline->start_addr + hexline->current_byte_count) != addr) {
        /* flush the current line */
        hexline_flush(fh, hexline);
        memset(hexline, 0, sizeof(hex_line_t));
        hexline->start_addr = addr;
    }

    hexline->bytes[hexline->current_byte_count++] = byte;
    if(hexline->current_byte_count == hex_bytes_per_line) {
        /* flush the current line */
        hexline_flush(fh, hexline);
        memset(hexline, 0, sizeof(hex_line_t));
        hexline->start_addr = addr + 1;
    }
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
                operand = y_evaluate_val(pcurrent->data->value, pcurrent->data->line, pcurrent->data->offset);

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

FILE *make_fh(char *basename, char *extension, int binary) {
    FILE *fh;
    char *filename = NULL;
    char *mode = NULL;

    asprintf(&filename, "%s.%s", basename, extension);
    if(!filename) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    if (binary) {
        mode = "w";
    } else {
        mode = "wb";
    }

    if(!(fh = fopen(filename, mode))) {
        free(filename);
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    free(filename);
    return fh;
}


void write_output(char *basename, int write_map, int write_bin, int write_hex, int split_bin) {
    FILE *map = NULL, *bin = NULL, *hex = NULL;
    opdata_list_t *pcurrent = list.next;
    int len;
    symtable_t *psym;
    uint16_t current_offset;
    value_t *pvalue;
    hex_line_t hexline;

    if(write_map) {
        map = make_fh(basename, "map", 0);
        DPRINTF(DBG_INFO, "Writing output.\n");

        fprintf(map, "Symbol Table:\n=======================================================\n\n");

        psym = symtable.next;
        while(psym) {
            if (strcmp(psym->label, "*") != 0) {
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
            }
            psym = psym->next;
        }

        fprintf(map, "\n\nOutput:\n=======================================================\n\n");

        while(pcurrent) {
            switch(pcurrent->data->type) {
            case TYPE_INSTRUCTION:
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
                break;
            case TYPE_DATA:
                len = 0;
                len = fprintf(map,"%04x: ", pcurrent->data->offset);
                if(pcurrent->data->value->type == Y_TYPE_BYTE) {
                    len += fprintf(map, "%02x", pcurrent->data->value->byte);
                    fprintf(map, "%*c", 20 - len, ' ');
                    fprintf(map, ".data %02x", pcurrent->data->value->byte);
                } else if(pcurrent->data->value->type == Y_TYPE_WORD) {
                    len += fprintf(map, "%02x %02x",
                                   (pcurrent->data->value->word & 0x00FF),
                                   (pcurrent->data->value->word & 0xFF00) >> 8);
                    fprintf(map, "%*c", 20 - len, ' ');
                    fprintf(map, ".data %04x", pcurrent->data->value->word);
                } else if(pcurrent->data->value->type == Y_TYPE_LABEL) {
                    pvalue = y_evaluate_val(pcurrent->data->value, pcurrent->data->line, pcurrent->data->offset);
                    len += fprintf(map, "%02x %02x",
                                   (pvalue->word & 0x00FF),
                                   (pvalue->word & 0xFF00) >> 8);
                    fprintf(map, "%*c", 20 - len, ' ');
                    fprintf(map, ".data %04x", pvalue->word);
                } else {
                    DPRINTF(DBG_ERROR, "Bad Y-type in static data\n");
                    exit(EXIT_FAILURE);
                }
                fprintf(map, "\n");
                break;
            case TYPE_OFFSET:
                fprintf(map, "\n");
                break;
            case TYPE_LABEL:
                break;
            default:
                DPRINTF(DBG_ERROR, "unhandled optype while printing map\n");
                exit (EXIT_FAILURE);
            }
            pcurrent = pcurrent->next;
        }
        fclose(map);
    }

    if((write_bin) || (write_hex)) {
        memset(&hexline, 0, sizeof(hex_line_t));

        pcurrent = list.next;
        current_offset = pcurrent->data->offset;

        /* fast forward to the real start */
        while(pcurrent && (pcurrent->data->type == TYPE_OFFSET)) {
            DPRINTF(DBG_DEBUG, "Found offset instruction for $%04x\n", pcurrent->data->value->word);
            current_offset = pcurrent->data->value->word;
            pcurrent = pcurrent->next;
        }


        /* map done, write bin and hex, if we want them */
        if(write_bin) {
            if (split_bin) {
                char *tmp_filename = NULL;
                asprintf(&tmp_filename, "%04X.bin", current_offset);
                if(!tmp_filename) {
                    perror("malloc");
                    exit(EXIT_FAILURE);
                }
                bin = make_fh(basename, tmp_filename, 1);
                free(tmp_filename);
            } else {
                bin = make_fh(basename, "bin", 1);
            }
        }

        if(write_hex)
            hex = make_fh(basename, "hex", 0);


        DPRINTF(DBG_INFO, "Fast-forwarded offset to $%04x\n", current_offset);

        while(pcurrent) {
            switch (pcurrent->data->type) {
            case TYPE_INSTRUCTION:
                if(write_bin) {
                    fprintf(bin, "%c", pcurrent->data->opcode);
                    if(pcurrent->data->len == 2)
                        fprintf(bin, "%c", pcurrent->data->value->byte);
                    else if(pcurrent->data->len == 3)
                        fprintf(bin, "%c%c", pcurrent->data->value->word & 0x00ff,
                                (pcurrent->data->value->word >> 8) & 0x00ff);
                }

                if(write_hex) {
                    add_hexline_byte(hex, &hexline, current_offset, pcurrent->data->opcode);
                    if(pcurrent->data->len == 2)
                        add_hexline_byte(hex, &hexline, current_offset + 1, pcurrent->data->value->byte);
                    if(pcurrent->data->len == 3) {
                        add_hexline_byte(hex, &hexline, current_offset + 1, (pcurrent->data->value->word) & 0x00FF);
                        add_hexline_byte(hex, &hexline, current_offset + 2, (pcurrent->data->value->word >> 8) & 0x00FF);
                    }
                }

                current_offset += pcurrent->data->len;
                break;
            case TYPE_DATA:
                switch(pcurrent->data->value->type) {
                case Y_TYPE_BYTE:
                    if(write_bin)
                        fprintf(bin,"%c", pcurrent->data->value->byte);
                    if(write_hex)
                        add_hexline_byte(hex, &hexline, current_offset, pcurrent->data->value->byte);
                    break;

                case Y_TYPE_WORD:
                    if(write_bin)
                        fprintf(bin,"%c%c",
                                (pcurrent->data->value->word & 0x00FF),
                                (pcurrent->data->value->word & 0xFF00) >> 8);
                    if(write_hex) {
                        add_hexline_byte(hex, &hexline, current_offset, pcurrent->data->value->word & 0x00FF);
                        add_hexline_byte(hex, &hexline, current_offset+1, (pcurrent->data->value->word & 0xFF00) >> 8);
                    }
                    break;

                case Y_TYPE_LABEL:
                    pvalue = y_evaluate_val(pcurrent->data->value,
                                            pcurrent->data->line,
                                            pcurrent->data->offset);
                    if(write_bin)
                        fprintf(bin, "%c%c",
                                (pvalue->word & 0x00FF),
                                (pvalue->word & 0xFF00) >> 8);

                    if(write_hex) {
                        add_hexline_byte(hex, &hexline, current_offset, pvalue->word & 0x00FF);
                        add_hexline_byte(hex, &hexline, current_offset + 1, (pvalue->word >> 8) & 0x00FF);
                    }
                    break;

                default:
                    DPRINTF(DBG_ERROR, "Bad TYPE_DATA writing bin file: %d\n", pcurrent->data->type);
                    exit(EXIT_FAILURE);
                }
                current_offset += pcurrent->data->len;
                break;

            case TYPE_LABEL:
                break;

            case TYPE_OFFSET:
                /* we would open a new binfile here, if we were splitting */
                if (pcurrent->data->value->word != current_offset) {
                    /* we have a gap.  we fill if we're not doing splits, or if the
                     * split is less than the min_split_gap */
                    if(write_bin) {
                        if (((pcurrent->data->value->word - current_offset) < min_split_gap) || (!split_bin)) {
                            DPRINTF(DBG_DEBUG, "Filling %d byte gap to $%04x\n",
                                    pcurrent->data->value->word - current_offset, pcurrent->data->value->word);

                            while(current_offset < pcurrent->data->value->word) {
                                fprintf(bin, "%c", skip_data_byte);
                                current_offset++;
                            }
                        } else { /* split bins with excessive gap */
                            char *tmp_filename = NULL;

                            fclose(bin);

                            asprintf(&tmp_filename, "%04X.bin", pcurrent->data->value->word);
                            if(!tmp_filename) {
                                perror("malloc");
                                exit(EXIT_FAILURE);
                            }
                            bin = make_fh(basename, tmp_filename, 1);
                            free(tmp_filename);
                        }
                    }

                    /* hex doesn't need to gap fill... we'll just start a new line */
                    if(write_hex) {
                        hexline_flush(hex, &hexline);
                        memset(&hexline, 0, sizeof(hex_line_t));
                    }
                }
                current_offset = pcurrent->data->value->word;
                break;

            default:
                fprintf(stderr, "unhandled data type: %d", pcurrent->data->type);
                exit(EXIT_FAILURE);
            }

            pcurrent = pcurrent->next;
        }

        if(write_bin)
            fclose(bin);

        if(write_hex) {
            hexline_flush(hex, &hexline);
            fprintf(hex, ":00000001FF\n");
            fclose(hex);
        }
    }
}

int main(int argc, char *argv[]) {
    int result;
    FILE *fin;
    int option;
    char *basepath;
    char *suffix;
    int do_split = 0;
    int do_map = 1;
    int do_bin = 1;
    int do_hex = 0;

    value_t star_symbol = { Y_TYPE_WORD, 0, 0, "*", NULL, NULL };

    debug_level(2);

    while((option = getopt(argc, argv, "d:smbh")) != -1) {
        switch(option) {
        case 's':
            do_split = 1;
            break;

        case 'm':
            do_map = 0;
            break;

        case 'b':
            do_bin = 0;
            break;

        case 'h':
            do_hex = 1;
            break;

        case 'd':
            debug_level(atoi(optarg));
            break;

        default:
            fprintf(stderr,"Unknown option %c", option);
            exit(EXIT_FAILURE);
        }
    }

    if(optind + 1 != argc) {
        fprintf(stderr,"Must specify filename as first non-option\n");
        exit(EXIT_FAILURE);
    }

    basepath = strdup(argv[optind]);
    if(!basepath) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    if((suffix = strrchr(basepath, '.')))
        *suffix = 0;

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

    /* default: map and split bin */
    write_output(basepath, do_map, do_bin, do_hex, do_split);

    DPRINTF(DBG_INFO, "Done.\n");
    exit(EXIT_SUCCESS);
}
