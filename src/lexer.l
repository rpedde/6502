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


#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "compiler.h"
#include "debug.h"
#include "parser.h"
#include "opcodes.h"

extern int yydebug;
extern int parse_failure;
unsigned int tempint;

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

%%

[\t ]+
\r
;.*\n                          { return(EOL); }
\n                             { return(EOL); }

adc                            { yylval.opcode = CPU_OPCODE_ADC; return(OP); }
anc                            { yylval.opcode = CPU_OPCODE_ANC; return(OP); }
and                            { yylval.opcode = CPU_OPCODE_AND; return(OP); }
arr                            { yylval.opcode = CPU_OPCODE_ARR; return(OP); }
asl                            { yylval.opcode = CPU_OPCODE_ASL; return(OP); }
asr                            { yylval.opcode = CPU_OPCODE_ASR; return(OP); }
asx                            { yylval.opcode = CPU_OPCODE_ASX; return(OP); }
ax7                            { yylval.opcode = CPU_OPCODE_AX7; return(OP); }
axe                            { yylval.opcode = CPU_OPCODE_AXE; return(OP); }
bcc                            { yylval.opcode = CPU_OPCODE_BCC; return(RELOP); }
bcs                            { yylval.opcode = CPU_OPCODE_BCS; return(RELOP); }
beq                            { yylval.opcode = CPU_OPCODE_BEQ; return(RELOP); }
bit                            { yylval.opcode = CPU_OPCODE_BIT; return(OP); }
bmi                            { yylval.opcode = CPU_OPCODE_BMI; return(RELOP); }
bne                            { yylval.opcode = CPU_OPCODE_BNE; return(RELOP); }
bpl                            { yylval.opcode = CPU_OPCODE_BPL; return(RELOP); }
brk                            { yylval.opcode = CPU_OPCODE_BRK; return(OP); }
bvc                            { yylval.opcode = CPU_OPCODE_BVC; return(RELOP); }
bvs                            { yylval.opcode = CPU_OPCODE_BVS; return(RELOP); }
clc                            { yylval.opcode = CPU_OPCODE_CLC; return(OP); }
cld                            { yylval.opcode = CPU_OPCODE_CLD; return(OP); }
cli                            { yylval.opcode = CPU_OPCODE_CLI; return(OP); }
clv                            { yylval.opcode = CPU_OPCODE_CLV; return(OP); }
cmp                            { yylval.opcode = CPU_OPCODE_CMP; return(OP); }
cpx                            { yylval.opcode = CPU_OPCODE_CPX; return(OP); }
cpy                            { yylval.opcode = CPU_OPCODE_CPY; return(OP); }
dcp                            { yylval.opcode = CPU_OPCODE_DCP; return(OP); }
dec                            { yylval.opcode = CPU_OPCODE_DEC; return(OP); }
dex                            { yylval.opcode = CPU_OPCODE_DEX; return(OP); }
dey                            { yylval.opcode = CPU_OPCODE_DEY; return(OP); }
eor                            { yylval.opcode = CPU_OPCODE_EOR; return(OP); }
inc                            { yylval.opcode = CPU_OPCODE_INC; return(OP); }
inx                            { yylval.opcode = CPU_OPCODE_INX; return(OP); }
iny                            { yylval.opcode = CPU_OPCODE_INY; return(OP); }
isb                            { yylval.opcode = CPU_OPCODE_ISB; return(OP); }
jam                            { yylval.opcode = CPU_OPCODE_JAM; return(OP); }
jmp                            { yylval.opcode = CPU_OPCODE_JMP; return(OP); }
jsr                            { yylval.opcode = CPU_OPCODE_JSR; return(OP); }
las                            { yylval.opcode = CPU_OPCODE_LAS; return(OP); }
lax                            { yylval.opcode = CPU_OPCODE_LAX; return(OP); }
lda                            { yylval.opcode = CPU_OPCODE_LDA; return(OP); }
ldx                            { yylval.opcode = CPU_OPCODE_LDX; return(OP); }
ldy                            { yylval.opcode = CPU_OPCODE_LDY; return(OP); }
lsr                            { yylval.opcode = CPU_OPCODE_LSR; return(OP); }
nop                            { yylval.opcode = CPU_OPCODE_NOP; return(OP); }
ora                            { yylval.opcode = CPU_OPCODE_ORA; return(OP); }
pha                            { yylval.opcode = CPU_OPCODE_PHA; return(OP); }
php                            { yylval.opcode = CPU_OPCODE_PHP; return(OP); }
pla                            { yylval.opcode = CPU_OPCODE_PLA; return(OP); }
plp                            { yylval.opcode = CPU_OPCODE_PLP; return(OP); }
rla                            { yylval.opcode = CPU_OPCODE_RLA; return(OP); }
rol                            { yylval.opcode = CPU_OPCODE_ROL; return(OP); }
ror                            { yylval.opcode = CPU_OPCODE_ROR; return(OP); }
rra                            { yylval.opcode = CPU_OPCODE_RRA; return(OP); }
rti                            { yylval.opcode = CPU_OPCODE_RTI; return(OP); }
rts                            { yylval.opcode = CPU_OPCODE_RTS; return(OP); }
sax                            { yylval.opcode = CPU_OPCODE_SAX; return(OP); }
sbc                            { yylval.opcode = CPU_OPCODE_SBC; return(OP); }
sec                            { yylval.opcode = CPU_OPCODE_SEC; return(OP); }
sed                            { yylval.opcode = CPU_OPCODE_SED; return(OP); }
sei                            { yylval.opcode = CPU_OPCODE_SEI; return(OP); }
slo                            { yylval.opcode = CPU_OPCODE_SLO; return(OP); }
sre                            { yylval.opcode = CPU_OPCODE_SRE; return(OP); }
sta                            { yylval.opcode = CPU_OPCODE_STA; return(OP); }
stx                            { yylval.opcode = CPU_OPCODE_STX; return(OP); }
sty                            { yylval.opcode = CPU_OPCODE_STY; return(OP); }
sx7                            { yylval.opcode = CPU_OPCODE_SX7; return(OP); }
sy7                            { yylval.opcode = CPU_OPCODE_SY7; return(OP); }
tax                            { yylval.opcode = CPU_OPCODE_TAX; return(OP); }
tay                            { yylval.opcode = CPU_OPCODE_TAY; return(OP); }
tsx                            { yylval.opcode = CPU_OPCODE_TSX; return(OP); }
txa                            { yylval.opcode = CPU_OPCODE_TXA; return(OP); }
txs                            { yylval.opcode = CPU_OPCODE_TXS; return(OP); }
tya                            { yylval.opcode = CPU_OPCODE_TYA; return(OP); }
xea                            { yylval.opcode = CPU_OPCODE_XEA; return(OP); }
xs7                            { yylval.opcode = CPU_OPCODE_XS7; return(OP); }

x                              { return(XREG); }
y                              { return(YREG); }
a                              { return(AREG); }

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
{qstring}                      { yylval.string = (char*)&(strdup(yytext)[1]);
                                 yylval.string[strlen(yylval.string) - 1] = '\0';
                                 return(STRING); }
[A-Za-z0-9_]+:?                { yylval.string = strdup(yytext); return(LABEL); }

.                              { return yytext[0]; }

%%

void yyerror(char *msg) {
    parse_failure = 1;
    DPRINTF(DBG_ERROR,"Parser error, line %d: %s\n",yylineno, msg);
}

/* Local Variables:  */
/* mode: fundamental */
/* End:              */
