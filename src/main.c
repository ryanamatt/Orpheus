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

/*
 * Usage: orp [file ...]
 *
 * Keybindings
 * -----------
 * Arrow keys / PgUp / PgDn / Home / End  - navigation
 * Ctrl-S   save
 * Ctrl-Q   quit (warns on unsaved changes)
 * Ctrl-F   find  (Enter to cycle, Esc to cancel)
 * Ctrl-R   replace (prompts for search term, replacement, then all/next)
 * Ctrl-G   go to line
 * Ctrl-K   cut line
 * Ctrl-U   paste (yank) line
 * Ctrl-D   delete line
 * Ctrl-A   go to start of line
 * Ctrl-E   go to end of line
 * Ctrl-W   Toggle Hiding/Showing the Status Bar
 * Ctrl-O   Open a new empty buffer
 * Ctrl-N   Switch to next buffer/tab
 * Ctrl-P   Switch to previous buffer/tab
 * 
 * Optional Settings placed in ~/.orpheusrc
 * -----------
 * Works as follows with # as comments:
 * setting=value
 * 
 * tab_width: int 
 *   Sets the number of spaces to tab over
 *  
 * show_line_numbers: int 
 *   If 0, hides the line-number gutter. Any other value shows it. Default: 1.
 * 
 * auto_indent: int
 *   If non-zero, pressing Enter copies the leading whitespace of the current
 *   line to the new line automatically. Default: 1.
 * 
 * show_statusbar: int
 *   If 0, hides the bottom status/command bar, giving one extra row of text.
 *   Default: 1.
 *
 * cursor_style: int
 *   Controls the terminal cursor shape passed to curs_set().
 *     0 = invisible, 1 = normal (default), 2 = very visible / block.
 * 
 * color_scheme: string
 *   Built-in colour theme for the UI chrome.
 *     default  - system default colours (no change)
 *     dark     - white text on dark backgrounds
 *     light    - black text on light backgrounds
 *     mono     - disables colour entirely (A_REVERSE for highlights)
 * 
 * gutter_width: int
 *   The number of spaces for the width of the line number gutter. Default 5.
 * 
 * key_delay: int
 *  The time it takes to wait for escape-sequence processing. Default 50.
 */

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <ncurses.h>
#include "version.h"
#include "config.h"
#include "gap.h"
#include "buffer.h"
#include "fileio.h"
#include "display.h"

// --- Mini Input Line (search / goto) ---

/**
 * @brief Read a single-line string from the user via the command bar.
 *
 * Displays @p prompt on the bottom row and echoes characters as the user types. Backspace removes 
 * the last character. Pressing Enter confirms. Pressing Escape cancels and returns 0.
 *
 * @param prompt Prompt string shown before the input area.
 * @param out Buffer to receive the entered string (NUL-terminated).
 * @param max Size of @p out in bytes, including the NUL terminator.
 * @return 1 if the user confirmed a non-empty string, 0 if cancelled or empty.
 */
static int mini_input(const char *prompt, char *out, int max) {
    int len = 0;
    int input_row = E.rows - 1;
    out[0]  = '\0';
    move(input_row, 0);
    attron(COLOR_PAIR(CP_CMDBAR) | A_BOLD);
    clrtoeol();
    printw(" %s", prompt);
    attroff(A_BOLD);
    refresh();
    curs_set(1);
    int c;
    while ((c = getch()) != '\n' && c != KEY_ENTER) {
        if (c == 27) { attroff(COLOR_PAIR(CP_CMDBAR)); return 0; }
        if ((c == KEY_BACKSPACE || c == 127 || c == '\b') && len > 0) {
            out[--len] = '\0';
        } else if (isprint(c) && len < max - 1) {
            out[len++] = (char)c;
            out[len]   = '\0';
        }
        move(input_row, 0);
        clrtoeol();
        printw(" %s%s", prompt, out);
        refresh();
    }
    attroff(COLOR_PAIR(CP_CMDBAR));
    return len > 0;
}

// --- Search ---

