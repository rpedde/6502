%{

/*
 * Simple 6502 assembly parser
 *
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

#define _INCLUDE_OPCODE_MAP
#include "opcodes.h"

#include "compiler.h"
#include "debug.h"

#define YYERROR_VERBOSE 1

extern int yyerror(char *msg);
extern int yylex (void);

value_t *y_new_nval(int type, uint16_t nval, char *lval);
void y_add_opdata(uint8_t,  uint8_t, struct value_t_struct *);
void y_add_label(char *);
void y_add_symtable(char *label, value_t *value);
value_t *y_lookup_symbol(char *label);
void y_add_offset(uint16_t offset);
void y_add_wordsym(char *label, uint16_t word);
void y_add_bytesym(char *label, uint8_t byte);
void y_add_byte(uint8_t byte);
void y_add_word(uint16_t word);
void y_add_nval(value_t *value);
void y_add_string(char *string);
value_t *y_create_arith(value_t *left, value_t *right, uint8_t operator);

symtable_t symtable = { NULL, NULL, NULL };
int parser_line = 1;

%}

%union {
    uint8_t byte;
    uint16_t word;
    char *string;
    uint8_t opcode;
    value_t *nval;
};

%token <opcode> OP
%token <opcode> RELOP
%token <byte> BYTE
%token <word> WORD
%token <string> XREG
%token <string> YREG
%token <string> AREG
%token <string> LABEL
%token <string> ADDRLABEL
%token <string> STRING
%token BYTELITERAL
%token WORDLITERAL
%token EOL
%token TAB
%token EQ
%token ORG

%type <nval> lvalue
%type <nval> value
%type <nval> wordlvalue
%type <nval> bytelvalue

%%

program: line {}
| program line {}
;

line: OP EOL { y_add_opdata($1, CPU_ADDR_MODE_IMPLICIT, NULL); parser_line++; }
| OP AREG EOL { y_add_opdata($1, CPU_ADDR_MODE_ACCUMULATOR, NULL); parser_line++; }
| OP '#' value EOL { y_add_opdata($1, CPU_ADDR_MODE_IMMEDIATE, $3); parser_line++; } // Must be BYTE
| RELOP value EOL { y_add_opdata($1, CPU_ADDR_MODE_RELATIVE, $2); parser_line++;} // Must be BYTE
| OP value EOL { y_add_opdata($1, CPU_ADDR_MODE_UNKNOWN, $2); parser_line++; } // OR REL!
| OP value ',' XREG EOL { y_add_opdata($1, CPU_ADDR_MODE_UNKNOWN_X, $2); parser_line++; } // should be word, optimize at compile
| OP value ',' YREG EOL { y_add_opdata($1, CPU_ADDR_MODE_UNKNOWN_Y, $2); parser_line++; } // should be word, optimize at compile
| OP '(' value ')' EOL { y_add_opdata($1, CPU_ADDR_MODE_INDIRECT, $3); parser_line++; } // Must be WORD
| OP '(' value ',' XREG ')' EOL { y_add_opdata($1, CPU_ADDR_MODE_IND_X, $3); parser_line++; } // Must be BYTE
| OP '(' value ')' ',' YREG EOL { y_add_opdata($1, CPU_ADDR_MODE_IND_Y, $3); parser_line++; } // Must be BYTE
| LABEL EQ WORD EOL { y_add_wordsym($1, $3); parser_line++; }
| LABEL EQ BYTE EOL { y_add_bytesym($1, $3); parser_line++; }
| LABEL '=' WORD EOL { y_add_wordsym($1, $3); parser_line++; }
| LABEL '=' BYTE EOL { y_add_bytesym($1, $3); parser_line++; }
| LABEL { y_add_label($1); } /* an address label */
| EOL { parser_line++; }
| '*' '=' WORD EOL { y_add_offset($3); parser_line++; }
| ORG WORD EOL { y_add_offset($2); parser_line++; }
| BYTELITERAL bytestring EOL { parser_line++; }
| BYTELITERAL STRING EOL { y_add_string($2); parser_line++; }
| WORDLITERAL wordstring EOL { parser_line++; }
;

wordlvalue: WORD { $$ = y_new_nval(Y_TYPE_WORD, $1, NULL); }
| LABEL { $$ = y_new_nval(Y_TYPE_LABEL, 0, $1); }
| '*' { $$ = y_new_nval(Y_TYPE_LABEL, 0, "*"); }
;

/* We'll typecheck these on pass 2 */
bytelvalue: BYTE { $$ = y_new_nval(Y_TYPE_BYTE, $1, NULL); }
;

bytestring: bytelvalue { y_add_nval($1); }
| bytestring ',' bytelvalue { y_add_nval($3); }
;

wordstring: wordlvalue { y_add_nval($1); }
| wordstring ',' wordlvalue { y_add_nval($3); }
;

lvalue: wordlvalue
| bytelvalue
;

value: lvalue { $$ = $1; }
| lvalue '+' lvalue { $$ = y_create_arith($1, $3, '+'); }
| lvalue '-' lvalue { $$ = y_create_arith($1, $3, '-'); }
;


%%

value_t *y_new_nval(int type, uint16_t nval, char *lval) {
    value_t *pnew;

    pnew = (value_t*)malloc(sizeof(value_t));
    if(!pnew) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    pnew->type = type;

    /* here, I could do immediate lookups, but I'll just defer it...
     * we're stuck in 2 pass anyway, might as well just do lookups
     * and sanity checks in one place.  */

    switch(type) {
    case Y_TYPE_BYTE:
        pnew->byte = nval;
        break;
    case Y_TYPE_WORD:
        pnew->word = nval;
        break;
    case Y_TYPE_LABEL:
        pnew->label = lval;
        break;
    case Y_TYPE_ARITH:
        pnew->byte = nval;
        break;
    default:
        DPRINTF(DBG_ERROR, "Bad type");
        exit(EXIT_FAILURE);
        break;
    }

    return pnew;
}

/*
 * y_evaluate_val
 */
value_t *y_evaluate_val(value_t *value, int line, uint16_t addr) {
    value_t *retval;
    value_t *left, *right;
    uint16_t left_val, right_val;
    uint16_t retval_val;

    if(!value)
        return NULL;

    if(value->type == Y_TYPE_WORD || value->type == Y_TYPE_BYTE)
        return value;

    if(value->type == Y_TYPE_LABEL) {
        retval = y_lookup_symbol(value->label);
        if(!retval) {
            DPRINTF(DBG_ERROR, "Error: Line %d: Cannot lookup symbol %s\n",
                    line, value->label);
            exit(EXIT_FAILURE);
        }

        /* dummy up the "*" label */
        if (strcmp(value->label, "*") == 0) {
            retval->word = addr;
        }

        return retval;
    }

    if(value->type == Y_TYPE_ARITH) {
        left = y_evaluate_val(value->left, line, addr);
        right = y_evaluate_val(value->right, line, addr);

        retval = (value_t*)malloc(sizeof(value_t));
        if(!retval) {
            perror("malloc");
            exit(EXIT_FAILURE);
        }

        if(left->type == Y_TYPE_WORD)
            left_val = left->word;
        else
            left_val = left->byte;

        if(right->type == Y_TYPE_WORD)
            right_val = right->word;
        else
            right_val = right->byte;

        if(left->type == right->type) {
            retval->type = left->type;
        } else {
            DPRINTF(DBG_WARN,"Line %d: Warning: Promoting BYTE value to WORD\n", line);
            retval->type = Y_TYPE_WORD;
        }

        switch(value->byte) {
        case '+':
            /* should we be checking for overflow? */
            retval_val = right_val + left_val;
            break;
        case '-':
            retval_val = right_val - left_val;
            break;
        default:
            DPRINTF(DBG_ERROR, "Bad op: %c\n", value->byte);
            exit(EXIT_FAILURE);
        }

        if(retval->type == Y_TYPE_WORD)
            retval->word = retval_val;
        else
            retval->byte = retval_val;

        return retval;
    }

    DPRINTF(DBG_ERROR,"Bad y-type in eval (%d)\n", value->type);
    exit(EXIT_FAILURE);
}


/*
 * y_add_wordsym
 */
void y_add_wordsym(char *label, uint16_t word) {
    value_t *value;

    DPRINTF(DBG_DEBUG, " - Adding WORD symbol '%s' as %04x\n", label, word);
    value = y_new_nval(Y_TYPE_WORD, word, NULL);
    y_add_symtable(label, value);
}


/*
 * y_add_bytesym
 */
void y_add_bytesym(char *label, uint8_t byte) {
    value_t *value;

    DPRINTF(DBG_DEBUG, " - Adding BYTE symbol '%s' as %02x\n", label, byte);
    value = y_new_nval(Y_TYPE_BYTE, byte, NULL);
    y_add_symtable(label, value);
}

/*
 * y_create_arith
 *
 * create an arithmetic parse tree to be evaluated later
 */
value_t *y_create_arith(value_t *left, value_t *right, uint8_t operator) {
    value_t *pnew;

    pnew = y_new_nval(Y_TYPE_ARITH, operator, NULL);
    pnew->left = left;
    pnew->right = right;
    return pnew;
}

/*
 * y_add_string
 *
 * Add a string (a byte at a time...) to the output
 */
void y_add_string(char *string) {
    uint8_t *pcurrent = (uint8_t*)string;

    while(*pcurrent) {
        y_add_byte(*pcurrent);
        pcurrent++;
    }
}

void y_add_byte(uint8_t byte) {
    value_t *value;
    opdata_t *pvalue;

    value = y_new_nval(Y_TYPE_BYTE, byte, NULL);

    pvalue = (opdata_t *)malloc(sizeof(opdata_t));
    if(!pvalue) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    memset(pvalue,0,sizeof(opdata_t));
    pvalue->type = TYPE_DATA;
    pvalue->value = value;
    pvalue->len = 1;

    add_opdata(pvalue);
}


/*
 * y_add_word
 */
void y_add_word(uint16_t word) {
    value_t *value;
    opdata_t *pvalue;

    value = y_new_nval(Y_TYPE_WORD, word, NULL);

    pvalue = (opdata_t *)malloc(sizeof(opdata_t));
    if(!pvalue) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    memset(pvalue,0,sizeof(opdata_t));
    pvalue->type = TYPE_DATA;
    pvalue->value = value;
    pvalue->len = 2;

    add_opdata(pvalue);
}

void y_add_nval(value_t *value) {
    opdata_t *pvalue;

    pvalue = (opdata_t *)malloc(sizeof(opdata_t));
    if(!pvalue) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    memset(pvalue,0,sizeof(opdata_t));
    pvalue->type = TYPE_DATA;
    pvalue->value = value;
    switch(value->type) {
    case Y_TYPE_BYTE:
        pvalue->len = 1;
        break;
    case Y_TYPE_WORD:
    case Y_TYPE_LABEL:
        pvalue->len = 2;
        break;
    default:
        fprintf(stderr, "line %d: don't know type of arith expression\n",
            parser_line);
        exit(EXIT_FAILURE);
    }

    add_opdata(pvalue);
}


/*
 * y_add_label
 */
void y_add_label(char *label) {
    value_t *value;
    opdata_t *pvalue;

    DPRINTF(DBG_DEBUG, " - Adding address label '%s'\n", label);

    /* we don't really know the address until we determine indexing
     * modes on pass 2, so we'll add a dummy symbol with the right
     * type as a placeholder */
    value = y_new_nval(Y_TYPE_WORD, 0, NULL);
    y_add_symtable(label, value);

    /* also need to drop a marker in the opdata list, so we can
     * figure the value later */
    pvalue = (opdata_t *)malloc(sizeof(opdata_t));
    if(!pvalue) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    value = y_new_nval(Y_TYPE_LABEL, 0, label);

    memset(pvalue,0,sizeof(opdata_t));
    pvalue->type = TYPE_LABEL;
    pvalue->value = value;

    add_opdata(pvalue);
}

/*
 * add a cpu offset adjustmet (*=$VALUE)
 */
void y_add_offset(uint16_t offset) {
    opdata_t *pvalue;
    value_t *value;

    DPRINTF(DBG_DEBUG, " - Adding new origin '%04x'\n", offset);

    pvalue = (opdata_t *)malloc(sizeof(opdata_t));
    if(!pvalue) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    value = y_new_nval(Y_TYPE_WORD, offset, NULL);

    memset(pvalue,0,sizeof(opdata_t));
    pvalue->type = TYPE_OFFSET;
    pvalue->value = value;

    add_opdata(pvalue);
}


/*
 * add a symbol table entry
 */
void y_add_symtable(char *label, value_t *value) {
    symtable_t *pnew;

    DPRINTF(DBG_DEBUG, " - Adding symbol '%s'\n", label);

    if(y_lookup_symbol(label)) {
        DPRINTF(DBG_ERROR, "Error:  duplicate symbol: %s\n", label);
        exit(EXIT_FAILURE);
    }

    pnew = (symtable_t*)malloc(sizeof(symtable_t));
    if(!pnew) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    pnew->label = label;
    pnew->value = value;

    pnew->next = symtable.next;
    symtable.next = pnew;

    return;
}

value_t *y_lookup_symbol(char *label) {
    symtable_t *pcurrent = symtable.next;
    while(pcurrent && (strcasecmp(label, pcurrent->label) != 0)) {
        pcurrent = pcurrent->next;
    }
    if(pcurrent)
        return pcurrent->value;

    return NULL;
}


/*
 * y_add_opdata
 */

void y_add_opdata(uint8_t opcode_family, uint8_t addressing_mode,
                  value_t *pvalue) {
    opdata_t *pnew;

    pnew = (opdata_t *)malloc(sizeof(opdata_t));
    if(!pnew) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    /* Some addressing modes are unknown on the first
     * pass, specifically, we can't tell the difference
     * between absolute and zero page unless we know the size
     * of the operand.
     */

    memset(pnew,0,sizeof(opdata_t));
    pnew->type = TYPE_INSTRUCTION;
    pnew->opcode_family = opcode_family;
    pnew->addressing_mode = addressing_mode;
    pnew->value = pvalue;

    if(pnew->type == TYPE_LABEL)
        pnew->needs_fixup = 1;

    switch(addressing_mode) {
    case CPU_ADDR_MODE_IMPLICIT:
    case CPU_ADDR_MODE_ACCUMULATOR:
        pnew->len = 1;
        break;
    case CPU_ADDR_MODE_IMMEDIATE:
    case CPU_ADDR_MODE_ZPAGE:
    case CPU_ADDR_MODE_ZPAGE_X:
    case CPU_ADDR_MODE_ZPAGE_Y:
    case CPU_ADDR_MODE_RELATIVE:
    case CPU_ADDR_MODE_IND_X:
    case CPU_ADDR_MODE_IND_Y:
        pnew->len = 2;
        break;
    case CPU_ADDR_MODE_ABSOLUTE:
    case CPU_ADDR_MODE_ABSOLUTE_X:
    case CPU_ADDR_MODE_ABSOLUTE_Y:
    case CPU_ADDR_MODE_INDIRECT:
        pnew->len = 3;
        break;

    case CPU_ADDR_MODE_UNKNOWN:
    case CPU_ADDR_MODE_UNKNOWN_X:
    case CPU_ADDR_MODE_UNKNOWN_Y:
        /* We won't be able to figure out real length
         * until pass 2 */
        pnew->len = 0;
        break;

    default:
        DPRINTF(DBG_FATAL,"Bad addressing mode: %d\n", addressing_mode);
        break;
    }

    pnew->line = parser_line;

    add_opdata(pnew);
}
