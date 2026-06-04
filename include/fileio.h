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

#ifndef FILEIO_H
#define FILEIO_H

/**
 * @brief Load a file from disk into the active buffer.
 * 
 * @param edcon The EditorContext Instance.
 * @return 1 on success, 0 if the file could not be opened.
 */
int load_file(EditorContext *edcon);

/**
 * @brief Write the active buffer to edcon->buffer->filename.
 * 
 * @param edcon The EditorContext Instance.
 * @return 1 on success, 0 on failure or missing filename.
 */
int save_file(EditorContext *edcon);

#endif // FILEIO_H