/**
 * @brief Interactive forward search (Ctrl-F).
 *
 * Prompts the user for a search term via mini_input(). If found, moves the cursor to the first 
 * match at or after the current position (wrapping around the end of the buffer) and updates 
 * @c E.search_term for repeat searches.  Sets @c E.status to indicate success or failure.
 */
static void do_find(void) {
    char term[256] = {0};
    if (!mini_input("Find: ", term, sizeof term)) {
        E.status[0] = '\0';
        return;
    }
    strncpy(E.search_term, term, sizeof(E.search_term) - 1);
    E.search_term[sizeof(E.search_term) - 1] = '\0'; // Manually ensure null termination
    E.last_search_pos = E.cursor;

    int len = gap_len(&E.text);
    int tlen = strlen(term);
    int start = (E.cursor + 1) % len;

    for (int i = 0; i < len; i++) {
        int pos = (start + i) % len;
        int match = 1;
        for (int j = 0; j < tlen && match; j++) {
            int p2 = (pos + j) % len;
            if (gap_char(&E.text, p2) != term[j]) match = 0;
        }
        if (match) {
            E.cursor = pos;
            set_status("Found \"%s\"", term);
            E.current_line = pos_to_line(pos);
            return;
        }
    }
    set_status("Not found: \"%s\"", term);
}

// --- Replace ---

/**
 * @brief Interactive find-and-replace (Ctrl-R).
 *
 * Prompts the user for a search term and a replacement string via
 * mini_input(), then asks whether to replace @b All occurrences or only the
 * @b Next occurrence after the cursor. Pressing Escape at any prompt cancels
 * the operation.
 *
 * - "Replace All" scans from position 0 and replaces every match, reporting
 *   the total count on completion.
 * - "Replace Next" replaces the first match at or after the cursor (wrapping
 *   around the buffer).
 *
 * An empty replacement string is valid and deletes each matched substring.
 */
static void do_replace(void) {
    char term[256]    = {0};
    char rep[256]     = {0};

    if (!mini_input("Replace: ", term, sizeof term))
        return;
    if (!mini_input("With: ", rep, sizeof rep)) {
        // empty replacement is valid — it means "delete the match"
        rep[0] = '\0';
    }

    int tlen = strlen(term);
    int rlen = strlen(rep);
    if (tlen == 0) { set_status("Nothing to replace"); return; }

    move(E.rows - 1, 0);
    attron(COLOR_PAIR(CP_CMDBAR) | A_BOLD);
    clrtoeol();
    printw(" Replace [A]ll / [N]ext / Esc to cancel");
    attroff(COLOR_PAIR(CP_CMDBAR) | A_BOLD);
    refresh();
    int choice = getch();
    if (choice == 27) { set_status("Replace cancelled"); return; }

    int total_len = gap_len(&E.text);

    if (choice == 'a' || choice == 'A') {
        // Replace All — scan from position 0, replace each match.
        int count = 0;
        int pos   = 0;
        while (pos <= gap_len(&E.text) - tlen) {
            int match = 1;
            for (int j = 0; j < tlen && match; j++)
                if (gap_char(&E.text, pos + j) != term[j]) match = 0;
            if (match) {
                // delete tlen chars at pos
                for (int j = 0; j < tlen; j++)
                    gap_delete(&E.text, pos);
                // insert replacement at pos 
                for (int j = 0; j < rlen; j++)
                    gap_insert(&E.text, pos + j, rep[j]);
                pos += rlen;   // skip past what we just inserted
                count++;
                E.dirty = 1;
            } else {
                pos++;
            }
        }
        if (count) {
            rebuild_line_count();
            E.current_line = pos_to_line(E.cursor);
            set_status("Replaced %d occurrence%s", count, count == 1 ? "" : "s");
        } else {
            set_status("Not found: \"%s\"", term);
        }
    } else if (choice == 'n' || choice == 'N') {
        // Replace Next — search forward from cursor (wrapping).
        total_len = gap_len(&E.text);
        int start = E.cursor;
        int found = -1;
        for (int i = 0; i < total_len; i++) {
            int pos = (start + i) % total_len;
            if (pos + tlen > total_len) continue;
            int match = 1;
            for (int j = 0; j < tlen && match; j++)
                if (gap_char(&E.text, pos + j) != term[j]) match = 0;
            if (match) { found = pos; break; }
        }
        if (found < 0) {
            set_status("Not found: \"%s\"", term);
            return;
        }
        for (int j = 0; j < tlen; j++)
            gap_delete(&E.text, found);
        for (int j = 0; j < rlen; j++)
            gap_insert(&E.text, found + j, rep[j]);
        E.cursor = found + rlen;
        E.dirty  = 1;
        rebuild_line_count();
        E.current_line = pos_to_line(E.cursor);
        set_status("Replaced \"%s\" with \"%s\"", term, rep);
    } else {
        set_status("Replace cancelled");
    }
}

