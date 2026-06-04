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

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include "buffer.h"
#include "fileio.h"


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
int load_file(const char *path) {
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
int save_file(void) {
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