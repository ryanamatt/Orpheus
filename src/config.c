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
#include "logger.h"
#include "config.h"
 
// /* Global config instance - defined here, declared extern in config.h. */
// Config cfg;

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
    strncpy(cfg->color_scheme, "default", sizeof(cfg->color_scheme) - 1);
    cfg->color_scheme[sizeof(cfg->color_scheme) - 1] = '\0';
}

/**
 * @brief Load user settings from @c ~/.orpheusrc.
 * 
 * Reads key=value from the user's home directory config file, skipping blank lines and lines with
 * @c #. Recognized keys and the global variables they populate.
 * 
 * | Key               | Variable        |
 * |-------------------|-----------------|
 * | tab_width         | TAB_WIDTH       |
 * | show_line_numbers | SHOW_LINE_NUMBERS |
 * | auto_indent       | AUTO_INDENT     |
 * | show_statusbar    | SHOW_STATUSBAR  |
 * | cursor_style      | CURSOR_STYLE    |
 * | color_scheme      | COLOR_SCHEME    |
 * | gutter_width      | GUTTER_WIDTH    |
 * | key_delay         | KEY_DELAY       |
 * 
 * If the file does not exist the function returns silently and all settings retain their defaults.
 */
void load_config(Config *c) {
    char path[512];
    snprintf(path, sizeof(path), "%s/.orpheusrc", getenv("HOME"));

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
                strncpy(c->color_scheme, val, sizeof(c->color_scheme - 1));
                c->color_scheme[sizeof(c->color_scheme) - 1] = '\0';
            }

            else if (strcmp(key, "gutter_width") == 0) {
                c->gutter_width = atoi(val);
            }

            else if (strcmp(key, "key_delay") == 0) {
                c->key_delay = atoi(val);
            }
        }
    }
}