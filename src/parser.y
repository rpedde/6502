%{

/*
 * Simple 6502 assembly parser
 *
 * Copyright (C) 2009-2013 Ron Pedde (ron@pedde.com)
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

#include "config.h"

#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define _INCLUDE_OPCODE_MAP
#include "opcodes.h"

#include "compiler.h"
#include "debug.h"

#define YERROR_VERBOSE 1

extern int yylex (void);

value_t *y_new_nval(int type, uint16_t nval, char *lval);
void y_add_opdata(uint8_t,  uint8_t, struct value_t_struct *);
void y_add_label(char *);
void y_add_symtable(char *label, value_t *value);
value_t *y_lookup_symbol(char *label);
void y_add_offset(value_t *value);
void y_add_wordsym(char *label, uint16_t word);
void y_add_bytesym(char *label, uint8_t byte);
void y_add_byte(uint8_t byte);
void y_add_word(uint16_t word);
void y_add_nval(value_t *value);
void y_add_nval_typed(value_t *value, int type);
void y_add_string(char *string);
value_t *y_create_arith(value_t *left, value_t *right, int operator);
value_t *y_evaluate_val(value_t *value, int line, uint16_t addr);
void *error_malloc(ssize_t size);
void y_value_free(value_t *value);
void y_dump_value(value_t *value);

%}

%union {
    uint8_t byte;
    uint16_t word;
    char *string;
    uint8_t opcode;
    value_t *nval;
    int arithop;
};

%token <opcode> TADC
%token <opcode> TAND
%token <opcode> TASL
%token <opcode> TBCC
%token <opcode> TBCS
%token <opcode> TBEQ
%token <opcode> TBIT
%token <opcode> TBMI
%token <opcode> TBNE
%token <opcode> TBPL
%token <opcode> TBRK
%token <opcode> TBVC
%token <opcode> TBVS
%token <opcode> TCLC
%token <opcode> TCLD
%token <opcode> TCLI
%token <opcode> TCLV
%token <opcode> TCMP
%token <opcode> TCPX
%token <opcode> TCPY
%token <opcode> TDEC
%token <opcode> TDEX
%token <opcode> TDEY
%token <opcode> TEOR
%token <opcode> TINC
%token <opcode> TINX
%token <opcode> TINY
%token <opcode> TJMP
%token <opcode> TJSR
%token <opcode> TLDA
%token <opcode> TLDX
%token <opcode> TLDY
%token <opcode> TLSR
%token <opcode> TNOP
%token <opcode> TORA
%token <opcode> TPHA
%token <opcode> TPHP
%token <opcode> TPLA
%token <opcode> TPLP
%token <opcode> TROL
%token <opcode> TROR
%token <opcode> TRTI
%token <opcode> TRTS
%token <opcode> TSBC
%token <opcode> TSEC
%token <opcode> TSED
%token <opcode> TSEI
%token <opcode> TSTA
%token <opcode> TSTX
%token <opcode> TSTY
%token <opcode> TTAX
%token <opcode> TTAY
%token <opcode> TTSX
%token <opcode> TTXA
%token <opcode> TTXS
%token <opcode> TTYA

%type <opcode> IMPLICIT_OP
%type <opcode> ACCUM_OP
%type <opcode> IMMEDIATE_OP
%type <opcode> REL_OP
%type <opcode> INDIRECT_OP
%type <opcode> IND_XY_OP
%type <opcode> ABS_OP
%type <opcode> ZP_ABS_ALL_OP

%token <byte> BYTE
%token <word> WORD
%token <string> XREG
%token <string> YREG
%token <string> AREG
%token <string> LABEL
%token <string> STRING

%token <arithop> AND
%token <arithop> OR

%token <arithop> BAND
%token <arithop> BOR
%token <arithop> BXOR
%token <arithop> BNOT
%token <arithop> BSL
%token <arithop> BSR
%token <arithop> LOWB
%token <arithop> HIB

%token <arithop> LT
%token <arithop> GT

%token <arithop> PLUS
%token <arithop> MINUS
%token <arithop> MUL
%token <arithop> DIV
%token <arithop> MOD

%token BYTELITERAL
%token WORDLITERAL
%token EOL
%token EQ
%token ORG

%type <nval> expression
%type <arithop> math_operator
%type <arithop> bool_operator
%type <arithop> binary_operator
%type <arithop> unary_operator

%left BAND BXOR BOR

%left AND OR
%left LT GT

%left PLUS MINUS
%left MUL DIV MOD

%left BNOT LOWB HIB

%nonassoc binary_op
%nonassoc bool_op
%nonassoc math_op
%nonassoc unary_op

/* %glr-parser */

%%

program: EOL { l_advance_line(); }
| line EOL { l_advance_line(); }
| program EOL { l_advance_line(); }
| program line EOL { l_advance_line(); }
;

line: LINE {}
| LABELLINE {}
| LABEL LABELLINE { y_add_wordsym($1, compiler_offset); }
;

/* line that cannot be labeled */
LINE: MUL '=' expression { y_add_offset($3); }
| ORG expression { y_add_offset($2); }
| LABEL { y_add_wordsym($1, compiler_offset); }
| LABEL EQ expression { y_add_symtable($1, $3); }
| LABEL '=' expression { y_add_symtable($1, $3); }
;

/* lines that can have an optional label */
LABELLINE: OPCODE_SEQ
| BYTELITERAL bytestring
| WORDLITERAL wordstring
;

/* valid opcode data */
OPCODE_SEQ: IMPLICIT_OP { y_add_opdata($1, CPU_ADDR_MODE_IMPLICIT, NULL); }
| ACCUM_OP { y_add_opdata($1, CPU_ADDR_MODE_ACCUMULATOR, NULL); }
| ACCUM_OP AREG { y_add_opdata($1, CPU_ADDR_MODE_ACCUMULATOR, NULL); }
| IMMEDIATE_OP '#' expression { y_add_opdata($1, CPU_ADDR_MODE_IMMEDIATE, $3); }
| REL_OP expression { y_add_opdata($1, CPU_ADDR_MODE_RELATIVE, $2); }
| ABS_OP expression { y_add_opdata($1, CPU_ADDR_MODE_ABSOLUTE, $2); }
| ZP_ABS_ALL_OP expression { y_add_opdata($1, CPU_ADDR_MODE_UNKNOWN, $2); }
| ZP_ABS_ALL_OP expression ',' XREG { y_add_opdata($1, CPU_ADDR_MODE_UNKNOWN_X, $2); }
| ZP_ABS_ALL_OP expression ',' YREG { y_add_opdata($1, CPU_ADDR_MODE_UNKNOWN_Y, $2); }
| INDIRECT_OP '(' expression ')' { y_add_opdata($1, CPU_ADDR_MODE_INDIRECT, $3); }
| IND_XY_OP '(' expression ',' XREG ')' { y_add_opdata($1, CPU_ADDR_MODE_IND_X, $3); }
| IND_XY_OP '(' expression ')' ',' YREG { y_add_opdata($1, CPU_ADDR_MODE_IND_Y, $3); }
;

// CPU_ADDR_MODE_IMPLICIT
IMPLICIT_OP: TBRK { $$ = $1; }
| TPHP { $$ = $1; }
| TCLC { $$ = $1; }
| TNOP { $$ = $1; }
| TPLP { $$ = $1; }
| TSEC { $$ = $1; }
| TRTI { $$ = $1; }
| TPHA { $$ = $1; }
| TCLI { $$ = $1; }
| TRTS { $$ = $1; }
| TPLA { $$ = $1; }
| TSEI { $$ = $1; }
| TDEY { $$ = $1; }
| TTXA { $$ = $1; }
| TTYA { $$ = $1; }
| TTXS { $$ = $1; }
| TTAY { $$ = $1; }
| TTAX { $$ = $1; }
| TCLV { $$ = $1; }
| TTSX { $$ = $1; }
| TINY { $$ = $1; }
| TDEX { $$ = $1; }
| TCLD { $$ = $1; }
| TINX { $$ = $1; }
| TSED { $$ = $1; }
;

// CPU_ADDR_MODE_ACCUMULATOR
ACCUM_OP: TASL { $$ = $1; }
| TROL { $$ = $1; }
| TLSR { $$ = $1; }
| TROR { $$ = $1; }
;

// CPU_ADDR_MODE_IMMEDIATE
IMMEDIATE_OP: TORA { $$ = $1; }
| TAND { $$ = $1; }
| TEOR { $$ = $1; }
| TADC { $$ = $1; }
| TLDY { $$ = $1; }
| TLDX { $$ = $1; }
| TLDA { $$ = $1; }
| TCPY { $$ = $1; }
| TCMP { $$ = $1; }
| TCPX { $$ = $1; }
| TSBC { $$ = $1; }
;

// CPU_ADDR_MODE_RELATIVE
REL_OP: TBPL { $$ = $1; }
| TBMI { $$ = $1; }
| TBVC { $$ = $1; }
| TBVS { $$ = $1; }
| TBCC { $$ = $1; }
| TBCS { $$ = $1; }
| TBNE { $$ = $1; }
| TBEQ { $$ = $1; }
;

// CPU_ADDR_MODE_INDIRECT
INDIRECT_OP: TJMP { $$ = $1; }
;

// CPU_ADDR_MODE_IND_Y
IND_XY_OP: TORA { $$ = $1; }
| TAND { $$ = $1; }
| TEOR { $$ = $1; }
| TADC { $$ = $1; }
| TSTA { $$ = $1; }
| TLDA { $$ = $1; }
| TCMP { $$ = $1; }
| TSBC { $$ = $1; }
;

// ZP_Y, ZP_X, ZP, ABS_Y, ABS_X, ABS
ZP_ABS_ALL_OP: IND_XY_OP { $$ = $1; }
| TASL { $$ = $1; }
| TROL { $$ = $1; }
| TLSR { $$ = $1; }
| TROR { $$ = $1; }
| TSTX { $$ = $1; }   // strangely no stx abs,y.. pick that up in pass2
| TLDX { $$ = $1; }
| TDEC { $$ = $1; }
| TINC { $$ = $1; }
| TBIT { $$ = $1; }   // ZP/ABS only
| TSTY { $$ = $1; }   // ZP/ABS/ZP,X only
| TLDY { $$ = $1; }   // ZP/ABS/ZP,X/ABS,X
| TCPY { $$ = $1; }   // ZP/ABS only
| TCPX { $$ = $1; }   // ZP/ABS only
;

ABS_OP: TJMP { $$ = $1; }
| TJSR { $$ = $1; }
;

expression: WORD { $$ = y_new_nval(Y_TYPE_WORD, $1, NULL); }
| BYTE { $$ = y_new_nval(Y_TYPE_BYTE, $1, NULL); }
| LABEL { $$ = y_new_nval(Y_TYPE_LABEL, 0, $1); }
| MUL { $$ = y_new_nval(Y_TYPE_LABEL, 0, "*"); }
| unary_operator expression %prec unary_op { $$ = y_create_arith($2, NULL, $1); }
| expression math_operator expression %prec math_op { $$ = y_create_arith($1, $3, $2); }
| expression bool_operator expression %prec bool_op { $$ = y_create_arith($1, $3, $2); }
| expression binary_operator expression %prec binary_op { $$ = y_create_arith($1, $3, $2); }
| '[' expression ']' { $$ = $2; }
;

math_operator: PLUS { $$ = $1; }
| MINUS { $$ = $1; }
| MUL { $$ = $1; }
| DIV { $$ = $1; }
| MOD { $$ = $1; }
;

bool_operator: GT { $$ = $1; }
| LT { $$ = $1; }
| AND { $$ = $1; }
| OR { $$ = $1; }
;

binary_operator: BXOR { $$ = $1; }
| BOR { $$ = $1; }
| BAND { $$ = $1; }
;

unary_operator: BNOT { $$ = $1; }
| LOWB { $$ = $1; }
| HIB { $$ = $1; }
;

bytestring: expression { y_add_nval_typed($1, Y_TYPE_BYTE); }
| STRING { y_add_string($1); }
| bytestring ',' expression { y_add_nval_typed($3, Y_TYPE_BYTE); }
| bytestring ',' STRING { y_add_string($3); }
;

wordstring: expression { y_add_nval_typed($1, Y_TYPE_WORD); }
| wordstring ',' expression { y_add_nval_typed($3, Y_TYPE_WORD); }
;


%%

value_t *y_new_nval(int type, uint16_t nval, char *lval) {
    value_t *pnew;

    pnew = (value_t*)error_malloc(sizeof(value_t));
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
        pnew->label = strdup(lval);
        if(lval[strlen(lval) - 1] == ':')
            lval[strlen(lval) - 1] = 0;
        break;
    case Y_TYPE_ARITH:
        pnew->word = nval;
        break;
    default:
        PERROR("Bad type");
        exit(EXIT_FAILURE);
        break;
    }

    return pnew;
}

