/*
 * tests/stubs/ncurses.h
 *
 * Minimal ncurses stub for the test build.
 *
 * input.c includes <ncurses.h> at the top. When compiled for tests we
 * redirect that include here via -I tests/stubs (which takes priority over
 * the system include path). This file provides just enough declarations and
 * no-op macros to satisfy the compiler.
 *
 * The ncurses-coupled functions (main_input, mini_input, do_find, do_replace,
 * confirm_quit, handle_mouse) are never called by the test suite, so their
 * bodies can reference these stubs safely without any runtime effect.
 *
 * Functions that ARE tested (cut_line, paste_line, delete_line, move_*)
 * contain no ncurses calls, so they compile and run cleanly against this
 * stub regardless.
 */

#ifndef NCURSES_STUB_H
#define NCURSES_STUB_H

#include <stdio.h>   /* for FILE */

/* --- Types --- */
typedef unsigned long chtype;
typedef unsigned long mmask_t;

typedef struct {
    int bstate;
    int x, y, z;
} MEVENT;

/* --- Constants --- */
#define OK          0
#define ERR         (-1)
#define TRUE        1
#define FALSE       0

/* Colour constants */
#define COLOR_BLACK   0
#define COLOR_RED     1
#define COLOR_GREEN   2
#define COLOR_YELLOW  3
#define COLOR_BLUE    4
#define COLOR_MAGENTA 5
#define COLOR_CYAN    6
#define COLOR_WHITE   7

/* Attribute bits */
#define A_NORMAL    0UL
#define A_BOLD      (1UL << 13)
#define A_REVERSE   (1UL << 14)

/* Key codes */
#define KEY_UP        259
#define KEY_DOWN      258
#define KEY_LEFT      260
#define KEY_RIGHT     261
#define KEY_HOME      262
#define KEY_END       360
#define KEY_PPAGE     339
#define KEY_NPAGE     338
#define KEY_BACKSPACE 263
#define KEY_DC        330
#define KEY_ENTER     343
#define KEY_MOUSE     409

/* Mouse button masks */
#define BUTTON1_PRESSED   0x0002L
#define BUTTON4_PRESSED   0x0080L
#define BUTTON5_PRESSED   0x0100L  /* not standard but harmless */

/* Dummy window pointer type */
typedef void WINDOW;
extern WINDOW *stdscr;

/* --- No-op inline stubs for every ncurses function used in input.c --- */

static inline int  getch(void)                         { return ERR; }
static inline int  wgetch(WINDOW *w)                   { (void)w; return ERR; }
static inline int  move(int y, int x)                  { (void)y; (void)x; return OK; }
static inline int  mvprintw(int y, int x, const char *fmt, ...) {
    (void)y; (void)x; (void)fmt; return OK;
}
static inline int  printw(const char *fmt, ...)        { (void)fmt; return OK; }
static inline int  addch(chtype c)                     { (void)c; return OK; }
static inline int  clrtoeol(void)                      { return OK; }
static inline int  refresh(void)                       { return OK; }
static inline int  wnoutrefresh(WINDOW *w)             { (void)w; return OK; }
static inline int  doupdate(void)                      { return OK; }
static inline int  attron(chtype a)                    { (void)a; return OK; }
static inline int  attroff(chtype a)                   { (void)a; return OK; }
static inline int  curs_set(int v)                     { (void)v; return OK; }
static inline int  has_colors(void)                    { return FALSE; }
static inline int  start_color(void)                   { return OK; }
static inline int  use_default_colors(void)            { return OK; }
static inline int  init_pair(short p, short f, short b){ (void)p;(void)f;(void)b; return OK; }
static inline chtype COLOR_PAIR(int n)                 { (void)n; return 0; }
static inline int  initscr(void)                       { return OK; }
static inline int  endwin(void)                        { return OK; }
static inline int  raw(void)                           { return OK; }
static inline int  noecho(void)                        { return OK; }
static inline int  keypad(WINDOW *w, int bf)           { (void)w;(void)bf; return OK; }
static inline int  set_escdelay(int t)                 { (void)t; return OK; }
static inline int  mousemask(mmask_t m, mmask_t *o)   { (void)m;(void)o; return OK; }
static inline int  mouseinterval(int t)                { (void)t; return OK; }
static inline int  getmouse(MEVENT *e)                 { (void)e; return ERR; }

/* getmaxyx is a macro in real ncurses */
#define getmaxyx(win, y, x) do { (void)(win); (y) = 24; (x) = 80; } while(0)

/*
 * display.c functions referenced by input.c.
 * Providing them here avoids linking display.c (which has real ncurses calls).
 * The test suite never exercises these paths; the no-ops are safe.
 */
#include "config.h"   /* Config */
#include "buffer.h"   /* EditorContext */

static inline void toggle_status(Config *c, EditorContext *e) { (void)c; (void)e; }
static inline void refresh_screen(Config *c, EditorContext *e) { (void)c; (void)e; }

#endif /* NCURSES_STUB_H */