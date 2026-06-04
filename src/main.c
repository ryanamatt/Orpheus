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

#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include "version.h"
#include "config.h"

// --- Constants ---

#define MAX_STATUS  512
#define CHUNK       64  // gap-buffer growth step (in chars)
#define MAX_BUFFERS 32  // maximum number of open buffers/tabs

// --- Color Pairs ---
#define CP_NORMAL   1
#define CP_STATUS   2
#define CP_CMDBAR   3
#define CP_LNUM     4
#define CP_SEARCH   5

// --- Gap Buffer ---

typedef struct {
    char  *buf;         // raw storage
    int    gap_start;   // index of first gap byte
    int    gap_end;     // index of first byte after gap
    int    size;        // total allocated bytes
} Gap;

/**
 * @brief Initialize a Gap Buffer with a small default allocation.
 * 
 * Allocates @c CHUNK butes of raw storage, places the entire allocation inside the gap
 * (gap_start = 0, gap_end = size) and sets, the logical content length to zero.
 * 
 * @param g Pointer to uninitalized Gap Struct.  
 */
static void gap_init(Gap *g) {
    g->size      = CHUNK;
    g->buf       = malloc(g->size);
    g->gap_start = 0;
    g->gap_end   = g->size;
}

/**
 * @brief Release the heap storage that a Gap Buffer owns.
 * 
 * @param g Pointer to the Gap Buffer to free.
 */
static void gap_free(Gap *g) { free(g->buf); }

/**
 * @brief Return the logical length of a Gap.
 * 
 * The length of the Gap is equal to = Gap size - (Gap End - Gap Start)
 * 
 * @param g Pointer to Gap Buffer.
 * @return Number of content characters stored in the buffer.
 */
static int  gap_len(const Gap *g)   { return g->size - (g->gap_end - g->gap_start); }

/**
 * @brief Return the character at the logical position @p pos.
 * 
 * Positions before the gao are read directly; positions at or after gap are
 * translated past the gap region before indexing the raw buffer.
 * 
 * @param g Pointer to the Gap buffer.
 * @param pos Zero-based logical character index (0 ... gap_len(g) - 1)
 * @return The character stored at @p pos.
 * 
 */
static char gap_char(const Gap *g, int pos) {
    return pos < g->gap_start ? g->buf[pos] : g->buf[g->gap_end + (pos - g->gap_start)];
}

/**
 * @brief Ensure the gap is at least @p need bytes wide, growing if necessary.
 * 
 * If the current gap is already wide enough the function returns immediately.
 * Otherwise it reallocates the backing array and shifts the tail of the buffer
 * so the gap remains contiguous at its current postions.
 * 
 * @param g Pointer to the Gap buffer.
 * @param need Minimum number of free gap bytes required.
 */
static void gap_grow(Gap *g, int need) {
    int gap = g->gap_end - g->gap_start;
    if (gap >= need) return;
    int add  = need - gap + CHUNK;
    int nsz  = g->size + add;
    char *nb = realloc(g->buf, nsz);
    // shift tail to make gap continuous at same gap_start
    memmove(nb + g->gap_end + add, nb + g->gap_end, g->size - g->gap_end);
    g->buf     = nb;
    g->gap_end += add;
    g->size     = nsz;
}

/**
 * @brief Move the gap position one position to the right.
 * 
 * Copies the single byte immediately after the gap to the first byte of the gap, then advances
 * both @c gap_start and @c gap_end by one. This is the fast path called by gap_move for delta of
 * +1. 
 * 
 * Does nothing if the gap is already at the end of the buffer.
 * 
 * @param g Pointer to the Gap buffer.
 */
static void gap_shift_right(Gap *g) {
    if (g->gap_end >= g->size) return;
    g->buf[g->gap_start] = g->buf[g->gap_end];
    g->gap_start++;
    g->gap_end++;
}
 
/**
 * @brief Move the gap one position to the left.
 *
 * Copies the single byte immediately before the gap to the last byte of the gap, then decrements 
 * both @c gap_start and @c gap_end by one. This is thefast path called by gap_move() for a delta 
 * of -1.
 *
 * Does nothing if the gap is already at the start of the buffer.
 *
 * @param g Pointer to the Gap buffer.
 */
