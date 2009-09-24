CFLAGS:=-Wall -pedantic -g -O0 -std=c99

EMULATOR_SOURCES=emulator.c memory.c 6502.c debug.c stepwise.c
EMULATOR_HEADERS=emulator.h memory.h 6502.h debug.h opcodes.h stepwise.h
EMULATOR_OBJS=$(EMULATOR_SOURCES:.c=.o)

COMPILER_SOURCES=compiler.c parser.c lexer.c debug.c
COMPILER_OBJS=$(COMPILER_SOURCES:.c=.o)

STEPIF_SOURCES=stepif.c libtui.c
STEPIF_OBJS=$(STEPIF_SOURCES:.c=.o)

all: emulator compiler stepif

emulator: $(EMULATOR_OBJS) $(EMULATOR_HEADERS)
	$(CC) -o emulator $(LDFLAGS) $(EMULATOR_OBJS)

.c.o:
	$(CC) -c $(CFLAGS) $<

clean:
	rm -f emulator stepif
	rm -f compiler lexer.c parser.c
	rm -f $(STEPIF_OBJS)
	rm -f $(EMULATOR_OBJS)
	rm -f $(COMPILER_OBJS)

parser.c: parser.y opcodes.h
	yacc -d -o parser.c parser.y

lexer.c: lexer.l opcodes.h
	flex -olexer.c lexer.l

compiler: $(COMPILER_OBJS)
	$(CC) -o compiler $(LDFLAGS) $(COMPILER_OBJS)

stepif: $(STEPIF_OBJS)
	$(CC) -o stepif $(LDFLAGS) $(STEPIF_OBJS) -lncurses