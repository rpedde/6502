BUILT_SOURCES = parser.h
AM_YFLAGS = -d

bin_PROGRAMS = rp65emu rp65asm rp65dbg

rp65emu_SOURCES = 6502.c 6502.h debug.c debug.h emulator.c emulator.h \
         hardware.h memory.c memory.h opcodes.h stepwise.c stepwise.h

rp65emu_LDFLAGS = -lpthread

rp65asm_SOURCES = rp65asm.c rp65asm.h opcodes.h parser.y lexer.l debug.c debug.h

rp65dbg_SOURCES = rp65dbg.c rp65dbg.h libtui.c libtui.h \
        debuginfo.h debuginfo.c redblack.c redblack.h

rp65dbg_LDFLAGS = $(libcurses_LIBS)

SUBDIRS = hardware
