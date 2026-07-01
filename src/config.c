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
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <ncurses.h>
#include "logger.h"
#include "config.h"

/**
 * @brief Fill @p cfg with compiled in default values.
 * @param cfg Pointer to the Config to initiliaze.
 */
void config_defaults(Config *cfg) {
    cfg->tab_width         = 4;
    cfg->show_line_numbers = 1;
    cfg->auto_indent       = 1;
    cfg->show_statusbar    = 1;
    cfg->cursor_style      = 1;
    cfg->gutter_width      = 5;
    cfg->key_delay         = 50;
    cfg->focus_mode        = 0;
    cfg->focus_width       = 72;
    strncpy(cfg->color_scheme, "default", sizeof(cfg->color_scheme) - 1);
    cfg->color_scheme[sizeof(cfg->color_scheme) - 1] = '\0';
    strncpy(cfg->time_format, "%-m/%-d/%y", sizeof(cfg->time_format) - 1);
    cfg->time_format[sizeof(cfg->time_format) - 1] = '\0';

    cfg->color_normal_fg = COLOR_UNSET;
    cfg->color_normal_bg = COLOR_UNSET;
    cfg->color_status_fg = COLOR_UNSET;
    cfg->color_status_bg = COLOR_UNSET;
    cfg->color_cmdbar_fg = COLOR_UNSET;
    cfg->color_cmdbar_bg = COLOR_UNSET;
    cfg->color_lnum_fg   = COLOR_UNSET;
    cfg->color_lnum_bg   = COLOR_UNSET;
    cfg->color_search_fg = COLOR_UNSET;
    cfg->color_search_bg = COLOR_UNSET;
    cfg->color_select_fg = COLOR_UNSET;
    cfg->color_select_bg = COLOR_UNSET;
}

