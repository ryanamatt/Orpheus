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

#include <stdlib.h>
#include <string.h>
#include "logger.h"
#include "gap.h"

/**
 * @brief Initialize a Gap Buffer with a small default allocation.
 * 
 * Allocates @c CHUNK butes of raw storage, places the entire allocation inside the gap
 * (gap_start = 0, gap_end = size) and sets, the logical content length to zero.
 * 
 * @param g Pointer to uninitalized Gap Struct.  
 */
void gap_init(Gap *g) {
    g->size = CHUNK;
    g->buf = malloc(g->size);
    if (!g->buf) {
        log_error("gap_init: out of memory allocating %d bytes", g->size);
        exit(1);
    }
    g->gap_start = 0;
    g->gap_end = g->size;
}

/**
 * @brief Release the heap storage that a Gap Buffer owns.
 * 
 * @param g Pointer to the Gap Buffer to free.
 */
void gap_free(Gap *g) { free(g->buf); }

/**
 * @brief Return the logical length of a Gap.
 * 
 * length = size - (gap_end - gap_start)
 * 
 * @param g Pointer to Gap Buffer.
 * @return Number of content characters stored in the buffer.
 */
int gap_len(const Gap *g) { 
    return g->size - (g->gap_end - g->gap_start); 
}

/**
 * @brief Return the character at the logical position @p pos.
 * 
 * Positions before the gap are read directly. Positions at or after gap are
 * translated past the gap region before indexing the raw buffer.
 * 
 * @param g Pointer to the Gap buffer.
 * @param pos Zero-based logical character index (0 ... gap_len(g) - 1)
 * @return The character stored at @p pos.
 * 
 */
char gap_char(const Gap *g, int pos) {
    return pos < g->gap_start 
    ? g->buf[pos] 
    : g->buf[g->gap_end + (pos - g->gap_start)];
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
void gap_grow(Gap *g, int need) {
    int gap = g->gap_end - g->gap_start;
    if (gap >= need) return;

    int add  = need - gap + CHUNK;
    int nsz  = g->size + add;
    char *nb = realloc(g->buf, nsz);
    if (!nb) {
        log_error("gap_grow: realloc failed growing %d -> %d bytes", g->size, nsz);
        exit(1);
    }
    log_debug("gap_grow: realloc %d -> %d bytes", g->size, nsz);

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
void gap_shift_right(Gap *g) {
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
void gap_shift_left(Gap *g) {
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
void gap_move(Gap *g, int pos) {
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
    } 
    
    else {
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
void gap_insert(Gap *g, int pos, char c) {
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
void gap_delete(Gap *g, int pos) {
    gap_move(g, pos);
    if (g->gap_end < g->size) g->gap_end++;
}