// --- Cut & Paste ---

/**
 * @brief Cut (copy + delete) the current line into the clipboard (Ctrl-K).
 *
 * Copies the content of @c E.current_line (excluding the newline) into
 * @c E.clipboard, then deletes the line and its trailing newline from the
 * buffer. Rebuilds cached statistics and updates the cursor.
 */
static void cut_line(void) {
    int ln = E.current_line;
    int s = line_start(ln);
    int len = line_len(ln);
    int end = s + len;

    // copy to clipboard
    int clen = 0;
    for (int i = s; i < end; i++)
        if (clen < (int)sizeof(E.clipboard) - 1)
            E.clipboard[clen++] = gap_char(&E.text, i);
    E.clipboard[clen] = '\0';
    E.cb_len = clen;

    // delete line content + newline
    int del = len + (end < gap_len(&E.text) ? 1 : 0);
    for (int i = 0; i < del; i++) gap_delete(&E.text, s);
    E.cursor = s;
    E.dirty  = 1;
    rebuild_line_count();
    E.current_line = pos_to_line(E.cursor);
    set_status("Cut line");
}

/**
 * @brief Paste the clipboard contents as a new line above the current line (Ctrl-U).
 *
 * Inserts the text stored in @c E.clipboard at the start of the current line,
 * followed by a newline character. Does nothing and sets a status message if
 * the clipboard is empty. Rebuilds cached statistics after the insertion.
 */
static void paste_line(void) {
    if (!E.cb_len) { set_status("Clipboard empty"); return; }
    int ln = E.current_line;
    int s  = line_start(ln);
    // insert clipboard + newline
    for (int i = 0; i < E.cb_len; i++) gap_insert(&E.text, s + i, E.clipboard[i]);
    gap_insert(&E.text, s + E.cb_len, '\n');
    E.cursor = s;
    E.dirty  = 1;
    rebuild_line_count();
    E.current_line = pos_to_line(E.cursor);
    set_status("Pasted");
}

/**
 * @brief Delete the current line without copying it to the clipboard (Ctrl-D).
 *
 * Removes all characters on @c E.current_line plus its trailing newline.
 * The cursor is moved to the start of the same line position (now occupied
 * by the following line). Rebuilds cached statistics.
 */
static void delete_line(void) {
    int ln = E.current_line;
    int s = line_start(ln);
    int len = line_len(ln);
    int end = s + len;
    int del = len + (end < gap_len(&E.text) ? 1 : 0);
    for (int i = 0; i < del; i++) gap_delete(&E.text, s);
    E.cursor = s;
    if (E.cursor > gap_len(&E.text)) E.cursor = gap_len(&E.text);
    E.dirty  = 1;
    rebuild_line_count();
    E.current_line = pos_to_line(E.cursor);
    set_status("Deleted line");
}

/**
 * @brief Toggle visibility of the status and command bars (Ctrl-W).
 *
 * Flips @c SHOW_STATUSBAR between 0 and 1, then immediately redraws the
 * screen so the change is visible without waiting for the next keypress.
 * 
 * @param cfg_ptr A pointer to the Config Instance.
 */
