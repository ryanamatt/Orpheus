/*
 * Orpheus - a small ncurses text editor
 *
 * Copyright (C) 2026 Ryan Mattson
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <string.h>
#include <ncurses.h>
#include "buffer.h"
#include "display.h"

/**
 * @brief Initialise the ncurses library and apply colour scheme settings.
 *
 * Calls @c initscr(), enables raw input, disables echo, enables the keypad,
 * and sets the escape-sequence delay from @c KEY_DELAY. When the terminal
 * supports colour and the scheme is not @c "mono", initialises five colour
 * pairs (normal text, status bar, command bar, line numbers, search highlight)
 * according to the @c Ccfg_ptr->OLOR_SCHEME global:
 * - @c "dark"  — white on black palette.
 * - @c "light" — black on white palette.
 * - @c "default" — inherits the terminal's own colours.
 * 
 * @param cfg_ptr A pointer to the Config Instance.
 *
 * Finally sets the cursor shape via @c curs_set(CURSOR_STYLE).
 */
void init_ncurses(Config *cfg_ptr) {
    initscr();
    raw();
    noecho();
    keypad(stdscr, TRUE);
    set_escdelay(cfg_ptr->key_delay);

    if (has_colors() && strcmp(cfg_ptr->color_scheme, "mono") != 0) {
        start_color();
        use_default_colors();
 
        if (strcmp(cfg_ptr->color_scheme, "dark") == 0) {
            init_pair(CP_NORMAL, COLOR_WHITE,  COLOR_BLACK);
            init_pair(CP_STATUS, COLOR_BLACK,  COLOR_WHITE);
            init_pair(CP_CMDBAR, COLOR_BLACK,  COLOR_CYAN);
            init_pair(CP_LNUM,   COLOR_YELLOW, COLOR_BLACK);
            init_pair(CP_SEARCH, COLOR_BLACK,  COLOR_YELLOW);
        } else if (strcmp(cfg_ptr->color_scheme, "light") == 0) {
            init_pair(CP_NORMAL, COLOR_BLACK,  COLOR_WHITE);
            init_pair(CP_STATUS, COLOR_WHITE,  COLOR_BLUE);
            init_pair(CP_CMDBAR, COLOR_WHITE,  COLOR_BLUE);
            init_pair(CP_LNUM,   COLOR_BLUE,   COLOR_WHITE);
            init_pair(CP_SEARCH, COLOR_WHITE,  COLOR_RED);
        } else {
            // default: use terminal's own colours
            init_pair(CP_NORMAL, -1,           -1);
            init_pair(CP_STATUS, -1,           -1);
            init_pair(CP_CMDBAR, -1,           -1);
            init_pair(CP_LNUM,   COLOR_YELLOW, -1);
            init_pair(CP_SEARCH, COLOR_BLACK,  COLOR_YELLOW);
        }
    }

    curs_set(cfg_ptr->cursor_style);

    // Enable mouse: click-to-position and scroll wheel
    mousemask(BUTTON1_PRESSED | BUTTON4_PRESSED | BUTTON5_PRESSED, NULL);
    mouseinterval(0);  // no click interval - report press immediately
}

/**
 * @brief Adjust the viewport offsets so the cursor remainds visible.
 * 
 * Update @c edcon->buffer->row_off and @c edcon->buffer->col_off to ensure the current line and 
 * visual column are within the displayed text area. Accounts for the tab bar row when multiple 
 * buffers are open and for the status bar rows when @c cfg_ptr->SHOW_STATUS_BAR is enabled.
 * 
 * @param cfg_ptr A pointer to the Config Instance.
 * @param edcon The EditorContext Instance.
 */
void adjust_scroll(Config *cfg_ptr, EditorContext *edcon) {
    // Extra row used by the tab bar when more than one buffer is open
    int tab_rows = (edcon->buf_count > 1) ? 1 : 0;
    int cur_line = edcon->buffer->current_line;
    int vcol = cursor_vcol(cfg_ptr, edcon);
    int text_rows = edcon->buffer->rows - (cfg_ptr->show_statusbar ? 2 : 0) - tab_rows;   // status + command bar

    if (cur_line < edcon->buffer->row_off)
        edcon->buffer->row_off = cur_line;
    if (cur_line >= edcon->buffer->row_off + text_rows)
        edcon->buffer->row_off = cur_line - text_rows + 1;
    if (vcol < edcon->buffer->col_off)
        edcon->buffer->col_off = vcol;
    if (vcol >= edcon->buffer->col_off + edcon->buffer->cols - 6)
        edcon->buffer->col_off = vcol - (edcon->buffer->cols - 6) + 1;
}

