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

#ifndef CONFIG_H
#define CONFIG_H

#define CONFIG_DIR_NAME   "Orpheus"
#define CONFIG_FILE_NAME  "orpheus.config"
#define TEMPLATES_DIRNAME "templates"

/**
 * @brief Runtime configuration loaded from ~/.config/Orpheus/orpheus.config.
 * 
 * All fields have defaults set by config_defaults(). load_config() overwrites
 * them with values found in the user's config file.
 */
typedef struct {
    int  tab_width;         /**< Spaces per Tab keypress. Default: 4 */
    int  show_line_numbers; /**< Show line-number gutter. Default: 1 */
    int  auto_indent;       /**< Copy leading whitespace on Enter. Default: 1 */
    int  show_statusbar;    /**< Show the status/command bar row. Default: 1 */
    int  cursor_style;      /**< 0=invisible 1=normal 2=block. Default: 1 */
    char color_scheme[32];  /**< "default" | "dark" | "light" | "mono" */
    int  gutter_width;      /**< Width of the line-number gutter. Default: 5 */
    int  key_delay;         /**< Escape-sequence processing delay ms. Default: 50 */
    int  focus_mode;        /**< 0=normal, 1=focus/typewriter mode (Ctrl-T). Default: 0 */
    int  focus_width;       /**< Text column width in focus mode. Default: 72 */
    char time_format[64];   /**< strftime() format string for the {{currentTime}}
                                  template variable. Default: "%-m/%-d/%y" (e.g. "6/25/26").
                                  Note: %-m/%-d (no leading zero) are glibc extensions;
                                  on non-glibc systems (e.g. macOS/BSD libc) fall back to
                                  %m/%d if the output looks wrong (zero-padded or literal). */
} Config;

/**
 * @brief Fill @p cfg with compiled-in default values.
 * @param cfg Pointer to the Config to initialise.
 */
void config_defaults(Config *cfg);

/**
 * @brief Load user settings from @c ~/.config/Orpheus/orpheus.config.
 * 
 * Reads key=value pairs, skipping blank lines and # comments. Unrecognised
 * keys are silently ignored. If the file does not exist the function returns
 * immediately, leaving @p cfg unchanged.
 * 
 * @param cfg Pointer to the Config to populate.
 */
void load_config(Config *cfg);

/**
 * @brief Build the absolute path of Orpheus's config directory.
 *
 * Writes "$HOME/.config/Orpheus" into @p out, creating no directories.
 * Used by load_config() and by the template loader so both agree on a
 * single source of truth for where Orpheus keeps its files.
 *
 * @param out     Destination buffer.
 * @param outsize Capacity of @p out.
 * @return 1 on success, 0 if $HOME is unset or the path would not fit.
 */
int config_dir_path(char *out, int outsize);

/**
 * @brief Global editor configuration — initialised once in main().
 *
 * All modules access settings through this single instance rather than
 * through individual global variables.
 */
extern Config cfg;

#endif // CONFIG_H