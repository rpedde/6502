/*
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#define NCURSES_OPAQUE 0
#include <curses.h>

#include "emulator.h"
#include "libtui.h"

static int tui_initialized = 0;
static window_t *tui_rootwindow = NULL;
static window_t *tui_statusbar = NULL;
static window_t *tui_titlebar = NULL;
static colorset_t tui_colorlist[] = {
    { 0, 0 },
    { 1, 1 },
    { 2, 2 },
    { 3, 3 },
    { 4, 4 },
    { 5, 5 },
};

void tui_deinit(void);
window_t *tui_makewindow(WINDOW *pcwindow);


window_t *tui_init(char *title, int statusbar) {
    initscr();
    tui_initialized = 1;
    atexit(tui_deinit);

    start_color();

    init_pair(1, COLOR_WHITE, COLOR_RED);
    init_pair(2, COLOR_BLACK, COLOR_YELLOW);
    init_pair(3, COLOR_BLACK, COLOR_WHITE);  /* black on white */
    init_pair(4, COLOR_WHITE, COLOR_BLUE);
    init_pair(5, COLOR_WHITE, COLOR_GREEN);

    tui_rootwindow = tui_makewindow(stdscr);
    tui_rootwindow->width = COLS;
    tui_rootwindow->height = LINES;

    if(title) {
        tui_titlebar = tui_window(0, 0, COLS, 1, 0, title, COLORSET_INVERT);
        tui_refresh(tui_titlebar);
    }

    if(statusbar) {
        tui_statusbar = tui_window(0, LINES-1, COLS, LINES-1, 0, NULL, COLORSET_INVERT);
        tui_refresh(tui_statusbar);
    }


    return tui_rootwindow;
}

void tui_getstring(window_t *pwindow, char *buffer, int len) {
    wgetnstr(pwindow->pcwindow, buffer, len);
}

void tui_setpos(window_t *pwindow, int xpos, int ypos) {
    wmove(pwindow->pcwindow, ypos, xpos);
}

void tui_inputwindow(window_t *pwindow) {
    scrollok(pwindow->pcwindow, 1);
    tui_setpos(pwindow, 0, 0);
}

void tui_putva(window_t *pwindow, char *format, va_list args) {
    vwprintw(pwindow->pcwindow, format, args);
    wrefresh(pwindow->pcwindow);
}


void tui_putstring(window_t *pwindow, char *format, ...) {
    va_list args;

    va_start(args, format);
    vwprintw(pwindow->pcwindow, format, args);
    va_end(args);
    wrefresh(pwindow->pcwindow);
}

void tui_deinit(void) {
    if(tui_initialized)
        endwin();
    tui_initialized = 0;
}

window_t *tui_makewindow(WINDOW *pcwindow) {
    window_t *pwindow;

    pwindow = malloc(sizeof(window_t));
    if(!pwindow) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }

    pwindow->pcwindow = pcwindow;
    return pwindow;
}

void tui_window_callback(window_t *pwindow, void(*callback)(void)) {
    pwindow->on_refresh = callback;
    tui_refresh(pwindow);
}

window_t *tui_window(int x, int y, int width, int height, int border,
                     char *title, int colorset) {
    window_t *pwindow;
    WINDOW *borderwindow;

    if(border) {
        borderwindow = newwin(height, width, y, x);
        height = height - 2;
        width = width - 2;
        pwindow = tui_makewindow(newwin(height, width, y + 1, x + 1));
        pwindow->borderwindow = borderwindow;

        wattron(borderwindow,tui_colorlist[colorset].borderpair);
        werase(borderwindow);
        wattroff(borderwindow,tui_colorlist[colorset].borderpair);
    } else {
        pwindow = tui_makewindow(newwin(height, width, y, x));
        pwindow->borderwindow = NULL;
    }

    pwindow->width = width;
    pwindow->height = height;
    pwindow->title = title;
    pwindow->border = border;
    pwindow->borderpair = tui_colorlist[colorset].borderpair;
    pwindow->windowpair = tui_colorlist[colorset].windowpair;
    wbkgd(pwindow->pcwindow,COLOR_PAIR(pwindow->windowpair));

    pwindow->fullrefresh = 1;
    tui_refresh(pwindow);
    werase(pwindow->pcwindow);

    return pwindow;
}

void tui_refresh(window_t *pwindow) {
    char *buffer;

    if((pwindow->border)  && (pwindow->fullrefresh)){
        wattron(pwindow->borderwindow,COLOR_PAIR(pwindow->borderpair));

        werase(pwindow->borderwindow);

        /* box(pwindow->pcwindow, '|', '|', '-', '-', '+', '+', '+', '+'); */
        box(pwindow->borderwindow, 0, 0);

        if(pwindow->title) {
            asprintf(&buffer,"[ %s ]", pwindow->title);
            mvwprintw(pwindow->borderwindow, 0, ((pwindow->width - strlen(buffer)) / 2) + 1,
                      buffer);
        }

        wattroff(pwindow->borderwindow,COLOR_PAIR(pwindow->borderpair));
        wrefresh(pwindow->borderwindow);
    }

    if(pwindow->fullrefresh)
        werase(pwindow->borderwindow);

    pwindow->fullrefresh=0;

    if(pwindow->on_refresh) {
        pwindow->on_refresh();
    }

    wrefresh(pwindow->pcwindow);
}
