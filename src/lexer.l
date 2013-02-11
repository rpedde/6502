%{

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

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "rp65asm.h"
#include "debug.h"
#include "parser.h"
#include "opcodes.h"

#define MAX_INCLUDE_DEPTH 10

extern int yydebug;

char *parser_file;
int parser_line = 1;

typedef struct include_stack_t {
    char *file;
    int line_no;
    YY_BUFFER_STATE yy_buffer_state;
} include_stack_t;

int include_depth=0;

include_stack_t l_include_stack[MAX_INCLUDE_DEPTH];

int l_parse_error;
unsigned int tempint;

void l_include_file(char *filename);
void l_include_error(void);
void l_advance_line(void);
int l_eof(void);

uint16_t t161;
uint8_t t81;

%}

%option yylineno
%option case-insensitive
%option noyywrap

hex_byte $[a-f0-9][a-f0-9]
hex_word $[a-f0-9][a-f0-9][a-f0-9][a-f0-9]
binary_literal %[0-1]{1,16}
qstring \"[^\"\n]*[\"\n]


%x incl

%%

[\t ]+
\r
;.*\n                          { return(EOL); }
\n                             { return(EOL); }


(include|#include|.include)    BEGIN(incl);
<incl>[\t ]+
<incl>[^\t\n ]+                { l_include_file(yytext); }
<incl>\n                       { l_include_error(); }
<<EOF>>                        { if(l_eof()) yyterminate(); }


adc                            { yylval.opcode = CPU_OPCODE_ADC; return(TADC); }
and                            { yylval.opcode = CPU_OPCODE_AND; return(TAND); }
asl                            { yylval.opcode = CPU_OPCODE_ASL; return(TASL); }
bcc                            { yylval.opcode = CPU_OPCODE_BCC; return(TBCC); }
bcs                            { yylval.opcode = CPU_OPCODE_BCS; return(TBCS); }
beq                            { yylval.opcode = CPU_OPCODE_BEQ; return(TBEQ); }
bit                            { yylval.opcode = CPU_OPCODE_BIT; return(TBIT); }
bmi                            { yylval.opcode = CPU_OPCODE_BMI; return(TBMI); }
bne                            { yylval.opcode = CPU_OPCODE_BNE; return(TBNE); }
bpl                            { yylval.opcode = CPU_OPCODE_BPL; return(TBPL); }
brk                            { yylval.opcode = CPU_OPCODE_BRK; return(TBRK); }
bvc                            { yylval.opcode = CPU_OPCODE_BVC; return(TBVC); }
bvs                            { yylval.opcode = CPU_OPCODE_BVS; return(TBVS); }
clc                            { yylval.opcode = CPU_OPCODE_CLC; return(TCLC); }
cld                            { yylval.opcode = CPU_OPCODE_CLD; return(TCLD); }
cli                            { yylval.opcode = CPU_OPCODE_CLI; return(TCLI); }
clv                            { yylval.opcode = CPU_OPCODE_CLV; return(TCLV); }
cmp                            { yylval.opcode = CPU_OPCODE_CMP; return(TCMP); }
cpx                            { yylval.opcode = CPU_OPCODE_CPX; return(TCPX); }
cpy                            { yylval.opcode = CPU_OPCODE_CPY; return(TCPY); }
dec                            { yylval.opcode = CPU_OPCODE_DEC; return(TDEC); }
dex                            { yylval.opcode = CPU_OPCODE_DEX; return(TDEX); }
dey                            { yylval.opcode = CPU_OPCODE_DEY; return(TDEY); }
eor                            { yylval.opcode = CPU_OPCODE_EOR; return(TEOR); }
inc                            { yylval.opcode = CPU_OPCODE_INC; return(TINC); }
inx                            { yylval.opcode = CPU_OPCODE_INX; return(TINX); }
iny                            { yylval.opcode = CPU_OPCODE_INY; return(TINY); }
jmp                            { yylval.opcode = CPU_OPCODE_JMP; return(TJMP); }
jsr                            { yylval.opcode = CPU_OPCODE_JSR; return(TJSR); }
lda                            { yylval.opcode = CPU_OPCODE_LDA; return(TLDA); }
ldx                            { yylval.opcode = CPU_OPCODE_LDX; return(TLDX); }
ldy                            { yylval.opcode = CPU_OPCODE_LDY; return(TLDY); }
lsr                            { yylval.opcode = CPU_OPCODE_LSR; return(TLSR); }
nop                            { yylval.opcode = CPU_OPCODE_NOP; return(TNOP); }
ora                            { yylval.opcode = CPU_OPCODE_ORA; return(TORA); }
pha                            { yylval.opcode = CPU_OPCODE_PHA; return(TPHA); }
php                            { yylval.opcode = CPU_OPCODE_PHP; return(TPHP); }
pla                            { yylval.opcode = CPU_OPCODE_PLA; return(TPLA); }
plp                            { yylval.opcode = CPU_OPCODE_PLP; return(TPLP); }
rol                            { yylval.opcode = CPU_OPCODE_ROL; return(TROL); }
ror                            { yylval.opcode = CPU_OPCODE_ROR; return(TROR); }
rti                            { yylval.opcode = CPU_OPCODE_RTI; return(TRTI); }
rts                            { yylval.opcode = CPU_OPCODE_RTS; return(TRTS); }
sbc                            { yylval.opcode = CPU_OPCODE_SBC; return(TSBC); }
sec                            { yylval.opcode = CPU_OPCODE_SEC; return(TSEC); }
sed                            { yylval.opcode = CPU_OPCODE_SED; return(TSED); }
sei                            { yylval.opcode = CPU_OPCODE_SEI; return(TSEI); }
sta                            { yylval.opcode = CPU_OPCODE_STA; return(TSTA); }
stx                            { yylval.opcode = CPU_OPCODE_STX; return(TSTX); }
sty                            { yylval.opcode = CPU_OPCODE_STY; return(TSTY); }
tax                            { yylval.opcode = CPU_OPCODE_TAX; return(TTAX); }
tay                            { yylval.opcode = CPU_OPCODE_TAY; return(TTAY); }
tsx                            { yylval.opcode = CPU_OPCODE_TSX; return(TTSX); }
txa                            { yylval.opcode = CPU_OPCODE_TXA; return(TTXA); }
txs                            { yylval.opcode = CPU_OPCODE_TXS; return(TTXS); }
tya                            { yylval.opcode = CPU_OPCODE_TYA; return(TTYA); }

x                              { return(XREG); }
y                              { return(YREG); }
a                              { return(AREG); }

&&                             { yylval.arithop = AND; return(AND); }
\|\|                           { yylval.arithop = OR; return(OR); }

&                              { yylval.arithop = BAND; return(BAND); }
\|                             { yylval.arithop = BOR; return(BOR); }
\^                             { yylval.arithop = BXOR; return(BXOR); }
~                              { yylval.arithop = BNOT; return(BNOT); }
\<<                            { yylval.arithop = BSL; return(BSL); }
>>                             { yylval.arithop = BSR; return(BSR); }

\<                             { yylval.arithop = LOWB; return(LOWB); }
>                              { yylval.arithop = HIB; return(HIB); }

\+                             { yylval.arithop = PLUS; return(PLUS); }
\-                             { yylval.arithop = MINUS; return(MINUS); }
\*                             { yylval.arithop = MUL; return(MUL); }
\/                             { yylval.arithop = DIV; return(DIV); }
%                              { yylval.arithop = MOD; return(MOD); }



equ                            { return(EQ); }
(org|\.org)                    { return(ORG); }
(\.db|db|\.byte)               { return(BYTELITERAL); }
(\.dw|dw|\.word)               { return(WORDLITERAL); }

{hex_byte}                     { sscanf(&yytext[1],"%02x",&tempint); yylval.byte = tempint; return(BYTE); }
{hex_word}                     { sscanf(&yytext[1],"%04x",&tempint); yylval.word = tempint; return(WORD); }

{binary_literal}               { if((t161 = strtol(&yytext[1], NULL, 2)) < 256) { yylval.byte = t161; return(BYTE);
                                                                                } else { yylval.word = t161; return(WORD); }}

[0-9]+                         { if(atoi(yytext) < 256) { yylval.byte = atoi(yytext); return(BYTE);
                                                        } else { yylval.word = atoi(yytext); return (WORD); }}

\'.\'                          { yylval.byte = yytext[1]; return(BYTE); }
\".\"                          { yylval.byte = yytext[1]; return(BYTE); }

{qstring}                      { yylval.string = (char*)&(strdup(yytext)[1]);
                                 yylval.string[strlen(yylval.string) - 1] = '\0';
                                 return(STRING); }
[A-Za-z0-9_]+:?                { yylval.string = strdup(yytext); return(LABEL); }

.                              { return yytext[0]; }

%%

void yyerror(char *format, ...) {
    va_list args;

    fprintf(stderr, "%s:%d: error: ", parser_file, parser_line);

    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);

    fprintf(stderr, "\n");
    l_parse_error = 1;
}