static void gap_shift_left(Gap *g) {
    if (g->gap_start <= 0) return;
    g->gap_end--;
    g->gap_start--;
    g->buf[g->gap_end] = g->buf[g->gap_start];
}

/**
 * @brief Move the gap so that is start aligns with logical position @p pos.
 * 
 * Uses gap_shift_left() / gap_shift_right() for single-step moves, and
 * @c memmove for larger jumps to minimise per-character overhead.
 * 
 * @param g Pointer to the Gap buffer.
 * @param pos Target logical position for @c gap_start (0 ... gap_len(g)).
 */
static void gap_move(Gap *g, int pos) {
    if (pos == g->gap_start) return;
    if (pos < g->gap_start) {
        if (g->gap_start - pos == 1) {
            gap_shift_left(g);
            return;
        }
        int n = g->gap_start - pos;
        memmove(g->buf + g->gap_end - n, g->buf + pos, n);
        g->gap_start -= n;
        g->gap_end   -= n;
    } else {
        if (pos - g->gap_start == 1) {
            gap_shift_right(g);
            return;
        }
        int n = pos - g->gap_start;
        memmove(g->buf + g->gap_start, g->buf + g->gap_end, n);
        g->gap_start += n;
        g->gap_end   += n;
    }
}

/**
 * @brief Insert a single character into the Gap buffer at logical position @p pos.
 * 
 * Grows the gap if necessary, moves the gap to @p pos, then writes @p c into the first gap
 * slot and advances @c gap_start.
 * 
 * @param g Pointer to the Gap buffer.
 * @param pos Logical position of the index to insert at (0 ... gap_len(g)).
 * @param c Character to insert.
 */
static void gap_insert(Gap *g, int pos, char c) {
    gap_grow(g, 1);
    gap_move(g, pos);
    g->buf[g->gap_start++] = c;
}

/**
 * @brief Delete the character at the logical position @p pos.
 * 
 * Moves the gap to @p pos, then expands the gap by one byte to the right, effectively discarding
 * the character the follows the gap.
 * 
 * @param g Pointer to the Gap buffer.
 * @param pos Logical positon of the index of the character to remove (0 ... gap_len(g)).
 */
static void gap_delete(Gap *g, int pos) {
    gap_move(g, pos);
    if (g->gap_end < g->size) g->gap_end++;
}

// --- Buffer State (one per open file) ---

typedef struct {
    Gap   text;
    int   cursor;               // logical char offset
    int   row_off;              // first visible row
    int   col_off;              // first visible column
    int   rows, cols;           // terminal size (updated each frame)
    int   dirty;                // unsaved changes flag
    char  filename[256];
    char  status[MAX_STATUS];
    char  clipboard[4096];      // cut/paste buffer
    int   cb_len;
    int   last_search_pos;      // for repeated Ctrl-F search
    char  search_term[256];

    int   line_count;           // total newlines + 1 (kept incrementally)
    int   word_count;           // kept incrementally via stats_dirty
    int   stats_dirty;          // non-zero -> word_count needs a full rescan
    int   current_line;         // 0-based line the cursor is on
} Buffer;

// --- Multi-buffer globals ---

static Buffer  buffers[MAX_BUFFERS];
static int     buf_count = 0;   // number of open buffers
static int     cur_buf   = 0;   // index of active buffer

// E is a convenience pointer — always points to buffers[cur_buf].
// All existing code that touches E.field continues to work unchanged.
#define E (*E_ptr)
static Buffer *E_ptr;

/**
 * @brief Make the buffer @p i the active buffer, clamping to the valid range.
 * 
 * Updates @c cur_bug and the convenience pointer @c E_ptr. If @p i is negative the last buffer is
 * selected. If i is >= @c buf_count the first buffer is selected providing wrap around 
 * navigation,
 * 
 * Does nothing if no buffers are open.
 * 
 * @param i The desired buffer index.
 */
static void switch_buffer(int i) {
    if (buf_count == 0) return;
    if (i < 0) i = buf_count - 1;
    if (i >= buf_count) i = 0;
    cur_buf = i;
    E_ptr   = &buffers[cur_buf];
}