/**
 * @brief Render the visible text rows, including the line-number gutter.
 *
 * Iterates over each visible row, drawing the line-number gutter (respecting @c cfg_ptr->SHOW_LINE_NUMBERS 
 * and @c cfg_ptr-> GUTTER_WIDTH) followed by the line's characters with horizontal scrolling and tab 
 * expansion applied. Rows beyond the lastline display a @c ~ sentinel, matching traditional 
 * text-editor conventions.
 * 
 * @param cfg_ptr A pointer to the Config Instance.
 * @param edcon The EditorContext Instance.
 */
void draw_rows(Config *cfg_ptr, EditorContext *edcon) {
    int total = total_lines(edcon);
    int tab_rows = (edcon->buf_count > 1) ? 1 : 0;
    int text_rows = edcon->buffer->rows - (cfg_ptr->show_statusbar ? 2 : 0) - tab_rows;

    char fmt[16];
    snprintf(fmt, sizeof(fmt), "%%%dd ", cfg_ptr->gutter_width - 1);

    for (int y = 0; y < text_rows; y++) {
        int ln = y + edcon->buffer->row_off;
        move(y, 0);

        // line number gutter
        attron(COLOR_PAIR(CP_LNUM));
        if ((ln < total) && cfg_ptr->show_line_numbers) {
            printw(fmt, ln + 1);
        } else {
            for(int i=0; i < cfg_ptr->gutter_width - 1; i++) addch(' ');
            addch('~');
            addch(' ');
        }
        attroff(COLOR_PAIR(CP_LNUM));

        attron(COLOR_PAIR(CP_NORMAL));
        if (ln < total) {
            int s = line_start(edcon, ln);
            int end = s + line_len(edcon, ln);
            int col = 0;
            for (int i = s; i < end; i++) {
                char c = gap_char(&edcon->buffer->text, i);
                if (c == '\t') {
                    int next = (col / cfg_ptr->tab_width + 1) * cfg_ptr->tab_width;
                    while (col < next && col - edcon->buffer->col_off < edcon->buffer->cols - 5) {
                        if (col >= edcon->buffer->col_off) addch(' ');
                        col++;
                    }
                } else {
                    if (col >= edcon->buffer->col_off 
                            && col - edcon->buffer->col_off < edcon->buffer->cols - 5)
                        addch((unsigned char)c);
                    col++;
                }
            }
        }
        clrtoeol();
        attroff(COLOR_PAIR(CP_NORMAL));
    }
}

/**
 * @brief Render the buffer tab bar when more than one buffer is open.
 *
 * Draws a row of abbreviated buffer names above the status bar.  The active buffer's tab is 
 * highlighted with bold+reverse video. Modified buffers show a @c + suffix. The function is a 
 * no-op when only one buffer is open.
 * 
 * @param cfg_ptr A pointer to the Config Instance.
 * @param edcon The EditorContext Instance.
 */
void draw_tabbar(Config *cfg_ptr, EditorContext *edcon) {
    // Tab bar sits at E.rows - 3 when statusbar is shown, else E.rows - 1.
    // We only draw it when there is more than one buffer open.
    if (edcon->buf_count <= 1) return;

    int row = edcon->buffer->rows - (cfg_ptr->show_statusbar ? 3 : 1);
    attron(COLOR_PAIR(CP_STATUS));
    move(row, 0);
    clrtoeol();

    int x = 0;
    for (int i = 0; i < edcon->buf_count && x < edcon->buffer->cols - 1; i++) {
        const char *name = edcon->buffers[i].filename[0]
                           ? edcon->buffers[i].filename : "[No Name]";
        // Strip directory prefix for display
        const char *base = strrchr(name, '/');
        base = base ? base + 1 : name;

        char tab[64];
        int tlen = snprintf(tab, sizeof tab, " %s%s ",
                            base,
                            edcon->buffers[i].dirty ? "+" : "");

        if (i == edcon->cur_buf)
            attron(A_BOLD | A_REVERSE);
        if (x + tlen < edcon->buffer->cols)
            mvprintw(row, x, "%s", tab);
        if (i == edcon->cur_buf)
            attroff(A_BOLD | A_REVERSE);

        x += tlen;
    }
    attroff(COLOR_PAIR(CP_STATUS));
}

