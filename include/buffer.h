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
} Buffer;

/** Array of all open buffers. */
extern Buffer  buffers[MAX_BUFFERS];

/** Number of buffers currently open. */
extern int     buf_count;

/** Index of the currently active buffer. */
extern int     cur_buf;
 
/**
 * @brief Convenience macro — dereferences the active-buffer pointer.
 *
 * All code can write @c E.field instead of @c buffers[cur_buf].field.
 */
#define E (*E_ptr)
extern Buffer *E_ptr;

/**
 * @brief Allocate and zero-initialise the next buffer slot.
 * @return Index of the new buffer, or -1 if MAX_BUFFERS is reached.
 */
int  new_buffer(void);
 
/**
 * @brief Make buffer @p i the active buffer (wraps at both ends).
 * @param i Desired buffer index.
 */
void switch_buffer(int i);

/** @brief Return logical position of the first character on line @p ln. */
int  line_start(int ln);
 
/** @brief Return the character count of line @p ln (newline excluded). */
int  line_len(int ln);
 
/** @brief Return the total number of lines (O(1) via cache). */
int  total_lines(void);

/** @brief Full O(n) rebuild of the cached line count. */
void rebuild_line_count(void);
 
/** @brief Return the total character count of the active buffer. */
int  count_chars(void);

/** @brief Perform a full O(n) word count scan of the active buffer. */
int count_words_full(void);
 
/** @brief Return word count, rescanning only when stats_dirty is set. */
int  count_words(void);
 
/** @brief Incrementally update line and word stats after a single edit. */
void update_stats(char c, int delta);
 
/** 
 * @brief Return the visual (display) column of the cursor. 
 * 
 * @param cfg_ptr A pointer to the Config Instance.
 * */
int  cursor_vcol(Config *cfg_ptr);
 
/** @brief Convert a logical offset to a zero-based line number (O(n)). */
int  pos_to_line(int pos);
 
/**
 * @brief Incrementally synchronise E.current_line after a cursor move.
 * @param old_cursor Position before the move.
 * @param new_cursor Position after the move.
 */
void update_current_line_delta(int old_cursor, int new_cursor);
 
/** @brief Write a formatted message into E.status (printf-style). */
void set_status(const char *fmt, ...);

#endif // BUFFER_H