static void toggle_status(Config *cfg_ptr) {
    cfg_ptr->show_statusbar = !cfg_ptr->show_statusbar;
    refresh_screen(cfg_ptr);
}

// --- Movement ---

/**
 * @brief Move the cursor one line up, preserving visual column where possible.
 *
 * Attempts to place the cursor at the same visual column on the previous line.
 * If that column exceeds the line's length the cursor is placed at the end of
 * the line. Moves to offset 0 when already on line 0.
 * 
 * @param cfg_ptr A pointer to the Config Instance.
 */
static void move_up(Config *cfg_ptr) {
    int ln = E.current_line;
    if (ln == 0) { E.cursor = 0; E.current_line = 0; return; }
    int vcol = cursor_vcol(cfg_ptr);
    int s    = line_start(ln - 1);
    int l    = line_len(ln - 1);
    E.cursor = s + (vcol < l ? vcol : l);
    E.current_line = ln - 1;
}

/**
 * @brief Move the cursor one line down, preserving visual column where possible.
 *
 * Attempts to place the cursor at the same visual column on the next line.
 * If that column exceeds the line's length the cursor is placed at the end of
 * the line.  Does nothing when already on the last line.
 * 
 * @param cfg_ptr A pointer to the Config Instance.
 */
static void move_down(Config *cfg_ptr) {
    int ln = E.current_line;
    if (ln >= total_lines() - 1) return;
    int vcol = cursor_vcol(cfg_ptr);
    int s    = line_start(ln + 1);
    int l    = line_len(ln + 1);
    E.cursor = s + (vcol < l ? vcol : l);
    E.current_line = ln + 1;
}

/**
 * @brief Move the cursor one character to the left.
 *
 * Decrements @c E.cursor by one and calls update_current_line_delta() to keep
 * @c E.current_line in sync.  Does nothing at the start of the buffer.
 */
static void move_left(void) {
    if (E.cursor > 0) {
        int old = E.cursor;
        E.cursor--;
        update_current_line_delta(old, E.cursor);
    }
}

/**
 * @brief Move the cursor one character to the right.
 *
 * Increments @c E.cursor by one and calls update_current_line_delta() to keep
 * @c E.current_line in sync.  Does nothing at the end of the buffer.
 */
static void move_right(void) {
    if (E.cursor < gap_len(&E.text)) {
        int old = E.cursor;
        E.cursor++;
        update_current_line_delta(old, E.cursor);
    }
}

/**
 * @brief Move the cursor to the first character of the current line (Home).
 */
static void move_line_start(void) {
    int ln = E.current_line;
    E.cursor = line_start(ln);
}

/**
 * @brief Move the cursor past the last character of the current line (End).
 */
static void move_line_end(void) {
    int ln = E.current_line;
    E.cursor = line_start(ln) + line_len(ln);
}

/**
 * @brief Scroll the view up by one full page (Page Up).
 *
 * Calls move_up() once per visible text row, effectively jumping the cursor
 * up by the current viewport height.
 * 
 * @param cfg_ptr A pointer to the Config Instance.
 */
static void move_page_up(Config *cfg_ptr) {
    int text_rows = E.rows - (cfg_ptr->show_statusbar ? 2 : 0);
    for (int i = 0; i < text_rows; i++) move_up(cfg_ptr);
}

/**
 * @brief Scroll the view down by one full page (Page Down).
 *
 * Calls move_down() once per visible text row, effectively jumping the cursor
 * down by the current viewport height.
 * 
 * @param cfg_ptr A pointer to the Config Instance.
 */
static void move_page_down(Config *cfg_ptr) {
    int text_rows = E.rows - (cfg_ptr->show_statusbar ? 2 : 0);
    for (int i = 0; i < text_rows; i++) move_down(cfg_ptr);
}

/**
 * @brief Prompt the user for a line number and jump to it (Ctrl-G).
 *
 * Reads a 1-based line number via mini_input(). The value is clamped to the
 * valid range [1, total_lines()]. The cursor is moved to the start of the
 * target line and @c E.current_line is updated accordingly.
 */