// --- helpers ---

/**
 * @brief Cinvert a logical character offest to a zero-based line number.
 * 
 * Counts every newline character in the active buffer before @p pos. This is an O(n) scan. Prefer
 * to use the cached @c E.current_line where possible, falling back to this only for arbitrary
 * position queries (search, goto, auto-indent, etc.).
 * 
 * @param pos Logical Character offset (0 ... gap_len(&E.ttext)).
 * @return Zero-based line number containing @p pos.
 */
static int pos_to_line(int pos) {
    int ln = 0;
    for (int i = 0; i < pos; i++)
        if (gap_char(&E.text, i) == '\n') ln++;
    return ln;
}
 
/**
 * @brief Incrementally synchronise @c E.current_line after a cursor move.
 *
 * Avoids a full O(n) scan whenever possible:
 * - Moving right by 1: if the character stepped over was @c '\\n', increments
 *   @c E.current_line by 1.
 * - Moving left by 1: if the character stepped back over was @c '\\n', decrements
 *   @c E.current_line by 1.
 * - Larger jumps: calls pos_to_line() once and caches the result.
 * 
 * @param old_cursor Cursor position before the move.
 * @param new_cursor Cursor position after the move.
 */
static void update_current_line_delta(int old_cursor, int new_cursor) {
    int delta = new_cursor - old_cursor;
    if (delta == 1) {
        if (old_cursor < gap_len(&E.text) &&
            gap_char(&E.text, old_cursor) == '\n')
            E.current_line++;
    } else if (delta == -1) {
        if (new_cursor >= 0 && new_cursor < gap_len(&E.text) &&
            gap_char(&E.text, new_cursor) == '\n')
            E.current_line--;
    } else {
        // arbitrary jump — full scan, but only once per key-press
        E.current_line = pos_to_line(new_cursor);
    }
}

/**
 * @brief Return the logical position of the first character of line @p ln.
 * 
 * Scans the buffer from the beginning, counting newlines until @p ln is reached. Returns 0 for
 * line 0 and the total buffer length for a line index beyond the last line.
 * 
 * @param ln Zero-based line number. 
 * @return Logical character offset of the first character on line @p ln.
 */
static int line_start(int ln) {
    int p = 0, len = gap_len(&E.text);
    for (int l = 0; l < ln && p < len; p++)
        if (gap_char(&E.text, p) == '\n') l++;
    return p;
}

/**
 * @biref Return the number of characters on line @p ln, excluding the newline.
 * 
 * @param ln Zero-base line number.
 * @return Character count of line @p ln (newline not included).
 */
static int line_len(int ln) {
    int s = line_start(ln), e = s, len = gap_len(&E.text);
    while (e < len && gap_char(&E.text, e) != '\n') e++;
    return e - s;
}

/**
 * @brief Returns the total number of lines in the active buffer.
 * 
 * Returns the cached @c E.line_count value maintained incrementally by
 * update_stats() and rebuild_line_count(), making this an O(1) call.
 * 
 * @return Total line count (always >= 1)
 */
static int total_lines(void) {
    return E.line_count;
}

/**
 * @brief Return the total number of characters in the active buffer.
 *
 * Includes all characters — printable, whitespace, and newlines.
 *
 * @return Total character count.
 */
static int count_chars(void) {
    return gap_len(&E.text);
}

/**
 * @brief Perform a full O(n) word count scan of the active buffer.
 *
 * A "word" is a maximal sequence of non-whitespace characters. This
 * function is only invoked when @c E.stats_dirty is set. The result is
 * cached by count_words() to avoid repeated scans within the same frame.
 *
 * @return Total word count.
 */
static int count_words_full(void) {
    int words = 0, in_word = 0, len = gap_len(&E.text);
    for (int i = 0; i < len; i++) {
        char c = gap_char(&E.text, i);
        if (isspace(c)) in_word = 0;
        else if (!in_word) { in_word = 1; words++; }
    }
    return words;
}

