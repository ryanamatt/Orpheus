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
}

char* trim(char* str) {
    char* end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
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
                c->tab_width = atoi(val);
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
        }
    }
}