static void goto_line(void) {
    char buf[32];
    if (!mini_input("Go to line: ", buf, sizeof buf)) return;
    int ln = atoi(buf) - 1;
    if (ln < 0) ln = 0;
    int tot = total_lines();
    if (ln >= tot) ln = tot - 1;
    int s = line_start(ln);
    E.cursor = s;
    E.current_line = ln;
    set_status("Jumped to line %d", ln + 1);
}

// --- Quit Confirmation ---

/**
 * @brief Ask the user to confirm quitting when there are unsaved changes.
 *
 * If the active buffer is clean (E.dirty == 0) the function returns 1
 * immediately.  Otherwise it renders an inline prompt and waits for a keypress:
 * - @c Q — quit without saving, returns 1.
 * - @c S — save then quit, returns 1.
 * - Any other key — cancel, returns 0.
 *
 * @return 1 if the editor should exit, 0 if the quit was cancelled.
 */
static int confirm_quit(void) {
    if (!E.dirty) return 1;
    move(E.rows - 1, 0);
    attron(COLOR_PAIR(CP_CMDBAR) | A_BOLD);
    clrtoeol();
    printw(" Unsaved changes! Press Q to quit without saving, S to save, or Esc to cancel.");
    attroff(COLOR_PAIR(CP_CMDBAR) | A_BOLD);
    refresh();
    int c = getch();
    if (c == 'q' || c == 'Q') return 1;
    if (c == 's' || c == 'S') { save_file(); return 1; }
    E.status[0] = '\0';
    return 0;
}

// --- Main Loop ---

/**
 * @brief Parse command-line arguments and set up initial buffers.
 *
 * Handles three cases:
 * - No arguments: opens a single empty unnamed buffer and returns 0.
 * - @c -h / @c --help: prints usage information to stdout and returns 1.
 * - @c -v / @c --version: prints version and build date to stdout and returns 1.
 * - One or more filenames: opens each as its own buffer, loading content from
 *   disk where possible, and returns 0.
 *
 * A return value of 1 signals that the editor should not start (the caller
 * should exit after this function returns).
 *
 * @param argc Argument count from @c main().
 * @param argv Argument vector from @c main().
 * @return 1 if a terminal flag was handled and the editor should not start.
 *         0 if the editor should proceed to its main loop.
 */
int handle_args(int argc, char *argv[]) {
    if (argc < 2) {
        // No file: one empty unnamed buffer
        new_buffer();
        switch_buffer(0);
        set_status("orpheus - no file. Ctrl-S to save, Ctrl-Q to quit.");
        return 0;
    }

    // orp [-h | --help]
    if (argc == 2 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        printf("Orpheus %s\n", ORPHEUS_VERSION);
        printf("Usage: orp [file ...]\n\n");
        printf("Options:\n");
        printf("  -h, --help        Show this help message\n");
        printf("  -v, --version     Show version information\n\n");
        printf("Keybindings:\n");
        printf("Arrow keys          Navigation - move 1 char in direction of arrow\n");
        printf("PgUp/PgDn Home/End  Navigation - top/bottom, front/end of line\n");
        printf(" Ctrl-S             Save\n");
        printf(" Ctrl-Q             Quit (warns on unsaved changes)\n");
        printf(" Ctrl-F             Find  (Enter to cycle, Esc to cancel)\n");
        printf(" Ctrl-R             Replace (search, replacement, then All/Next)\n");
        printf(" Ctrl-G             Go to line\n");
        printf(" Ctrl-K             Cut line\n");
        printf(" Ctrl-U             Paste (yank) line\n");
        printf(" Ctrl-D             Delete line\n");
        printf(" Ctrl-A             Go to start of line\n");
        printf(" Ctrl-E             Go to end of line\n");
        printf(" Ctrl-W             Toggle hiding/showing the status bar\n");
        printf(" Ctrl-O             Open a new empty buffer\n");
        printf(" Ctrl-N             Switch to next buffer/tab\n");
        printf(" Ctrl-P             Switch to previous buffer/tab\n");
        printf("\n");
        return 1;
    }

    // orp [-v | --version]
    if (argc == 2 && (strcmp(argv[1], "-v") == 0 || strcmp(argv[1], "--version") == 0)) {
        printf("Orpheus\n");
        printf("Version: %s\n", ORPHEUS_VERSION);
        printf("Built: %s\n", BUILD_DATE);
        return 1;
    }

    // orp [file ...] — open each file as its own buffer
    for (int i = 1; i < argc; i++) {
        int idx = new_buffer();
        if (idx < 0) {
            fprintf(stderr, "orpheus: too many files (max %d)\n", MAX_BUFFERS);
            break;
        }
        // Temporarily point E at this buffer so load_file / set_status work
        cur_buf = idx;
        E_ptr   = &buffers[idx];
        strncpy(E.filename, argv[i], sizeof(E.filename) - 1);
        E.filename[sizeof(E.filename) - 1] = '\0';
        if (!load_file(E.filename))
            set_status("New file: \"%s\"", E.filename);
        else
            set_status("Opened \"%s\"", E.filename);
    }
    switch_buffer(0);
    return 0;
}

