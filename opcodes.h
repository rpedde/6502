/*
 *
 */

#ifndef _OPCODES_H_
#define _OPCODES_H_

#ifndef TRUE
# define TRUE 1
# define FALSE 0
#endif

#define CPU_OPCODE_ADC 0x00
#define CPU_OPCODE_ANC 0x01 /* undoc */
#define CPU_OPCODE_AND 0x02
#define CPU_OPCODE_ARR 0x03 /* undoc */
#define CPU_OPCODE_ASL 0x04
#define CPU_OPCODE_ASR 0x05 /* undoc */
#define CPU_OPCODE_ASX 0x06 /* undoc */
#define CPU_OPCODE_AX7 0x07 /* undoc */
#define CPU_OPCODE_AXE 0x08 /* undoc */
#define CPU_OPCODE_BCC 0x09
#define CPU_OPCODE_BCS 0x0a
#define CPU_OPCODE_BEQ 0x0b
#define CPU_OPCODE_BIT 0x0c
#define CPU_OPCODE_BMI 0x0d
#define CPU_OPCODE_BNE 0x0e
#define CPU_OPCODE_BPL 0x0f
#define CPU_OPCODE_BRK 0x10
#define CPU_OPCODE_BVC 0x11
#define CPU_OPCODE_BVS 0x12
#define CPU_OPCODE_CLC 0x13
#define CPU_OPCODE_CLD 0x14
#define CPU_OPCODE_CLI 0x15
#define CPU_OPCODE_CLV 0x16
#define CPU_OPCODE_CMP 0x17
#define CPU_OPCODE_CPX 0x18
#define CPU_OPCODE_CPY 0x19
#define CPU_OPCODE_DCP 0x1a /* undoc */
#define CPU_OPCODE_DEC 0x1b
#define CPU_OPCODE_DEX 0x1c
#define CPU_OPCODE_DEY 0x1d
#define CPU_OPCODE_EOR 0x1e
#define CPU_OPCODE_INC 0x1f
#define CPU_OPCODE_INX 0x20
#define CPU_OPCODE_INY 0x21
#define CPU_OPCODE_ISB 0x22 /* undoc */
#define CPU_OPCODE_JAM 0x23 /* undoc, hang */
#define CPU_OPCODE_JMP 0x24
#define CPU_OPCODE_JSR 0x25
#define CPU_OPCODE_LAS 0x26 /* undoc */
#define CPU_OPCODE_LAX 0x27 /* undoc */
#define CPU_OPCODE_LDA 0x28
#define CPU_OPCODE_LDX 0x29
#define CPU_OPCODE_LDY 0x2a
#define CPU_OPCODE_LSR 0x2b
#define CPU_OPCODE_NOP 0x2c
#define CPU_OPCODE_ORA 0x2d
#define CPU_OPCODE_PHA 0x2e
#define CPU_OPCODE_PHP 0x2f
#define CPU_OPCODE_PLA 0x30 /* undoc */
#define CPU_OPCODE_PLP 0x31
#define CPU_OPCODE_RLA 0x32
#define CPU_OPCODE_ROL 0x33
#define CPU_OPCODE_ROR 0x34
#define CPU_OPCODE_RRA 0x35 /* undoc */
#define CPU_OPCODE_RTI 0x36
#define CPU_OPCODE_RTS 0x37
#define CPU_OPCODE_SAX 0x38 /* undoc */
#define CPU_OPCODE_SBC 0x39
#define CPU_OPCODE_SEC 0x3a
#define CPU_OPCODE_SED 0x3b
#define CPU_OPCODE_SEI 0x3c
#define CPU_OPCODE_SLO 0x3d /* undoc */
#define CPU_OPCODE_SRE 0x3e /* undoc */
#define CPU_OPCODE_STA 0x3f
#define CPU_OPCODE_STX 0x40
#define CPU_OPCODE_STY 0x41
#define CPU_OPCODE_SX7 0x42 /* undoc */
#define CPU_OPCODE_SY7 0x43
#define CPU_OPCODE_TAX 0x44
#define CPU_OPCODE_TAY 0x45
#define CPU_OPCODE_TSX 0x46
#define CPU_OPCODE_TXA 0x47
#define CPU_OPCODE_TXS 0x48
#define CPU_OPCODE_TYA 0x49
#define CPU_OPCODE_XEA 0x4a /* undoc */
#define CPU_OPCODE_XS7 0x4b /* undoc */

#define CPU_ADDR_MODE_IMPLICIT    0x00
#define CPU_ADDR_MODE_ACCUMULATOR 0x01
#define CPU_ADDR_MODE_IMMEDIATE   0x02
#define CPU_ADDR_MODE_ZPAGE       0x03
#define CPU_ADDR_MODE_ZPAGE_X     0x04
#define CPU_ADDR_MODE_ZPAGE_Y     0x05
#define CPU_ADDR_MODE_RELATIVE    0x06
#define CPU_ADDR_MODE_ABSOLUTE    0x07
#define CPU_ADDR_MODE_ABSOLUTE_X  0x08
#define CPU_ADDR_MODE_ABSOLUTE_Y  0x09
#define CPU_ADDR_MODE_INDIRECT    0x0a
#define CPU_ADDR_MODE_IND_X       0x0b
#define CPU_ADDR_MODE_IND_Y       0x0c

typedef struct opcode_t_struct {
    uint8_t opcode_family;
    uint8_t opcode_undocumented;
    uint8_t addressing_mode;
    uint8_t cycles;
    uint8_t page_overflow;
} opcode_t;

extern char *cpu_addressing_mode[];
extern char *cpu_opcode_mnemonics[];
extern opcode_t cpu_opcode_map[];

#ifdef _INCLUDE_OPCODE_MAP
char *cpu_addressing_mode[] = {
    "Implicit", "Accumulator", "Immediate",
    "Zero-Page", "Zero-Page X", "Zero-Page Y", "Relative",
    "Absolute", "Absolute X", "Absolute Y", "Indirect",
    "Indirect X", "Indirect Y"
};

