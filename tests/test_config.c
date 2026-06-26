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

#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>
#include "../include/config.h"
#include "test_framework.h"

/* -----------------------------------------------------------------------
 * Helpers
 *
 * load_config() reads from $HOME/.config/Orpheus/orpheus.config, so we
 * redirect HOME to a temp directory and write a known config file there for
 * each test that needs it. Tests that want no config file point HOME at a
 * fresh tmpdir with nothing written into it.
 * ----------------------------------------------------------------------- */

/* Write content to $dir/.config/Orpheus/orpheus.config. Returns 1 on success, 0 on failure. */
static int write_rc(const char *dir, const char *content) {
    char topdir[PATH_MAX];
    char confdir[PATH_MAX];
    char path[PATH_MAX];

    if (snprintf(topdir, sizeof(topdir), "%s/.config", dir) >= sizeof(topdir)) return 0;
    mkdir(topdir, 0700);

    if (snprintf(confdir, sizeof(confdir), "%s/Orpheus", topdir) >= sizeof(confdir)) return 0;
    mkdir(confdir, 0700);

    if (snprintf(path, sizeof(path), "%s/orpheus.config", confdir) >= sizeof(path)) return 0;

    FILE *f = fopen(path, "w");
    if (!f) return 0;
    fputs(content, f);
    fclose(f);
    return 1;
}

/* Create a unique temp directory under /tmp and return it via `out`
 * (which must hold at least 64 bytes). Returns 1 on success. */
static int make_tmpdir(char *out) {
    strcpy(out, "/tmp/orpheus_test_XXXXXX");
    if (!mkdtemp(out)) return 0;
    return 1;
}

/* Remove the orpheus.config (and its Orpheus/.config dirs) from a temp dir
 * (best-effort). */
static void cleanup_rc(const char *dir) {
    char path[512];
    char orpdir[512];
    char confdir[512];
    snprintf(path, sizeof(path), "%s/.config/Orpheus/orpheus.config", dir);
    remove(path);
    snprintf(orpdir, sizeof(orpdir), "%s/.config/Orpheus", dir);
    rmdir(orpdir);
    snprintf(confdir, sizeof(confdir), "%s/.config", dir);
    rmdir(confdir);
    rmdir(dir);
}

/* -----------------------------------------------------------------------
 * config_defaults
 * ----------------------------------------------------------------------- */

TEST(defaults_tab_width) {
    Config c;
    config_defaults(&c);
    ASSERT_EQ(c.tab_width, 4);
}

TEST(defaults_show_line_numbers) {
    Config c;
    config_defaults(&c);
    ASSERT_EQ(c.show_line_numbers, 1);
}

TEST(defaults_auto_indent) {
    Config c;
    config_defaults(&c);
    ASSERT_EQ(c.auto_indent, 1);
}

TEST(defaults_show_statusbar) {
    Config c;
    config_defaults(&c);
    ASSERT_EQ(c.show_statusbar, 1);
}

TEST(defaults_cursor_style) {
    Config c;
    config_defaults(&c);
    ASSERT_EQ(c.cursor_style, 1);
}

TEST(defaults_gutter_width) {
    Config c;
    config_defaults(&c);
    ASSERT_EQ(c.gutter_width, 5);
}

TEST(defaults_key_delay) {
    Config c;
    config_defaults(&c);
    ASSERT_EQ(c.key_delay, 50);
}

TEST(defaults_focus_mode) {
    Config c;
    config_defaults(&c);
    ASSERT_EQ(c.focus_mode, 0);
}

TEST(defaults_focus_width) {
    Config c;
    config_defaults(&c);
    ASSERT_EQ(c.focus_width, 72);
}

TEST(defaults_color_scheme) {
    Config c;
    config_defaults(&c);
    ASSERT_STR_EQ(c.color_scheme, "default");
}

TEST(defaults_time_format) {
    Config c;
    config_defaults(&c);
    ASSERT_STR_EQ(c.time_format, "%-m/%-d/%y");
}

/* -----------------------------------------------------------------------
 * load_config — no rc file
 * ----------------------------------------------------------------------- */