/**
 * @brief Return the word count for the active buffer, rescanning only if needed.
 *
 * Returns the cached @c E.word_count unless @c E.stats_dirty is set, in which case it calls 
 * count_words_full() and clears the dirty flag.
 *
 * @return Current word count.
 */
static int count_words(void) {
    if (E.stats_dirty) {
        E.word_count  = count_words_full();
        E.stats_dirty = 0;
    }
    return E.word_count;
}

/**
 * @brief Incrementally update line and word statistics after a single edit.
 *
 * Called on every character insertion or deletion. Updates @c E.line_count
 * exactly when a newline is involved and sets @c E.stats_dirty to trigger a
 * deferred word rescan on the next draw frame.
 *
 * @param c     The character that was inserted or deleted.
 * @param delta +1 for an insertion, -1 for a deletion.
 */
static void update_stats(char c, int delta) {
    if (c == '\n') E.line_count += delta;
    E.stats_dirty = 1;
}

/**
 * @brief Rebuild the cached line count from scratch.
 *
 * Performs a full buffer scan to recount newlines and resets @c E.stats_dirty to force a word 
 * rescan on the next draw. Called after load_file() and after bulk edits such as cut, paste, 
 * and delete-line where incremental tracking would be error-prone.
 */
static void rebuild_line_count(void) {
    int n = 1, len = gap_len(&E.text);
    for (int i = 0; i < len; i++)
        if (gap_char(&E.text, i) == '\n') n++;
    E.line_count  = n;
    E.stats_dirty = 1; // force word rescan on next draw
}

/**
 * @brief Compute the visual (display) column of the cursor on its current line.
 *
 * Expands tab characters to the next multiple of @c TAB_WIDTH so that the
 * returned column reflects what the user actually sees on screen, not just the
 * raw character offset.
 * 
 * @param cfg_ptr A pointer to the Config Instance.
 * @return Zero-based visual column of @c E.cursor.
 */
static int cursor_vcol(Config *cfg_ptr) {
    int ln  = E.current_line;
    int s   = line_start(ln);
    int col = 0;
    for (int i = s; i < E.cursor; i++) {
        char c = gap_char(&E.text, i);
        if (c == '\t') col = (col / cfg_ptr->tab_width + 1) * cfg_ptr->tab_width;
        else           col++;
    }
    return col;
}

/**
 * @brief Write a formatted message into the active buffer's status field.
 *
 * Accepts a printf-style format string and variadic arguments.  The result
 * is stored in @c E.status and rendered on the command bar during the next
 * call to refresh_screen().  The status is cleared after one frame.
 *
 * @param fmt printf-compatible format string.
 * @param ... Additional arguments corresponding to @p fmt.
 */
static void set_status(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(E.status, sizeof E.status, fmt, ap);
    va_end(ap);
}

// --- File I/O ---

/**
 * @brief Load a file from disk into the active buffer's Gap storage.
 * 
 * Open @p path for reading and appends each byte to the end of the Gap buffer. On success,
 * rebuild_line_count() is called to initialize the cached statistics, and the cursor is
 * rest to position 0.
 * 
 * @param path A File system path of the file to open.
 * @return 1 on success, 0 if file could not be opened.
 */
static int load_file(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    int c;
    while ((c = fgetc(f)) != EOF)
        gap_insert(&E.text, gap_len(&E.text), (char)c);
    fclose(f);
    rebuild_line_count();   // initialise cached line_count + stats_dirty
    E.current_line = 0;
    return 1;
}

/**
 * @brief Write the contents of the active buffer to @c E.filename.
 *
 * Opens the file for writing (truncating it), iterates over every logical character in the 
 * Gap buffer, and writes each byte. On success the dirty flag is cleared and a confirmation 
 * is written to @c E.status.
 *
 * @return 1 on success, 0 if no filename is set or the file cannot be written.
 */
static int save_file(void) {
    if (!E.filename[0]) {
        set_status("No filename - use Ctrl-W to set one");
        return 0;
    }
    FILE *f = fopen(E.filename, "w");
    if (!f) { set_status("Cannot write: %s", strerror(errno)); return 0; }
    int len = gap_len(&E.text);
    for (int i = 0; i < len; i++) fputc(gap_char(&E.text, i), f);
    fclose(f);
    E.dirty = 0;
    set_status("Saved \"%s\"  (%d bytes)", E.filename, len);
    return 1;
}