/**
 * include a file and switch back out of incl-mode
 *
 * @param filename file to include
 */
void l_include_file(char *filename) {
    char *startptr = filename;
    char *fname;

    if(include_depth >= MAX_INCLUDE_DEPTH) {
        PFATAL("Includes nested too deeply");
        exit(EXIT_FAILURE);
    }

    if(*startptr == '\"')
        startptr++;

    fname = strdup(startptr);
    if(fname[strlen(fname) - 1] == '\"')
        fname[strlen(fname) - 1] = 0;

    PDEBUG("including %s", fname);

    l_include_stack[include_depth].yy_buffer_state = YY_CURRENT_BUFFER;

    yyin = fopen(fname, "r" );

    if ( ! yyin ) {
        PFATAL("cannot open file '%s' for include", fname);
        exit(EXIT_FAILURE);
    }

    include_depth++;
    l_include_stack[include_depth].line_no = 1;
    l_include_stack[include_depth].file = fname;

    parser_line = l_include_stack[include_depth].line_no;
    parser_file = l_include_stack[include_depth].file;

    yy_switch_to_buffer(yy_create_buffer(yyin, YY_BUF_SIZE));

    BEGIN(INITIAL);
}

void l_include_error(void) {
     PFATAL("missing filename for include\n");
     exit(EXIT_FAILURE);
}