TEST(load_config_no_file_leaves_defaults) {
    /* Point HOME at /tmp where there is definitely no .orpheusrc
     * (or one we haven't created). Use a dedicated subdir to be safe. */
    char tmpdir[64];
    if (!make_tmpdir(tmpdir)) { ASSERT_TRUE(0); return; }

    char *old_home = getenv("HOME");
    setenv("HOME", tmpdir, 1);

    Config c;
    config_defaults(&c);
    load_config(&c);

    if (old_home) setenv("HOME", old_home, 1);
    else          unsetenv("HOME");
    rmdir(tmpdir);

    ASSERT_EQ(c.tab_width, 4);
    ASSERT_EQ(c.show_line_numbers, 1);
    ASSERT_STR_EQ(c.color_scheme, "default");
}

/* -----------------------------------------------------------------------
 * load_config — integer keys
 * ----------------------------------------------------------------------- */

TEST(load_config_tab_width) {
    char tmpdir[64];
    if (!make_tmpdir(tmpdir)) { ASSERT_TRUE(0); return; }
    write_rc(tmpdir, "tab_width=2\n");

    char *old_home = getenv("HOME");
    setenv("HOME", tmpdir, 1);

    Config c;
    config_defaults(&c);
    load_config(&c);

    if (old_home) setenv("HOME", old_home, 1);
    else          unsetenv("HOME");
    cleanup_rc(tmpdir);

    ASSERT_EQ(c.tab_width, 2);
}

TEST(load_config_show_line_numbers_off) {
    char tmpdir[64];
    if (!make_tmpdir(tmpdir)) { ASSERT_TRUE(0); return; }
    write_rc(tmpdir, "show_line_numbers=0\n");

    char *old_home = getenv("HOME");
    setenv("HOME", tmpdir, 1);

    Config c;
    config_defaults(&c);
    load_config(&c);

    if (old_home) setenv("HOME", old_home, 1);
    else          unsetenv("HOME");
    cleanup_rc(tmpdir);

    ASSERT_EQ(c.show_line_numbers, 0);
}

TEST(load_config_auto_indent_off) {
    char tmpdir[64];
    if (!make_tmpdir(tmpdir)) { ASSERT_TRUE(0); return; }
    write_rc(tmpdir, "auto_indent=0\n");

    char *old_home = getenv("HOME");
    setenv("HOME", tmpdir, 1);

    Config c;
    config_defaults(&c);
    load_config(&c);

    if (old_home) setenv("HOME", old_home, 1);
    else          unsetenv("HOME");
    cleanup_rc(tmpdir);

    ASSERT_EQ(c.auto_indent, 0);
}

TEST(load_config_show_statusbar_off) {
    char tmpdir[64];
    if (!make_tmpdir(tmpdir)) { ASSERT_TRUE(0); return; }
    write_rc(tmpdir, "show_statusbar=0\n");

    char *old_home = getenv("HOME");
    setenv("HOME", tmpdir, 1);

    Config c;
    config_defaults(&c);
    load_config(&c);

    if (old_home) setenv("HOME", old_home, 1);
    else          unsetenv("HOME");
    cleanup_rc(tmpdir);

    ASSERT_EQ(c.show_statusbar, 0);
}

TEST(load_config_cursor_style) {
    char tmpdir[64];
    if (!make_tmpdir(tmpdir)) { ASSERT_TRUE(0); return; }
    write_rc(tmpdir, "cursor_style=2\n");

    char *old_home = getenv("HOME");
    setenv("HOME", tmpdir, 1);

    Config c;
    config_defaults(&c);
    load_config(&c);

    if (old_home) setenv("HOME", old_home, 1);
    else          unsetenv("HOME");
    cleanup_rc(tmpdir);

    ASSERT_EQ(c.cursor_style, 2);
}

TEST(load_config_gutter_width) {
    char tmpdir[64];
    if (!make_tmpdir(tmpdir)) { ASSERT_TRUE(0); return; }
    write_rc(tmpdir, "gutter_width=8\n");

    char *old_home = getenv("HOME");
    setenv("HOME", tmpdir, 1);

    Config c;
    config_defaults(&c);
    load_config(&c);

    if (old_home) setenv("HOME", old_home, 1);
    else          unsetenv("HOME");
    cleanup_rc(tmpdir);

    ASSERT_EQ(c.gutter_width, 8);
}

