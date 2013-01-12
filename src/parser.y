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

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define _INCLUDE_OPCODE_MAP
#include "opcodes.h"

#include "compiler.h"
#include "debug.h"

#define YYERROR_VERBOSE 1

extern void yyerror(char *msg);
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
void y_add_string(char *string);
value_t *y_create_arith(value_t *left, value_t *right, int operator);
value_t *y_evaluate_val(value_t *value, int line, uint16_t addr);
void *error_malloc(ssize_t size);
void y_value_free(value_t *value);

%}

%union {
    uint8_t byte;
    uint16_t word;
    char *string;
    uint8_t opcode;
    value_t *nval;
    int arithop;
};

%token <opcode> OP
%token <opcode> RELOP
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
%type <nval> wordlvalue
%type <nval> bytelvalue
%type <arithop> math_operator
%type <arithop> bool_operator
%type <arithop> binary_operator
%type <arithop> unary_operator


%left BAND BXOR BOR

%left AND OR
%left LT GT

%left PLUS MINUS
%left MUL DIV MOD

%left BNOT

%nonassoc binary_op
%nonassoc bool_op
%nonassoc math_op
%nonassoc unary_op

/* %glr-parser */

%%

program: line {}
| program line {}
;

line: OP EOL { y_add_opdata($1, CPU_ADDR_MODE_IMPLICIT, NULL); parser_line++; }
| OP AREG EOL { y_add_opdata($1, CPU_ADDR_MODE_ACCUMULATOR, NULL); parser_line++; }
| OP '#' expression EOL { y_add_opdata($1, CPU_ADDR_MODE_IMMEDIATE, $3); parser_line++; } // Must be BYTE
| RELOP expression EOL { y_add_opdata($1, CPU_ADDR_MODE_RELATIVE, $2); parser_line++;} // Must be BYTE
| OP expression EOL { y_add_opdata($1, CPU_ADDR_MODE_UNKNOWN, $2); parser_line++; } // OR REL!
| OP expression ',' XREG EOL { y_add_opdata($1, CPU_ADDR_MODE_UNKNOWN_X, $2); parser_line++; } // should be word, optimize at compile
| OP expression ',' YREG EOL { y_add_opdata($1, CPU_ADDR_MODE_UNKNOWN_Y, $2); parser_line++; } // should be word, optimize at compile
| OP '[' expression ']' EOL { y_add_opdata($1, CPU_ADDR_MODE_INDIRECT, $3); parser_line++; } // Must be WORD
| OP '[' expression ',' XREG ']' EOL { y_add_opdata($1, CPU_ADDR_MODE_IND_X, $3); parser_line++; } // Must be BYTE
| OP '[' expression ']' ',' YREG EOL { y_add_opdata($1, CPU_ADDR_MODE_IND_Y, $3); parser_line++; } // Must be BYTE
| LABEL EQ expression EOL { y_add_symtable($1, $3); parser_line++; }
| LABEL '=' expression EOL { y_add_symtable($1, $3); parser_line++; }
| LABEL { y_add_wordsym($1, compiler_offset); } /* an address label */
| EOL { parser_line++; }
| MUL '=' expression EOL { y_add_offset($3); parser_line++; }
| ORG expression EOL { y_add_offset($2); parser_line++; }
| BYTELITERAL bytestring EOL { parser_line++; }
| WORDLITERAL wordstring EOL { parser_line++; }
;


expression: WORD { $$ = y_new_nval(Y_TYPE_WORD, $1, NULL); }
| BYTE { $$ = y_new_nval(Y_TYPE_BYTE, $1, NULL); }
| LABEL { $$ = y_new_nval(Y_TYPE_LABEL, 0, $1); }
| MUL { $$ = y_new_nval(Y_TYPE_LABEL, 0, "*"); }
| unary_operator expression %prec unary_op { $$ = y_create_arith($2, NULL, $1); }
| expression math_operator expression %prec math_op { $$ = y_create_arith($1, $3, $2); }
| expression bool_operator expression %prec bool_op { $$ = y_create_arith($1, $3, $2); }
| expression binary_operator expression %prec binary_op { $$ = y_create_arith($1, $3, $2); }
| '(' expression ')' { $$ = $2; }
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
;


