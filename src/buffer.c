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
#include "logger.h"
#include "buffer.h"

// Declare Multi-Buffer Globals
Buffer buffers[MAX_BUFFERS];
int buf_count = 0;
int cur_buf = 0;
Buffer *E_ptr = NULL;

/**
 * @brief Allocate and initialise a new buffer slot.
 *
 * Zeroes the next available entry in the @c edcon->buffers array, initialises its
 * Gap storage, and sets sensible defaults for all counters. The new buffer
 * becomes the last slot but is not made active; call switch_buffer() to focus
 * it.
 * 
 * @param edcon The EditorContext Instance.
 * @return The index of the newly created buffer, or -1 if @c MAX_BUFFERS has
 *         been reached.
 */
int new_buffer(EditorContext *edcon) {
    if (edcon->buf_count >= MAX_BUFFERS) {
        log_error("new_buffer: MAX_BUFFERS (%d) reached — cannot open new buffer", MAX_BUFFERS);
        return -1;
    }
    int idx = edcon->buf_count++;

    memset(&edcon->buffers[idx], 0, sizeof(Buffer));
    gap_init(&edcon->buffers[idx].text);

    edcon->buffers[idx].line_count = 1;
    edcon->buffers[idx].word_count = 0;
    edcon->buffers[idx].stats_dirty = 0;
    edcon->buffers[idx].current_line= 0;

    log_debug("new_buffer: created buffer[%d], total buffers=%d", idx, edcon->buf_count);
    return idx;
}

/**
 * @brief Make the buffer @p i the active buffer, clamping to the valid range.
 * 
 * Updates @c edcon->cur_bug and the convenience pointer @c edcon->buffer. If @p i is negative the last buffer is
 * selected. If i is >= @c edcon->buf_count the first buffer is selected providing wrap around 
 * navigation,
 * 
 * Does nothing if no buffers are open.
 * 
 * @param edcon The EditorContext Instance.
 * @param i The desired buffer index.
 */
void switch_buffer(EditorContext *edcon, int i) {
    if (edcon->buf_count == 0) {
        log_error("switch_buffer: called with no open buffers");
        return;
    }
    if (i < 0) i = edcon->buf_count - 1;
    if (i >= edcon->buf_count) i = 0;
    log_debug("switch_buffer: %d -> %d, (filename='%s')", edcon->cur_buf, i, 
        edcon->buffers[i].filename[0] ? edcon->buffers[i].filename : "[No Name]");
    edcon->cur_buf = i;
    edcon->buffer = &edcon->buffers[edcon->cur_buf];
}

// --- Buffer Helpers ---

/**
 * @brief Return the logical position of the first character of line @p ln.
 * 
 * Scans the buffer from the beginning, counting newlines until @p ln is reached. Returns 0 for
 * line 0 and the total buffer length for a line index beyond the last line.
 * 
 * @param edcon The EditorContext Instance.
 * @param ln Zero-based line number. 
 * @return Logical character offset of the first character on line @p ln.
 */
int line_start(EditorContext *edcon, int ln) {
    int p = 0;
    int len = gap_len(&edcon->buffer->text);
    for (int l = 0; l < ln && p < len; p++)
        if (gap_char(&edcon->buffer->text, p) == '\n') l++;
    return p;
}

/**
 * @brief Return the number of characters on line @p ln, excluding the newline.
 * 
 * @param edcon The EditorContext Instance.
 * @param ln Zero-base line number.
 * @return Character count of line @p ln (newline not included).
 */
int line_len(EditorContext *edcon, int ln) {
    int s = line_start(edcon, ln), e = s, len = gap_len(&edcon->buffer->text);
    while (e < len && gap_char(&edcon->buffer->text, e) != '\n') e++;
    return e - s;
}

/**
 * @brief Returns the total number of lines in the active buffer.
 * 
 * Returns the cached @c edcon->buffer->line_count value maintained incrementally by
 * update_stats() and rebuild_line_count(), making this an O(1) call.
 * 
 * @param edcon The EditorContext Instance.
 * @return Total line count (always >= 1)
 */
int total_lines(EditorContext *edcon) {
    return edcon->buffer->line_count;
}

/**
 * @brief Rebuild the cached line count from scratch.
 *
 * Performs a full buffer scan to recount newlines and resets @c edcon->buffer->stats_dirty to 
 * force a word rescan on the next draw. Called after load_file() and after bulk edits such as 
 * cut, paste, and delete-line where incremental tracking would be error-prone.
 */
void rebuild_line_count(EditorContext *edcon) {
    int n = 1, len = gap_len(&edcon->buffer->text);
    for (int i = 0; i < len; i++)
        if (gap_char(&edcon->buffer->text, i) == '\n') n++;
    edcon->buffer->line_count = n;
    edcon->buffer->stats_dirty = 1; // force word rescan on next draw
    log_debug("rebuild_line_count: '%s' -> %d lines, %d chars", 
        edcon->buffer->filename[0] ? edcon->buffer->filename : "[No Name]", n, len);
}

/**
 * @brief Return the total number of characters in the active buffer.
 *
 * Includes all characters — printable, whitespace, and newlines.
 *
 * @return Total character count.
 */
int count_chars(EditorContext *edcon) {
    return gap_len(&edcon->buffer->text);
}