typedef struct opcode_info_t_struct {
    char *mnemonic;
    int loads;
    int stores;
    int legal;
} opcode_info_t;

opcode_info_t cpu_opcode_info[] = {
    /*       L  S  OK */
    { "adc", 1, 0, 1 },
    { "anc", 0, 0, 0 }, /* undoc */
    { "and", 1, 0, 1 },
    { "arr", 0, 0, 0 }, /* undoc */
    { "asl", 0, 0, 1 },
    { "asr", 0, 0, 0 }, /* undoc */
    { "asx", 0, 0, 0 }, /* undoc */
    { "ax7", 0, 0, 0 }, /* undoc */
    { "axe", 0, 0, 0 }, /* undoc */
    { "bcc", 0, 0, 1 },
    { "bcs", 0, 0, 1 },
    { "beq", 0, 0, 1 },
    { "bit", 0, 0, 1 },
    { "bmi", 0, 0, 1 },
    { "bne", 0, 0, 1 },
    { "bpl", 0, 0, 1 },
    { "brk", 0, 0, 1 },
    { "bvc", 0, 0, 1 },
    { "bvs", 0, 0, 1 },
    { "clc", 0, 0, 1 },
    { "cld", 0, 0, 1 },
    { "cli", 0, 0, 1 },
    { "clv", 0, 0, 1 },
    { "cmp", 1, 0, 1 },
    { "cpx", 1, 0, 1 },
    { "cpy", 1, 0, 1 },
    { "dcp", 0, 0, 0 },
    { "dec", 1, 1, 1 },
    { "dex", 0, 0, 1 },
    { "dey", 0, 0, 1 },
    { "eor", 0, 0, 1 },
    { "inc", 1, 1, 1 },
    { "inx", 0, 0, 1 },
    { "iny", 0, 0, 1 },
    { "isb", 0, 0, 0 },
    { "jam", 0, 0, 0 },
    { "jmp", 0, 0, 1 },
    { "jsr", 0, 0, 1 },
    { "las", 0, 0, 0 },
    { "lax", 0, 0, 0 },
    { "lda", 1, 0, 1 },
    { "ldx", 1, 0, 1 },
    { "ldy", 1, 0, 1 },
    { "lsr", 1, 1, 1 },
    { "nop", 0, 0, 1 },
    { "ora", 1, 0, 1 },
    { "pha", 0, 0, 1 },
    { "php", 0, 0, 1 },
    { "pla", 0, 0, 1 },
    { "plp", 0, 0, 1 },
    { "rla", 0, 0, 0 },
    { "rol", 1, 1, 1 },
    { "ror", 1, 1, 1 },
    { "rra", 0, 0, 0 },
    { "rti", 0, 0, 1 },
    { "rts", 0, 0, 1 },
    { "sax", 0, 0, 0 },
    { "sbc", 1, 0, 1 },
    { "sec", 0, 0, 1 },
    { "sed", 0, 0, 1 },
    { "sei", 0, 0, 1 },
    { "slo", 0, 0, 0 },
    { "sre", 0, 0, 0 },
    { "sta", 0, 1, 1 },
    { "stx", 0, 1, 1 },
    { "sty", 0, 1, 1 },
    { "sx7", 0, 0, 0 },
    { "sy7", 0, 0, 0 },
    { "tax", 0, 0, 1 },
    { "tay", 0, 0, 1 },
    { "tsx", 0, 0, 1 },
    { "txa", 0, 0, 1 },
    { "txs", 0, 0, 1 },
    { "tya", 0, 0, 1 },
    { "xea", 0, 0, 0 },
    { "xs7", 0, 0, 0 }
};

char *cpu_opcode_mnemonics[] = {
    "adc", "anc", "and", "arr", "asl", "asr", "asx", "ax7", "axe", "bcc", "bcs", "beq", "bit",
    "bmi", "bne", "bpl", "brk", "bvc", "bvs", "clc", "cld", "cli", "clv", "cmp", "cpx", "cpy",
    "dcp", "dec", "dex", "dey", "eor", "inc", "inx", "iny", "isb", "jam", "jmp", "jsr", "las",
    "lax", "lda", "ldx", "ldy", "lsr", "nop", "ora", "pha", "php", "pla", "plp", "rla", "rol",
    "ror", "rra", "rti", "rts", "sax", "sbc", "sec", "sed", "sei", "slo", "sre", "sta", "stx",
    "sty", "sx7", "sy7", "tax", "tay", "tsx", "txa", "txs", "tya", "xea", "xs7"
};

/**
 * All:
 *   Cycle times noted with a + add one cycle if page boundary is crossed
 *
 * Branches:
 *   2 cycles on miss
 *   3 cycles on hit (4 if crossing page boundary)
 */

