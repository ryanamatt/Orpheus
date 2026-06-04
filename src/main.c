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
#include <errno.h>
#include <ncurses.h>
#include "version.h"
#include "config.h"
#include "gap.h"
#include "buffer.h"
#include "fileio.h"
#include "display.h"
#include "input.h"

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
 * @param edcon The EditorContext Instance.
 * @param argc Argument count from @c main().
 * @param argv Argument vector from @c main().
 * @return 1 if a terminal flag was handled and the editor should not start.
 *         0 if the editor should proceed to its main loop.
 */
int handle_args(EditorContext *edcon, int argc, char *argv[]) {
    if (argc < 2) {
        // No file: one empty unnamed buffer
        new_buffer(edcon);
        switch_buffer(edcon, 0);
        set_status(edcon, "orpheus - no file. Ctrl-S to save, Ctrl-Q to quit.");
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
        int idx = new_buffer(edcon);
        if (idx < 0) {
            fprintf(stderr, "orpheus: too many files (max %d)\n", MAX_BUFFERS);
            break;
        }
        // Temporarily point E at this buffer so load_file / set_status work
        edcon->cur_buf = idx;
        edcon->buffer = &edcon->buffers[idx];
        strncpy(edcon->buffer->filename, argv[i], sizeof(edcon->buffer->filename) - 1);
        edcon->buffer->filename[sizeof(edcon->buffer->filename) - 1] = '\0';
        if (!load_file(edcon))
            set_status(edcon, "New file: \"%s\"", edcon->buffer->filename);
        else
            set_status(edcon, "Opened \"%s\"", edcon->buffer->filename);
    }
    switch_buffer(edcon, 0);
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
    EditorContext edcon_ctx = {0};
    EditorContext *edcon = &edcon_ctx;

    Config cfg;
    config_defaults(&cfg);
    load_config(&cfg);

    // If handle_args returns 1 it was a flag like --help; exit early.
    if (handle_args(edcon, argc, argv)) return 0;

    init_ncurses(&cfg);
    getmaxyx(stdscr, edcon->buffer->rows, edcon->buffer->cols);

    while (1) {
        getmaxyx(stdscr, edcon->buffer->rows, edcon->buffer->cols);
        // Keep terminal size in sync for all buffers
        for (int i = 0; i < edcon->buf_count; i++) {
            edcon->buffers[i].rows = edcon->buffer->rows;
            edcon->buffers[i].cols = edcon->buffer->cols;
        }
        refresh_screen(&cfg, edcon);
        edcon->buffer->status[0] = '\0'; // clear status after one frame

        int c = getch();

        switch (c) {
        // Quit
        case ('q' & 0x1f): // Ctrl-Q
            if (confirm_quit(edcon)) goto done;
            break;

        // Save
        case ('s' & 0x1f): // Ctrl-S
            if (!edcon->buffer->filename[0]) {
                char buf[256];
                if (mini_input(edcon, "Save as: ", buf, sizeof buf)) {
                    strncpy(edcon->buffer->filename, buf, sizeof(edcon->buffer->filename));
                    edcon->buffer->search_term[sizeof(edcon->buffer->search_term) - 1] = '\0';
                    save_file(edcon);
                }
            } else save_file(edcon);
            break;

        // Find
        case ('f' & 0x1f): // Ctrl-F
            do_find(edcon);
            break;

        // Replace
        case ('r' & 0x1f): // Ctrl-R
            do_replace(edcon);
            break;

        // Go to Line
        case ('g' & 0x1f): // Ctrl-G
            goto_line(edcon);
            break;

        // Cut / Paste / Delete
        case ('k' & 0x1f): cut_line(edcon);   break;
        case ('u' & 0x1f): paste_line(edcon); break;
        case ('d' & 0x1f): delete_line(edcon); break;

        // Toggle Status
        case ('w' & 0x1f): toggle_status(&cfg, edcon); break;

        case ('o' & 0x1f): { // Ctrl-O — new empty buffer
            char fname[256];
            if (mini_input(edcon, "Open file (blank for new): ", fname, sizeof fname) && fname[0]) {
                int idx = new_buffer(edcon);
                if (idx < 0) {
                    set_status(edcon, "Too many buffers open (max %d)", MAX_BUFFERS);
                } else {
                    switch_buffer(edcon, idx);
                    strncpy(edcon->buffer->filename, fname, sizeof(edcon->buffer->filename));
                    edcon->buffer->filename[sizeof(edcon->buffer->filename) - 1] = '\0';
                    if (!load_file(edcon))
                        set_status(edcon, "New file: \"%s\"", edcon->buffer->filename);
                    else
                        set_status(edcon, "Opened \"%s\"", edcon->buffer->filename);
                }
            }
            else {
                // blank input - open empty unnamed buffer.
                int idx = new_buffer(edcon);
                if (idx < 0) {
                    set_status(edcon, "Too many buffers open (max %d)", MAX_BUFFERS);
                } else {
                    switch_buffer(edcon, idx);
                    set_status(edcon, "New buffer %d/%d  [No Name]", edcon->cur_buf + 1, edcon->buf_count);
                }
            break;
            }
        }

        case ('n' & 0x1f): // Ctrl-N — next buffer
            if (edcon->buf_count > 1) {
                switch_buffer(edcon, edcon->cur_buf + 1);
                set_status(edcon, "Buffer %d/%d: %s",
                           edcon->cur_buf + 1, edcon->buf_count,
                           edcon->buffer->filename[0] ? edcon->buffer->filename : "[No Name]");
            } else {
                set_status(edcon, "Only one buffer open");
            }
            break;

        case ('p' & 0x1f): // Ctrl-P — previous buffer
            if (edcon->buf_count > 1) {
                switch_buffer(edcon, edcon->cur_buf - 1);
                set_status(edcon, "Buffer %d/%d: %s",
                           edcon->cur_buf + 1, edcon->buf_count,
                           edcon->buffer->filename[0] ? edcon->buffer->filename : "[No Name]");
            } else {
                set_status(edcon, "Only one buffer open");
            }
            break;

        // Movement
        case KEY_UP:         move_up(&cfg, edcon);          break;
        case KEY_DOWN:       move_down(&cfg, edcon);        break;
        case KEY_LEFT:       move_left(edcon);              break;
        case KEY_RIGHT:      move_right(edcon);             break;
        case KEY_HOME:
        case ('a' & 0x1f):   move_line_start(edcon);        break;
        case KEY_END:
        case ('e' & 0x1f):   move_line_end(edcon);          break;
        case KEY_PPAGE:      move_page_up(&cfg, edcon);     break;
        case KEY_NPAGE:      move_page_down(&cfg, edcon);   break;

        // Editing
        case KEY_BACKSPACE:
        case 127:
        case '\b':
            if (edcon->buffer->cursor > 0) {
                char deleted = gap_char(&edcon->buffer->text, edcon->buffer->cursor - 1);
                gap_delete(&edcon->buffer->text, edcon->buffer->cursor - 1);
                int old = edcon->buffer->cursor;
                edcon->buffer->cursor--;
                update_current_line_delta(edcon, old, edcon->buffer->cursor);
                update_stats(edcon, deleted, -1);
                edcon->buffer->dirty = 1;
            }
            break;

        case KEY_DC: // Delete Key
            if (edcon->buffer->cursor < gap_len(&edcon->buffer->text)) {
                gap_delete(&edcon->buffer->text, edcon->buffer->cursor);
                edcon->buffer->dirty = 1;
            }
            break;

        case '\t':
            for (int i = 0; i < cfg.tab_width; i++) {
                gap_insert(&edcon->buffer->text, edcon->buffer->cursor, ' ');
                edcon->buffer->cursor++;
                update_stats(edcon, ' ', +1);
            }
            edcon->buffer->dirty = 1;
            break;

        case '\n':
        case KEY_ENTER:
            gap_insert(&edcon->buffer->text, edcon->buffer->cursor, '\n');
            edcon->buffer->cursor++;
            update_stats(edcon, '\n', +1);
            edcon->buffer->current_line++;

            if (cfg.auto_indent) {
                int ln = edcon->buffer->current_line - 1;
                int s = line_start(edcon, ln);
                int end = s + line_len(edcon, ln);
                int ws = s;
                while (ws < end && (gap_char(&edcon->buffer->text, ws) == ' ' 
                    || gap_char(&edcon->buffer->text, ws) == '\t'))
                    ws++;
                int indent = ws - s;
                for (int i = 0; i < indent; i++) {
                    char wc = gap_char(&edcon->buffer->text, s + i);
                    gap_insert(&edcon->buffer->text, edcon->buffer->cursor, wc);
                    edcon->buffer->cursor++;
                    update_stats(edcon, wc, +1);
                }
            }
            edcon->buffer->dirty = 1;
            break;

        default:
            if (c >= 32 && c < 256 && c != 127) {
                gap_insert(&edcon->buffer->text, edcon->buffer->cursor, (char)c);
                edcon->buffer->cursor++;
                update_stats(edcon, (char)c, +1);
                edcon->buffer->dirty = 1;
            }
            break;
        }
    }

done:
    endwin();
    for (int i = 0; i < edcon->buf_count; i++)
        gap_free(&edcon->buffers[i].text);
    return 0;
}