TEST(load_config_key_delay) {
    char tmpdir[64];
    if (!make_tmpdir(tmpdir)) { ASSERT_TRUE(0); return; }
    write_rc(tmpdir, "key_delay=100\n");

    char *old_home = getenv("HOME");
    setenv("HOME", tmpdir, 1);

    Config c;
    config_defaults(&c);
    load_config(&c);

    if (old_home) setenv("HOME", old_home, 1);
    else          unsetenv("HOME");
    cleanup_rc(tmpdir);

    ASSERT_EQ(c.key_delay, 100);
}

TEST(load_config_focus_mode) {
    char tmpdir[64];
    if (!make_tmpdir(tmpdir)) { ASSERT_TRUE(0); return; }
    write_rc(tmpdir, "focus_mode=1\n");

    char *old_home = getenv("HOME");
    setenv("HOME", tmpdir, 1);

    Config c;
    config_defaults(&c);
    load_config(&c);

    if (old_home) setenv("HOME", old_home, 1);
    else          unsetenv("HOME");
    cleanup_rc(tmpdir);

    ASSERT_EQ(c.focus_mode, 1);
}

TEST(load_config_focus_width) {
    char tmpdir[64];
    if (!make_tmpdir(tmpdir)) { ASSERT_TRUE(0); return; }
    write_rc(tmpdir, "focus_width=80\n");

    char *old_home = getenv("HOME");
    setenv("HOME", tmpdir, 1);

    Config c;
    config_defaults(&c);
    load_config(&c);

    if (old_home) setenv("HOME", old_home, 1);
    else          unsetenv("HOME");
    cleanup_rc(tmpdir);

    ASSERT_EQ(c.focus_width, 80);
}

/* -----------------------------------------------------------------------
 * load_config — color_scheme string key
 * ----------------------------------------------------------------------- */

TEST(load_config_color_scheme_dark) {
    char tmpdir[64];
    if (!make_tmpdir(tmpdir)) { ASSERT_TRUE(0); return; }
    write_rc(tmpdir, "color_scheme=dark\n");

    char *old_home = getenv("HOME");
    setenv("HOME", tmpdir, 1);

    Config c;
    config_defaults(&c);
    load_config(&c);

    if (old_home) setenv("HOME", old_home, 1);
    else          unsetenv("HOME");
    cleanup_rc(tmpdir);

    ASSERT_STR_EQ(c.color_scheme, "dark");
}

TEST(load_config_color_scheme_light) {
    char tmpdir[64];
    if (!make_tmpdir(tmpdir)) { ASSERT_TRUE(0); return; }
    write_rc(tmpdir, "color_scheme=light\n");

    char *old_home = getenv("HOME");
    setenv("HOME", tmpdir, 1);

    Config c;
    config_defaults(&c);
    load_config(&c);

    if (old_home) setenv("HOME", old_home, 1);
    else          unsetenv("HOME");
    cleanup_rc(tmpdir);

    ASSERT_STR_EQ(c.color_scheme, "light");
}

TEST(load_config_color_scheme_mono) {
    char tmpdir[64];
    if (!make_tmpdir(tmpdir)) { ASSERT_TRUE(0); return; }
    write_rc(tmpdir, "color_scheme=mono\n");

    char *old_home = getenv("HOME");
    setenv("HOME", tmpdir, 1);

    Config c;
    config_defaults(&c);
    load_config(&c);

    if (old_home) setenv("HOME", old_home, 1);
    else          unsetenv("HOME");
    cleanup_rc(tmpdir);

    ASSERT_STR_EQ(c.color_scheme, "mono");
}

/* -----------------------------------------------------------------------
 * load_config — time_format string key
 * ----------------------------------------------------------------------- */

TEST(load_config_time_format) {
    char tmpdir[64];
    if (!make_tmpdir(tmpdir)) { ASSERT_TRUE(0); return; }
    write_rc(tmpdir, "time_format=%Y-%m-%d\n");

    char *old_home = getenv("HOME");
    setenv("HOME", tmpdir, 1);

    Config c;
    config_defaults(&c);
    load_config(&c);

    if (old_home) setenv("HOME", old_home, 1);
    else          unsetenv("HOME");
    cleanup_rc(tmpdir);

    ASSERT_STR_EQ(c.time_format, "%Y-%m-%d");
}

/* -----------------------------------------------------------------------
 * load_config — robustness (comments, blank lines, unknown keys)
 * ----------------------------------------------------------------------- */

