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
#include "buffer.h"

/**
 * @brief Handles the Main Input.
 * 
 * @param cfg_ptr A pointer to the Config Instance.
 * @param edcon The EditorContext Instance.
 * @return 0, False to stop the main programming if user quits. 1 to continue the programming
 */
int main_input(Config *cfg_ptr, EditorContext *edcon);

/**
 * @brief Read a single-line string from the user via the command bar.
 *
 * @param edcon The EditorContext Instance.
 * @param prompt Prompt string shown before the input area.
 * @param out Buffer to receive the entered string (NUL-terminated).
 * @param max Size of @p out in bytes, including the NUL terminator.
 * @return 1 if the user confirmed a non-empty string, 0 if cancelled or empty.
 */
int mini_input(EditorContext *edcon, const char *prompt, char *out, int max);

// --- Mouse Input ---

/**
 * @brief Convert a clicked screen column to a logical character offset within a line.
 *
 * @param cfg_ptr A pointer to the Config Instance.
 * @param edcon   The EditorContext Instance.
 * @param ln      Zero-based line number to search within.
 * @param target_vcol  Visual column the user clicked (after subtracting gutter width and col_off).
 * @return Logical character offset into the buffer.
 */
int vcol_to_pos(Config *cfg_ptr, EditorContext *edcon, int ln, int target_vcol);

/**
 * @brief Handle a mouse event from ncurses.
 *
 * @param cfg_ptr A pointer to the Config Instance.
 * @param edcon   The EditorContext Instance.
 */
void handle_mouse(Config *cfg_ptr, EditorContext *edcon);

// --- Navigation --- 

/**
 * @brief Move the cursor one line up, preserving visual column where possible.
 *
 * @param cfg_ptr A pointer to the Config Instance.
 * @param edcon The EditorContext Instance.
 */
void move_up(Config *cfg_ptr, EditorContext *edcon);

/**
 * @brief Move the cursor one line down, preserving visual column where possible.
 * 
 * @param cfg_ptr A pointer to the Config Instance.
 * @param edcon The EditorContext Instance.
 */
void move_down(Config *cfg_ptr, EditorContext *edcon);

/**
 * @brief Move the cursor one character to the left. 
 * 
 * @param edcon The EditorContext Instance.
 * */
void move_left(EditorContext *edcon);

/** 
 * @brief Move the cursor one character to the right. 
 * 
 * @param edcon The EditorContext Instance.
*/
void move_right(EditorContext *edcon);

/** 
 * @brief Move the cursor to the first character of the current line (Home). 
 * 
 * @param edcon The EditorContext Instance.
 * */
void move_line_start(EditorContext *edcon);

/** 
 * @brief Move the cursor past the last character of the current line (End). 
 * 
 * @param edcon The EditorContext Instance.
 * */
void move_line_end(EditorContext *edcon);

/**
 * @brief Scroll the view up by one full page (Page Up).
 * 
 * @param cfg_ptr A pointer to the Config Instance.
 * @param edcon The EditorContext Instance.
 */
void move_page_up(Config *cfg_ptr, EditorContext *edcon);

/**
 * @brief Scroll the view down by one full page (Page Down).
 * 
 * @param cfg_ptr A pointer to the Config Instance.
 * @param edcon The EditorContext Instance.
 */
void move_page_down(Config *cfg_ptr, EditorContext *edcon);

// --- Command Operations ---

/**
 * @brief Interactive forward search (Ctrl-F).
 * 
 * @param edcon The EditorContext Instance.
*/
void do_find(EditorContext *edcon);

/**
 * @brief Interactive find-and-replace (Ctrl-R).
 * 
 * @param edcon The EditorContext Instance.
 * */
void do_replace(EditorContext *edcon);

/** 
 * @brief Cut (copy + delete) the current line into the clipboard (Ctrl-K).
 * 
 * @param edcon The EditorContext Instance.
 * */
void cut_line(EditorContext *edcon);

/** 
 * @brief Paste the clipboard contents as a new line above the current line (Ctrl-U). 
 * 
 * @param edcon The EditorContext Instance.
 * */
void paste_line(EditorContext *edcon);

/** 
 * @brief Delete the current line without copying it to the clipboard (Ctrl-D). 
 * 
 * @param edcon The EditorContext Instance.
 * */
void delete_line(EditorContext *edcon);

/** 
 * @brief Prompt the user for a line number and jump to it (Ctrl-G). 
 * 
 * @param edcon The EditorContext Instance.
 * */
void goto_line(EditorContext *edcon);

// --- Quit Confirmation ---

/**
 * @brief Ask the user to confirm quitting when there are unsaved changes.
 *
 * @return 1 if the editor should exit, 0 if the quit was cancelled.
 */
int confirm_quit(EditorContext *edcon);

#endif // INPUT_H