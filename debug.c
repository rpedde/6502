/*
 *
 */

#include "emulator.h"
#include "debug.h"

static int debug_threshold = 2;

void debug_level(int newlevel) {
    debug_threshold = newlevel;
}

void debug_printf(int level, char *format, ...) {
    va_list args;
    if(level > debug_threshold)
        return;

    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
}
