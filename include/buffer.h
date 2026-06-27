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
 
#ifndef BUFFER_H
#define BUFFER_H

#include "config.h"
#include "gap.h"
 
#define MAX_STATUS  512
#define MAX_BUFFERS 32

/**
 * @brief All state associated with a single open file.
 */
typedef struct {
    Gap  text;                  /**< Gap buffer holding the file contents. */
    int  cursor;                /**< Logical character offset of the cursor. */
    int  row_off;               /**< First visible row (scroll offset). */
    int  col_off;               /**< First visible column (scroll offset). */
    int  rows, cols;            /**< Terminal size, refreshed every frame. */
    int  dirty;                 /**< Non-zero when there are unsaved changes. */
    char filename[256];         /**< Path of the open file, or "" for [No Name]. */
    char status[MAX_STATUS];    /**< One-shot status message shown for one frame. */
    char clipboard[4096];       /**< Cut/paste buffer. */
    int  cb_len;                /**< Number of valid bytes in @c clipboard. */
    int  last_search_pos;       /**< Cursor position at last Ctrl-F search. */
    char search_term[256];      /**< Most recent search string. */
    int  line_count;            /**< Total lines (newlines + 1), kept incrementally. */
    int  word_count;            /**< Cached word count; invalid when stats_dirty. */
    int  stats_dirty;           /**< Non-zero when word_count needs a full rescan. */
    int  current_line;          /**< Zero-based line the cursor is on. */
    int sel_active;             /**< 1 while a selection exists (drag in progress completed and not yet cleared by another edit/move). */
    int sel_anchor;             /**< logical position where the selection/drag started. */
    int sel_start;              /** normalised (start <= end) logical range, recomputed any time the anchor or cursor changes while a selection is active. sel_end is exclusive.*/
    int sel_end;                /** normalised (start <= end) logical range, recomputed any time the anchor or cursor changes while a selection is active. sel_end is exclusive.*/
} Buffer;

/**
 * @brief All states associated with the context of the editor.
 */
typedef struct {
    Buffer buffers[MAX_BUFFERS];    /**< Array of all open buffers. */
    int buf_count;                  /**< Number of buffers currently open. */
    int cur_buf;                    /**< Index of the currently active buffer. */
    Buffer *buffer;                 /**< Dereferences the active-buffer pointer. */
} EditorContext;

/**
 * @brief Allocate and zero-initialise the next buffer slot.
 * 
 * @param edcon The EditorContext Instance.
 * @return Index of the new buffer, or -1 if MAX_BUFFERS is reached.
 */
int new_buffer(EditorContext *edcon);
 
/**
 * @brief Make buffer @p i the active buffer (wraps at both ends).
 * 
 * @param edcon The EditorContext Instance.
 * @param i Desired buffer index.
 */
void switch_buffer(EditorContext *edcon, int i);

/** 
 * @brief Return logical position of the first character on line @p ln. 
 * 
 * @param edcon The EditorContext Instance.
 * */
int  line_start(EditorContext *edcon, int ln);
 
/** 
 * @brief Return the character count of line @p ln (newline excluded). 
 * 
 * @param edcon The EditorContext Instance.
 * */
int  line_len(EditorContext *edcon, int ln);
 
/** 
 * @brief Return the total number of lines (O(1) via cache). 
 * 
 * @param edcon The EditorContext Instance.
 * */
int total_lines(EditorContext *edcon);

/** 
 * @brief Full O(n) rebuild of the cached line count. 
 * 
 * @param edcon The EditorContext Instance.
 * */
void rebuild_line_count(EditorContext *edcon);
 
/** 
 * @brief Return the total character count of the active buffer. 
 * 
 * @param edcon The EditorContext Instance.
 * */
int count_chars(EditorContext *edcon);

/** 
 * @brief Perform a full O(n) word count scan of the active buffer. 
 * 
 * @param edcon The EditorContext Instance.
 * */
int count_words_full(EditorContext *edcon);
 
/** 
 * @brief Return word count, rescanning only when stats_dirty is set. 
 * 
 * @param edcon The EditorContext Instance.
 * */
int  count_words(EditorContext *edcon);
 
/** 
 * @brief Incrementally update line and word stats after a single edit. 
 * 
 * @param edcon The EditorContext Instance.
 * */
void update_stats(EditorContext *edcon, char c, int delta);
 
/** 
 * @brief Return the visual (display) column of the cursor. 
 * 
 * @param cfg_ptr A pointer to the Config Instance.
 * @param edcon The EditorContext Instance.
 * */
int cursor_vcol(Config *cfg_ptr, EditorContext *edcon);
 
/** 
 * @brief Convert a logical offset to a zero-based line number (O(n)). 
 * 
 * @param edcon The EditorContext Instance.
 * */
int pos_to_line(EditorContext *edcon, int pos);
 
/**
 * @brief Incrementally synchronise E.current_line after a cursor move.
 * 
 * @param edcon The EditorContext Instance.
 * @param old_cursor Position before the move.
 * @param new_cursor Position after the move.
 */
void update_current_line_delta(EditorContext *edcon, int old_cursor, int new_cursor);

// --- Mouse Selection Helpers ---

/**
 * @brief Clear any active selection on the active buffer.
 *
 * Resets @c sel_active to 0. The anchor/start/end values are left as-is
 * (stale) since they are only ever read while @c sel_active is set.
 *
 * @param edcon The EditorContext Instance.
 */
void clear_selection(EditorContext *edcon);

/**
 * @brief Begin a new selection anchored at logical position @p pos.
 *
 * Called on a left-mouse-button press in the text area. Until update_selection()
 * is called again the selection is a zero-length range at @p pos.
 *
 * @param edcon The EditorContext Instance.
 * @param pos   Logical character offset where the drag/selection started.
 */
void start_selection(EditorContext *edcon, int pos);

/**
 * @brief Extend the in-progress selection to logical position @p pos.
 *
 * Recomputes the normalised [sel_start, sel_end) range from the fixed
 * @c sel_anchor and the new endpoint @p pos, so the range is always valid
 * regardless of whether the user dragged left or right of the anchor.
 * Does nothing if no selection is active (call start_selection() first).
 *
 * @param edcon The EditorContext Instance.
 * @param pos   Logical character offset of the current drag/cursor position.
 */
void update_selection(EditorContext *edcon, int pos);

/**
 * @brief Report whether the active buffer currently has a non-empty selection.
 *
 * @param edcon The EditorContext Instance.
 * @return 1 if @c sel_active is set and the range is non-empty, 0 otherwise.
 */
int has_selection(EditorContext *edcon);

/**
 * @brief Fetch the normalised selection range, if any.
 *
 * @param edcon The EditorContext Instance.
 * @param start Out-param receiving the (inclusive) start offset.
 * @param end   Out-param receiving the (exclusive) end offset.
 * @return 1 and populates the out-params if a non-empty selection exists,
 *         0 otherwise (out-params left untouched).
 */
int selection_range(EditorContext *edcon, int *start, int *end);

 
/** 
 * @brief Write a formatted message into E.status (printf-style). 
 * 
 * @param edcon The EditorContext Instance.
 * @param fmt printf-compatible format string.
 * @param ... Additional arguments corresponding to @p fmt.
 * */
void set_status(EditorContext *edcon, const char *fmt, ...);

#endif // BUFFER_H