wordlvalue: WORD { $$ = y_new_nval(Y_TYPE_WORD, $1, NULL); }
| LABEL { $$ = y_new_nval(Y_TYPE_LABEL, 0, $1); }
| '*' { $$ = y_new_nval(Y_TYPE_LABEL, 0, "*"); }
;

/* We'll typecheck these on pass 2 */
bytelvalue: BYTE { $$ = y_new_nval(Y_TYPE_BYTE, $1, NULL); }
;

bytestring: bytelvalue { y_add_nval($1); }
| STRING { y_add_string($1); }
| bytestring ',' bytelvalue { y_add_nval($3); }
;

wordstring: wordlvalue { y_add_nval($1); }
| wordstring ',' wordlvalue { y_add_nval($3); }
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
        DPRINTF(DBG_ERROR, "Bad type\n");
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
        DPRINTF(DBG_FATAL, "Malloc: %s", strerror(errno));
        exit(EXIT_FAILURE);
    }

    return retval;
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
            if((value->right) && (y_can_evaluate(value->right))) {
                return 1;
            }
        }
        return 0;
    }

    DPRINTF(DBG_FATAL, "Cannot evaluate a value of type %d\n", value->type);
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
            DPRINTF(DBG_FATAL, "Can't evaluate something I should have!\n");
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
    DPRINTF(DBG_FATAL, "Cannot evaluate a value of type %d\n", value->type);
    exit(EXIT_FAILURE);
}


/**
 * value_promote
 *
 * in-place promote a byte to a word
 *
 * @param value item to promote
 * @param line copilter line number for error message (unused, for symmetry)
 */
