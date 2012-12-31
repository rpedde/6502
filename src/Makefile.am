BUILT_SOURCES = parser.h
AM_YFLAGS = -d

bin_PROGRAMS = emulator compiler stepif

emulator_SOURCES = 6502.c 6502.h debug.c debug.h emulator.c emulator.h hardware.h \
	memory.c memory.h opcodes.h stepwise.c stepwise.h

compiler_SOURCES = compiler.c compiler.h opcodes.h parser.y lexer.l debug.c debug.h

stepif_SOURCES = stepif.c stepif.h libtui.c libtui.h
stepif_LDFLAGS = $(libcurses_LIBS)
