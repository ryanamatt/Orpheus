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

    int running = 1;

    while (running) {
        getmaxyx(stdscr, edcon->buffer->rows, edcon->buffer->cols);
        // Keep terminal size in sync for all buffers
        for (int i = 0; i < edcon->buf_count; i++) {
            edcon->buffers[i].rows = edcon->buffer->rows;
            edcon->buffers[i].cols = edcon->buffer->cols;
        }
        refresh_screen(&cfg, edcon);
        edcon->buffer->status[0] = '\0'; // clear status after one frame

        running = main_input(&cfg, edcon);
    }

    endwin();
    for (int i = 0; i < edcon->buf_count; i++)
        gap_free(&edcon->buffers[i].text);
    return 0;
}