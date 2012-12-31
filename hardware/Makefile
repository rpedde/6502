HARDWARE_SOURCES=memory.c
HARDWARE_OBJS=$(HARDWARE_SOURCES:.c=.so)
SHARED_DEPS=hw-common.c hw-common.h
SHARED_SOURCES=hw-common.c

all: $(HARDWARE_OBJS)

%.so: %.c $(SHARED_DEPS) $($@:.c=.h)
	$(CC) -rdynamic -shared $(CFLAGS) -o $@ $< $(SHARED_SOURCES)

clean:
	rm -f *.o
	rm -f *.so
	rm -rf *.so.dSYM