// --- display ---

/**
 * @brief Adjust the viewport offsets so the cursor remainds visible.
 * 
 * Update @c E.row_off and @c E.col_off to ensure the current line and visual column are within
 * the displayed text area. Accounts for the tab bar row when multiple buffers are open and for
 * the status bar rows when @c SHOW_STATUS_BAR is enabled.
 * 
 * @param cfg_ptr A pointer to the Config Instance.
 */
static void adjust_scroll(Config *cfg_ptr) {
    // Extra row used by the tab bar when more than one buffer is open
    int tab_rows = (buf_count > 1) ? 1 : 0;
    int cur_line = E.current_line;
    int vcol = cursor_vcol(cfg_ptr);
    int text_rows = E.rows - (cfg_ptr->show_statusbar ? 2 : 0) - tab_rows;   // status + command bar

    if (cur_line < E.row_off)               E.row_off = cur_line;
    if (cur_line >= E.row_off + text_rows)  E.row_off = cur_line - text_rows + 1;
    if (vcol < E.col_off)                   E.col_off = vcol;
    if (vcol >= E.col_off + E.cols - 6)     E.col_off = vcol - (E.cols - 6) + 1;
}

/**
 * @brief Render the visible text rows, including the line-number gutter.
 *
 * Iterates over each visible row, drawing the line-number gutter (respecting @c SHOW_LINE_NUMBERS 
 * and @c GUTTER_WIDTH) followed by the line's characters with horizontal scrolling and tab 
 * expansion applied. Rows beyond the lastline display a @c ~ sentinel, matching traditional 
 * text-editor conventions.
 * 
 * @param cfg_ptr A pointer to the Config Instance.
 */
