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
#include <stdarg.h>
#include <ctype.h>
#include <stdio.h>
#include "buffer.h"

// Declare Multi-Buffer Globals
Buffer buffers[MAX_BUFFERS];
int buf_count = 0;
int cur_buf = 0;
Buffer *E_ptr = NULL;

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
int new_buffer(void) {
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
void switch_buffer(int i) {
    if (buf_count == 0) return;
    if (i < 0) i = buf_count - 1;
    if (i >= buf_count) i = 0;
    cur_buf = i;
    E_ptr   = &buffers[cur_buf];
}

// --- Buffer Helpers ---

/**
 * @brief Return the logical position of the first character of line @p ln.
 * 
 * Scans the buffer from the beginning, counting newlines until @p ln is reached. Returns 0 for
 * line 0 and the total buffer length for a line index beyond the last line.
 * 
 * @param ln Zero-based line number. 
 * @return Logical character offset of the first character on line @p ln.
 */
int line_start(int ln) {
    int p = 0, len = gap_len(&E.text);
    for (int l = 0; l < ln && p < len; p++)
        if (gap_char(&E.text, p) == '\n') l++;
    return p;
}

/**
 * @brief Return the number of characters on line @p ln, excluding the newline.
 * 
 * @param ln Zero-base line number.
 * @return Character count of line @p ln (newline not included).
 */
int line_len(int ln) {
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
int total_lines(void) {
    return E.line_count;
}

/**
 * @brief Rebuild the cached line count from scratch.
 *
 * Performs a full buffer scan to recount newlines and resets @c E.stats_dirty to force a word 
 * rescan on the next draw. Called after load_file() and after bulk edits such as cut, paste, 
 * and delete-line where incremental tracking would be error-prone.
 */
void rebuild_line_count(void) {
    int n = 1, len = gap_len(&E.text);
    for (int i = 0; i < len; i++)
        if (gap_char(&E.text, i) == '\n') n++;
    E.line_count  = n;
    E.stats_dirty = 1; // force word rescan on next draw
}

/**
 * @brief Return the total number of characters in the active buffer.
 *
 * Includes all characters — printable, whitespace, and newlines.
 *
 * @return Total character count.
 */
int count_chars(void) {
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
int count_words_full(void) {
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
int count_words(void) {
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
void update_stats(char c, int delta) {
    if (c == '\n') E.line_count += delta;
    E.stats_dirty = 1;
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
int cursor_vcol(Config *cfg_ptr) {
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
 * @brief Convert a logical character offest to a zero-based line number.
 * 
 * Counts every newline character in the active buffer before @p pos. This is an O(n) scan. Prefer
 * to use the cached @c E.current_line where possible, falling back to this only for arbitrary
 * position queries (search, goto, auto-indent, etc.).
 * 
 * @param pos Logical Character offset (0 ... gap_len(&E.ttext)).
 * @return Zero-based line number containing @p pos.
 */
int pos_to_line(int pos) {
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
void update_current_line_delta(int old_cursor, int new_cursor) {
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
 * @brief Write a formatted message into the active buffer's status field.
 *
 * Accepts a printf-style format string and variadic arguments.  The result
 * is stored in @c E.status and rendered on the command bar during the next
 * call to refresh_screen().  The status is cleared after one frame.
 *
 * @param fmt printf-compatible format string.
 * @param ... Additional arguments corresponding to @p fmt.
 */
void set_status(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(E.status, sizeof E.status, fmt, ap);
    va_end(ap);
}