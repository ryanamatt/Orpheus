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
 * Ctrl-T   Toggle Focus (typewriter) mode — centers text, hides chrome
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
 *
 * focus_mode: int
 *   If non-zero, start in focus mode on launch. Default: 0.
 *
 * focus_width: int
 *   Width of the centred text column used in focus mode. Default: 72.
 */

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <ncurses.h>
#include "logger.h"
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
    log_debug("handle_args: argc=%d", argc);
    if (argc < 2) {
        // No file: one empty unnamed buffer
        new_buffer(edcon);
        switch_buffer(edcon, 0);
        set_status(edcon, "orpheus - no file. Ctrl-S to save, Ctrl-Q to quit.");
        log_debug("No file argument - opened empty unnamed buffer");
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
        printf(" Ctrl-T             Toggle focus (typewriter) mode\n");
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

    // orp [file ...] - open each file as its own buffer
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
        if (!load_file(edcon)) {
            set_status(edcon, "New file: \"%s\"", edcon->buffer->filename);
            log_debug("handle_args: new file '%s' (does not exist on disk)", edcon->buffer->filename);
        } else {
            set_status(edcon, "Opened \"%s\"", edcon->buffer->filename);
            log_debug("handle_args: loaded '%s' successfully", edcon->buffer->filename);
        }
    }
    switch_buffer(edcon, 0);
    return 0;
}

/**
 * @brief Program entry point for the Orpheus text editor.
 *
 * Execution order:
 * 1. load_config() - read @c ~/.orpheusrc settings.
 * 2. handle_args() - process CLI flags/filenames; exit early for @c --help /
 *    @c --version.
 * 3. init_ncurses() - set up the terminal.
 * 4. Main event loop - redraw the screen, read one keypress, dispatch to the
 *    appropriate handler, repeat until confirm_quit() returns true.
 * 5. @c endwin() and Gap buffer cleanup on exit.
 *
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return 0 on normal exit.
 */
int main(int argc, char *argv[]) {
#ifdef DEBUG
    time_t raw_time;
    time(&raw_time);
    struct tm *local_time = localtime(&raw_time);
    char time_string[32];
    strftime(time_string, sizeof(time_string), "%Y-%m-%d_%H-%M-%S", local_time);
    char filename[64];
    snprintf(filename, sizeof(filename), "orpheus_log_%s.log", time_string);
    if (init_logger(filename)) {
        // Perror called by init_logger
        return 1;
    }
#endif 

    EditorContext edcon_ctx = {0};
    EditorContext *edcon = &edcon_ctx;
    log_debug("EditorContext initialized");

    Config cfg;
    config_defaults(&cfg);
    log_debug("Config defaults applied");
    load_config(&cfg);
    log_debug("Config loaded: tab_width=%d show_line_numbers=%d auto_indent=%d show_statusbar=%d "
            "cursor_style=%d gutter_width=%d key_delay=%d color_scheme=%s, focus_mode=%d, focus_width=%d",
            cfg.tab_width, cfg.show_line_numbers, cfg.auto_indent,
            cfg.show_statusbar, cfg.cursor_style, cfg.gutter_width,
            cfg.key_delay, cfg.color_scheme, cfg.focus_mode, cfg.focus_width);

    // If handle_args returns 1 it was a flag like --help, exit early.
    if (handle_args(edcon, argc, argv)) return 0;

    log_debug("Starting ncurses");
    init_ncurses(&cfg);
    getmaxyx(stdscr, edcon->buffer->rows, edcon->buffer->cols);
    log_debug("Terminal size: %d rows x %d cols", edcon->buffer->rows, edcon->buffer->cols);

    int running = 1;
    log_debug("Entering main event loop");

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

    log_debug("Exiting main event loop - shutting down");
    endwin();
    for (int i = 0; i < edcon->buf_count; i++)
        gap_free(&edcon->buffers[i].text);
    log_debug("Gap buffers freed - exit");
    return 0;
}