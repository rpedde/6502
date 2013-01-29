#!/usr/bin/env python

import sys
import emu.py65emu
import emu.rp65emu
import emu.assembler

import genrandom

pyemu = emu.py65emu.PY65Emu()
rpemu = emu.rp65emu.RP65Emu()

instr_count = 0


def get_state(emulator, write_addr=-1):
    state = {}

    for key in ['x', 'y', 'a', 'pc', 'sp']:
        state[key] = "$%04x" % getattr(emulator, key)

    if write_addr != -1:
        state['addr'] = emulator.get_memory(write_addr, 1)


    flags = emulator.p & 0xc3

    flag_str = ""
    if flags & 0x80:
        flag_str += "N"
    if flags & 0x40:
        flag_str += "V"
    if flags & 0x10:
        flag_str += "B"
    if flags & 0x08:
        flag_str += "D"
    if flags & 0x04:
        flag_str += "I"
    if flags & 0x02:
        flag_str += "Z"
    if flags & 0x01:
        flag_str += "C"

    state['p'] = "$%02x [%s]" % (flags, flag_str)

    return state

def compare_state(instr, pystate, rpstate):
    if pystate != rpstate:
        print "\n\nError:  Instruction: %s" % instr

        print "%12s %25s %25s" % ('value', 'pyemu', 'rpemu')

        for k in pystate.keys():
            print "%12s %25s %25s" % (k, "%25s" % pystate[k],
                                      "%25s" % rpstate[k])

        print "\n\n%s" % opcode_list

        sys.exit(1)

def execute_code(code, location):
    opcodes = emu.assembler.assemble(code, location)
    pyemu.set_memory(location, opcodes)
    rpemu.set_memory(location, opcodes)

    rpemu.pc = location
    pyemu.pc = location

    for x in range(0, len(code)):
        pyemu.step()
        rpemu.step()

setup_prologue = ['clc',
                  'cli',
                  'cld',
                  'clv',
                  'lda #$00',
                  'pha',
                  'plp',
                  'ldx #$ff',
                  'txs',
                  'lda #$00',
                  'ldx #$00',
                  'ldy #$00']

execute_code(setup_prologue, 8192)

print "resetting memory"
for x in range(0, 64):
    pyemu.set_memory(x * 1024, [0] * 1024)
    rpemu.set_memory(x * 1024, [0] * 1024)

pyemu.pc = 8192
rpemu.pc = 8192

pystate = get_state(pyemu)
rpstate = get_state(rpemu)

opcode_list = []

if pystate != rpstate:
    print "Initial state mismatch!"
    compare_state("startup", pystate, rpstate)
    sys.exit(1)



while(1):
    instr_count += 1

    descr, instr, store_addr = genrandom.get_opcode(pyemu)

    pyemu.set_memory(8192, instr)
    rpemu.set_memory(8192, instr)

    (byte_len, real_instr) = pyemu.disassemble(8192)
    opcode_list.append(real_instr)

    if(byte_len != len(instr)):
        print "%s does not match %s" % (descr, real_instr)
        sys.exit(1)

    pyemu.pc = 8192
    rpemu.pc = 8192

    pyemu.step()
    rpemu.step()

    pystate = get_state(pyemu)
    rpstate = get_state(rpemu)

    compare_state("%s (%s)" % (descr, real_instr), pystate, rpstate)

    print '\rInstructions: %s' % (instr_count,),
