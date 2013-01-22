
#ifndef _LIBTUI_H_
#define _LIBTUI_H_

#include <ncurses.h>

typedef struct colorset_t_struct {
    int borderpair;
    int windowpair;
} colorset_t;

#define COLORSET_DEFAULT 0
#define COLORSET_RED     1
#define COLORSET_YELLOW  2
#define COLORSET_INVERT  3
#define COLORSET_BLUE    4
#define COLORSET_GREEN   5

typedef struct window_t_struct {
    WINDOW *pcwindow;
    WINDOW *borderwindow;
    int border;
    char *title;
    int width;
    int height;
    int borderpair;
    int windowpair;
    int fullrefresh;
    void (*on_refresh)(void);
} window_t;

extern window_t *tui_init(char *title, int statusbar);
extern window_t *tui_window(int x, int y, int width, int height, int border, char *title, int colorset);
extern void tui_inputwindow(window_t *pwindow);
extern void tui_refresh(window_t *pwindow);
extern void tui_getstring(window_t *pwindow, char *buffer, int len,
                          void(*callback)(int));
extern void tui_putstring(window_t *pwindow, char *format, ...);
extern void tui_putva(window_t *pwindow, char *format, va_list args);
extern void tui_setpos(window_t *pwindow, int xpos, int ypos);
extern void tui_window_callback(window_t *pwindow, void(*callback)(void));

extern void tui_window_nodelay(window_t *pwindow);
extern void tui_window_delay(window_t *pwindow);
extern int tui_getch(window_t *pwindow);
extern void tui_setcolor(window_t *pwindow, int colorpair);
extern void tui_resetcolor(window_t *pwindow);
extern void tui_cleareol(window_t *pwindow);

#endif /* _LIBTUI_H_ */
