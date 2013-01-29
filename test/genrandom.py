#!/usr/bin/env python

import random

import emu
import emu.opcodes


def get_opcode(cpu_state):
    done = 0
    while(not done):
        opcode = random.randint(0, 255)

        family, undocumented, addressing_mode, cycles, page_of = \
            emu.opcodes.cpu_opcode_map[opcode]

        mnemonic = emu.opcodes.cpu_opcode_mnemonics[family]
        str_addr_mode = emu.opcodes.cpu_addressing_mode[addressing_mode]
        instr_len = emu.opcodes.cpu_addressing_mode_length[addressing_mode]
        (loads, stores, legal) = emu.opcodes.cpu_opcode_info[mnemonic]

        if not legal:
            continue

        if undocumented:
            continue

        if addressing_mode == emu.opcodes.CPU_ADDR_MODE_RELATIVE or \
                addressing_mode == emu.opcodes.CPU_ADDR_MODE_INDIRECT or \
                mnemonic == 'jmp' or mnemonic == 'rti' or mnemonic == 'rts' or \
                mnemonic == 'brk' or mnemonic == 'jsr':
            continue

        break


    instr = [ opcode ]
    store_addr = -1

    if addressing_mode == emu.opcodes.CPU_ADDR_MODE_IMPLICIT or \
            addressing_mode == emu.opcodes.CPU_ADDR_MODE_ACCUMULATOR:
        pass
    elif addressing_mode == emu.opcodes.CPU_ADDR_MODE_IMMEDIATE:
        instr.append(random.randint(0, 255))
    elif addressing_mode == emu.opcodes.CPU_ADDR_MODE_ZPAGE:
        zpage = random.randint(0,255)
        instr.append(zpage)
        if stores:
            store_addr = zpage
    elif addressing_mode == emu.opcodes.CPU_ADDR_MODE_ZPAGE_X:
        zpage = random.randint(0,255)
        instr.append(zpage)
        if stores:
            store_addr = (zpage + cpu_state.x) % 256
    elif addressing_mode == emu.opcodes.CPU_ADDR_MODE_ZPAGE_Y:
        zpage = random.randint(0,255)
        instr.append(zpage)
        if stores:
            store_addr = (zpage + cpu_state.y) % 256
    elif addressing_mode == emu.opcodes.CPU_ADDR_MODE_ABSOLUTE:
        lobyte = random.randint(0,255)
        hibyte = 0x30

        instr.append(lobyte)
        instr.append(hibyte)

        if stores:
            store_addr = hibyte * 256 + lobyte
    elif addressing_mode == emu.opcodes.CPU_ADDR_MODE_ABSOLUTE_X:
        lobyte = 0
        hibyte = 0x30

        instr.append(lobyte)
        instr.append(hibyte)

        if stores:
            store_addr = hibyte * 256 + lobyte + cpu_state.x

    elif addressing_mode == emu.opcodes.CPU_ADDR_MODE_ABSOLUTE_Y:
        lobyte = 0
        hibyte = 0x30

        instr.append(lobyte)
        instr.append(hibyte)

        if stores:
            store_addr = hibyte * 256 + lobyte + cpu_state.y

    elif addressing_mode == emu.opcodes.CPU_ADDR_MODE_IND_X:
        zpage = random.randint(0,255)
        instr.append(zpage)

        if stores:
            store_addr = (zpage + cpu_state.x) % 256
            lobyte, hibyte = cpu_state.get_memory(store_addr, 2)
            store_addr = hibyte * 256 + lobyte

    elif addressing_mode == emu.opcodes.CPU_ADDR_MODE_IND_Y:
        zpage = random.randint(0,255)
        instr.append(zpage)

        if stores:
            lobyte, hibyte = cpu_state.get_memory(zpage, 2)
            store_addr = hibyte * 256 + lobyte + cpu_state.y
    else:
        raise ValueError("whoops")

    descr = "%s (%s): %s" % (mnemonic, str_addr_mode.lower(),
                             map(lambda x: "$%02x" % x, instr))
    return descr, instr, store_addr
