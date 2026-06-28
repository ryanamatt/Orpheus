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
 * Usage: orp [-t template] [file ...]
 *
 * Keybindings
 * -----------
 * Arrow keys / PgUp / PgDn / Home / End  - navigation
 * Ctrl-S   save
 * Ctrl-Q   quit (warns on unsaved changes)
 * Ctrl-F   find  (Enter to cycle, Esc to cancel)
 * Ctrl-R   replace (prompts for search term, replacement, then all/next)
 * Ctrl-G   go to line
 * Mouse drag   select text (click and drag over text to highlight it)
 * Ctrl-C   copy selection
 * Ctrl-X   cut selection
 * Ctrl-V   paste at cursor (replaces selection if any)
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
 * Templates
 * -----------
 * -t NAME / --template NAME
 *   For every filename argument that does NOT already exist on disk, populate
 *   the new buffer with ~/.config/Orpheus/templates/NAME.tmpl instead of
 *   leaving it empty. Files that already exist are opened as-is; the
 *   template is never applied to them. If no filename is given at all,
 *   the template is applied to the new unnamed buffer instead. Templates
 *   are plain text and may use {{currentTime}}, expanded via strftime()
 *   with the time_format setting below. Example template:
 *
 *     ----- 
 *     Chapter 1
 *     Draft 1
 *     First Time: {{currentTime}}
 *     Last Update: {{currentTime}}
 *     -----
 *
 * Optional Settings placed in ~/.config/Orpheus/orpheus.config
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
 *
 * time_format: string
 *   strftime() format string used to expand {{currentTime}} in templates.
 *   Default: "%-m/%-d/%y" (e.g. "6/25/26"). Note: %-m/%-d are glibc
 *   extensions (no leading zero); on non-glibc systems use %m/%d instead.
 *
 * color_normal_fg, color_normal_bg: string
 * color_status_fg, color_status_bg: string
 * color_cmdbar_fg, color_cmdbar_bg: string
 * color_lnum_fg,   color_lnum_bg:   string
 * color_search_fg, color_search_bg: string
 * color_select_fg, color_select_bg: string
 *   Per-pair colour overrides applied on top of color_scheme. Each key sets
 *   just the foreground or background of one UI element (normal text,
 *   status bar, command bar, line numbers, search highlight, or mouse
 *   selection) - so a scheme can be used as a starting point and tweaked
 *   without abandoning it. Unset keys keep whatever color_scheme assigns.
 *   Recognised values (case-insensitive): black, red, green, yellow, blue,
 *   magenta, cyan, white, default. Example:
 *     color_scheme=dark
 *     color_search_bg=magenta
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
#include "template.h"

// --- Main Loop ---

/**
 * @brief Parse command-line arguments and set up initial buffers.
 *
 * Handles four cases:
 * - No arguments: opens a single empty unnamed buffer and returns 0.
 * - @c -h / @c --help: prints usage information to stdout and returns 1.
 * - @c -v / @c --version: prints version and build date to stdout and returns 1.
 * - One or more filenames, optionally preceded by @c -t / @c --template
 *   <name>: opens each filename as its own buffer, loading content from
 *   disk where possible. For any filename that does NOT already exist on
 *   disk, if a template name was given, apply_template() populates the new
 *   buffer with the named template instead of leaving it empty. Existing
 *   files are always opened as-is; the template is never applied to them.
 *   If @c -t / @c --template <name> is given with NO filenames at all, the
 *   template is applied to the new unnamed buffer instead.
 *
 * A non-zero return signals that the editor should not start (the caller
 * should exit after this function returns). Two non-zero values are used so
 * @c main() can choose the right process exit code:
 *   - 1: a clean informational exit (@c -h / @c --help, @c -v / @c --version).
 *        @c main() exits 0 for these, per the usual CLI convention.
 *   - 2: a usage error (e.g. @c -t / @c --template given without a name).
 *        @c main() exits 1 for these, so scripts can detect the failure.
 *
 * @param cfg_ptr Pointer to the Config Instance (supplies time_format to
 *                apply_template()).
 * @param edcon The EditorContext Instance.
 * @param argc Argument count from @c main().
 * @param argv Argument vector from @c main().
 * @return 0 if the editor should proceed to its main loop, 1 for a clean
 *         informational exit, or 2 for a usage error. See above.
 */
