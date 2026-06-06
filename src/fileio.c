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
#include "logger.h"
#include "buffer.h"
#include "fileio.h"


/**
 * @brief Load a file from disk into the active buffer's Gap storage.
 * 
 * Open @p path for reading and appends each byte to the end of the Gap buffer. On success,
 * rebuild_line_count() is called to initialize the cached statistics, and the cursor is
 * rest to position 0.
 * 
 * @param edcon The EditorContext Instance.
 * @return 1 on success, 0 if file could not be opened.
 */
int load_file(EditorContext *edcon) {
    log_debug("load_file: attempting to open '%s'", edcon->buffer->filename);
    FILE *f = fopen(edcon->buffer->filename, "r");
    if (!f) {
        log_error("load_file: cannot open '%s' - %s", edcon->buffer->filename, strerror(errno));
        return 0;
    }

    int c;
    while ((c = fgetc(f)) != EOF)
        gap_insert(&edcon->buffer->text, gap_len(&edcon->buffer->text), (char)c);
        
    fclose(f);
    rebuild_line_count(edcon);   // initialise cached line_count + stats_dirty
    edcon->buffer->current_line = 0;
    log_debug("load_file: loaded '%s' - %d chars, %d lines",
              edcon->buffer->filename, gap_len(&edcon->buffer->text), edcon->buffer->line_count);
    return 1;
}

/**
 * @brief Write the contents of the active buffer to @c edcon->buffer->filename.
 *
 * Opens the file for writing (truncating it), iterates over every logical character in the 
 * Gap buffer, and writes each byte. On success the dirty flag is cleared and a confirmation 
 * is written to @c edcon->buffer->status.
 *
 * @param edcon The EditorContext Instance.
 * @return 1 on success, 0 if no filename is set or the file cannot be written.
 */
int save_file(EditorContext *edcon) {
    if (!edcon->buffer->filename[0]) {
        log_error("save_file: no filename set");
        set_status(edcon, "No filename - use Ctrl-W to set one");
        return 0;
    }
    log_debug("save_file: writing '%s'", edcon->buffer->filename);
    FILE *f = fopen(edcon->buffer->filename, "w");
    if (!f) { 
        log_error("save_file: cannot write '%s' - %s", edcon->buffer->filename, strerror(errno));
        set_status(edcon, "Cannot write: %s", strerror(errno)); 
        return 0; 
    }

    int len = gap_len(&edcon->buffer->text);
    for (int i = 0; i < len; i++) fputc(gap_char(&edcon->buffer->text, i), f);

    fclose(f);
    edcon->buffer->dirty = 0;
    log_debug("save_file: saved '%s' - %d bytes", edcon->buffer->filename, len);
    set_status(edcon, "Saved \"%s\"  (%d bytes)", edcon->buffer->filename, len);
    return 1;
}