void value_promote(value_t *value, int line) {
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
 * @param line compiler line number for error message
 */
void value_demote(value_t *value, int line) {
    if(value->type == Y_TYPE_WORD) {
        if(value->word > 255) {
            DPRINTF(DBG_FATAL, "Line %d: Cannot demote word to byte",
                    line, value->word);
        }
        value->byte = value->word;
        value->type = Y_TYPE_BYTE;
    }
}

void y_value_free(value_t *value) {
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
        DPRINTF(DBG_FATAL, "Unexpected y-value in y_value_free: %d\n",
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
    value_t *left, *right;
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
            DPRINTF(DBG_DEBUG, "Dummying * label to $%04x\n", addr);

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
            value_promote(left, line);
            value_promote(right, line);
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
            retval_val = left_val ^ right_val;
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
            DPRINTF(DBG_ERROR, "Unexpected op: %d\n", value->word);
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

    DPRINTF(DBG_ERROR,"Bad y-type in eval (%d)\n", value->type);
    exit(EXIT_FAILURE);
}



/*
 * y_add_wordsym
 */
void y_add_wordsym(char *label, uint16_t word) {
    value_t *value;

    DPRINTF(DBG_DEBUG, "Adding WORD symbol '%s' as %04x\n", label, word);
    value = y_new_nval(Y_TYPE_WORD, word, NULL);
    y_add_symtable(label, value);
}

/*
 * y_add_bytesym
 */
void y_add_bytesym(char *label, uint8_t byte) {
    value_t *value;

    DPRINTF(DBG_DEBUG, "Adding BYTE symbol '%s' as %02x\n", label, byte);
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

    pvalue = (opdata_t *)malloc(sizeof(opdata_t));
    if(!pvalue) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    memset(pvalue,0,sizeof(opdata_t));
    pvalue->type = TYPE_DATA;
    pvalue->value = value;
    pvalue->len = 1;

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

    pvalue = (opdata_t *)malloc(sizeof(opdata_t));
    if(!pvalue) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    memset(pvalue,0,sizeof(opdata_t));
    pvalue->type = TYPE_DATA;
    pvalue->value = value;
    pvalue->len = 2;

    compiler_offset += 2;
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

    compiler_offset += pvalue->len;
    add_opdata(pvalue);
}

/*
 * add a cpu offset adjustment (*=$VALUE)
 */
void y_add_offset(value_t *value) {
//    opdata_t *pvalue;
    value_t *concrete_value;

    if(!y_can_evaluate(value)) {
        DPRINTF(DBG_FATAL, "Line %d: cannot resolve offset.\n", parser_line);
        exit(EXIT_FAILURE);
    }

    concrete_value = y_evaluate_val(value, parser_line, compiler_offset);
    value_promote(concrete_value, parser_line);

    DPRINTF(DBG_DEBUG, "Adding new origin '%04x'\n", concrete_value->word);
    compiler_offset = concrete_value->word;

    /* pvalue = (opdata_t *)malloc(sizeof(opdata_t)); */
    /* if(!pvalue) { */
    /*     perror("malloc"); */
    /*     exit(EXIT_FAILURE); */
    /* } */

    /* memset(pvalue,0,sizeof(opdata_t)); */
    /* pvalue->type = TYPE_OFFSET; */
    /* pvalue->value = concrete_value; */

    /* compiler_offset = concrete_value->word; */
    /* add_opdata(pvalue); */
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

    DPRINTF(DBG_DEBUG, "Adding symbol '%s'\n", nlabel);

    if(y_lookup_symbol(nlabel)) {
        DPRINTF(DBG_ERROR, "Error:  duplicate symbol: %s\n", nlabel);
        exit(EXIT_FAILURE);
    }

    pnew = (symtable_t*)malloc(sizeof(symtable_t));
    if(!pnew) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    pnew->label = nlabel;
    pnew->value = value;

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

    DPRINTF(DBG_DEBUG, "line %d ($%04x): adding opdata\n",
            parser_line, compiler_offset);


    memset(pnew,0,sizeof(opdata_t));
    pnew->type = TYPE_INSTRUCTION;
    pnew->opcode_family = opcode_family;
    pnew->addressing_mode = addressing_mode;

    pnew->offset = compiler_offset;

    if(y_can_evaluate(pvalue)) {
        /* let's do that! */
        pnew->value = y_evaluate_val(pvalue, parser_line, compiler_offset);
        y_value_free(pvalue);
    } else {
        /* :( */
        pnew->value = pvalue;
    }

    switch(addressing_mode) {
    case CPU_ADDR_MODE_IMPLICIT:
        /* sometimes things are written as implicit
         * that are actually accumulator.  ASL vs ASL A,
         * for example. */
        if(opcode_lookup(pnew, parser_line, 0) == OPCODE_NOTFOUND) {
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

                if(opcode_lookup(pnew, parser_line, 0) == OPCODE_NOTFOUND) {
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
        DPRINTF(DBG_FATAL,"Bad addressing mode: %d\n", addressing_mode);
        break;
    }


    if(!pnew->len) {
        DPRINTF(DBG_FATAL, "line %d: unknown opcode length\n", parser_line);
        exit(EXIT_FAILURE);
    }

    opcode = opcode_lookup(pnew, parser_line, 1);
    if(opcode == OPCODE_NOTFOUND) {
        DPRINTF(DBG_FATAL, "Line %d: cannot resolve opcode for %s as %s\n",
                cpu_opcode_mnemonics[pnew->opcode_family],
                cpu_addressing_mode[pnew->addressing_mode]);
        exit(EXIT_FAILURE);
    }

    pnew->opcode = opcode_lookup(pnew, parser_line, 1);

    DPRINTF(DBG_DEBUG, "looked up %s/%s as $%02x\n",
            cpu_opcode_mnemonics[pnew->opcode_family],
            cpu_addressing_mode[pnew->addressing_mode],
            pnew->opcode);

    switch(pnew->value->type) {
    case Y_TYPE_BYTE:
        DPRINTF(DBG_DEBUG, "operand BYTE: $%02x\n",
                pnew->value->byte);
        break;
    case Y_TYPE_WORD:
        DPRINTF(DBG_DEBUG, "operand WORD: $%04x\n",
                pnew->value->word);
        break;
    case Y_TYPE_LABEL:
        DPRINTF(DBG_DEBUG, "unresolved label %s\n",
                pnew->value->label);
        break;
    case Y_TYPE_ARITH:
        DPRINTF(DBG_DEBUG, "unresolvable arithmetic\n");
        break;
    }

    pnew->line = parser_line;
    pnew->offset = compiler_offset;
    compiler_offset += pnew->len;
    add_opdata(pnew);
}