/**
 * @brief Render the status bar showing file info and cursor statistics.
 *
 * Displays the filename (or @c [No Name]), a dirty indicator, and on the
 * right side: line number, column, total lines, word count, and character
 * count.  Word count is lazily refreshed via count_words().
 * 
 * @param cfg_ptr A pointer to the Config Instance.
 * @param edcon The EditorContext Instance.
 */
void draw_statusbar(Config *cfg_ptr, EditorContext *edcon) {
    attron(COLOR_PAIR(CP_STATUS) | A_BOLD);
    move(edcon->buffer->rows - 2, 0);
    int ln = edcon->buffer->current_line;
    int col = cursor_vcol(cfg_ptr, edcon);

    int chars = count_chars(edcon);
    int words = count_words(edcon);

    char left[128], right[128];

    snprintf(left,  sizeof left,  " %.40s%s",
             edcon->buffer->filename[0] ? edcon->buffer->filename : "[No Name]",
             edcon->buffer->dirty ? " [+]" : "");

    snprintf(right, sizeof right, "Ln %d, Col %d | %d lines | %d words %d chars ",
             ln + 1, col + 1, total_lines(edcon), words, chars);

    int pad = edcon->buffer->cols - (int)strlen(left) - (int)strlen(right);
    printw("%s", left);
    for (int i = 0; i < pad && i < edcon->buffer->cols; i++) addch(' ');
    printw("%s", right);
    attroff(COLOR_PAIR(CP_STATUS) | A_BOLD);
}

/**
 * @brief Render the command bar with keybinding hints and the status message.
 *
 * Draws the fixed keybinding cheatsheet on the bottom row. If @c edcon->buffer->status is 
 * non-empty it is  overlaid right-aligned on the same row in bold, providing one-shot feedback 
 * to the user.
 * 
 * @param edcon The EditorContext Instance.
 */
void draw_cmdbar(EditorContext *edcon) {
    attron(COLOR_PAIR(CP_CMDBAR));
    move(edcon->buffer->rows - 1, 0);
    printw(" ^S Save  ^Q Quit  ^F Find  ^R Repl  ^G Go-To  ^K Cut  ^U Paste  ^D Del-Ln  ^W Hide  ^N Next  ^P Prev");
    clrtoeol();
    attroff(COLOR_PAIR(CP_CMDBAR));

    // show one-shot status message on right side
    if (edcon->buffer->status[0]) {
        int slen = strlen(edcon->buffer->status);
        int x = edcon->buffer->cols - slen - 2;
        if (x < 0) x = 0;
        attron(COLOR_PAIR(CP_CMDBAR) | A_BOLD);
        mvprintw(edcon->buffer->rows - 1, x, " %s ", edcon->buffer->status);
        attroff(COLOR_PAIR(CP_CMDBAR) | A_BOLD);
    }
}

/**
 * @brief Redraw the entire terminal display for the current frame.
 *
 * Calls adjust_scroll(), then draws text rows, the optional tab bar, status bar, and command bar.
 * Finally positions the terminal cursor at the correct visual cell and flushes the update to 
 * the screen via @c doupdate().
 * 
 * @param cfg_ptr A pointer to the Config Instance.
 * @param edcon The EditorContext Instance.
 */
void refresh_screen(Config *cfg_ptr, EditorContext *edcon) {
    adjust_scroll(cfg_ptr, edcon);
    int ln = edcon->buffer->current_line;
    int vcol = cursor_vcol(cfg_ptr, edcon);

    draw_rows(cfg_ptr, edcon);
    if (cfg_ptr->show_statusbar) {
        draw_tabbar(cfg_ptr, edcon);
        draw_statusbar(cfg_ptr, edcon);
        draw_cmdbar(edcon);
    } else {
        draw_tabbar(cfg_ptr, edcon);
    }

    // position real cursor (tab bar does not shift text rows — it sits below)
    move(ln - edcon->buffer->row_off, cfg_ptr->gutter_width + vcol - edcon->buffer->col_off);
    wnoutrefresh(stdscr);
    doupdate();
}

/**
 * @brief Toggle visibility of the status and command bars (Ctrl-W).
 *
 * Flips @c SHOW_STATUSBAR between 0 and 1, then immediately redraws the
 * screen so the change is visible without waiting for the next keypress.
 * 
 * @param cfg_ptr A pointer to the Config Instance.
 * @param edcon The EditorContext Instance.
 */
void toggle_status(Config *cfg_ptr, EditorContext *edcon) {
    cfg_ptr->show_statusbar = !cfg_ptr->show_statusbar;
    refresh_screen(cfg_ptr, edcon);
}