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
#include <time.h>
#include "logger.h"
#include "template.h"

#define TEMPLATE_EXT     ".tmpl"
#define MAX_TEMPLATE_SIZE (64 * 1024)  // sanity cap - templates are headers, not novels

/**
 * @brief A single @c {{name}} -> expansion mapping.
 *
 * Only one variable exists today (@c currentTime), but the table makes
 * adding more (e.g. @c {{filename}}, @c {{author}}) a one-line change
 * rather than a new branch of bespoke string-matching code.
 */
typedef struct {
    const char *name;   /**< Variable name, without braces (e.g. "currentTime"). */
    char value[128];    /**< Expanded value, filled in just before substitution. */
} TemplateVar;

int templates_dir_path(char *out, int outsize) {
    char dir[480];
    if (!config_dir_path(dir, sizeof(dir))) return 0;
    int n = snprintf(out, outsize, "%s/%s", dir, TEMPLATES_DIRNAME);
    if (n < 0 || n >= outsize) {
        log_error("templates_dir_path: path too long for buffer (%d bytes needed)", n);
        return 0;
    }
    return 1;
}

/**
 * @brief Read an entire file into a heap-allocated, NUL-terminated buffer.
 *
 * @param path Path to read.
 * @param out_len If non-NULL, receives the number of bytes read (excluding
 *                the NUL terminator).
 * @return Newly malloc()'d buffer the caller must free(), or NULL on
 *         failure (missing file, read error, or file larger than
 *         MAX_TEMPLATE_SIZE).
 */
static char *read_whole_file(const char *path, long *out_len) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;

    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long size = ftell(f);
    if (size < 0 || size > MAX_TEMPLATE_SIZE) {
        log_error("read_whole_file: '%s' is too large or unreadable (%ld bytes)", path, size);
        fclose(f);
        return NULL;
    }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return NULL; }

    char *buf = malloc((size_t)size + 1);
    if (!buf) { fclose(f); return NULL; }

    size_t got = fread(buf, 1, (size_t)size, f);
    fclose(f);
    buf[got] = '\0';

    if (out_len) *out_len = (long)got;
    return buf;
}

/**
 * @brief Expand every @c {{name}} placeholder in @p src against @p vars.
 *
 * Single left-to-right pass. Unrecognised @c {{...}} placeholders are left
 * verbatim in the output (rather than silently deleted) so a typo in a
 * template is visible instead of invisible.
 *
 * @param src     Raw template text (NUL-terminated).
 * @param vars    Array of known variables.
 * @param n_vars  Number of entries in @p vars.
 * @param out_len Receives the length of the returned buffer (excl. NUL).
 * @return Newly malloc()'d, NUL-terminated expanded text. Caller must free().
 */
static char *expand_vars(const char *src, const TemplateVar *vars, int n_vars, long *out_len) {
    size_t src_len = strlen(src);
    // Worst case every byte is literal text with no substitution shrinkage.
    // generous variable values are bounded by sizeof(TemplateVar.value), so
    // double the source length as a safe upper bound on growth.
    size_t cap = src_len * 2 + 256;
    char *out = malloc(cap);
    if (!out) return NULL;

    size_t oi = 0;
    for (size_t i = 0; i < src_len; ) {
        if (src[i] == '{' && i + 1 < src_len && src[i + 1] == '{') {
            size_t close = i + 2;
            while (close + 1 < src_len && !(src[close] == '}' && src[close + 1] == '}'))
                close++;

            if (close + 1 < src_len && src[close] == '}' && src[close + 1] == '}') {
                size_t name_len = close - (i + 2);
                int matched = 0;
                for (int v = 0; v < n_vars; v++) {
                    if (strlen(vars[v].name) == name_len &&
                        strncmp(vars[v].name, src + i + 2, name_len) == 0) {
                        size_t vlen = strlen(vars[v].value);
                        if (oi + vlen >= cap) { cap = (oi + vlen) * 2; out = realloc(out, cap); }
                        memcpy(out + oi, vars[v].value, vlen);
                        oi += vlen;
                        matched = 1;
                        break;
                    }
                }
                if (!matched) {
                    // Unknown {{...}} - copy it through verbatim, braces included.
                    size_t whole_len = (close + 2) - i;
                    if (oi + whole_len >= cap) { cap = (oi + whole_len) * 2; out = realloc(out, cap); }
                    memcpy(out + oi, src + i, whole_len);
                    oi += whole_len;
                }
                i = close + 2;
                continue;
            }
            // No closing "}}" found - fall through and copy '{' literally.
        }

        if (oi + 1 >= cap) { cap *= 2; out = realloc(out, cap); }
        out[oi++] = src[i++];
    }

    out[oi] = '\0';
    if (out_len) *out_len = (long)oi;
    return out;
}

int apply_template(Config *cfg_ptr, EditorContext *edcon, const char *name) {
    char dir[480];
    if (!templates_dir_path(dir, sizeof(dir))) {
        set_status(edcon, "Cannot resolve templates directory");
        return 0;
    }

    char path[512];
    int n = snprintf(path, sizeof(path), "%s/%s%s", dir, name, TEMPLATE_EXT);
    if (n < 0 || n >= (int)sizeof(path)) {
        log_error("apply_template: template path too long for '%s'", name);
        set_status(edcon, "Template name too long");
        return 0;
    }

    long raw_len = 0;
    char *raw = read_whole_file(path, &raw_len);
    if (!raw) {
        log_error("apply_template: cannot read template '%s'", path);
        set_status(edcon, "Template not found: \"%s\"", name);
        return 0;
    }

    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    TemplateVar vars[1];
    vars[0].name = "currentTime";
    if (t) strftime(vars[0].value, sizeof(vars[0].value), cfg_ptr->time_format, t);
    else   vars[0].value[0] = '\0';

    long expanded_len = 0;
    char *expanded = expand_vars(raw, vars, 1, &expanded_len);
    free(raw);
    if (!expanded) {
        log_error("apply_template: out of memory expanding '%s'", path);
        set_status(edcon, "Out of memory applying template");
        return 0;
    }

    for (long i = 0; i < expanded_len; i++)
        gap_insert(&edcon->buffer->text, i, expanded[i]);
    free(expanded);

    edcon->buffer->cursor       = (int)expanded_len;
    edcon->buffer->dirty        = 1;
    rebuild_line_count(edcon);
    edcon->buffer->current_line = pos_to_line(edcon, edcon->buffer->cursor);

    log_debug("apply_template: applied '%s' (%ld bytes expanded from %ld)",
              path, expanded_len, raw_len);
    set_status(edcon, "Applied template \"%s\"", name);
    return 1;
}