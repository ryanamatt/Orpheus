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

#ifndef TEMPLATE_H
#define TEMPLATE_H

#include "config.h"
#include "buffer.h"

/**
 * @brief Build the absolute path of Orpheus's templates directory.
 *
 * Writes "$HOME/.config/Orpheus/templates" into @p out. Does not create
 * any directories - it only builds the path string.
 *
 * @param out     Destination buffer.
 * @param outsize Capacity of @p out.
 * @return 1 on success, 0 if $HOME is unset or the path would not fit.
 */
int templates_dir_path(char *out, int outsize);

/**
 * @brief Populate the active buffer with an expanded template.
 *
 * Looks up @c ~/.config/Orpheus/templates/<name>.tmpl, reads it in full,
 * expands every recognised @c {{variable}} placeholder (currently just
 * @c {{currentTime}}, formatted with @c cfg_ptr->time_format), and inserts
 * the result into @c edcon->buffer->text at position 0. Sets the dirty
 * flag and rebuilds line/word statistics on success.
 *
 * Intended to be called once, immediately after new_buffer()/switch_buffer(),
 * before the buffer has any other content - it does not check or preserve
 * existing text.
 *
 * @param cfg_ptr  Pointer to the Config Instance (supplies time_format).
 * @param edcon    The EditorContext Instance.
 * @param name     Template name as given on the command line, without the
 *                 .tmpl extension (e.g. "chapter" for "chapter.tmpl").
 * @return 1 on success, 0 if the template file could not be found/read.
 */
int apply_template(Config *cfg_ptr, EditorContext *edcon, const char *name);

#endif // TEMPLATE_H