opcode_t cpu_opcode_map[] = {
    { CPU_OPCODE_BRK, 0, CPU_ADDR_MODE_IMPLICIT,    7, 0 }, /* 0x00 */
    { CPU_OPCODE_ORA, 0, CPU_ADDR_MODE_IND_X,       6, 0 }, /* 0x01 */
    { CPU_OPCODE_JAM, 1, CPU_ADDR_MODE_IMPLICIT,    0, 0 }, /* 0x02 */
    { CPU_OPCODE_SLO, 1, CPU_ADDR_MODE_IND_X,       8, 0 }, /* 0x03 */
    { CPU_OPCODE_NOP, 1, CPU_ADDR_MODE_ZPAGE,       3, 0 }, /* 0x04 */
    { CPU_OPCODE_ORA, 0, CPU_ADDR_MODE_ZPAGE,       2, 0 }, /* 0x05 */
    { CPU_OPCODE_ASL, 0, CPU_ADDR_MODE_ZPAGE,       5, 0 }, /* 0x06 */
    { CPU_OPCODE_SLO, 1, CPU_ADDR_MODE_ZPAGE,       5, 0 }, /* 0x07 */
    { CPU_OPCODE_PHP, 0, CPU_ADDR_MODE_IMPLICIT,    3, 0 }, /* 0x08 */
    { CPU_OPCODE_ORA, 0, CPU_ADDR_MODE_IMMEDIATE,   2, 0 }, /* 0x09 */
    { CPU_OPCODE_ASL, 0, CPU_ADDR_MODE_ACCUMULATOR, 2, 0 }, /* 0x0a */
    { CPU_OPCODE_ANC, 1, CPU_ADDR_MODE_IMMEDIATE,   2, 0 }, /* 0x0b */
    { CPU_OPCODE_NOP, 1, CPU_ADDR_MODE_ABSOLUTE,    4, 0 }, /* 0x0c */
    { CPU_OPCODE_ORA, 0, CPU_ADDR_MODE_ABSOLUTE,    4, 0 }, /* 0x0d */
    { CPU_OPCODE_ASL, 0, CPU_ADDR_MODE_ABSOLUTE,    6, 0 }, /* 0x0e */
    { CPU_OPCODE_SLO, 1, CPU_ADDR_MODE_ABSOLUTE,    6, 0 }, /* 0x0f */
    { CPU_OPCODE_BPL, 0, CPU_ADDR_MODE_RELATIVE,    2, 1 }, /* 0x10 (2/3/4) */
    { CPU_OPCODE_ORA, 0, CPU_ADDR_MODE_IND_Y,       5, 1 }, /* 0x11 (5+) */
    { CPU_OPCODE_JAM, 1, CPU_ADDR_MODE_IMPLICIT,    0, 0 }, /* 0x12 */
    { CPU_OPCODE_SLO, 1, CPU_ADDR_MODE_IND_Y,       8, 0 }, /* 0x13 */
    { CPU_OPCODE_NOP, 1, CPU_ADDR_MODE_ZPAGE_X,     4, 0 }, /* 0x14 */
    { CPU_OPCODE_ORA, 0, CPU_ADDR_MODE_ZPAGE_X,     3, 0 }, /* 0x15 */
    { CPU_OPCODE_ASL, 0, CPU_ADDR_MODE_ZPAGE_X,     6, 0 }, /* 0x16 */
    { CPU_OPCODE_SLO, 1, CPU_ADDR_MODE_ZPAGE_X,     6, 0 }, /* 0x17 */
    { CPU_OPCODE_CLC, 0, CPU_ADDR_MODE_IMPLICIT,    2, 0 }, /* 0x18 */
    { CPU_OPCODE_ORA, 0, CPU_ADDR_MODE_ABSOLUTE_Y,  4, 1 }, /* 0x19 (4+) */
    { CPU_OPCODE_NOP, 1, CPU_ADDR_MODE_IMPLICIT,    2, 0 }, /* 0x1a */
    { CPU_OPCODE_SLO, 1, CPU_ADDR_MODE_ABSOLUTE_Y,  7, 0 }, /* 0x1b */
    { CPU_OPCODE_NOP, 1, CPU_ADDR_MODE_ABSOLUTE_X,  4, 0 }, /* 0x1c */
    { CPU_OPCODE_ORA, 0, CPU_ADDR_MODE_ABSOLUTE_X,  4, 1 }, /* 0x1d (4+) */
    { CPU_OPCODE_ASL, 0, CPU_ADDR_MODE_ABSOLUTE_X,  7, 0 }, /* 0x1e */
    { CPU_OPCODE_SLO, 1, CPU_ADDR_MODE_ABSOLUTE_X,  7, 0 }, /* 0x1f */
    { CPU_OPCODE_JSR, 0, CPU_ADDR_MODE_ABSOLUTE,    6, 0 }, /* 0x20 */
    { CPU_OPCODE_AND, 0, CPU_ADDR_MODE_IND_X,       6, 0 }, /* 0x21 */
    { CPU_OPCODE_JAM, 1, CPU_ADDR_MODE_IMPLICIT,    0, 0 }, /* 0x22 */
    { CPU_OPCODE_RLA, 1, CPU_ADDR_MODE_IND_X,       8, 0 }, /* 0x23 */
    { CPU_OPCODE_BIT, 0, CPU_ADDR_MODE_ZPAGE,       3, 0 }, /* 0x24 */
    { CPU_OPCODE_AND, 0, CPU_ADDR_MODE_ZPAGE,       2, 0 }, /* 0x25 */
    { CPU_OPCODE_ROL, 0, CPU_ADDR_MODE_ZPAGE,       5, 0 }, /* 0x26 */
    { CPU_OPCODE_RLA, 1, CPU_ADDR_MODE_ZPAGE,       5, 0 }, /* 0x27 */
    { CPU_OPCODE_PLP, 0, CPU_ADDR_MODE_IMPLICIT,    4, 0 }, /* 0x28 */
    { CPU_OPCODE_AND, 0, CPU_ADDR_MODE_IMMEDIATE,   2, 0 }, /* 0x29 */
    { CPU_OPCODE_ROL, 0, CPU_ADDR_MODE_ACCUMULATOR, 2, 0 }, /* 0x2a */
    { CPU_OPCODE_ANC, 1, CPU_ADDR_MODE_IMMEDIATE,   2, 0 }, /* 0x2b */
    { CPU_OPCODE_BIT, 0, CPU_ADDR_MODE_ABSOLUTE,    4, 0 }, /* 0x2c */
    { CPU_OPCODE_AND, 0, CPU_ADDR_MODE_ABSOLUTE,    4, 0 }, /* 0x2d */
    { CPU_OPCODE_ROL, 0, CPU_ADDR_MODE_ABSOLUTE,    6, 0 }, /* 0x2e */
    { CPU_OPCODE_RLA, 1, CPU_ADDR_MODE_ABSOLUTE,    6, 0 }, /* 0x2f */
    { CPU_OPCODE_BMI, 0, CPU_ADDR_MODE_RELATIVE,    2, 1 }, /* 0x30 (2/3/4) */
    { CPU_OPCODE_AND, 0, CPU_ADDR_MODE_IND_Y,       5, 1 }, /* 0x31 (5+) */
    { CPU_OPCODE_JAM, 1, CPU_ADDR_MODE_IMPLICIT,    0, 0 }, /* 0x32 */
    { CPU_OPCODE_RLA, 1, CPU_ADDR_MODE_IND_Y,       8, 0 }, /* 0x33 */
    { CPU_OPCODE_NOP, 1, CPU_ADDR_MODE_ZPAGE_X,     4, 0 }, /* 0x34 */
    { CPU_OPCODE_AND, 0, CPU_ADDR_MODE_ZPAGE_X,     3, 0 }, /* 0x35 */
    { CPU_OPCODE_ROL, 0, CPU_ADDR_MODE_ZPAGE_X,     6, 0 }, /* 0x36 */
    { CPU_OPCODE_RLA, 1, CPU_ADDR_MODE_ZPAGE_X,     6, 0 }, /* 0x37 */
    { CPU_OPCODE_SEC, 0, CPU_ADDR_MODE_IMPLICIT,    2, 0 }, /* 0x38 */
    { CPU_OPCODE_AND, 0, CPU_ADDR_MODE_ABSOLUTE_Y,  4, 1 }, /* 0x39 (4+) */
    { CPU_OPCODE_NOP, 1, CPU_ADDR_MODE_IMPLICIT,    0, 0 }, /* 0x3a */
    { CPU_OPCODE_RLA, 1, CPU_ADDR_MODE_ABSOLUTE_Y,  7, 0 }, /* 0x3b */
    { CPU_OPCODE_NOP, 1, CPU_ADDR_MODE_ABSOLUTE_X,  4, 0 }, /* 0x3c */
    { CPU_OPCODE_AND, 0, CPU_ADDR_MODE_ABSOLUTE_X,  4, 1 }, /* 0x3d (4+) */
    { CPU_OPCODE_ROL, 0, CPU_ADDR_MODE_ABSOLUTE_X,  7, 0 }, /* 0x3e */
    { CPU_OPCODE_RLA, 1, CPU_ADDR_MODE_ABSOLUTE_X,  7, 0 }, /* 0x3f */
    { CPU_OPCODE_RTI, 0, CPU_ADDR_MODE_IMPLICIT,    6, 0 }, /* 0x40 */
    { CPU_OPCODE_EOR, 0, CPU_ADDR_MODE_IND_X,       6, 0 }, /* 0x41 */
    { CPU_OPCODE_JAM, 1, CPU_ADDR_MODE_IMPLICIT,    0, 0 }, /* 0x42 */
    { CPU_OPCODE_SLO, 1, CPU_ADDR_MODE_IND_X,       8, 0 }, /* 0x43 */
    { CPU_OPCODE_NOP, 1, CPU_ADDR_MODE_ZPAGE,       3, 0 }, /* 0x44 */
    { CPU_OPCODE_EOR, 0, CPU_ADDR_MODE_ZPAGE,       3, 0 }, /* 0x45 */
    { CPU_OPCODE_LSR, 0, CPU_ADDR_MODE_ZPAGE,       5, 0 }, /* 0x46 */
    { CPU_OPCODE_SLO, 1, CPU_ADDR_MODE_ZPAGE,       5, 0 }, /* 0x47 */
    { CPU_OPCODE_PHA, 0, CPU_ADDR_MODE_IMPLICIT,    3, 0 }, /* 0x48 */
    { CPU_OPCODE_EOR, 0, CPU_ADDR_MODE_IMMEDIATE,   2, 0 }, /* 0x49 */
    { CPU_OPCODE_LSR, 0, CPU_ADDR_MODE_ACCUMULATOR, 2, 0 }, /* 0x4a */
    { CPU_OPCODE_ASR, 1, CPU_ADDR_MODE_IMMEDIATE,   2, 0 }, /* 0x4b */
    { CPU_OPCODE_JMP, 0, CPU_ADDR_MODE_ABSOLUTE,    3, 0 }, /* 0x4c */
    { CPU_OPCODE_EOR, 0, CPU_ADDR_MODE_ABSOLUTE,    4, 0 }, /* 0x4d */
    { CPU_OPCODE_LSR, 0, CPU_ADDR_MODE_ABSOLUTE,    6, 0 }, /* 0x4e */
    { CPU_OPCODE_SLO, 1, CPU_ADDR_MODE_ABSOLUTE,    6, 0 }, /* 0x4f */
    { CPU_OPCODE_BVC, 0, CPU_ADDR_MODE_RELATIVE,    2, 1 }, /* 0x50 (2/3/4) */
    { CPU_OPCODE_EOR, 0, CPU_ADDR_MODE_IND_Y,       5, 1 }, /* 0x51 (5+) */
    { CPU_OPCODE_JAM, 1, CPU_ADDR_MODE_IMPLICIT,    0, 0 }, /* 0x52 */
    { CPU_OPCODE_SLO, 1, CPU_ADDR_MODE_IND_Y,       8, 0 }, /* 0x53 */
    { CPU_OPCODE_NOP, 1, CPU_ADDR_MODE_ZPAGE_X,     4, 0 }, /* 0x54 */
    { CPU_OPCODE_EOR, 0, CPU_ADDR_MODE_ZPAGE_X,     4, 0 }, /* 0x55 */
    { CPU_OPCODE_LSR, 0, CPU_ADDR_MODE_ZPAGE_X,     6, 0 }, /* 0x56 */
    { CPU_OPCODE_SLO, 1, CPU_ADDR_MODE_ZPAGE_X,     6, 0 }, /* 0x57 */
    { CPU_OPCODE_CLI, 0, CPU_ADDR_MODE_IMPLICIT,    2, 0 }, /* 0x58 */
    { CPU_OPCODE_EOR, 0, CPU_ADDR_MODE_ABSOLUTE_Y,  4, 1 }, /* 0x59 (4+) */
    { CPU_OPCODE_NOP, 1, CPU_ADDR_MODE_IMPLICIT,    2, 0 }, /* 0x5a */
    { CPU_OPCODE_SLO, 1, CPU_ADDR_MODE_ABSOLUTE_Y,  5, 0 }, /* 0x5b */
    { CPU_OPCODE_NOP, 1, CPU_ADDR_MODE_ABSOLUTE_X,  4, 0 }, /* 0x5c */
    { CPU_OPCODE_EOR, 0, CPU_ADDR_MODE_ABSOLUTE_X,  4, 1 }, /* 0x5d (4+) */
    { CPU_OPCODE_LSR, 0, CPU_ADDR_MODE_ABSOLUTE_X,  7, 0 }, /* 0x5e */
    { CPU_OPCODE_SLO, 1, CPU_ADDR_MODE_ABSOLUTE_X,  7, 0 }, /* 0x5f */
    { CPU_OPCODE_RTS, 0, CPU_ADDR_MODE_IMPLICIT,    6, 0 }, /* 0x60 */
    { CPU_OPCODE_ADC, 0, CPU_ADDR_MODE_IND_X,       6, 0 }, /* 0x61 */
    { CPU_OPCODE_JAM, 1, CPU_ADDR_MODE_IMPLICIT,    0, 0 }, /* 0x62 */
    { CPU_OPCODE_RRA, 1, CPU_ADDR_MODE_IND_X,       8, 0 }, /* 0x63 */
    { CPU_OPCODE_NOP, 1, CPU_ADDR_MODE_ZPAGE,       3, 0 }, /* 0x64 */
    { CPU_OPCODE_ADC, 0, CPU_ADDR_MODE_ZPAGE,       3, 0 }, /* 0x65 */
    { CPU_OPCODE_ROR, 0, CPU_ADDR_MODE_ZPAGE,       5, 0 }, /* 0x66 */
    { CPU_OPCODE_RRA, 1, CPU_ADDR_MODE_ZPAGE,       6, 0 }, /* 0x67 */
    { CPU_OPCODE_PLA, 0, CPU_ADDR_MODE_IMPLICIT,    4, 0 }, /* 0x68 */
    { CPU_OPCODE_ADC, 0, CPU_ADDR_MODE_IMMEDIATE,   2, 0 }, /* 0x69 */
    { CPU_OPCODE_ROR, 0, CPU_ADDR_MODE_ACCUMULATOR, 2, 0 }, /* 0x6a */
    { CPU_OPCODE_ARR, 1, CPU_ADDR_MODE_IMMEDIATE,   2, 0 }, /* 0x6b */
    { CPU_OPCODE_JMP, 0, CPU_ADDR_MODE_INDIRECT,    5, 0 }, /* 0x6c */
    { CPU_OPCODE_ADC, 0, CPU_ADDR_MODE_ABSOLUTE,    4, 0 }, /* 0x6d */
    { CPU_OPCODE_ROR, 0, CPU_ADDR_MODE_ABSOLUTE,    6, 0 }, /* 0x6e */
    { CPU_OPCODE_RRA, 1, CPU_ADDR_MODE_ABSOLUTE,    6, 0 }, /* 0x6f */
    { CPU_OPCODE_BVS, 0, CPU_ADDR_MODE_RELATIVE,    2, 1 }, /* 0x70 (2/3/4) */
    { CPU_OPCODE_ADC, 0, CPU_ADDR_MODE_IND_Y,       5, 1 }, /* 0x71 (5+) */
    { CPU_OPCODE_JAM, 1, CPU_ADDR_MODE_IMPLICIT,    0, 0 }, /* 0x72 */
    { CPU_OPCODE_RRA, 1, CPU_ADDR_MODE_IND_Y,       8, 0 }, /* 0x73 */
    { CPU_OPCODE_NOP, 1, CPU_ADDR_MODE_ZPAGE,       3, 0 }, /* 0x74 */
    { CPU_OPCODE_ADC, 0, CPU_ADDR_MODE_ZPAGE_X,     4, 0 }, /* 0x75 */
    { CPU_OPCODE_ROR, 0, CPU_ADDR_MODE_ZPAGE_X,     6, 0 }, /* 0x76 */
    { CPU_OPCODE_RRA, 1, CPU_ADDR_MODE_ZPAGE_X,     6, 0 }, /* 0x77 */
    { CPU_OPCODE_SEI, 0, CPU_ADDR_MODE_IMPLICIT,    2, 0 }, /* 0x78 */
    { CPU_OPCODE_ADC, 0, CPU_ADDR_MODE_ABSOLUTE_Y,  4, 1 }, /* 0x79 (4+) */
    { CPU_OPCODE_NOP, 1, CPU_ADDR_MODE_IMPLICIT,    3, 0 }, /* 0x7a */
    { CPU_OPCODE_RRA, 1, CPU_ADDR_MODE_ABSOLUTE_Y,  7, 0 }, /* 0x7b */
    { CPU_OPCODE_NOP, 1, CPU_ADDR_MODE_ABSOLUTE_X,  4, 0 }, /* 0x7c */
    { CPU_OPCODE_ADC, 0, CPU_ADDR_MODE_ABSOLUTE_X,  4, 1 }, /* 0x7d (4+) */
    { CPU_OPCODE_ROR, 0, CPU_ADDR_MODE_ABSOLUTE_X,  7, 0 }, /* 0x7e */
    { CPU_OPCODE_RRA, 1, CPU_ADDR_MODE_ABSOLUTE_X,  7, 0 }, /* 0x7f */
    { CPU_OPCODE_NOP, 1, CPU_ADDR_MODE_IMMEDIATE,   2, 0 }, /* 0x80 */
    { CPU_OPCODE_STA, 0, CPU_ADDR_MODE_IND_X,       6, 0 }, /* 0x81 */
    { CPU_OPCODE_NOP, 1, CPU_ADDR_MODE_IMMEDIATE,   2, 0 }, /* 0x82 */
    { CPU_OPCODE_SAX, 1, CPU_ADDR_MODE_IND_X,       6, 0 }, /* 0x83 */
    { CPU_OPCODE_STY, 0, CPU_ADDR_MODE_ZPAGE,       3, 0 }, /* 0x84 */
    { CPU_OPCODE_STA, 0, CPU_ADDR_MODE_ZPAGE,       3, 0 }, /* 0x85 */
    { CPU_OPCODE_STX, 0, CPU_ADDR_MODE_ZPAGE,       3, 0 }, /* 0x86 */
    { CPU_OPCODE_SAX, 1, CPU_ADDR_MODE_ZPAGE,       3, 0 }, /* 0x87 */
    { CPU_OPCODE_DEY, 0, CPU_ADDR_MODE_IMPLICIT,    2, 0 }, /* 0x88 */
    { CPU_OPCODE_NOP, 1, CPU_ADDR_MODE_IMMEDIATE,   2, 0 }, /* 0x89 */
    { CPU_OPCODE_TXA, 0, CPU_ADDR_MODE_IMPLICIT,    2, 0 }, /* 0x8a */
    { CPU_OPCODE_AXE, 1, CPU_ADDR_MODE_IMMEDIATE,   2, 0 }, /* 0x8b */
    { CPU_OPCODE_STY, 0, CPU_ADDR_MODE_ABSOLUTE,    4, 0 }, /* 0x8c */
    { CPU_OPCODE_STA, 0, CPU_ADDR_MODE_ABSOLUTE,    4, 0 }, /* 0x8d */
    { CPU_OPCODE_STX, 0, CPU_ADDR_MODE_ABSOLUTE,    4, 0 }, /* 0x8e */
    { CPU_OPCODE_SAX, 1, CPU_ADDR_MODE_ABSOLUTE,    4, 0 }, /* 0x8f */
    { CPU_OPCODE_BCC, 0, CPU_ADDR_MODE_RELATIVE,    2, 1 }, /* 0x90 (2/3/4) */
    { CPU_OPCODE_STA, 0, CPU_ADDR_MODE_IND_Y,       6, 0 }, /* 0x91 */
    { CPU_OPCODE_JAM, 1, CPU_ADDR_MODE_IMPLICIT,    0, 0 }, /* 0x92 */
    { CPU_OPCODE_AX7, 1, CPU_ADDR_MODE_IND_Y,       5, 0 }, /* 0x93 */
    { CPU_OPCODE_STY, 0, CPU_ADDR_MODE_ZPAGE_X,     4, 0 }, /* 0x94 */
    { CPU_OPCODE_STA, 0, CPU_ADDR_MODE_ZPAGE_X,     4, 0 }, /* 0x95 */
    { CPU_OPCODE_STX, 0, CPU_ADDR_MODE_ZPAGE_Y,     4, 0 }, /* 0x96 */
    { CPU_OPCODE_SAX, 1, CPU_ADDR_MODE_ZPAGE_Y,     4, 0 }, /* 0x97 */
    { CPU_OPCODE_TYA, 0, CPU_ADDR_MODE_IMPLICIT,    2, 0 }, /* 0x98 */
    { CPU_OPCODE_STA, 0, CPU_ADDR_MODE_ABSOLUTE_Y,  5, 0 }, /* 0x99 */
    { CPU_OPCODE_TXS, 0, CPU_ADDR_MODE_IMPLICIT,    2, 0 }, /* 0x9a */
    { CPU_OPCODE_XS7, 1, CPU_ADDR_MODE_ABSOLUTE_Y,  5, 0 }, /* 0x9b */
    { CPU_OPCODE_SY7, 1, CPU_ADDR_MODE_ABSOLUTE_X,  5, 0 }, /* 0x9c */
    { CPU_OPCODE_STA, 0, CPU_ADDR_MODE_ABSOLUTE_X,  5, 0 }, /* 0x9d */
    { CPU_OPCODE_SX7, 1, CPU_ADDR_MODE_ABSOLUTE_Y,  5, 0 }, /* 0x9e */
    { CPU_OPCODE_AX7, 1, CPU_ADDR_MODE_ABSOLUTE_Y,  5, 0 }, /* 0x9f */
    { CPU_OPCODE_LDY, 0, CPU_ADDR_MODE_IMMEDIATE,   2, 0 }, /* 0xa0 */
    { CPU_OPCODE_LDA, 0, CPU_ADDR_MODE_IND_X,       6, 0 }, /* 0xa1 */
    { CPU_OPCODE_LDX, 0, CPU_ADDR_MODE_IMMEDIATE,   2, 0 }, /* 0xa2 */
    { CPU_OPCODE_LAX, 1, CPU_ADDR_MODE_IND_X,       6, 0 }, /* 0xa3 */
    { CPU_OPCODE_LDY, 0, CPU_ADDR_MODE_ZPAGE,       3, 0 }, /* 0xa4 */
    { CPU_OPCODE_LDA, 0, CPU_ADDR_MODE_ZPAGE,       3, 0 }, /* 0xa5 */
    { CPU_OPCODE_LDX, 0, CPU_ADDR_MODE_ZPAGE,       3, 0 }, /* 0xa6 */
    { CPU_OPCODE_LAX, 1, CPU_ADDR_MODE_ZPAGE,       3, 0 }, /* 0xa7 */
    { CPU_OPCODE_TAY, 0, CPU_ADDR_MODE_IMPLICIT,    2, 0 }, /* 0xa8 */
    { CPU_OPCODE_LDA, 0, CPU_ADDR_MODE_IMMEDIATE,   2, 0 }, /* 0xa9 */
    { CPU_OPCODE_TAX, 0, CPU_ADDR_MODE_IMPLICIT,    2, 0 }, /* 0xaa */
    { CPU_OPCODE_XEA, 1, CPU_ADDR_MODE_IMMEDIATE,   2, 0 }, /* 0xab */
    { CPU_OPCODE_LDY, 0, CPU_ADDR_MODE_ABSOLUTE,    4, 0 }, /* 0xac */
    { CPU_OPCODE_LDA, 0, CPU_ADDR_MODE_ABSOLUTE,    4, 0 }, /* 0xad */
    { CPU_OPCODE_LDX, 0, CPU_ADDR_MODE_ABSOLUTE,    4, 0 }, /* 0xae */
    { CPU_OPCODE_LAX, 1, CPU_ADDR_MODE_ABSOLUTE,    4, 0 }, /* 0xaf */
    { CPU_OPCODE_BCS, 0, CPU_ADDR_MODE_RELATIVE,    2, 1 }, /* 0xb0 (2/3/4) */
    { CPU_OPCODE_LDA, 0, CPU_ADDR_MODE_IND_Y,       5, 1 }, /* 0xb1 (5+) */
    { CPU_OPCODE_JAM, 1, CPU_ADDR_MODE_IMPLICIT,    0, 0 }, /* 0xb2 */
    { CPU_OPCODE_LAX, 1, CPU_ADDR_MODE_IND_Y,       5, 0 }, /* 0xb3 */
    { CPU_OPCODE_LDY, 0, CPU_ADDR_MODE_ZPAGE_X,     4, 0 }, /* 0xb4 */
    { CPU_OPCODE_LDA, 0, CPU_ADDR_MODE_ZPAGE_X,     4, 0 }, /* 0xb5 */
    { CPU_OPCODE_LDX, 0, CPU_ADDR_MODE_ZPAGE_Y,     4, 0 }, /* 0xb6 */
    { CPU_OPCODE_LAX, 1, CPU_ADDR_MODE_ZPAGE_Y,     4, 0 }, /* 0xb7 */
    { CPU_OPCODE_CLV, 0, CPU_ADDR_MODE_IMPLICIT,    2, 0 }, /* 0xb8 */
    { CPU_OPCODE_LDA, 0, CPU_ADDR_MODE_ABSOLUTE_Y,  4, 1 }, /* 0xb9 (4+) */
    { CPU_OPCODE_TSX, 0, CPU_ADDR_MODE_IMPLICIT,    2, 0 }, /* 0xba */
    { CPU_OPCODE_LAS, 1, CPU_ADDR_MODE_ABSOLUTE_Y,  4, 0 }, /* 0xbb */
    { CPU_OPCODE_LDY, 0, CPU_ADDR_MODE_ABSOLUTE_X,  4, 1 }, /* 0xbc (4+) */
    { CPU_OPCODE_LDA, 0, CPU_ADDR_MODE_ABSOLUTE_X,  4, 1 }, /* 0xbd (4+) */
    { CPU_OPCODE_LDX, 0, CPU_ADDR_MODE_ABSOLUTE_Y,  4, 1 }, /* 0xbe (4+) */
    { CPU_OPCODE_LAX, 1, CPU_ADDR_MODE_ABSOLUTE_Y,  4, 0 }, /* 0xbf */
    { CPU_OPCODE_CPY, 0, CPU_ADDR_MODE_IMMEDIATE,   2, 0 }, /* 0xc0 */
    { CPU_OPCODE_CMP, 0, CPU_ADDR_MODE_IND_X,       6, 0 }, /* 0xc1 */
    { CPU_OPCODE_NOP, 1, CPU_ADDR_MODE_IMMEDIATE,   2, 0 }, /* 0xc2 */
    { CPU_OPCODE_DCP, 1, CPU_ADDR_MODE_IND_X,       8, 0 }, /* 0xc3 */
    { CPU_OPCODE_CPY, 0, CPU_ADDR_MODE_ZPAGE,       3, 0 }, /* 0xc4 */
    { CPU_OPCODE_CMP, 0, CPU_ADDR_MODE_ZPAGE,       3, 0 }, /* 0xc5 */
    { CPU_OPCODE_DEC, 0, CPU_ADDR_MODE_ZPAGE,       5, 0 }, /* 0xc6 */
    { CPU_OPCODE_DCP, 1, CPU_ADDR_MODE_ZPAGE,       5, 0 }, /* 0xc7 */
    { CPU_OPCODE_INY, 0, CPU_ADDR_MODE_IMPLICIT,    2, 0 }, /* 0xc8 */
    { CPU_OPCODE_CMP, 0, CPU_ADDR_MODE_IMMEDIATE,   2, 0 }, /* 0xc9 */
    { CPU_OPCODE_DEX, 0, CPU_ADDR_MODE_IMPLICIT,    2, 0 }, /* 0xca */
    { CPU_OPCODE_ASX, 1, CPU_ADDR_MODE_IMMEDIATE,   2, 0 }, /* 0xcb */
    { CPU_OPCODE_CPY, 0, CPU_ADDR_MODE_ABSOLUTE,    4, 0 }, /* 0xcc */
    { CPU_OPCODE_CMP, 0, CPU_ADDR_MODE_ABSOLUTE,    4, 0 }, /* 0xcd */
    { CPU_OPCODE_DEC, 0, CPU_ADDR_MODE_ABSOLUTE,    6, 0 }, /* 0xce */
    { CPU_OPCODE_DCP, 1, CPU_ADDR_MODE_ABSOLUTE,    6, 0 }, /* 0xcf */
    { CPU_OPCODE_BNE, 0, CPU_ADDR_MODE_RELATIVE,    2, 1 }, /* 0xd0 (2/3/4) */
    { CPU_OPCODE_CMP, 0, CPU_ADDR_MODE_IND_Y,       5, 1 }, /* 0xd1 (5+) */
    { CPU_OPCODE_JAM, 1, CPU_ADDR_MODE_IMPLICIT,    0, 0 }, /* 0xd2 */
    { CPU_OPCODE_DCP, 1, CPU_ADDR_MODE_IND_Y,       8, 0 }, /* 0xd3 */
    { CPU_OPCODE_NOP, 1, CPU_ADDR_MODE_ZPAGE_X,     4, 0 }, /* 0xd4 */
    { CPU_OPCODE_CMP, 0, CPU_ADDR_MODE_ZPAGE_X,     4, 0 }, /* 0xd5 */
    { CPU_OPCODE_DEC, 0, CPU_ADDR_MODE_ZPAGE_X,     6, 0 }, /* 0xd6 */
    { CPU_OPCODE_DCP, 1, CPU_ADDR_MODE_ZPAGE_X,     6, 0 }, /* 0xd7 */
    { CPU_OPCODE_CLD, 0, CPU_ADDR_MODE_IMPLICIT,    2, 0 }, /* 0xd8 */
    { CPU_OPCODE_CMP, 0, CPU_ADDR_MODE_ABSOLUTE_Y,  4, 1 }, /* 0xd9 (4+) */
    { CPU_OPCODE_NOP, 1, CPU_ADDR_MODE_IMPLICIT,    2, 0 }, /* 0xda */
    { CPU_OPCODE_DCP, 1, CPU_ADDR_MODE_ABSOLUTE_Y,  7, 0 }, /* 0xdb */
    { CPU_OPCODE_NOP, 1, CPU_ADDR_MODE_ABSOLUTE_X,  4, 0 }, /* 0xdc */
    { CPU_OPCODE_CMP, 0, CPU_ADDR_MODE_ABSOLUTE_X,  4, 1 }, /* 0xdd (4+) */
    { CPU_OPCODE_DEC, 0, CPU_ADDR_MODE_ABSOLUTE_X,  7, 0 }, /* 0xde */
    { CPU_OPCODE_DCP, 1, CPU_ADDR_MODE_ABSOLUTE_X,  7, 0 }, /* 0xdf */
    { CPU_OPCODE_CPX, 0, CPU_ADDR_MODE_IMMEDIATE,   2, 0 }, /* 0xe0 */
    { CPU_OPCODE_SBC, 0, CPU_ADDR_MODE_IND_X,       6, 0 }, /* 0xe1 */
    { CPU_OPCODE_NOP, 1, CPU_ADDR_MODE_IMMEDIATE,   2, 0 }, /* 0xe2 */
    { CPU_OPCODE_ISB, 1, CPU_ADDR_MODE_IND_X,       8, 0 }, /* 0xe3 */
    { CPU_OPCODE_CPX, 0, CPU_ADDR_MODE_ZPAGE,       3, 0 }, /* 0xe4 */
    { CPU_OPCODE_SBC, 0, CPU_ADDR_MODE_ZPAGE,       3, 0 }, /* 0xe5 */
    { CPU_OPCODE_INC, 0, CPU_ADDR_MODE_ZPAGE,       5, 0 }, /* 0xe6 */
    { CPU_OPCODE_ISB, 1, CPU_ADDR_MODE_ZPAGE,       5, 0 }, /* 0xe7 */
    { CPU_OPCODE_INX, 0, CPU_ADDR_MODE_IMPLICIT,    2, 0 }, /* 0xe8 */
    { CPU_OPCODE_SBC, 0, CPU_ADDR_MODE_IMMEDIATE,   2, 0 }, /* 0xe9 */
    { CPU_OPCODE_NOP, 0, CPU_ADDR_MODE_IMPLICIT,    2, 0 }, /* 0xea */
    { CPU_OPCODE_SBC, 1, CPU_ADDR_MODE_IMMEDIATE,   2, 0 }, /* 0xeb */
    { CPU_OPCODE_CPX, 0, CPU_ADDR_MODE_ABSOLUTE,    4, 0 }, /* 0xec */
    { CPU_OPCODE_SBC, 0, CPU_ADDR_MODE_ABSOLUTE,    4, 0 }, /* 0xed */
    { CPU_OPCODE_INC, 0, CPU_ADDR_MODE_ABSOLUTE,    6, 0 }, /* 0xee */
    { CPU_OPCODE_ISB, 1, CPU_ADDR_MODE_ABSOLUTE,    6, 0 }, /* 0xef */
    { CPU_OPCODE_BEQ, 0, CPU_ADDR_MODE_RELATIVE,    2, 1 }, /* 0xf0 (2/3/4) */
    { CPU_OPCODE_SBC, 0, CPU_ADDR_MODE_IND_Y,       5, 1 }, /* 0xf1 (5+) */
    { CPU_OPCODE_JAM, 1, CPU_ADDR_MODE_IMPLICIT,    0, 0 }, /* 0xf2 */
    { CPU_OPCODE_ISB, 1, CPU_ADDR_MODE_IND_Y,       8, 0 }, /* 0xf3 */
    { CPU_OPCODE_NOP, 1, CPU_ADDR_MODE_ZPAGE_X,     4, 0 }, /* 0xf4 */
    { CPU_OPCODE_SBC, 0, CPU_ADDR_MODE_ZPAGE_X,     4, 0 }, /* 0xf5 */
    { CPU_OPCODE_INC, 0, CPU_ADDR_MODE_ZPAGE_X,     6, 0 }, /* 0xf6 */
    { CPU_OPCODE_ISB, 1, CPU_ADDR_MODE_ZPAGE_X,     6, 0 }, /* 0xf7 */
    { CPU_OPCODE_SED, 0, CPU_ADDR_MODE_IMPLICIT,    2, 0 }, /* 0xf8 */
    { CPU_OPCODE_SBC, 0, CPU_ADDR_MODE_ABSOLUTE_Y,  4, 1 }, /* 0xf9 (4+) */
    { CPU_OPCODE_NOP, 1, CPU_ADDR_MODE_IMPLICIT,    2, 0 }, /* 0xfa */
    { CPU_OPCODE_ISB, 1, CPU_ADDR_MODE_ABSOLUTE_Y,  7, 0 }, /* 0xfb */
    { CPU_OPCODE_NOP, 1, CPU_ADDR_MODE_ABSOLUTE_X,  4, 0 }, /* 0xfc */
    { CPU_OPCODE_SBC, 0, CPU_ADDR_MODE_ABSOLUTE_X,  4, 1 }, /* 0xfd (4+) */
    { CPU_OPCODE_INC, 0, CPU_ADDR_MODE_ABSOLUTE_X,  7, 0 }, /* 0xfe */
    { CPU_OPCODE_ISB, 1, CPU_ADDR_MODE_ABSOLUTE_X,  7, 0 }, /* 0xff */
};
#endif

#endif /* _OPCODES_H_ */