/**
 * @brief Program entry point for the Orpheus text editor.
 *
 * Execution order:
 * 1. load_config() — read @c ~/.orpheusrc settings.
 * 2. handle_args() — process CLI flags/filenames; exit early for @c --help /
 *    @c --version.
 * 3. init_ncurses() — set up the terminal.
 * 4. Main event loop — redraw the screen, read one keypress, dispatch to the
 *    appropriate handler, repeat until confirm_quit() returns true.
 * 5. @c endwin() and Gap buffer cleanup on exit.
 *
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return 0 on normal exit.
 */
int main(int argc, char *argv[]) {
    Config cfg;
    config_defaults(&cfg);
    load_config(&cfg);

    // If handle_args returns 1 it was a flag like --help; exit early.
    if (handle_args(argc, argv)) return 0;

    init_ncurses(&cfg);
    getmaxyx(stdscr, E.rows, E.cols);

    while (1) {
        getmaxyx(stdscr, E.rows, E.cols);
        // Keep terminal size in sync for all buffers
        for (int i = 0; i < buf_count; i++) {
            buffers[i].rows = E.rows;
            buffers[i].cols = E.cols;
        }
        refresh_screen(&cfg);
        E.status[0] = '\0'; // clear status after one frame

        int c = getch();

        switch (c) {
        // Quit
        case ('q' & 0x1f): // Ctrl-Q
            if (confirm_quit()) goto done;
            break;

        // Save
        case ('s' & 0x1f): // Ctrl-S
            if (!E.filename[0]) {
                char buf[256];
                if (mini_input("Save as: ", buf, sizeof buf)) {
                    strncpy(E.filename, buf, sizeof(E.filename));
                    E.search_term[sizeof(E.search_term) - 1] = '\0';
                    save_file();
                }
            } else save_file();
            break;

        // Find
        case ('f' & 0x1f): // Ctrl-F
            do_find();
            break;

        // Replace
        case ('r' & 0x1f): // Ctrl-R
            do_replace();
            break;

        // Go to Line
        case ('g' & 0x1f): // Ctrl-G
            goto_line();
            break;

        // Cut / Paste / Delete
        case ('k' & 0x1f): cut_line();   break;
        case ('u' & 0x1f): paste_line(); break;
        case ('d' & 0x1f): delete_line(); break;

        // Toggle Status
        case ('w' & 0x1f): toggle_status(&cfg); break;

        case ('o' & 0x1f): { // Ctrl-O — new empty buffer
            char fname[256];
            if (mini_input("Open file (blank for new): ", fname, sizeof fname) && fname[0]) {
                int idx = new_buffer();
                if (idx < 0) {
                    set_status("Too many buffers open (max %d)", MAX_BUFFERS);
                } else {
                    switch_buffer(idx);
                    strncpy(E.filename, fname, sizeof(E.filename) - 1);
                    E.filename[sizeof(E.filename) - 1] = '\0';
                    if (!load_file(E.filename))
                        set_status("New file: \"%s\"", E.filename);
                    else
                        set_status("Opened \"%s\"", E.filename);
                }
            }
            else {
                // blank input - open empty unnamed buffer.
                int idx = new_buffer();
                if (idx < 0) {
                    set_status("Too many buffers open (max %d)", MAX_BUFFERS);
                } else {
                    switch_buffer(idx);
                    set_status("New buffer %d/%d  [No Name]", cur_buf + 1, buf_count);
                }
            break;
            }
        }

        case ('n' & 0x1f): // Ctrl-N — next buffer
            if (buf_count > 1) {
                switch_buffer(cur_buf + 1);
                set_status("Buffer %d/%d: %s",
                           cur_buf + 1, buf_count,
                           E.filename[0] ? E.filename : "[No Name]");
            } else {
                set_status("Only one buffer open");
            }
            break;

        case ('p' & 0x1f): // Ctrl-P — previous buffer
            if (buf_count > 1) {
                switch_buffer(cur_buf - 1);
                set_status("Buffer %d/%d: %s",
                           cur_buf + 1, buf_count,
                           E.filename[0] ? E.filename : "[No Name]");
            } else {
                set_status("Only one buffer open");
            }
            break;

        // Movement
        case KEY_UP:         move_up(&cfg);        break;
        case KEY_DOWN:       move_down(&cfg);      break;
        case KEY_LEFT:       move_left();      break;
        case KEY_RIGHT:      move_right();     break;
        case KEY_HOME:
        case ('a' & 0x1f):   move_line_start(); break;
        case KEY_END:
        case ('e' & 0x1f):   move_line_end();   break;
        case KEY_PPAGE:      move_page_up(&cfg);     break;
        case KEY_NPAGE:      move_page_down(&cfg);   break;

        // Editing
        case KEY_BACKSPACE:
        case 127:
        case '\b':
            if (E.cursor > 0) {
                char deleted = gap_char(&E.text, E.cursor - 1);
                if (E.text.gap_start == E.cursor)
                    gap_shift_left(&E.text);
                else
                    gap_delete(&E.text, E.cursor - 1);
                int old = E.cursor;
                E.cursor--;
                update_current_line_delta(old, E.cursor);
                update_stats(deleted, -1);
                E.dirty = 1;
            }
            break;

        case KEY_DC: // Delete Key
            if (E.cursor < gap_len(&E.text)) {
                gap_delete(&E.text, E.cursor);
                E.dirty = 1;
            }
            break;

        case '\t':
            for (int i = 0; i < cfg.tab_width; i++) {
                gap_insert(&E.text, E.cursor, ' ');
                E.cursor++;
                update_stats(' ', +1);
            }
            E.dirty = 1;
            break;

        case '\n':
        case KEY_ENTER:
            gap_insert(&E.text, E.cursor, '\n');
            E.cursor++;
            update_stats('\n', +1);
            E.current_line++;

            if (cfg.auto_indent) {
                int ln = E.current_line - 1;
                int s = line_start(ln);
                int end = s + line_len(ln);
                int ws = s;
                while (ws < end && (gap_char(&E.text, ws) == ' ' || gap_char(&E.text, ws) == '\t'))
                    ws++;
                int indent = ws - s;
                for (int i = 0; i < indent; i++) {
                    char wc = gap_char(&E.text, s + i);
                    gap_insert(&E.text, E.cursor, wc);
                    E.cursor++;
                    update_stats(wc, +1);
                }
            }
            E.dirty = 1;
            break;

        default:
            if (c >= 32 && c < 256 && c != 127) {
                gap_insert(&E.text, E.cursor, (char)c);
                E.cursor++;
                update_stats((char)c, +1);
                E.dirty = 1;
            }
            break;
        }
    }

done:
    endwin();
    for (int i = 0; i < buf_count; i++)
        gap_free(&buffers[i].text);
    return 0;
}