/*
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


#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include <time.h>

#include "emulator.h"
#include "memory.h"
#include "6502.h"
#include "debug.h"

#define _INCLUDE_OPCODE_MAP
#include "opcodes.h"

cpu_t cpu_state;

/* Forwards */
uint16_t cpu_makeword(uint8_t lo, uint8_t hi);
uint8_t cpu_fetch(void);
void cpu_dump(char *msg);

/* FIXME: inline macroize these */
int cpu_flag(uint8_t flag);
void cpu_set_flag(uint8_t flag, int value);

/**
 * start up the processor, initializing registers appropriately
 */
void cpu_init(void) {
    memset((void*)&cpu_state, 0, sizeof(cpu_state));
    cpu_state.ip = cpu_makeword(memory_read(0xfffc), memory_read(0xfffd));
    cpu_state.sp = 0xff;
    cpu_state.p = 0x20;
    cpu_set_flag(FLAG_I,1);
    cpu_set_flag(FLAG_D,0);
    cpu_set_flag(FLAG_B,1); /* sim65 does this, not sure if it's right */
    /* now we're ready to roll */
}

/**
 * shut down the processor
 */
void cpu_deinit(void) {
}

/*
 * private
 *
 * Make a 16 bit word given a low and high byte
 */
uint16_t cpu_makeword(uint8_t lo, uint8_t hi) {
    uint16_t retval;

    retval = hi << 8 | lo;
    return retval;
}


/*
 * private
 *
 * cpu_push_8(uint8_t val)
 */
void cpu_push_8(uint8_t val) {
    memory_write(cpu_state.sp + 0x100, val);
    cpu_state.sp--;
}

void cpu_push_16(uint16_t val) {
    cpu_push_8((val & 0xFF00) >> 8);
    cpu_push_8(val & 0xFF);
}

uint8_t cpu_pull_8(void) {
    cpu_state.sp++;
    return memory_read(cpu_state.sp + 0x100);
}

uint16_t cpu_pull_16(void) {
    uint8_t lo = cpu_pull_8();
    uint8_t hi = cpu_pull_8();
    return cpu_makeword(lo, hi);
}

int16_t twos_complement(uint8_t value) {
    return value & 0x80 ? (value ^ 0xFF) + 1 : value;
}

/*
 * private
 *
 * Set the appropriate flag
 */
void cpu_set_flag(uint8_t flag, int value) {
    if(value)
        cpu_state.p |= flag;
    else
        cpu_state.p &= ~flag;
}

/*
 * private
 *
 * Get the appropriate flag
 */
int cpu_flag(uint8_t flag) {
    if(cpu_state.p & flag)
        return 1;
    return 0;
}

/*
 * private
 *
 * Fetch the next instruction from the IP, incrementing the ip
 * returns 8 bit instruction (or data)
 */
uint8_t cpu_fetch(void) {
    uint8_t retval;

    retval = memory_read(cpu_state.ip);
    cpu_state.ip++;
    return retval;
}

/**
 * execute the next instruction, incrementing the cpu state
 * appropriately (incrmenting IP, etc), and returning the number
 * of cycles required for this instruction
 *
 * @returns number of cpu cycles
 */