static void draw_rows(Config *cfg_ptr) {
    int total = total_lines();
    int tab_rows = (buf_count > 1) ? 1 : 0;
    int text_rows = E.rows - (cfg_ptr->show_statusbar ? 2 : 0) - tab_rows;

    char fmt[16];
    snprintf(fmt, sizeof(fmt), "%%%dd ", cfg_ptr->gutter_width - 1);

    for (int y = 0; y < text_rows; y++) {
        int ln = y + E.row_off;
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
            int s = line_start(ln);
            int end = s + line_len(ln);
            int col = 0;
            for (int i = s; i < end; i++) {
                char c = gap_char(&E.text, i);
                if (c == '\t') {
                    int next = (col / cfg_ptr->tab_width + 1) * cfg_ptr->tab_width;
                    while (col < next && col - E.col_off < E.cols - 5) {
                        if (col >= E.col_off) addch(' ');
                        col++;
                    }
                } else {
                    if (col >= E.col_off && col - E.col_off < E.cols - 5)
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
 */
static void draw_tabbar(Config *cfg_ptr) {
    // Tab bar sits at E.rows - 3 when statusbar is shown, else E.rows - 1.
    // We only draw it when there is more than one buffer open.
    if (buf_count <= 1) return;

    int row = E.rows - (cfg_ptr->show_statusbar ? 3 : 1);
    attron(COLOR_PAIR(CP_STATUS));
    move(row, 0);
    clrtoeol();

    int x = 0;
    for (int i = 0; i < buf_count && x < E.cols - 1; i++) {
        const char *name = buffers[i].filename[0]
                           ? buffers[i].filename : "[No Name]";
        // Strip directory prefix for display
        const char *base = strrchr(name, '/');
        base = base ? base + 1 : name;

        char tab[64];
        int tlen = snprintf(tab, sizeof tab, " %s%s ",
                            base,
                            buffers[i].dirty ? "+" : "");

        if (i == cur_buf)
            attron(A_BOLD | A_REVERSE);
        if (x + tlen < E.cols)
            mvprintw(row, x, "%s", tab);
        if (i == cur_buf)
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
 */
static void draw_statusbar(Config *cfg_ptr) {
    attron(COLOR_PAIR(CP_STATUS) | A_BOLD);
    move(E.rows - 2, 0);
    int ln = E.current_line;
    int col = cursor_vcol(cfg_ptr);

    int chars = count_chars();
    int words = count_words();

    char left[128], right[128];

    snprintf(left,  sizeof left,  " %.40s%s",
             E.filename[0] ? E.filename : "[No Name]",
             E.dirty ? " [+]" : "");

    snprintf(right, sizeof right, "Ln %d, Col %d | %d lines | %d words %d chars ",
             ln + 1, col + 1, total_lines(), words, chars);

    int pad = E.cols - (int)strlen(left) - (int)strlen(right);
    printw("%s", left);
    for (int i = 0; i < pad && i < E.cols; i++) addch(' ');
    printw("%s", right);
    attroff(COLOR_PAIR(CP_STATUS) | A_BOLD);
}

/**
 * @brief Render the command bar with keybinding hints and the status message.
 *
 * Draws the fixed keybinding cheatsheet on the bottom row.  If @c E.status is non-empty it is 
 * overlaid right-aligned on the same row in bold, providing one-shot feedback to the user.
 */
static void draw_cmdbar(void) {
    attron(COLOR_PAIR(CP_CMDBAR));
    move(E.rows - 1, 0);
    printw(" ^S Save  ^Q Quit  ^F Find  ^R Repl  ^G Go-To  ^K Cut  ^U Paste  ^D Del-Ln  ^W Hide  ^N Next  ^P Prev");
    clrtoeol();
    attroff(COLOR_PAIR(CP_CMDBAR));

    // show one-shot status message on right side
    if (E.status[0]) {
        int slen = strlen(E.status);
        int x    = E.cols - slen - 2;
        if (x < 0) x = 0;
        attron(COLOR_PAIR(CP_CMDBAR) | A_BOLD);
        mvprintw(E.rows - 1, x, " %s ", E.status);
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
 */
static void refresh_screen(Config *cfg_ptr) {
    adjust_scroll(cfg_ptr);
    int ln = E.current_line;
    int vcol = cursor_vcol(cfg_ptr);

    draw_rows(cfg_ptr);
    if (cfg_ptr->show_statusbar) {
        draw_tabbar(cfg_ptr);
        draw_statusbar(cfg_ptr);
        draw_cmdbar();
    } else {
        draw_tabbar(cfg_ptr);
    }

    // position real cursor (tab bar does not shift text rows — it sits below)
    move(ln - E.row_off, cfg_ptr->gutter_width + vcol - E.col_off);
    wnoutrefresh(stdscr);
    doupdate();
}

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
 * @brief Initialise the ncurses library and apply colour scheme settings.
 *
 * Calls @c initscr(), enables raw input, disables echo, enables the keypad,
 * and sets the escape-sequence delay from @c KEY_DELAY. When the terminal
 * supports colour and the scheme is not @c "mono", initialises five colour
 * pairs (normal text, status bar, command bar, line numbers, search highlight)
 * according to the @c COLOR_SCHEME global:
 * - @c "dark"  — white on black palette.
 * - @c "light" — black on white palette.
 * - @c "default" — inherits the terminal's own colours.
 * 
 * @param cfg_ptr A pointer to the Config Instance.
 *
 * Finally sets the cursor shape via @c curs_set(CURSOR_STYLE).
 */
static void init_ncurses(Config *cfg_ptr) {
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
}

/**
 * @brief Allocate and initialise a new buffer slot.
 *
 * Zeroes the next available entry in the @c buffers array, initialises its
 * Gap storage, and sets sensible defaults for all counters. The new buffer
 * becomes the last slot but is not made active; call switch_buffer() to focus
 * it.
 *
 * @return The index of the newly created buffer, or -1 if @c MAX_BUFFERS has
 *         been reached.
 */
static int new_buffer(void) {
    if (buf_count >= MAX_BUFFERS) return -1;
    int idx = buf_count++;
    memset(&buffers[idx], 0, sizeof(Buffer));
    gap_init(&buffers[idx].text);
    buffers[idx].line_count  = 1;
    buffers[idx].word_count  = 0;
    buffers[idx].stats_dirty = 0;
    buffers[idx].current_line= 0;
    return idx;
}

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