TEST(load_config_ignores_comments) {
    char tmpdir[64];
    if (!make_tmpdir(tmpdir)) { ASSERT_TRUE(0); return; }
    write_rc(tmpdir,
             "# this is a comment\n"
             "tab_width=8\n"
             "# another comment\n");

    char *old_home = getenv("HOME");
    setenv("HOME", tmpdir, 1);

    Config c;
    config_defaults(&c);
    load_config(&c);

    if (old_home) setenv("HOME", old_home, 1);
    else          unsetenv("HOME");
    cleanup_rc(tmpdir);

    ASSERT_EQ(c.tab_width, 8);
}

TEST(load_config_ignores_blank_lines) {
    char tmpdir[64];
    if (!make_tmpdir(tmpdir)) { ASSERT_TRUE(0); return; }
    write_rc(tmpdir, "\n\ntab_width=3\n\n");

    char *old_home = getenv("HOME");
    setenv("HOME", tmpdir, 1);

    Config c;
    config_defaults(&c);
    load_config(&c);

    if (old_home) setenv("HOME", old_home, 1);
    else          unsetenv("HOME");
    cleanup_rc(tmpdir);

    ASSERT_EQ(c.tab_width, 3);
}

TEST(load_config_ignores_unknown_keys) {
    char tmpdir[64];
    if (!make_tmpdir(tmpdir)) { ASSERT_TRUE(0); return; }
    write_rc(tmpdir, "nonexistent_key=99\ntab_width=6\n");

    char *old_home = getenv("HOME");
    setenv("HOME", tmpdir, 1);

    Config c;
    config_defaults(&c);
    load_config(&c);

    if (old_home) setenv("HOME", old_home, 1);
    else          unsetenv("HOME");
    cleanup_rc(tmpdir);

    /* Unknown key must not crash and known key must still be read. */
    ASSERT_EQ(c.tab_width, 6);
}

TEST(load_config_multiple_keys) {
    char tmpdir[64];
    if (!make_tmpdir(tmpdir)) { ASSERT_TRUE(0); return; }
    write_rc(tmpdir,
             "tab_width=2\n"
             "gutter_width=6\n"
             "key_delay=25\n"
             "color_scheme=dark\n");

    char *old_home = getenv("HOME");
    setenv("HOME", tmpdir, 1);

    Config c;
    config_defaults(&c);
    load_config(&c);

    if (old_home) setenv("HOME", old_home, 1);
    else          unsetenv("HOME");
    cleanup_rc(tmpdir);

    ASSERT_EQ(c.tab_width,    2);
    ASSERT_EQ(c.gutter_width, 6);
    ASSERT_EQ(c.key_delay,    25);
    ASSERT_STR_EQ(c.color_scheme, "dark");
}

/* -----------------------------------------------------------------------
 * Test runner
 * ----------------------------------------------------------------------- */

int main(void) {
    SUITE("config_defaults");
    RUN(defaults_tab_width);
    RUN(defaults_show_line_numbers);
    RUN(defaults_auto_indent);
    RUN(defaults_show_statusbar);
    RUN(defaults_cursor_style);
    RUN(defaults_gutter_width);
    RUN(defaults_key_delay);
    RUN(defaults_focus_mode);
    RUN(defaults_focus_width);
    RUN(defaults_color_scheme);
    RUN(defaults_time_format);

    SUITE("load_config — no rc file");
    RUN(load_config_no_file_leaves_defaults);

    SUITE("load_config — integer keys");
    RUN(load_config_tab_width);
    RUN(load_config_show_line_numbers_off);
    RUN(load_config_auto_indent_off);
    RUN(load_config_show_statusbar_off);
    RUN(load_config_cursor_style);
    RUN(load_config_gutter_width);
    RUN(load_config_key_delay);
    RUN(load_config_focus_mode);
    RUN(load_config_focus_width);

    SUITE("load_config — color_scheme string");
    RUN(load_config_color_scheme_dark);
    RUN(load_config_color_scheme_light);
    RUN(load_config_color_scheme_mono);

    SUITE("load_config — time_format string");
    RUN(load_config_time_format);

    SUITE("load_config — robustness");
    RUN(load_config_ignores_comments);
    RUN(load_config_ignores_blank_lines);
    RUN(load_config_ignores_unknown_keys);
    RUN(load_config_multiple_keys);

    SUMMARY();
    return g_failed > 0 ? 1 : 0;
}