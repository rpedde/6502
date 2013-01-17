pkglibdir=$(libdir)/6502
pkglib_LTLIBRARIES=memory.la uart-16550.la acia-6551.la

memory_la_SOURCES = memory.c hw-common.h hw-common.c ../hardware.h
memory_la_CPPFLAGS = -I..
memory_la_LDFLAGS = -module -avoid-version -shared

uart_16550_la_SOURCES = uart-16550.c uart-16550.h hw-common.h hw-common.c ../hardware.h
uart_16550_la_CPPFLAGS = -I..
uart_16550_la_LDFLAGS = -module -avoid-version -shared -lpthread

acia_6551_la_SOURCES = acia-6551.c acia-6551.h hw-common.h hw-common.c ../hardware.h
acia_6551_la_CPPFLAGS = -I..
acia_6551_la_LDFLAGS = -module -avoid-version -shared -lpthread

EXTRA_DIST = skeleton.c