char* trim(char* str) {
    char* end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

/**
 * @brief Translate a color name from the config file into an ncurses color.
 *
 * Recognises (case-insensitive): "black", "red", "green", "yellow", "blue",
 * "magenta", "cyan", "white", and "default" (-1, the terminal's own color
 * under use_default_colors()).
 *
 * @param name Null-terminated color name, as read from the config file.
 * @return The corresponding ncurses color value, or COLOR_UNSET if @p name
 *         is not a recognised color.
 */
int parse_color_name(const char *name) {
    if (!name) return COLOR_UNSET;

    char buf[16];
    size_t len = strlen(name);
    if (len >= sizeof(buf)) return COLOR_UNSET;
    for (size_t i = 0; i < len; i++) buf[i] = (char)tolower((unsigned char)name[i]);
    buf[len] = '\0';

    if (strcmp(buf, "black")   == 0) return COLOR_BLACK;
    if (strcmp(buf, "red")     == 0) return COLOR_RED;
    if (strcmp(buf, "green")   == 0) return COLOR_GREEN;
    if (strcmp(buf, "yellow")  == 0) return COLOR_YELLOW;
    if (strcmp(buf, "blue")    == 0) return COLOR_BLUE;
    if (strcmp(buf, "magenta") == 0) return COLOR_MAGENTA;
    if (strcmp(buf, "cyan")    == 0) return COLOR_CYAN;
    if (strcmp(buf, "white")   == 0) return COLOR_WHITE;
    if (strcmp(buf, "default") == 0) return -1;

    log_error("parse_color_name: unrecognised color '%s' - ignoring", name);
    return COLOR_UNSET;
}

/**
 * @brief Build "$HOME/.config/Orpheus" into @p out.
 *
 * Shared by load_config() and the template loader so both agree on a single
 * source of truth for Orpheus's config directory. Does not create any
 * directories - it only builds the path string.
 *
 * @param out     Destination buffer.
 * @param outsize Capacity of @p out.
 * @return 1 on success, 0 if $HOME is unset or the result would not fit.
 */
int config_dir_path(char *out, int outsize) {
    const char *home = getenv("HOME");
    if (!home) {
        log_error("config_dir_path: $HOME is not set");
        return 0;
    }
    int n = snprintf(out, outsize, "%s/.config/%s", home, CONFIG_DIR_NAME);
    if (n < 0 || n >= outsize) {
        log_error("config_dir_path: path too long for buffer (%d bytes needed)", n);
        return 0;
    }
    return 1;
}

/**
 * @brief Match @p key against @p name and, if equal, resolve @p val as a
 *        color name into @p target.
 *
 * Strips trailing CR/LF from @p val (in place, same treatment as the other
 * string-valued keys in load_config()) before resolving it. If the color
 * name isn't recognised, parse_color_name() has already logged why and
 * returned COLOR_UNSET; in that case @p target is left untouched, so a
 * malformed line can't clobber a previously-set value with garbage.
 *
 * @param key    The trimmed key parsed from the current config line.
 * @param val    The raw value string parsed from the current config line.
 * @param name   The config key this color belongs to, e.g. "color_normal_fg".
 * @param target Pointer to the Config field to update on a match.
 * @return 1 if @p key matched @p name (regardless of whether the color was
 *         valid), 0 otherwise - so callers can chain this with else-if.
 */
static int apply_color_key(const char *key, char *val, const char *name, int *target) {
    if (strcmp(key, name) != 0) return 0;
    val[strcspn(val, "\r\n")] = 0;
    int resolved = parse_color_name(trim(val));
    if (resolved != COLOR_UNSET) *target = resolved;
    return 1;
}

/**
 * @brief Load user settings from @c ~/.config/Orpheus/orpheus.config.
 * 
 * Reads key=value from the user's config directory, skipping blank lines and lines with
 * @c #. Recognized keys and the global variables they populate.
 * 
 * If the file does not exist the function returns silently and all settings retain their defaults.
 */
void load_config(Config *c) {
    char dir[480];
    char path[512];

    if (!config_dir_path(dir, sizeof(dir))) {
        log_debug("load_config: could not resolve config directory - using defaults.");
        return;
    }
    snprintf(path, sizeof(path), "%s/%s", dir, CONFIG_FILE_NAME);

    FILE *pfile = fopen(path, "r");
    if (!pfile) {
        log_debug("load_config: no config file at '%s' - using defaults.", path);   
        return; // no config file use defaults
    }
    log_debug("load_config: reading '%s'", path);

    char line[128];
    while (fgets(line, sizeof(line), pfile)) {
        if (line[0] == '#' || line[0] == '\n') continue;

        char *key = strtok(line, "=");
        char *val = strtok(NULL, "=");

        if (key && val) {
            key = trim(key);

            if (strcmp(key, "tab_width") == 0) {
                int v = atoi(val);
                c->tab_width = (v > 0) ? v : 4;
            }

            else if (strcmp(key, "show_line_numbers") == 0) {
                c->show_line_numbers = atoi(val);
            }

            else if (strcmp(key, "auto_indent") == 0) {
                c->auto_indent = atoi(val);
            }

            else if (strcmp(key, "show_statusbar") == 0) {
                c->show_statusbar = atoi(val);
            }

            else if (strcmp(key, "cursor_style") == 0) {
                c->cursor_style = atoi(val);
            }

            else if (strcmp(key, "color_scheme") == 0) {
                val[strcspn(val, "\r\n")] = 0;
                strncpy(c->color_scheme, val, sizeof(c->color_scheme) - 1);
                c->color_scheme[sizeof(c->color_scheme) - 1] = '\0';
            }

            else if (strcmp(key, "gutter_width") == 0) {
                c->gutter_width = atoi(val);
            }

            else if (strcmp(key, "key_delay") == 0) {
                c->key_delay = atoi(val);
            }

            else if (strcmp(key, "focus_mode") == 0) {
                c->focus_mode = atoi(val);
            }

            else if (strcmp(key, "focus_width") == 0) {
                c->focus_width = atoi(val);
            }

            else if (strcmp(key, "time_format") == 0) {
                val[strcspn(val, "\r\n")] = 0;
                strncpy(c->time_format, val, sizeof(c->time_format) - 1);
                c->time_format[sizeof(c->time_format) - 1] = '\0';
            }

            else if (apply_color_key(key, val, "color_normal_fg", &c->color_normal_fg)) {}
            else if (apply_color_key(key, val, "color_normal_bg", &c->color_normal_bg)) {}
            else if (apply_color_key(key, val, "color_status_fg", &c->color_status_fg)) {}
            else if (apply_color_key(key, val, "color_status_bg", &c->color_status_bg)) {}
            else if (apply_color_key(key, val, "color_cmdbar_fg", &c->color_cmdbar_fg)) {}
            else if (apply_color_key(key, val, "color_cmdbar_bg", &c->color_cmdbar_bg)) {}
            else if (apply_color_key(key, val, "color_lnum_fg",   &c->color_lnum_fg))   {}
            else if (apply_color_key(key, val, "color_lnum_bg",   &c->color_lnum_bg))   {}
            else if (apply_color_key(key, val, "color_search_fg", &c->color_search_fg)) {}
            else if (apply_color_key(key, val, "color_search_bg", &c->color_search_bg)) {}
            else if (apply_color_key(key, val, "color_select_fg", &c->color_select_fg)) {}
            else if (apply_color_key(key, val, "color_select_bg", &c->color_select_bg)) {}
        }
    }
}