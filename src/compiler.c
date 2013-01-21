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

symtable_t symtable = { NULL, NULL, NULL, 0, NULL };

extern FILE *yyin;
extern int yyparse(void*);

uint16_t compiler_offset = 0x8000;

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

extern uint16_t last_line_offset;

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

uint16_t opcode_lookup(opdata_t *op, int fatal) {
    opcode_t *ptable = (opcode_t*)&cpu_opcode_map;
    int index = 0;

    while(index < 256 && (op->opcode_family != ptable->opcode_family ||
                          op->addressing_mode != ptable->addressing_mode)) {
        index++;
        ptable++;
    }

    if(index == 256) {
        if(fatal) {
            PERROR("Can't lookup indexing mode '%s' for '%s'",
                   cpu_addressing_mode[op->addressing_mode],
                   cpu_opcode_mnemonics[op->opcode_family]);
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
    value_t *operand = NULL;
    uint16_t eaddr, taddr;
    int offset;
    int symchanges = 1;
    int failed_symchanges = 0;
    symtable_t *psym;

    INFO("Pass 2: Symbol resolution");

    /* first, walk through the entire symbol table and express everything */
    while(symchanges) {
        psym = symtable.next;
        symchanges = 0;
        failed_symchanges = 0;
        while(psym) {
            if ((psym->value->type != Y_TYPE_BYTE) &&
                (psym->value->type != Y_TYPE_WORD)) {
                if(y_can_evaluate(psym->value)) {
                    operand = y_evaluate_val(psym->value, psym->line, 0);
                    if(operand) {
                        free(psym->value);
                        psym->value = operand;
                        symchanges++;
                    } else {
                        failed_symchanges++;
                    }
                } else {
                    failed_symchanges++;
                }
            }
            psym = psym->next;
        }
    }

    /* any symbol we can't resolve is a problem */
    if(failed_symchanges) {
        psym = symtable.next;
        while(psym) {
            parser_line = psym->line;
            parser_file = psym->file;

            if ((psym->value->type != Y_TYPE_BYTE) &&
                (psym->value->type != Y_TYPE_WORD)) {
                YERROR("cannot resolve symbol: %s", psym->label);
            }
            psym = psym->next;
        }
    }

    while(pcurrent) {
        parser_file = pcurrent->data->file;
        parser_line = pcurrent->data->line;

        if (pcurrent->data->type == TYPE_DATA) {
            /* force expression of values */
            operand = y_evaluate_val(pcurrent->data->value, pcurrent->data->line, pcurrent->data->offset);
            if(!operand) {
                YERROR("unresolvable symbol");
                break;
            }
            pcurrent->data->value = operand;

            if(pcurrent->data->forced_type == Y_TYPE_WORD) {
                value_promote(operand);
            } else if (pcurrent->data->forced_type == Y_TYPE_BYTE) {
                if(!y_value_is_byte(operand)) {
                    YERROR("byte literal value is not a byte");
                    break;
                }
                value_demote(operand);
            } else {
                /* should not happen */
                PFATAL("bad forced type");
                exit(EXIT_FAILURE);
            }
        }

        if (pcurrent->data->type == TYPE_INSTRUCTION) {
            if(pcurrent->data->value) {
                operand = y_evaluate_val(pcurrent->data->value, pcurrent->data->line, pcurrent->data->offset);
                if(!operand) {
                    YERROR("unresolvable symbol");
                    break;
                }
                pcurrent->data->value = operand;
            }

            switch(pcurrent->data->addressing_mode) {
            case CPU_ADDR_MODE_IMPLICIT:
            case CPU_ADDR_MODE_ACCUMULATOR:
                break;
            case CPU_ADDR_MODE_IMMEDIATE:
            case CPU_ADDR_MODE_ZPAGE:
            case CPU_ADDR_MODE_ZPAGE_X:
            case CPU_ADDR_MODE_ZPAGE_Y:
            case CPU_ADDR_MODE_IND_X:
            case CPU_ADDR_MODE_IND_Y:
                if(!y_value_is_byte(operand)) {
                    YERROR("Addressing mode requires BYTE, not WORD");
                    break;
                }
                value_demote(operand);

                break;

            case CPU_ADDR_MODE_RELATIVE:
                eaddr = pcurrent->data->offset + 2;
                value_promote(operand);

                /* I don't think it's valid to put a BYTE value
                 * directly in a relative branch, so we won't
                 * address that point
                 */

                taddr = operand->word;
                offset = taddr - eaddr;

                if(offset < -128 || offset > 127) {
                    YERROR("Branch out of range");
                    break;
                }

                operand->type = Y_TYPE_BYTE;
                if(offset < 0) {
                    operand->byte = abs(offset) ^ 0xFF;
                    operand->byte += 1;
                } else {
                    operand->byte = offset;
                }
                PDEBUG("Fixed up branch to $%02x",
                      (uint16_t)operand->byte);
                break;

            case CPU_ADDR_MODE_ABSOLUTE:
            case CPU_ADDR_MODE_ABSOLUTE_X:
            case CPU_ADDR_MODE_ABSOLUTE_Y:
            case CPU_ADDR_MODE_INDIRECT:
                value_promote(operand);
                break;

            default:
                PERROR("unnknown addressing mode");
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
    FILE *map = NULL, *bin = NULL, *hex = NULL, *dbg = NULL;
    opdata_list_t *pcurrent = list.next;
    int len;
    symtable_t *psym;
    uint16_t current_offset;
    uint16_t last_offset;
    value_t *pvalue;
    hex_line_t hexline;

    if(write_map) {
        map = make_fh(basename, "map", 0);
        INFO("Writing output.");

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
                    PERROR("Unexpressed symbol table entry: %s", psym->label);
                    fprintf(map, "???");
                }
                fprintf(map,"\n");
            }
            psym = psym->next;
        }

        fprintf(map, "\n\nOutput:\n=======================================================\n\n");

        while(pcurrent) {
            parser_file = pcurrent->data->file;
            parser_line = pcurrent->data->line;

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
                    fprintf(map, "($%02x,X)", pcurrent->data->value->byte);
                    break;
                case CPU_ADDR_MODE_IND_Y:
                    fprintf(map, "($%02x),Y", pcurrent->data->value->byte);
                    break;
                default:
                    PERROR("Unknown addressing mode");
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
                    PERROR("Bad Y-type in static data");
                    exit(EXIT_FAILURE);
                }
                fprintf(map, "\n");
                break;
            default:
                PERROR("unhandled optype while printing map");
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

        while(pcurrent) {
            parser_file = pcurrent->data->file;
            parser_line = pcurrent->data->line;

            /* decide if we need to split */
            last_offset = current_offset; /* this is actually the calculated offset */
            current_offset = pcurrent->data->offset;

            if (current_offset != last_offset) {
                /* we have a gap.  we fill if we're not doing splits, or if the
                 * split is less than the min_split_gap */
                if(write_bin) {
                    if (((current_offset - last_offset) < min_split_gap) || (!split_bin)) {
                        PDEBUG("Filling %d byte gap to $%04x",
                               current_offset - last_offset, current_offset);

                        while(last_offset < current_offset) {
                            fprintf(bin, "%c", skip_data_byte);
                            last_offset++;
                        }
                    } else { /* split bins with excessive gap */
                        char *tmp_filename = NULL;

                        fclose(bin);

                        asprintf(&tmp_filename, "%04X.bin", current_offset);
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

                default:
                    PERROR("bad TYPE_DATA writing bin file: %d", pcurrent->data->type);
                    exit(EXIT_FAILURE);
                }
                current_offset += pcurrent->data->len;
                break;

            default:
                PFATAL("unhandled data type: %d", pcurrent->data->type);
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

        dbg = make_fh(basename, "dbg", 1);
        pcurrent = list.next;
        current_offset = pcurrent->data->offset - 1;

        while(pcurrent) {
            uint16_t fsize;

            if ((pcurrent->data) && (pcurrent->data->offset != current_offset)) {
                char *fullpath;
                fullpath = realpath(pcurrent->data->file, NULL);
                fwrite(&pcurrent->data->offset, 1, sizeof(uint16_t), dbg);
                fwrite(&pcurrent->data->line, 1, sizeof(uint32_t), dbg);
                fsize = strlen(fullpath) + 1;
                fwrite(&fsize, 1, sizeof(uint16_t), dbg);
                fwrite(fullpath, 1, strlen(fullpath) + 1, dbg);
                free(fullpath);
                current_offset = pcurrent->data->offset;
            }
            pcurrent = pcurrent->next;
        }
        fclose(dbg);
    }
}

int main(int argc, char *argv[]) {
    int option;
    char *basepath;
    char *suffix;
    int do_split = 0;
    int do_map = 1;
    int do_bin = 1;
    int do_hex = 0;

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

    compiler_offset = 0x8000;

    INFO("Pass 1:  Parsing.");
    l_parse_file(argv[optind]);

    compiler_offset = 0x8000;

    pass2();


    if(l_parse_error) {
        ERROR("Assemble aborted.");
        exit(EXIT_FAILURE);
    }


    /* default: map and split bin */
    write_output(basepath, do_map, do_bin, do_hex, do_split);

    INFO("Done.");
    exit(EXIT_SUCCESS);
}