/**
 * @brief Perform a full O(n) word count scan of the active buffer.
 *
 * A "word" is a maximal sequence of non-whitespace characters. This
 * function is only invoked when @c edcon->buffer->stats_dirty is set. The result is
 * cached by count_words() to avoid repeated scans within the same frame.
 *
 * @param edcon The EditorContext Instance.
 * @return Total word count.
 */
int count_words_full(EditorContext *edcon) {
    int words = 0, in_word = 0, len = gap_len(&edcon->buffer->text);
    for (int i = 0; i < len; i++) {
        char c = gap_char(&edcon->buffer->text, i);
        if (isspace(c)) in_word = 0;
        else if (!in_word) { in_word = 1; words++; }
    }
    return words;
}

/**
 * @brief Return the word count for the active buffer, rescanning only if needed.
 *
 * Returns the cached @c edcon->buffer->word_count unless @c edcon->buffer->stats_dirty is set, 
 * in which case it calls count_words_full() and clears the dirty flag.
 *
 * @param edcon The EditorContext Instance.
 * @return Current word count.
 */
int count_words(EditorContext *edcon) {
    if (edcon->buffer->stats_dirty) {
        edcon->buffer->word_count  = count_words_full(edcon);
        edcon->buffer->stats_dirty = 0;
    }
    return edcon->buffer->word_count;
}

/**
 * @brief Incrementally update line and word statistics after a single edit.
 *
 * Called on every character insertion or deletion. Updates @c edcon->buffer->line_count
 * exactly when a newline is involved and sets @c edcon->buffer->stats_dirty to trigger a
 * deferred word rescan on the next draw frame.
 *
 * @param edcon The EditorContext Instance.
 * @param c     The character that was inserted or deleted.
 * @param delta +1 for an insertion, -1 for a deletion.
 */
void update_stats(EditorContext *edcon, char c, int delta) {
    if (c == '\n') edcon->buffer->line_count += delta;
    edcon->buffer->stats_dirty = 1;
}

/**
 * @brief Compute the visual (display) column of the cursor on its current line.
 *
 * Expands tab characters to the next multiple of @c cfg_ptr->TAB_WIDTH so that the
 * returned column reflects what the user actually sees on screen, not just the
 * raw character offset.
 * 
 * @param cfg_ptr A pointer to the Config Instance.
 * @param edcon The EditorContext Instance.
 * @return Zero-based visual column of @c edcon->buffer->cursor.
 */
int cursor_vcol(Config *cfg_ptr, EditorContext *edcon) {
    int ln = edcon->buffer->current_line;
    int s = line_start(edcon, ln);
    int col = 0;
    for (int i = s; i < edcon->buffer->cursor; i++) {
        char c = gap_char(&edcon->buffer->text, i);
        if (c == '\t') col = (col / cfg_ptr->tab_width + 1) * cfg_ptr->tab_width;
        else col++;
    }
    return col;
}

/**
 * @brief Convert a logical character offest to a zero-based line number.
 * 
 * Counts every newline character in the active buffer before @p pos. This is an O(n) scan. Prefer
 * to use the cached @c edcon->buffer->current_line where possible, falling back to this only for 
 * arbitrary position queries (search, goto, auto-indent, etc.).
 * 
 * @param edcon The EditorContext Instance.
 * @param pos Logical Character offset (0 ... gap_len(&E.ttext)).
 * @return Zero-based line number containing @p pos.
 */
int pos_to_line(EditorContext *edcon, int pos) {
    int ln = 0;
    for (int i = 0; i < pos; i++)
        if (gap_char(&edcon->buffer->text, i) == '\n') ln++;
    return ln;
}

/**
 * @brief Incrementally synchronise @c E.current_line after a cursor move.
 *
 * Avoids a full O(n) scan whenever possible:
 * - Moving right by 1: if the character stepped over was @c '\\n', increments
 *   @c edcon->buffer->current_line by 1.
 * - Moving left by 1: if the character stepped back over was @c '\\n', decrements
 *   @c edcon->buffer->current_line by 1.
 * - Larger jumps: calls pos_to_line() once and caches the result.
 * 
 * @param edcon The EditorContext Instance.
 * @param old_cursor Cursor position before the move.
 * @param new_cursor Cursor position after the move.
 */
void update_current_line_delta(EditorContext *edcon, int old_cursor, int new_cursor) {
    int delta = new_cursor - old_cursor;
    if (delta == 1) {
        if (old_cursor < gap_len(&edcon->buffer->text) &&
            gap_char(&edcon->buffer->text, old_cursor) == '\n')
            edcon->buffer->current_line++;
    } else if (delta == -1) {
        if (new_cursor >= 0 && new_cursor < gap_len(&edcon->buffer->text) &&
            gap_char(&edcon->buffer->text, new_cursor) == '\n')
            edcon->buffer->current_line--;
    } else {
        // arbitrary jump — full scan, but only once per key-press
        edcon->buffer->current_line = pos_to_line(edcon, new_cursor);
    }
}

/**
 * @brief Write a formatted message into the active buffer's status field.
 *
 * Accepts a printf-style format string and variadic arguments.  The result
 * is stored in @c E.status and rendered on the command bar during the next
 * call to refresh_screen().  The status is cleared after one frame.
 *
 * @param edcon The EditorContext Instance.
 * @param fmt printf-compatible format string.
 * @param ... Additional arguments corresponding to @p fmt.
 */
void set_status(EditorContext *edcon, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(edcon->buffer->status, sizeof edcon->buffer->status, fmt, ap);
    va_end(ap);
}