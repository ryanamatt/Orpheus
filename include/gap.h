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

#ifndef GAP_H
#define GAP_H

/** Growth step in bytes when the gap needs to be expanded. */
#define CHUNK 64

/**
 * @brief A gap buffer for efficient single-character insertion and deletion.
 *
 * The logical content is split into two contiguous regions: bytes before the
 * gap (@c buf[0 .. gap_start-1]) and bytes after it (@c buf[gap_end .. size-1]).
 * Inserting or deleting at @c gap_start costs O(1). Moving the gap costs O(|delta|).
 */
typedef struct {
    char *buf;       /**< Raw backing storage. */
    int   gap_start; /**< Index of first gap byte. */
    int   gap_end;   /**< Index of first byte after the gap. */
    int   size;      /**< Total allocated bytes. */
} Gap;

/** @brief Initialise @p g with a small default allocation. */
void gap_init(Gap *g);
 
/** @brief Release the heap storage owned by @p g. */
void gap_free(Gap *g);
 
/** @brief Return the logical content length of @p g. */
int  gap_len(const Gap *g);
 
/** @brief Return the character at logical position @p pos. */
char gap_char(const Gap *g, int pos);

/** @brief Ensure the gap is at least @p need bytes wide, growing if necessary. */
void gap_grow(Gap *g, int need);

/** @brief Move the gap position one position to the right. */
void gap_shift_right(Gap *g);

/** @brief Move the gap one position to the left. */
void gap_shift_left(Gap *g);

/** @brief Move the gap so that is start aligns with logical position @p pos. */
void gap_move(Gap *g, int pos);
 
/** @brief Insert character @p c at logical position @p pos. */
void gap_insert(Gap *g, int pos, char c);
 
/** @brief Delete the character at logical position @p pos. */
void gap_delete(Gap *g, int pos);

#endif // GAP_H