uint8_t cpu_execute(void) {
    opcode_t *opmap;
    opcode_info_t *opinfo;
    uint8_t opcode;
    uint16_t addr;
    uint8_t value;
    uint8_t t81, t82;     /* temp 8 bit numbers */
    uint16_t t161, t162;  /* temp 16 bit numbers */
    int16_t ts161, ts162; /* temp 16 bit signed number */
    struct timespec rqtp;

    opcode = cpu_fetch();
    opmap = &cpu_opcode_map[opcode];
    opinfo = &cpu_opcode_info[opmap->opcode_family];

    /* get base operands */
    switch(opmap->addressing_mode) {
    case CPU_ADDR_MODE_IMPLICIT:
    case CPU_ADDR_MODE_ACCUMULATOR:
        break;
    case CPU_ADDR_MODE_IMMEDIATE:
    case CPU_ADDR_MODE_ZPAGE:
    case CPU_ADDR_MODE_ZPAGE_X:
    case CPU_ADDR_MODE_ZPAGE_Y:
    case CPU_ADDR_MODE_IND_X:
    case CPU_ADDR_MODE_IND_Y:
    case CPU_ADDR_MODE_RELATIVE:
        addr = cpu_fetch();
        break;
    case CPU_ADDR_MODE_ABSOLUTE:
    case CPU_ADDR_MODE_ABSOLUTE_X:
    case CPU_ADDR_MODE_ABSOLUTE_Y:
    case CPU_ADDR_MODE_INDIRECT:
        t81 = cpu_fetch();
        t82 = cpu_fetch();
        addr = cpu_makeword(t81, t82);
        break;
    default:
        DPRINTF(DBG_FATAL,"Unsupported indexing mode: %d\n", opmap->addressing_mode);
        exit(EXIT_FAILURE);
    }

    /* get *real* addr based on indexing mode */
    switch(opmap->addressing_mode) {
    case CPU_ADDR_MODE_ACCUMULATOR:
    case CPU_ADDR_MODE_IMMEDIATE:
    case CPU_ADDR_MODE_IMPLICIT:
    case CPU_ADDR_MODE_ZPAGE:
    case CPU_ADDR_MODE_ABSOLUTE:
        break;
    case CPU_ADDR_MODE_RELATIVE:
        if(addr & 0x80) {
            addr--;
            addr ^= 0xFF;
            addr = cpu_state.ip - addr;
        } else {
            addr = cpu_state.ip + addr;
        }
        break;
    case CPU_ADDR_MODE_ZPAGE_X:
        addr = (addr + cpu_state.x) % 256;
        break;
    case CPU_ADDR_MODE_ZPAGE_Y:
        addr = (addr + cpu_state.y) % 256;
        break;
    case CPU_ADDR_MODE_ABSOLUTE_X:
        addr = addr + cpu_state.x;
        break;
    case CPU_ADDR_MODE_ABSOLUTE_Y:
        addr = addr + cpu_state.y;
        break;
    case CPU_ADDR_MODE_INDIRECT:
        addr = cpu_makeword(memory_read(addr), memory_read(addr+1));
        break;
    case CPU_ADDR_MODE_IND_X:
        addr = (addr + cpu_state.x) % 256;
        addr = cpu_makeword(memory_read(addr), memory_read(addr + 1));

        /* addr = cpu_makeword(memory_read(addr + cpu_state.x), */
        /*                     memory_read(addr + cpu_state.x + 1)); */
        break;
    case CPU_ADDR_MODE_IND_Y:
        addr = cpu_makeword(memory_read(addr),
                            memory_read(addr + 1)) + cpu_state.y;
        INFO("Ind_Y addr: $%04x", addr);
        break;

    default:
        DPRINTF(DBG_FATAL,"Unsupported indexing mode (resolving addr): %d\n", opmap->addressing_mode);
        exit(EXIT_FAILURE);
    }

    if (opinfo->loads) {
        if(opmap->addressing_mode == CPU_ADDR_MODE_IMMEDIATE)
            value = addr;
        else if (opmap->addressing_mode == CPU_ADDR_MODE_ACCUMULATOR)
            value = cpu_state.a;
        else
            value = memory_read(addr);
    }

    /* now we have the real addr, let's do the opcode business */
    switch(opmap->opcode_family) {
    case CPU_OPCODE_SBC:
        /* /\* A - M - C -> A :: N Z C V *\/ */
        if(cpu_state.p & FLAG_D) {
            /* BCD mode */
            ts161 = (cpu_state.a & 0x0f) - (value & 0x0f) + cpu_flag(FLAG_C) - 1;
            if(ts161 < 0)
                ts161 = ((ts161 - 6) & 0x0f) - 0x10;

            ts162 = (cpu_state.a & 0xf0) - (value & 0xf0) + ts161;
            if(ts162 < 0)
                ts162 -= 0x60;

            cpu_state.a = ts162 & 0xff;

            cpu_set_flag(FLAG_C, ts162 >= 0);
            cpu_set_flag(FLAG_N, cpu_state.a & 0x80);
            cpu_set_flag(FLAG_V, ((ts162) < -128) || ((ts162) > 127));
            cpu_set_flag(FLAG_Z, cpu_state.a == 0);
            break;
        } else {
        /*     /\* standard mode *\/ */
        /*     ts161 = cpu_state.a - value - (1 - cpu_flag(FLAG_C)); */
        /*     cpu_state.a = ts161 & 0xff; */

        /*     cpu_set_flag(FLAG_C, ts161 >= 0); */
        /*     cpu_set_flag(FLAG_N, cpu_state.a & 0x80); */
        /*     cpu_set_flag(FLAG_V, ((ts161) < -128) || ((ts161) > 127)); */
        /*     cpu_set_flag(FLAG_Z, cpu_state.a == 0); */
        /* } */
        /* break; /\* SBC *\/ */
        value ^= 0xff;  /* fall-through */
        }

    case CPU_OPCODE_ADC:
        /* A + M + C -> A :: N Z C V */
        if(cpu_state.p & FLAG_D) {
            /* BCD mode */
            t161 = (cpu_state.a & 0x0f) + (value & 0x0f) + cpu_flag(FLAG_C);
            if(t161 >= 0x0a)
                t161 = ((t161 + 6) & 0x0f) + 0x10;

            t162 = (cpu_state.a & 0xf0) + (value & 0xf0) + t161;
            if(t162 >= 0xa0)
                t162 += 0x60;

            cpu_state.a = t162 & 0xff;

            cpu_set_flag(FLAG_C, t162 >= 0x100);
            cpu_set_flag(FLAG_N, t162 & 0x80);
            cpu_set_flag(FLAG_V, (((int16_t)t162) < -128) || (((int16_t)t162) > 127));
            cpu_set_flag(FLAG_Z, cpu_state.a == 0);
        } else {
            /* standard mode */
            t161 = cpu_state.a + value + cpu_flag(FLAG_C);
            t81 = t161 & 0xff;

            cpu_set_flag(FLAG_V, ((cpu_state.a ^ t81) & (value ^ t81)) & 0x80);
            cpu_state.a = t81;

            /* t162 = twos_complement(cpu_state.a) + twos_complement(value) + cpu_flag(FLAG_C); */

            cpu_set_flag(FLAG_C, t161 > 0xff);
            cpu_set_flag(FLAG_N, cpu_state.a & 0x80);
            /* cpu_set_flag(FLAG_V, ((t162) < -128) || ((t162) > 127)); */
            cpu_set_flag(FLAG_Z, cpu_state.a == 0);
        }
        break; /* ADC */

    case CPU_OPCODE_AND:
        /* A AND M -> A :: N Z */
        cpu_state.a = (cpu_state.a & value);
        cpu_set_flag(FLAG_N, cpu_state.a & 0x80);
        cpu_set_flag(FLAG_Z, cpu_state.a == 0);
        break; /* AND */

    case CPU_OPCODE_ASL:
        /* C <- [76543210] <- 0 :: N Z C */
        t161 = value;
        t161 <<= 1;
        value = t161 & 0xff;
        cpu_set_flag(FLAG_N, value & 0x80);
        cpu_set_flag(FLAG_Z, value == 0);
        cpu_set_flag(FLAG_C, t161 & 0x0100);
        break;

    case CPU_OPCODE_BCC:
        if(!cpu_flag(FLAG_C))
            cpu_state.ip = addr;
        break;

    case CPU_OPCODE_BCS:
        if(cpu_flag(FLAG_C))
            cpu_state.ip = addr;
        break;

    case CPU_OPCODE_BEQ:
        if(cpu_flag(FLAG_Z))
            cpu_state.ip = addr;
        break;

    case CPU_OPCODE_BIT:
        /* A AND M, M7 -> N, M6 -> V :: Z */
        t81 = cpu_state.a & value;

        /* only the Z flag is set in immediate mode.  This is
           the only 6502 opcode that affects different flags
           depending on addressing mode */
        if (opmap->addressing_mode != CPU_ADDR_MODE_IMMEDIATE) {
            cpu_set_flag(FLAG_N, value & 0x80);
            cpu_set_flag(FLAG_V, value & 0x40);
        }

        cpu_set_flag(FLAG_Z, t81 == 0);
        break;

    case CPU_OPCODE_BMI:
        if(cpu_flag(FLAG_N))
            cpu_state.ip = addr;
        break;

    case CPU_OPCODE_BNE:
        if(!cpu_flag(FLAG_Z))
            cpu_state.ip = addr;
        break;

    case CPU_OPCODE_BPL:
        if(!cpu_flag(FLAG_N))
            cpu_state.ip = addr;
        break;

    case CPU_OPCODE_BRK:
        cpu_state.ip++;
        cpu_state.irq |= FLAG_IRQ;
        cpu_set_flag(FLAG_B,1);

        cpu_push_16(cpu_state.ip);
        cpu_push_8(cpu_state.p);

        /* as with NMI, disable interrupts in the handler */
        cpu_set_flag(FLAG_I, 1);
        cpu_state.ip = cpu_makeword(memory_read(0xfffe), memory_read(0xffff));
        break;

    case CPU_OPCODE_BVC:
        if(!cpu_flag(FLAG_V))
            cpu_state.ip = addr;
        break;

    case CPU_OPCODE_BVS:
        if(cpu_flag(FLAG_V))
            cpu_state.ip = addr;
        break;

    case CPU_OPCODE_CLC:
        cpu_set_flag(FLAG_C, 0);
        break;

    case CPU_OPCODE_CLD:
        cpu_set_flag(FLAG_D, 0);
        break;

    case CPU_OPCODE_CLI:
        cpu_set_flag(FLAG_I, 0);
        break;

    case CPU_OPCODE_CLV:
        cpu_set_flag(FLAG_V, 0);
        break;

    case CPU_OPCODE_CMP:
        /* A - M :: N Z C */
        t81 = cpu_state.a - value;
        cpu_set_flag(FLAG_Z, cpu_state.a == value);
        cpu_set_flag(FLAG_C, cpu_state.a >= value);
        cpu_set_flag(FLAG_N, t81 & 0x80);
        break;

    case CPU_OPCODE_CPX:
        /* X - M :: N Z C */
        t81 = cpu_state.x - value;
        cpu_set_flag(FLAG_Z, cpu_state.x == value);
        cpu_set_flag(FLAG_C, cpu_state.x >= value);
        cpu_set_flag(FLAG_N, t81 & 0x80);
        break;

    case CPU_OPCODE_CPY:
        /* Y - M :: N Z C */
        t81 = cpu_state.y - value;
        cpu_set_flag(FLAG_Z, cpu_state.y == value);
        cpu_set_flag(FLAG_C, cpu_state.y >= value);
        cpu_set_flag(FLAG_N, t81 & 0x80);
        break;

    case CPU_OPCODE_DEC:
        /* M - 1 -> M :: N Z */
        value--;
        cpu_set_flag(FLAG_Z, value == 0);
        cpu_set_flag(FLAG_N, value & 0x80);
        break;

    case CPU_OPCODE_DEX:
        /* X - 1 -> X :: N Z */
        cpu_state.x--;
        cpu_set_flag(FLAG_Z, cpu_state.x == 0);
        cpu_set_flag(FLAG_N, cpu_state.x & 0x80);
        break;

    case CPU_OPCODE_DEY:
        /* Y - 1 -> Y  :: N Z */
        cpu_state.y--;
        cpu_set_flag(FLAG_Z, cpu_state.y == 0);
        cpu_set_flag(FLAG_N, cpu_state.y & 0x80);
        break;

    case CPU_OPCODE_EOR:
        /* A EOR M -> A :: N Z */
        cpu_state.a = cpu_state.a ^ value;
        cpu_set_flag(FLAG_Z, cpu_state.a == 0);
        cpu_set_flag(FLAG_N, cpu_state.a & 0x80);
        break;

    case CPU_OPCODE_INC:
        /* M + 1 -> M :: N Z */
        value++;
        cpu_set_flag(FLAG_Z, value == 0);
        cpu_set_flag(FLAG_N, value & 0x80);
        break;

    case CPU_OPCODE_INX:
        /* X + 1 -> X :: N Z */
        cpu_state.x++;
        cpu_set_flag(FLAG_Z, cpu_state.x == 0);
        cpu_set_flag(FLAG_N, cpu_state.x & 0x80);
        break;

    case CPU_OPCODE_INY:
        /* Y + 1 -> Y  :: N Z */
        cpu_state.y++;
        cpu_set_flag(FLAG_Z, cpu_state.y == 0);
        cpu_set_flag(FLAG_N, cpu_state.y & 0x80);
        break;

    case CPU_OPCODE_JMP:
        cpu_state.ip = addr;
        break;

    case CPU_OPCODE_JSR:
        cpu_push_16(cpu_state.ip - 1);
        cpu_state.ip = addr;
        break;

    case CPU_OPCODE_LDA:
        /* M -> A :: N Z */
        cpu_state.a = value;
        cpu_set_flag(FLAG_Z, value == 0);
        cpu_set_flag(FLAG_N, value & 0x80);
        break;

    case CPU_OPCODE_LDX:
        /* M -> X :: N Z */
        cpu_state.x = value;
        cpu_set_flag(FLAG_Z, value == 0);
        cpu_set_flag(FLAG_N, value & 0x80);
        break;

    case CPU_OPCODE_LDY:
        /* M -> Y :: N Z */
        cpu_state.y = value;
        cpu_set_flag(FLAG_Z, value == 0);
        cpu_set_flag(FLAG_N, value & 0x80);
        break;

    case CPU_OPCODE_LSR:
        /* 0 -> [76543210] -> C :: Z C */
        cpu_set_flag(FLAG_C, value & 0x01);

        value = value >> 1;
        cpu_set_flag(FLAG_Z, value == 0);
        cpu_set_flag(FLAG_N, value & 0x80);
        break;

    case CPU_OPCODE_NOP:
        /* there should probably be an undoc opcode
           for waiting until an irq, but we'll just
           sleep a bit on a nop, to keep my laptop
           from overheating.  :p */
        rqtp.tv_sec = 0;
        rqtp.tv_nsec = 100000000;  /* .1 sec */
        nanosleep(&rqtp, NULL);
        break;

    case CPU_OPCODE_ORA:
        /* A OR M -> A :: N Z */
        cpu_state.a = cpu_state.a | value;
        cpu_set_flag(FLAG_Z, cpu_state.a == 0);
        cpu_set_flag(FLAG_N, cpu_state.a & 0x80);
        break;

    case CPU_OPCODE_PHA:
        cpu_push_8(cpu_state.a);
        break;

    case CPU_OPCODE_PHP:
        cpu_push_8(cpu_state.p);
        break;

    case CPU_OPCODE_PLA:
        cpu_state.a = cpu_pull_8();
        cpu_set_flag(FLAG_Z, cpu_state.a == 0);
        cpu_set_flag(FLAG_N, cpu_state.a & 0x80);
        break;

    case CPU_OPCODE_PLP:
        /* flag 0x20 is not overwritten by PLP */
        t81 = cpu_pull_8();

        cpu_state.p &= (FLAG_UNUSED | FLAG_B);
        cpu_state.p |= (t81 & ~(FLAG_UNUSED | FLAG_B));
        break;

    case CPU_OPCODE_ROL:
        /* C <- [76543210] <- C :: N Z C */
        t81 = cpu_flag(FLAG_C);
        cpu_set_flag(FLAG_C, value & 0x80);
        value = value << 1;
        value |= t81;
        cpu_set_flag(FLAG_N, value & 0x80);
        cpu_set_flag(FLAG_Z, value == 0);
        break;

    case CPU_OPCODE_ROR:
        /* C -> [76543210] -> C :: N Z C */
        t81 = cpu_flag(FLAG_C);
        cpu_set_flag(FLAG_C, value & 0x01);
        value = value >> 1;
        if(t81)
            value |= 0x80;
        cpu_set_flag(FLAG_N, value & 0x80);
        cpu_set_flag(FLAG_Z, value == 0);
        break;

    case CPU_OPCODE_RTI:
        cpu_state.p = cpu_pull_8();
        cpu_state.ip = cpu_pull_16();
        break;

    case CPU_OPCODE_RTS:
        cpu_state.ip = cpu_pull_16();
        cpu_state.ip++;
        break;

    case CPU_OPCODE_SEC:
        cpu_set_flag(FLAG_C, 1);
        break;

    case CPU_OPCODE_SED:
        cpu_set_flag(FLAG_D, 1);
        break;

    case CPU_OPCODE_SEI:
        cpu_set_flag(FLAG_I, 1);
        break;

    case CPU_OPCODE_STA:
        value = cpu_state.a;
        break;

    case CPU_OPCODE_STX:
        value = cpu_state.x;
        break;

    case CPU_OPCODE_STY:
        value = cpu_state.y;
        break;

    case CPU_OPCODE_TAX:
        /* A -> X :: N Z */
        cpu_state.x = cpu_state.a;
        cpu_set_flag(FLAG_N, cpu_state.x & 0x80);
        cpu_set_flag(FLAG_Z, cpu_state.x == 0);
        break;

    case CPU_OPCODE_TAY:
        /* A -> Y :: N Z */
        cpu_state.y = cpu_state.a;
        cpu_set_flag(FLAG_N, cpu_state.y & 0x80);
        cpu_set_flag(FLAG_Z, cpu_state.y == 0);
        break;

    case CPU_OPCODE_TSX:
        /* SP -> X :: N Z */
        cpu_state.x = cpu_state.sp;
        cpu_set_flag(FLAG_N, cpu_state.x & 0x80);
        cpu_set_flag(FLAG_Z, cpu_state.x == 0);
        break;

    case CPU_OPCODE_TXA:
        /* X -> A :: N Z */
        cpu_state.a = cpu_state.x;
        cpu_set_flag(FLAG_N, cpu_state.x & 0x80);
        cpu_set_flag(FLAG_Z, cpu_state.x == 0);
        break;

    case CPU_OPCODE_TXS:
        /* TXS does not, in fact, set N or Z */
        /* X -> SP :: N Z */
        cpu_state.sp = cpu_state.x;
        /* cpu_set_flag(FLAG_N, cpu_state.x & 0x80); */
        /* cpu_set_flag(FLAG_Z, cpu_state.x == 0); */
        break;

    case CPU_OPCODE_TYA:
        /* Y -> A :: N Z */
        cpu_state.a = cpu_state.y;
        cpu_set_flag(FLAG_N, cpu_state.y & 0x80);
        cpu_set_flag(FLAG_Z, cpu_state.y == 0);
        break;

    default:
        DPRINTF(DBG_FATAL,"Invalid opcode $%02x at $%04x (ish)\n",
                opcode, cpu_state.ip);
        exit(EXIT_FAILURE);
    }

    if (opinfo->stores) {
        switch(opmap->addressing_mode) {
        case CPU_ADDR_MODE_IMPLICIT:
        case CPU_ADDR_MODE_IMMEDIATE:
        case CPU_ADDR_MODE_RELATIVE:
            break;
        case CPU_ADDR_MODE_ACCUMULATOR:
            cpu_state.a = value;
            break;
        default:
            memory_write(addr, value);
            break;
        }
    }


    /*  woo hoo */
    return 1;
}

/*
 * private
 *
 * dump the cpu state
 */
void cpu_dump(char *msg) {

}
