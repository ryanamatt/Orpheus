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

#ifndef DISPLAY_H
#define DISPLAY_H

#include "config.h"

/* ncurses colour-pair IDs used across display and input modules. */
#define CP_NORMAL 1
#define CP_STATUS 2
#define CP_CMDBAR 3
#define CP_LNUM   4
#define CP_SEARCH 5

/**
 * @brief Initialise ncurses and apply colour-scheme settings from cfg.
 *
 * Calls initscr(), raw(), noecho(), keypad(), set_escdelay(), starts colour
 * support (unless the scheme is "mono"), and sets the cursor shape.
 * 
 * @param cfg_ptr A pointer to the Config Instance.
 */
void init_ncurses(Config *cfg_ptr);

/**
 * @brief Adjust the viewport offsets so the cursor remainds visible.
 *
 * @param cfg_ptr A pointer to the Config Instance.
 */
void adjust_scroll(Config *cfg_ptr);

/**
 * @brief Render the visible text rows, including the line-number gutter.
 * 
 * @param cfg_ptr A pointer to the Config Instance.
 */
void draw_rows(Config *cfg_ptr);

/**
 * @brief Render the buffer tab bar when more than one buffer is open.
 * 
 * @param cfg_ptr A pointer to the Config Instance.
 */
void draw_tabbar(Config *cfg_ptr);

/**
 * @brief Render the status bar showing file info and cursor statistics.
 * 
 * @param cfg_ptr A pointer to the Config Instance.
 */
void draw_statusbar(Config *cfg_ptr);

/** @brief Render the command bar with keybinding hints and the status message. */
void draw_cmdbar(void);

/**
 * @brief Redraw the entire terminal display for the current frame.
 * 
 * @param cfg_ptr A pointer to the Config Instance.
 */
void refresh_screen(Config *cfg_ptr);

/**
 * @brief Toggle visibility of the status and command bars (Ctrl-W).
 * 
 * @param cfg_ptr A pointer to the Config Instance.
 */
void toggle_status(Config *cfg_ptr);

#endif // DISPLAY_H