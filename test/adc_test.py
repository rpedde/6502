#!/usr/bin/env python

import emu.py65emu
import emu.rp65emu
import emu.assembler

pyemu = emu.py65emu.PY65Emu()
rpemu = emu.rp65emu.RP65Emu()

for which in range(0, 4):
    for a in range(0, 256):
        for imm in range(0, 256):
            pyemu.pc = 4096
            rpemu.pc = 4096

            start = 4096;

            carry, opcode = [('clc', 'adc'), ('sec', 'adc'),
                             ('clc', 'sbc'), ('sec', 'sbc')][which]

            statements = [ carry,
                           'lda #$%02x' % a,
                           '%s #$%02x' % (opcode, imm) ]

            code = emu.assembler.assemble(statements, start)
            byte_len = len(code);

            pyemu.set_memory(start, code)
            rpemu.set_memory(start, code)

            for x in range(0, len(statements)):
                pyemu.step()
                rpemu.step()

            pyresult = pyemu.a
            rpresult = rpemu.a

            pyflags = pyemu.p & 0xc3
            rpflags = rpemu.p & 0xc3

            label = "$%02x + $%02x" % (a, imm)

            if pyresult == rpresult and pyflags == rpflags:
                pass
                # print "%s:  A=$%02x P=$%02x, A=$%02x/p=$%02x" % (
                #     label, pyresult, pyflags, rpresult, rpflags)
            else:
                print "%s:  expecting A=$%02x P=$%02x, got A=$%02x/p=$%02x" % (
                    label, pyresult, pyflags, rpresult, rpflags)
                sys.exit(1)
