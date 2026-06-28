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
 * @brief Sentinel meaning "not set in the config file - let color_scheme decide".
 *
 * Stored in each fg/bg color field below. Any ncurses color is >= -1
 * (-1 is COLOR_DEFAULT under use_default_colors()), so -2 is safe as an
 * "unset" marker that can never collide with a real resolved color.
 */
#define COLOR_UNSET (-2)

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

    /**
     * @name Per-pair color overrides
     *
     * Each field holds either an ncurses color value (COLOR_BLACK,
     * COLOR_WHITE, -1 for the terminal default, etc.) or COLOR_UNSET if the
     * user did not set it in the config file. When unset, init_ncurses()
     * falls back to whatever color_scheme would normally assign that pair.
     * Setting any color_*_fg or color_*_bg key in the config file overrides
     * just that one value, on top of (or instead of) color_scheme - so a
     * user can start from "dark" and only override the search highlight,
     * for example.
     *
     * Recognised color names (case-insensitive) for these keys:
     * black, red, green, yellow, blue, magenta, cyan, white, default.
     * @{
     */
    int color_normal_fg;  /**< Default: COLOR_UNSET */
    int color_normal_bg;  /**< Default: COLOR_UNSET */
    int color_status_fg;  /**< Default: COLOR_UNSET */
    int color_status_bg;  /**< Default: COLOR_UNSET */
    int color_cmdbar_fg;  /**< Default: COLOR_UNSET */
    int color_cmdbar_bg;  /**< Default: COLOR_UNSET */
    int color_lnum_fg;    /**< Default: COLOR_UNSET */
    int color_lnum_bg;    /**< Default: COLOR_UNSET */
    int color_search_fg;  /**< Default: COLOR_UNSET */
    int color_search_bg;  /**< Default: COLOR_UNSET */
    int color_select_fg;  /**< Default: COLOR_UNSET */
    int color_select_bg;  /**< Default: COLOR_UNSET */
    /** @} */
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
 * @brief Translate a color name from the config file into an ncurses color.
 *
 * Recognises (case-insensitive): "black", "red", "green", "yellow", "blue",
 * "magenta", "cyan", "white", and "default" (which maps to -1, the
 * terminal's own color under use_default_colors()).
 *
 * @param name Null-terminated color name, as read from the config file.
 * @return The corresponding ncurses color value, or COLOR_UNSET if @p name
 *         is not a recognised color (in which case the caller should leave
 *         the existing value untouched).
 */
int parse_color_name(const char *name);

/**
 * @brief Global editor configuration — initialised once in main().
 *
 * All modules access settings through this single instance rather than
 * through individual global variables.
 */
extern Config cfg;

#endif // CONFIG_H