int handle_args(Config *cfg_ptr, EditorContext *edcon, int argc, char *argv[]) {
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
        printf("Usage: orp [-t template] [file ...]\n\n");
        printf("Options:\n");
        printf("  -h, --help            Show this help message\n");
        printf("  -v, --version         Show version information\n");
        printf("  -t, --template NAME   Populate any NEW file with ~/.config/Orpheus/\n");
        printf("                        templates/NAME.tmpl. Files that already exist\n");
        printf("                        on disk are opened as-is and never templated.\n");
        printf("                        With no filename, applies to the new buffer.\n\n");
        printf("Keybindings:\n");
        printf("Arrow keys          Navigation - move 1 char in direction of arrow\n");
        printf("PgUp/PgDn Home/End  Navigation - top/bottom, front/end of line\n");
        printf(" Ctrl-S             Save\n");
        printf(" Ctrl-Q             Quit (warns on unsaved changes)\n");
        printf(" Ctrl-F             Find  (Enter to cycle, Esc to cancel)\n");
        printf(" Ctrl-R             Replace (search, replacement, then All/Next)\n");
        printf(" Ctrl-G             Go to line\n");
        printf(" Mouse drag         Select text\n");
        printf(" Ctrl-C             Copy selection\n");
        printf(" Ctrl-X             Cut selection\n");
        printf(" Ctrl-V             Paste at cursor (replaces selection if any)\n");
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

    // orp [-t|--template NAME] [file ...]
    // Scan for -t/--template up front so it applies regardless of where it
    // appears relative to the filename arguments, then strip it from the
    // list before the file-opening loop below sees argv.
    const char *template_name = NULL;
    char *files[argc];   // upper bound on the number of filename args
    int   file_count = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--template") == 0) {
            if (i + 1 >= argc) {
                fprintf(stderr, "orpheus: %s requires a template name\n", argv[i]);
                return 2;
            }
            template_name = argv[++i];
        } else {
            files[file_count++] = argv[i];
        }
    }

    if (file_count == 0) {
        // -t was given with no filenames - apply it to the new unnamed
        // buffer too, same as we would for a new (not-yet-existing) file.
        new_buffer(edcon);
        switch_buffer(edcon, 0);
        if (template_name) {
            if (apply_template(cfg_ptr, edcon, template_name)) {
                log_debug("handle_args: no filename - unnamed buffer populated from template '%s'",
                          template_name);
            } 
            
            else {
                // apply_template() already set a status message explaining
                // the failure (e.g. "Template not found"); leave the buffer
                // empty rather than silently discarding that message.
                log_debug("handle_args: no filename - template '%s' failed, buffer left empty",
                          template_name);
            }
        } 
        
        else {
            set_status(edcon, "orpheus - no file. Ctrl-S to save, Ctrl-Q to quit.");
            log_debug("handle_args: no filename, no template - opened empty unnamed buffer");
        }
        return 0;
    }

    // orp [file ...] - open each file as its own buffer
    for (int i = 0; i < file_count; i++) {
        int idx = new_buffer(edcon);
        if (idx < 0) {
            fprintf(stderr, "orpheus: too many files (max %d)\n", MAX_BUFFERS);
            break;
        }
        // Temporarily point E at this buffer so load_file / set_status work
        edcon->cur_buf = idx;
        edcon->buffer = &edcon->buffers[idx];
        strncpy(edcon->buffer->filename, files[i], sizeof(edcon->buffer->filename) - 1);
        edcon->buffer->filename[sizeof(edcon->buffer->filename) - 1] = '\0';
        if (!load_file(edcon)) {
            // File does not exist on disk yet.
            if (template_name) {
                if (apply_template(cfg_ptr, edcon, template_name)) {
                    log_debug("handle_args: new file '%s' populated from template '%s'",
                              edcon->buffer->filename, template_name);
                } 
                
                else {
                    // apply_template() already set a status message explaining
                    // the failure (e.g. "Template not found"); leave the buffer
                    // empty rather than silently discarding that message.
                    log_debug("handle_args: new file '%s' - template '%s' failed, buffer left empty",
                              edcon->buffer->filename, template_name);
                }
            } 
            
            else {
                set_status(edcon, "New file: \"%s\"", edcon->buffer->filename);
                log_debug("handle_args: new file '%s' (does not exist on disk)", edcon->buffer->filename);
            }
        } 
        
        else {
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
 * 1. load_config() - read @c ~/.config/Orpheus/orpheus.config settings.
 * 2. handle_args() - process CLI flags/filenames; exit early for @c --help /
 *    @c --version. Applies @c -t / @c --template to any new files.
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
            "cursor_style=%d gutter_width=%d key_delay=%d color_scheme=%s, focus_mode=%d, focus_width=%d, "
            "time_format=%s",
            cfg.tab_width, cfg.show_line_numbers, cfg.auto_indent,
            cfg.show_statusbar, cfg.cursor_style, cfg.gutter_width,
            cfg.key_delay, cfg.color_scheme, cfg.focus_mode, cfg.focus_width, cfg.time_format);

    // handle_args() returns 0 to proceed, 1 for a clean info exit (-h/-v),
    // or 2 for a usage error - see its doc comment for the exit-code mapping.
    int hargs = handle_args(&cfg, edcon, argc, argv);
    if (hargs == 1) return 0;
    if (hargs == 2) return 1;

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