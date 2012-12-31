/*
 *
 */

#ifndef _DEBUG_H_
#define _DEBUG_H_

#define DBG_FATAL 0
#define DBG_ERROR 1
#define DBG_WARN  2
#define DBG_INFO  3
#define DBG_DEBUG 4

#if defined(NDEBUG)
# define DPRINTF(level, format, args...);
#else
# define DPRINTF debug_printf
#endif /* NDEBUG */

#define EPRINTF debug_printf
extern void debug_level(int newlevel);
extern void debug_printf(int level, char *format, ...);

#endif /* _DEBUG_H_ */