/**
 * stupid malloc wrapper to crash on malloc errors
 *
 * @param size length of buffer to alloc
 */
void *error_malloc(ssize_t size) {
    void *retval;

    retval = (void*)malloc(size);
    if(!retval) {
        FATAL("malloc: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    return retval;
}

/**
 * dump a value
 *
 * @param value item to dump
 */
void y_do_dump_value(value_t *value, int depth) {
    if(!value) {
        PDEBUG("%*c NULL", depth*2, ' ');
        return;
    }

    switch(value->type) {
    case Y_TYPE_WORD:
        PDEBUG("%*c $%04x", depth*2, ' ', value->word);
        break;
    case Y_TYPE_BYTE:
        PDEBUG("%*c $%02x", depth*2, ' ', value->byte);
        break;
    case Y_TYPE_LABEL:
        PDEBUG( "%*c $%s", depth*2, ' ', value->label);
        break;
    case Y_TYPE_ARITH:
        PDEBUG("%*c Arith op $%04x", depth*2, ' ', value->word);
        PDEBUG("%*c L:", depth*2, ' ');
        y_do_dump_value(value->left, depth + 1);
        PDEBUG("%*c R:", depth*2, ' ');
        y_do_dump_value(value->right, depth + 1);
        break;
    }
}


void y_dump_value(value_t *value) {
    y_do_dump_value(value, 0);
}



/**
 * given a value (arithmetic or otherwise),
 * see if we can *right now* calculate a value for it.
 * (i.e. all labels are resolved).  This is useful to see
 * if we know in advance that a parameter is ZP, so we can
 * set addressing mode without another pass
 *
 * @param value the arithmetic or literal value to check
 */
int y_can_evaluate(value_t *value) {
    value_t *psym;

    if(!value)
        *((char*)(NULL)) = 0;

    assert(value);

    if(!value) {
        PFATAL("Internal error");
        exit(EXIT_FAILURE);
    }

    if ((value->type == Y_TYPE_WORD) || (value->type == Y_TYPE_BYTE))
        return 1;

    if (value->type == Y_TYPE_LABEL) {
        psym = y_lookup_symbol(value->label);
        if(!psym)
            return 0;
        return y_can_evaluate(psym);
    }

    if (value->type == Y_TYPE_ARITH) {
        if(y_can_evaluate(value->left)) {
            /* unary operators have no right branch */
            if((!value->right) || ((value->right) && (y_can_evaluate(value->right)))) {
                return 1;
            }
        }
        return 0;
    }

    PFATAL("Cannot evaluate a value of type %d\n", value->type);
    exit(EXIT_FAILURE);
}

/**
 * given a value, see if it is a BYTE type (or can be sized into
 * one.  If it is not or if it is not resolvable, then we will assume
 * it is a word.
 *
 * @param value item to type-size
 */
int y_value_is_byte(value_t *value) {
    int retval = 0;
    value_t *me;

    if(y_can_evaluate(value)) {
        me = y_evaluate_val(value, parser_line, compiler_offset);
        if(!me) {
            PFATAL("Can't evaluate something I should have!");
            exit(EXIT_FAILURE);
        }

        if(me->type == Y_TYPE_BYTE)
            retval = 1;

        if(me->word < 256) /* can be coerced to a byte */
            retval = 1;

        free(me);
        return retval;
    }

    /* we couldn't solve it, so clearly it's an undefined
     * label... have to assume it is a word, as discussed.
     * we'll warn later should it be valid ZP but we assembled
     * immediate. */
    if(value->type == Y_TYPE_LABEL)
        return 0;

    if(value->type == Y_TYPE_ARITH) {
        /* there are a couple cases that this is
         * doable.  x >> 8, for example,
         * or #<$WORD or #>$WORD.  But fsck it.  we'll
         * optimize that when we see it.
         */
        return 0;
    }

    /* can_evaluate should have already thrown this.  meh. */
    PFATAL("Cannot evaluate a value of type %d", value->type);
    exit(EXIT_FAILURE);
}


/**
 * value_promote
 *
 * in-place promote a byte to a word
 *
 * @param value item to promote
 */
void value_promote(value_t *value) {
    uint16_t new_value;

    if(value->type == Y_TYPE_BYTE) {
        new_value = value->byte;
        value->word = new_value;
        value->type = Y_TYPE_WORD;
    }
}

/**
 * value_demote
 *
 * in-place demote a word to a byte
 *
 * @param value item to demote
 */
void value_demote(value_t *value) {
    if(value->type == Y_TYPE_WORD) {
        if(value->word > 255) {
            YERROR("cannot demote word to byte");
            return;
        }
        value->byte = value->word;
        value->type = Y_TYPE_BYTE;
    }
}

void y_value_free(value_t *value) {
    if(!value)
        return;

    switch(value->type) {
    case Y_TYPE_LABEL:
        if(value->label)
            free(value->label);
        /* passthru */
    case Y_TYPE_BYTE:
    case Y_TYPE_WORD:
        free(value);
        break;
    case Y_TYPE_ARITH:
        if(value->left)
            y_value_free(value->left);
        if(value->right)
            y_value_free(value->right);
        free(value);
        break;
    default:
        /* can't happen */
        PFATAL("Unexpected y-value in y_value_free: %d",
               value->type);
        exit(EXIT_FAILURE);
    }
}

/**
 * y_evaluate_val
 *
 * given a value that may be an arithmetic value, express
 * it and return a new value.  Caller is responsible for
 * freeing the called value and the returned value, as
 * appropriate.
 *
 * @param value item to evaluate
 * @param line compiler line number for error messages
 * @param addr current compiler addr (for * label)
 * @return value_t, which caller must free
 */
value_t *y_evaluate_val(value_t *value, int line, uint16_t addr) {
    value_t *retval;
    value_t *left = NULL, *right = NULL;
    uint16_t left_val, right_val;
    uint16_t retval_val;

    if(!value)
        return NULL;

    if(!y_can_evaluate(value))
        return NULL;

    if(value->type == Y_TYPE_WORD || value->type == Y_TYPE_BYTE) {
        retval = (value_t*)error_malloc(sizeof(value_t));

        /* simple type, we can memcpy */
        memcpy(retval, value, sizeof(value_t));
        return retval;
    }

    if(value->type == Y_TYPE_LABEL) {
        /* dummy up the "*" label */
        if (strcmp(value->label, "*") == 0) {
            PDEBUG("Dummying * label to $%04x", addr);

            retval = (value_t*)error_malloc(sizeof(value_t));
            retval->type = Y_TYPE_WORD;
            retval->word = addr;
            return retval;
        }

        retval = y_lookup_symbol(value->label);
        if(y_can_evaluate(retval))
            return y_evaluate_val(retval, line, addr);
        return NULL;
    }

    if(value->type == Y_TYPE_ARITH) {
        if((!y_can_evaluate(value->left)) ||
           ((value->right) && (!y_can_evaluate(value->right))))
            return NULL;

        left = y_evaluate_val(value->left, line, addr);
        if(value->right)
            right = y_evaluate_val(value->right, line, addr);

        retval = (value_t*)error_malloc(sizeof(value_t));

        if((left && right) && ((left->type == Y_TYPE_WORD) || (right->type == Y_TYPE_WORD))) {
            value_promote(left);
            value_promote(right);
        }

        if(left->type == Y_TYPE_WORD)
            left_val = left->word;
        else
            left_val = left->byte;

        if(right) {
            if(right->type == Y_TYPE_WORD)
                right_val = right->word;
            else
                right_val = right->byte;
        }

        switch(value->word) {
        case BAND:
            retval_val = left_val & right_val;
            break;

        case BOR:
            retval_val = left_val | right_val;
            break;

        case BXOR:
            retval_val = left_val ^ right_val;
            break;

        case BNOT:
            retval_val = ~left_val;
            break;

        case HIB:
            retval_val = (left_val & 0xFF00) >> 8;
            break;

        case LOWB:
            retval_val = (left_val & 0x00FF);
            break;

        case BSL:
            retval_val = left_val << right_val;
            break;

        case BSR:
            retval_val = left_val >> right_val;
            break;

        case LT:
            retval_val = (left_val < right_val);
            break;

        case GT:
            retval_val = (left_val > right_val);
            break;

        case PLUS:
            /* should we be checking for overflow? */
            retval_val = left_val + right_val;
            break;

        case MINUS:
            retval_val = left_val - right_val;
            break;

        case MUL:
            retval_val = left_val * right_val;
            break;

        case MOD:
            retval_val = left_val % right_val;
            break;

        case DIV:
            retval_val = left_val / right_val;
            break;

        default:
            PERROR("Unexpected op: %d", value->word);
            exit(EXIT_FAILURE);
        }

        if(retval_val < 256) {
            retval->type = Y_TYPE_BYTE;
            retval->byte = retval_val;
        } else {
            retval->type = Y_TYPE_WORD;
            retval->word = retval_val;
        }

        y_value_free(left);
        y_value_free(right);

        return retval;
    }

    PERROR("Bad y-type in eval (%d)", value->type);
    exit(EXIT_FAILURE);
}



/*
 * y_add_wordsym
 */
void y_add_wordsym(char *label, uint16_t word) {
    value_t *value;

    PDEBUG("Adding WORD symbol '%s' as %04x", label, word);
    value = y_new_nval(Y_TYPE_WORD, word, NULL);
    y_add_symtable(label, value);
}

/*
 * y_add_bytesym
 */
void y_add_bytesym(char *label, uint8_t byte) {
    value_t *value;

    PDEBUG("Adding BYTE symbol '%s' as %02x", label, byte);
    value = y_new_nval(Y_TYPE_BYTE, byte, NULL);
    y_add_symtable(label, value);
}

/*
 * y_create_arith
 *
 * create an arithmetic parse tree to be evaluated later
 */
value_t *y_create_arith(value_t *left, value_t *right, int operator) {
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

    pvalue = (opdata_t *)error_malloc(sizeof(opdata_t));

    memset(pvalue,0,sizeof(opdata_t));
    pvalue->type = TYPE_DATA;
    pvalue->value = value;
    pvalue->len = 1;
    pvalue->offset = compiler_offset;

    pvalue->line = parser_line;
    pvalue->file = strdup(parser_file);

    compiler_offset += 1;
    add_opdata(pvalue);
}


/*
 * y_add_word
 */
void y_add_word(uint16_t word) {
    value_t *value;
    opdata_t *pvalue;

    value = y_new_nval(Y_TYPE_WORD, word, NULL);

    pvalue = (opdata_t *)error_malloc(sizeof(opdata_t));

    memset(pvalue,0,sizeof(opdata_t));
    pvalue->type = TYPE_DATA;
    pvalue->value = value;
    pvalue->len = 2;
    pvalue->offset = compiler_offset;

    pvalue->line = parser_line;
    pvalue->file = strdup(parser_file);

    compiler_offset += 2;
    add_opdata(pvalue);
}

void y_add_nval_typed(value_t *value, int type) {
    opdata_t *pvalue;

    pvalue = (opdata_t *)error_malloc(sizeof(opdata_t));

    memset(pvalue,0,sizeof(opdata_t));

    pvalue->type = TYPE_DATA;
    pvalue->value = value;
    pvalue->forced_type = type;

    switch(type) {
    case Y_TYPE_BYTE:
        pvalue->len = 1;
        break;
    case Y_TYPE_WORD:
        pvalue->len = 2;
        break;
    }

    pvalue->line = parser_line;
    pvalue->file = strdup(parser_file);

    pvalue->offset = compiler_offset;
    compiler_offset += pvalue->len;
    add_opdata(pvalue);
}

void y_add_nval(value_t *value) {
    switch(value->type) {
    case Y_TYPE_BYTE:
        y_add_nval_typed(value, Y_TYPE_BYTE);
        break;
    case Y_TYPE_WORD:
    case Y_TYPE_LABEL:   /* is this right?!?!? */
        y_add_nval_typed(value, Y_TYPE_WORD);
        break;
    default:
        PFATAL("don't know type of arith expression");
        exit(EXIT_FAILURE);
    }
}

/*
 * add a cpu offset adjustment (*=$VALUE)
 */
void y_add_offset(value_t *value) {
//    opdata_t *pvalue;
    value_t *concrete_value;

    if(!y_can_evaluate(value)) {
        YERROR("cannot resolve offset.");
        return;
    }

    concrete_value = y_evaluate_val(value, parser_line, compiler_offset);
    value_promote(concrete_value);

    PDEBUG("Adding new origin '%04x'", concrete_value->word);
    compiler_offset = concrete_value->word;
}


/*
 * add a symbol table entry
 */
void y_add_symtable(char *label, value_t *value) {
    symtable_t *pnew;
    char *nlabel;
    value_t *newvalue;

    nlabel = strdup(label);
    if(nlabel[strlen(nlabel) - 1] == ':') {
        nlabel[strlen(nlabel) - 1] = 0;
    }

    PDEBUG("Adding symbol '%s'", nlabel);

    if(y_lookup_symbol(nlabel)) {
        YERROR("duplicate symbol: %s", nlabel);
        return;
    }

    pnew = (symtable_t*)error_malloc(sizeof(symtable_t));

    pnew->label = nlabel;
    pnew->value = value;
    pnew->file = strdup(parser_file);
    pnew->line = parser_line;

    /* here's a question... should we allow symbols to be
       deferred? */
    newvalue = y_evaluate_val(value, parser_line, compiler_offset);
    if(newvalue) {
        pnew->value = newvalue;
    }


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
    opdata_t *pnew = (opdata_t *)error_malloc(sizeof(opdata_t));
    uint16_t opcode;

    /* Some addressing modes are unknown on the first
     * pass, specifically, we can't tell the difference
     * between absolute and zero page unless we know the size
     * of the operand.
     *
     * ... so we punt.  if we can't immediately resolve it, we
     * assume that it is a WORD size operand.  If if discover
     * that it could have been optimized down to a zero page
     * on pass 2, then we can emit a warning.
     *
     * probably would be nice to have hint syntax to suggest size
     * to force absolute when the value could be zp (to avoid
     * wraparound, for example) or to force zp when the
     * label is not yet known, but will be zp.
     */

    PDEBUG("adding opdata (%s)",
           cpu_opcode_mnemonics[opcode_family]);


    memset(pnew,0,sizeof(opdata_t));
    pnew->type = TYPE_INSTRUCTION;
    pnew->opcode_family = opcode_family;
    pnew->addressing_mode = addressing_mode;

    pnew->offset = compiler_offset;


    if((pvalue) && (y_can_evaluate(pvalue))) {
        /* let's do that! */
        pnew->value = y_evaluate_val(pvalue, parser_line, compiler_offset);
        y_value_free(pvalue);
    } else {
        /* :( */
        pnew->value = pvalue;
    }

    PDEBUG("operand value:");
    y_dump_value(pnew->value);

    switch(addressing_mode) {
    case CPU_ADDR_MODE_IMPLICIT:
        /* sometimes things are written as implicit
         * that are actually accumulator.  ASL vs ASL A,
         * for example. */
        if(opcode_lookup(pnew, 0) == OPCODE_NOTFOUND) {
            pnew->addressing_mode = CPU_ADDR_MODE_ACCUMULATOR;
        }
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
        /* we know for a fact these are bytes.  Don't care if
         * they are fully expressed right now, we'll express them
         * in pass 2 */
        pnew->len = 2;
        break;
    case CPU_ADDR_MODE_ABSOLUTE:
    case CPU_ADDR_MODE_ABSOLUTE_X:
    case CPU_ADDR_MODE_ABSOLUTE_Y:
    case CPU_ADDR_MODE_INDIRECT:
        /* again, we know these are words.  so we'll wait
         * to express them */
        pnew->len = 3;
        break;

    case CPU_ADDR_MODE_UNKNOWN:
    case CPU_ADDR_MODE_UNKNOWN_X:
    case CPU_ADDR_MODE_UNKNOWN_Y:
        /* These are either zero-page or absolute, depending on
         * the size of the operand. */
        if(!y_can_evaluate(pnew->value)) {
            /* we'll assume they are words, for absolute */
            pnew->addressing_mode -= CPU_ADDR_MODE_UNKNOWN;
            pnew->addressing_mode += CPU_ADDR_MODE_ABSOLUTE;
            pnew->len = 3;
        } else {
            if(y_value_is_byte(pnew->value)) {
                /* could be zero page, see if the opcode
                 * exists */
                pnew->addressing_mode -= CPU_ADDR_MODE_UNKNOWN;
                pnew->addressing_mode += CPU_ADDR_MODE_ZPAGE;
                pnew->len = 2;

                if(opcode_lookup(pnew, 0) == OPCODE_NOTFOUND) {
                    /* nope... let's set it back to absolute */
                    pnew->addressing_mode -= CPU_ADDR_MODE_ZPAGE;
                    pnew->addressing_mode += CPU_ADDR_MODE_ABSOLUTE;
                    pnew->len = 3;
                }
            } else {
                /* no byte operand, we'll call it a absolute */
                pnew->addressing_mode -= CPU_ADDR_MODE_UNKNOWN;
                pnew->addressing_mode += CPU_ADDR_MODE_ABSOLUTE;
                pnew->len = 3;
            }
        }
        break;

    default:
        YERROR("Bad addressing mode: %d", addressing_mode);
        return;

    }


    if(!pnew->len) {
        YERROR("unknown opcode length");
        return;
    }

    opcode = opcode_lookup(pnew, 1);
    if(opcode == OPCODE_NOTFOUND) {
        YERROR("cannot resolve opcode for %s as %s",
              cpu_opcode_mnemonics[pnew->opcode_family],
              cpu_addressing_mode[pnew->addressing_mode]);
        return;
    }

    pnew->opcode = opcode_lookup(pnew, 1);

    PDEBUG("looked up %s/%s as $%02x",
          cpu_opcode_mnemonics[pnew->opcode_family],
          cpu_addressing_mode[pnew->addressing_mode],
          pnew->opcode);

    if(pnew->value) {
        switch(pnew->value->type) {
        case Y_TYPE_BYTE:
            PDEBUG("operand BYTE: $%02x",
                  pnew->value->byte);
            break;
        case Y_TYPE_WORD:
            PDEBUG("operand WORD: $%04x",
                  pnew->value->word);
            break;
        case Y_TYPE_LABEL:
            PDEBUG("unresolved label %s",
                  pnew->value->label);
            break;
        case Y_TYPE_ARITH:
            PDEBUG("unresolvable arithmetic");
            break;
        }
    }

    pnew->line = parser_line;
    pnew->file = strdup(parser_file);

    pnew->offset = compiler_offset;
    compiler_offset += pnew->len;
    add_opdata(pnew);
}
