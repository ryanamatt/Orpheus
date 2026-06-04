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

#ifndef INPUT_H
#define INPUT_H

#include "config.h"

/**
 * @brief Read a single-line string from the user via the command bar.
 *
 * @param prompt Prompt string shown before the input area.
 * @param out Buffer to receive the entered string (NUL-terminated).
 * @param max Size of @p out in bytes, including the NUL terminator.
 * @return 1 if the user confirmed a non-empty string, 0 if cancelled or empty.
 */
int mini_input(const char *prompt, char *out, int max);

// --- Navigation --- 

/**
 * @brief Move the cursor one line up, preserving visual column where possible.
 *
 * @param cfg_ptr A pointer to the Config Instance.
 */
void move_up(Config *cfg_ptr);

/**
 * @brief Move the cursor one line down, preserving visual column where possible.
 * 
 * @param cfg_ptr A pointer to the Config Instance.
 */
void move_down(Config *cfg_ptr);

/** @brief Move the cursor one character to the left. */
void move_left(void);

/** @brief Move the cursor one character to the right. */
void move_right(void);

/** @brief Move the cursor to the first character of the current line (Home). */
void move_line_start(void);

/** @brief Move the cursor past the last character of the current line (End). */
void move_line_end(void);

/**
 * @brief Scroll the view up by one full page (Page Up).
 * 
 * @param cfg_ptr A pointer to the Config Instance.
 */
void move_page_up(Config *cfg_ptr);

/**
 * @brief Scroll the view down by one full page (Page Down).
 * 
 * @param cfg_ptr A pointer to the Config Instance.
 */
void move_page_down(Config *cfg_ptr);

// --- Command Operations ---

/** @brief Interactive forward search (Ctrl-F). */
void do_find(void);

/** @brief Interactive find-and-replace (Ctrl-R).*/
void do_replace(void);

/**  @brief Cut (copy + delete) the current line into the clipboard (Ctrl-K). */
void cut_line(void);

/** @brief Paste the clipboard contents as a new line above the current line (Ctrl-U). */
void paste_line(void);

/** @brief Delete the current line without copying it to the clipboard (Ctrl-D). */
void delete_line(void);

/** @brief Prompt the user for a line number and jump to it (Ctrl-G). */
void goto_line(void);

// --- Quit Confirmation ---

/**
 * @brief Ask the user to confirm quitting when there are unsaved changes.
 *
 * @return 1 if the editor should exit, 0 if the quit was cancelled.
 */
int confirm_quit(void);

#endif // INPUT_H