void l_advance_line(void) {
    l_include_stack[include_depth].line_no++;
    parser_line = l_include_stack[include_depth].line_no;
    PDEBUG("Advanced line to %d", parser_line);
}

/**
 * kick off parsing the top level file
 *
 * @param file top-level file to parse
 */
int l_parse_file(char *file) {
    int result;
    FILE *fin;
    value_t star_symbol = { Y_TYPE_WORD, 0, 0, "*", NULL, NULL };

    fin = fopen(file, "r");
    if(!fin) {
        PFATAL("cannot open file %s: %s",
              file, strerror(errno));
        exit(EXIT_FAILURE);
    }

    l_include_stack[include_depth].file = file;
    l_include_stack[include_depth].line_no = 1;

    parser_line = l_include_stack[include_depth].line_no;
    parser_file = l_include_stack[include_depth].file;

    /* add a dummy symtable entry */
    y_add_symtable("*", &star_symbol);

    yyin = fin;
    result = yyparse(NULL);
    fclose(fin);

    return (!l_parse_error);
}

int l_eof(void) {
    if(include_depth == 0) {
        PDEBUG("Last EOF... terminating");
        return 1;
    } else {
        free(l_include_stack[include_depth].file);
        yy_delete_buffer(YY_CURRENT_BUFFER);

        include_depth--;
        parser_line = l_include_stack[include_depth].line_no;
        parser_file = l_include_stack[include_depth].file;
        yy_switch_to_buffer(l_include_stack[include_depth].yy_buffer_state);
        PDEBUG("resume parsing");
    }

    return 0;
}

/* Local Variables:  */
/* mode: fundamental */
/* End:              */
