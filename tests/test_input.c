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

/*
 * This suite tests only the ncurses-free functions from input.c:
 *   cut_line, paste_line, delete_line,
 *   move_left, move_right, move_line_start, move_line_end,
 *   move_up, move_down
 *
 * Functions that call getch() / printw() / move() (main_input, mini_input,
 * do_find, do_replace, confirm_quit) require a real terminal and are out of
 * scope here.
 */

#include <string.h>
#include "../include/gap.h"
#include "../include/config.h"
#include "../include/buffer.h"
#include "../include/input.h"
#include "test_framework.h"

/* -----------------------------------------------------------------------
 * Helpers
 * ----------------------------------------------------------------------- */

/*
 * Initialise an EditorContext IN PLACE — see test_buffer.c for the rationale.
 */
static void init_edcon(EditorContext *e, const char *src) {
    memset(e, 0, sizeof(*e));
    new_buffer(e);
    switch_buffer(e, 0);
    int len = strlen(src);
    for (int i = 0; i < len; i++)
        gap_insert(&e->buffer->text, i, src[i]);
    rebuild_line_count(e);
    e->buffer->cursor       = 0;
    e->buffer->current_line = 0;
}

static void free_edcon(EditorContext *e) {
    for (int i = 0; i < e->buf_count; i++)
        gap_free(&e->buffers[i].text);
}

static Config default_cfg(void) {
    Config c;
    config_defaults(&c);
    return c;
}

/* Read full gap content into a stack buffer (caller responsible for size). */
static void read_gap(EditorContext *e, char *out, int max) {
    int len = gap_len(&e->buffer->text);
    if (len >= max) len = max - 1;
    for (int i = 0; i < len; i++) out[i] = gap_char(&e->buffer->text, i);
    out[len] = '\0';
}

/* -----------------------------------------------------------------------
 * cut_line
 * ----------------------------------------------------------------------- */

TEST(cut_line_copies_to_clipboard) {
    EditorContext e; init_edcon(&e, "hello\nworld\n");
    e.buffer->current_line = 0;
    e.buffer->cursor = 0;
    cut_line(&e);
    ASSERT_STR_EQ(e.buffer->clipboard, "hello");
    free_edcon(&e);
}

TEST(cut_line_removes_line_from_buffer) {
    EditorContext e; init_edcon(&e, "hello\nworld\n");
    e.buffer->current_line = 0;
    e.buffer->cursor = 0;
    cut_line(&e);
    char buf[64];
    read_gap(&e, buf, sizeof(buf));
    ASSERT_STR_EQ(buf, "world\n");
    free_edcon(&e);
}

TEST(cut_line_updates_line_count) {
    EditorContext e; init_edcon(&e, "a\nb\nc\n");
    /* 4 lines (trailing newline adds one): a, b, c, "" */
    e.buffer->current_line = 0;
    e.buffer->cursor = 0;
    int before = total_lines(&e);
    cut_line(&e);
    ASSERT_EQ(total_lines(&e), before - 1);
    free_edcon(&e);
}

TEST(cut_line_sets_dirty) {
    EditorContext e; init_edcon(&e, "hello\nworld\n");
    e.buffer->dirty = 0;
    e.buffer->current_line = 0;
    e.buffer->cursor = 0;
    cut_line(&e);
    ASSERT_EQ(e.buffer->dirty, 1);
    free_edcon(&e);
}

TEST(cut_line_second_line) {
    EditorContext e; init_edcon(&e, "first\nsecond\nthird\n");
    /* Move cursor to second line. */
    e.buffer->current_line = 1;
    e.buffer->cursor = line_start(&e, 1);
    cut_line(&e);
    ASSERT_STR_EQ(e.buffer->clipboard, "second");
    char buf[64];
    read_gap(&e, buf, sizeof(buf));
    ASSERT_STR_EQ(buf, "first\nthird\n");
    free_edcon(&e);
}

TEST(cut_line_sets_cb_len) {
    EditorContext e; init_edcon(&e, "hello\n");
    e.buffer->current_line = 0;
    e.buffer->cursor = 0;
    cut_line(&e);
    ASSERT_EQ(e.buffer->cb_len, 5);  /* "hello" is 5 chars */
    free_edcon(&e);
}

/* -----------------------------------------------------------------------
 * paste_line
 * ----------------------------------------------------------------------- */

TEST(paste_line_empty_clipboard_noop) {
    EditorContext e; init_edcon(&e, "hello\n");
    e.buffer->cb_len = 0;
    e.buffer->clipboard[0] = '\0';
    int before = gap_len(&e.buffer->text);
    paste_line(&e);
    ASSERT_EQ(gap_len(&e.buffer->text), before);
    free_edcon(&e);
}

TEST(paste_line_inserts_before_current_line) {
    EditorContext e; init_edcon(&e, "world\n");
    strncpy(e.buffer->clipboard, "hello", sizeof(e.buffer->clipboard));
    e.buffer->cb_len = 5;
    e.buffer->current_line = 0;
    e.buffer->cursor = 0;
    paste_line(&e);
    char buf[64];
    read_gap(&e, buf, sizeof(buf));
    ASSERT_STR_EQ(buf, "hello\nworld\n");
    free_edcon(&e);
}

TEST(paste_line_increases_line_count) {
    EditorContext e; init_edcon(&e, "world\n");
    strncpy(e.buffer->clipboard, "hello", sizeof(e.buffer->clipboard));
    e.buffer->cb_len = 5;
    e.buffer->current_line = 0;
    e.buffer->cursor = 0;
    int before = total_lines(&e);
    paste_line(&e);
    ASSERT_EQ(total_lines(&e), before + 1);
    free_edcon(&e);
}

TEST(paste_line_sets_dirty) {
    EditorContext e; init_edcon(&e, "world\n");
    strncpy(e.buffer->clipboard, "hello", sizeof(e.buffer->clipboard));
    e.buffer->cb_len = 5;
    e.buffer->dirty = 0;
    paste_line(&e);
    ASSERT_EQ(e.buffer->dirty, 1);
    free_edcon(&e);
}

TEST(cut_then_paste_roundtrip) {
    EditorContext e; init_edcon(&e, "line1\nline2\nline3\n");
    e.buffer->current_line = 1;
    e.buffer->cursor = line_start(&e, 1);
    cut_line(&e);   /* removes "line2" */
    /* Now buffer is "line1\nline3\n", cursor at start of "line3". */
    /* Paste before current line (which is now line3). */
    paste_line(&e);
    char buf[64];
    read_gap(&e, buf, sizeof(buf));
    ASSERT_STR_EQ(buf, "line1\nline2\nline3\n");
    free_edcon(&e);
}

/* -----------------------------------------------------------------------
 * delete_line
 * ----------------------------------------------------------------------- */

TEST(delete_line_removes_content) {
    EditorContext e; init_edcon(&e, "hello\nworld\n");
    e.buffer->current_line = 0;
    e.buffer->cursor = 0;
    delete_line(&e);
    char buf[64];
    read_gap(&e, buf, sizeof(buf));
    ASSERT_STR_EQ(buf, "world\n");
    free_edcon(&e);
}

TEST(delete_line_does_not_affect_clipboard) {
    EditorContext e; init_edcon(&e, "hello\nworld\n");
    strncpy(e.buffer->clipboard, "previous", sizeof(e.buffer->clipboard));
    e.buffer->cb_len = 8;
    e.buffer->current_line = 0;
    e.buffer->cursor = 0;
    delete_line(&e);
    ASSERT_STR_EQ(e.buffer->clipboard, "previous");
    free_edcon(&e);
}

TEST(delete_line_updates_line_count) {
    EditorContext e; init_edcon(&e, "a\nb\nc\n");
    int before = total_lines(&e);
    e.buffer->current_line = 0;
    e.buffer->cursor = 0;
    delete_line(&e);
    ASSERT_EQ(total_lines(&e), before - 1);
    free_edcon(&e);
}

TEST(delete_line_sets_dirty) {
    EditorContext e; init_edcon(&e, "hello\nworld\n");
    e.buffer->dirty = 0;
    e.buffer->current_line = 0;
    e.buffer->cursor = 0;
    delete_line(&e);
    ASSERT_EQ(e.buffer->dirty, 1);
    free_edcon(&e);
}

TEST(delete_line_cursor_clamped_on_last_line) {
    EditorContext e; init_edcon(&e, "only\n");
    e.buffer->current_line = 0;
    e.buffer->cursor = 0;
    delete_line(&e);
    /* After deleting the only line, buffer is empty; cursor must be 0. */
    ASSERT_EQ(e.buffer->cursor, 0);
    free_edcon(&e);
}

/* -----------------------------------------------------------------------
 * move_left / move_right
 * ----------------------------------------------------------------------- */

TEST(move_left_decrements_cursor) {
    EditorContext e; init_edcon(&e, "hello");
    e.buffer->cursor = 3;
    e.buffer->current_line = 0;
    move_left(&e);
    ASSERT_EQ(e.buffer->cursor, 2);
    free_edcon(&e);
}

TEST(move_left_at_zero_noop) {
    EditorContext e; init_edcon(&e, "hello");
    e.buffer->cursor = 0;
    move_left(&e);
    ASSERT_EQ(e.buffer->cursor, 0);
    free_edcon(&e);
}

TEST(move_left_over_newline_decrements_line) {
    EditorContext e; init_edcon(&e, "hello\nworld");
    e.buffer->cursor       = 6;
    e.buffer->current_line = 1;
    move_left(&e);
    /* Stepped back over '\n' at pos 5 */
    ASSERT_EQ(e.buffer->cursor, 5);
    ASSERT_EQ(e.buffer->current_line, 0);
    free_edcon(&e);
}

TEST(move_right_increments_cursor) {
    EditorContext e; init_edcon(&e, "hello");
    e.buffer->cursor = 0;
    move_right(&e);
    ASSERT_EQ(e.buffer->cursor, 1);
    free_edcon(&e);
}

TEST(move_right_at_end_noop) {
    EditorContext e; init_edcon(&e, "hello");
    e.buffer->cursor = 5;  /* one past last char */
    move_right(&e);
    ASSERT_EQ(e.buffer->cursor, 5);
    free_edcon(&e);
}

TEST(move_right_over_newline_increments_line) {
    EditorContext e; init_edcon(&e, "hello\nworld");
    e.buffer->cursor       = 5;   /* '\n' */
    e.buffer->current_line = 0;
    move_right(&e);
    ASSERT_EQ(e.buffer->cursor, 6);
    ASSERT_EQ(e.buffer->current_line, 1);
    free_edcon(&e);
}

/* -----------------------------------------------------------------------
 * move_line_start / move_line_end
 * ----------------------------------------------------------------------- */

TEST(move_line_start_on_first_line) {
    EditorContext e; init_edcon(&e, "hello");
    e.buffer->cursor = 4;
    e.buffer->current_line = 0;
    move_line_start(&e);
    ASSERT_EQ(e.buffer->cursor, 0);
    free_edcon(&e);
}

TEST(move_line_start_on_second_line) {
    EditorContext e; init_edcon(&e, "hello\nworld");
    e.buffer->current_line = 1;
    e.buffer->cursor = 9;
    move_line_start(&e);
    ASSERT_EQ(e.buffer->cursor, 6);
    free_edcon(&e);
}

TEST(move_line_end_on_first_line) {
    EditorContext e; init_edcon(&e, "hello\nworld");
    e.buffer->cursor = 0;
    e.buffer->current_line = 0;
    move_line_end(&e);
    ASSERT_EQ(e.buffer->cursor, 5);  /* just past 'o', before '\n' */
    free_edcon(&e);
}

TEST(move_line_end_on_second_line) {
    EditorContext e; init_edcon(&e, "hello\nworld");
    e.buffer->current_line = 1;
    e.buffer->cursor = 6;
    move_line_end(&e);
    ASSERT_EQ(e.buffer->cursor, 11);  /* end of "world" */
    free_edcon(&e);
}

TEST(move_line_end_empty_line) {
    EditorContext e; init_edcon(&e, "hello\n\nworld");
    e.buffer->current_line = 1;
    e.buffer->cursor = line_start(&e, 1);
    move_line_end(&e);
    /* Empty line — start == end */
    ASSERT_EQ(e.buffer->cursor, line_start(&e, 1));
    free_edcon(&e);
}

/* -----------------------------------------------------------------------
 * move_up / move_down
 * ----------------------------------------------------------------------- */

TEST(move_up_from_line_0_noop) {
    Config cfg = default_cfg();
    EditorContext e; init_edcon(&e, "hello");
    e.buffer->current_line = 0;
    e.buffer->cursor = 3;
    move_up(&cfg, &e);
    ASSERT_EQ(e.buffer->current_line, 0);
    ASSERT_EQ(e.buffer->cursor, 0);
    free_edcon(&e);
}

TEST(move_up_decrements_line) {
    Config cfg = default_cfg();
    EditorContext e; init_edcon(&e, "hello\nworld\n");
    e.buffer->current_line = 1;
    e.buffer->cursor = line_start(&e, 1);
    move_up(&cfg, &e);
    ASSERT_EQ(e.buffer->current_line, 0);
    free_edcon(&e);
}

TEST(move_up_preserves_column_when_possible) {
    Config cfg = default_cfg();
    EditorContext e; init_edcon(&e, "hello\nworld\n");
    /* cursor at col 3 on line 1 */
    e.buffer->current_line = 1;
    e.buffer->cursor = line_start(&e, 1) + 3;
    move_up(&cfg, &e);
    /* "hello" has 5 chars; col 3 is valid -> cursor at line_start(0) + 3 */
    ASSERT_EQ(e.buffer->cursor, 3);
    free_edcon(&e);
}

TEST(move_up_clamps_to_line_end_on_short_line) {
    Config cfg = default_cfg();
    /* line 0: "ab" (len 2), line 1: "hello" (len 5) */
    EditorContext e; init_edcon(&e, "ab\nhello\n");
    e.buffer->current_line = 1;
    e.buffer->cursor = line_start(&e, 1) + 4;  /* col 4 on "hello" */
    move_up(&cfg, &e);
    /* "ab" is only 2 chars; clamp to end */
    ASSERT_EQ(e.buffer->cursor, line_start(&e, 0) + 2);
    free_edcon(&e);
}

TEST(move_down_on_last_line_noop) {
    Config cfg = default_cfg();
    EditorContext e; init_edcon(&e, "hello");
    e.buffer->current_line = 0;
    e.buffer->cursor = 3;
    int cursor_before = e.buffer->cursor;
    move_down(&cfg, &e);
    ASSERT_EQ(e.buffer->current_line, 0);
    ASSERT_EQ(e.buffer->cursor, cursor_before);
    free_edcon(&e);
}

TEST(move_down_increments_line) {
    Config cfg = default_cfg();
    EditorContext e; init_edcon(&e, "hello\nworld\n");
    e.buffer->current_line = 0;
    e.buffer->cursor = 0;
    move_down(&cfg, &e);
    ASSERT_EQ(e.buffer->current_line, 1);
    free_edcon(&e);
}

TEST(move_down_preserves_column_when_possible) {
    Config cfg = default_cfg();
    EditorContext e; init_edcon(&e, "hello\nworld\n");
    e.buffer->current_line = 0;
    e.buffer->cursor = 2;  /* col 2 on "hello" */
    move_down(&cfg, &e);
    /* "world" has 5 chars; col 2 is valid */
    ASSERT_EQ(e.buffer->cursor, line_start(&e, 1) + 2);
    free_edcon(&e);
}

TEST(move_down_clamps_to_line_end_on_short_line) {
    Config cfg = default_cfg();
    /* line 0: "hello" (len 5), line 1: "ab" (len 2) */
    EditorContext e; init_edcon(&e, "hello\nab\n");
    e.buffer->current_line = 0;
    e.buffer->cursor = line_start(&e, 0) + 4;  /* col 4 on "hello" */
    move_down(&cfg, &e);
    ASSERT_EQ(e.buffer->cursor, line_start(&e, 1) + 2);
    free_edcon(&e);
}

/* -----------------------------------------------------------------------
 * Test runner
 * ----------------------------------------------------------------------- */

int main(void) {
    SUITE("cut_line");
    RUN(cut_line_copies_to_clipboard);
    RUN(cut_line_removes_line_from_buffer);
    RUN(cut_line_updates_line_count);
    RUN(cut_line_sets_dirty);
    RUN(cut_line_second_line);
    RUN(cut_line_sets_cb_len);

    SUITE("paste_line");
    RUN(paste_line_empty_clipboard_noop);
    RUN(paste_line_inserts_before_current_line);
    RUN(paste_line_increases_line_count);
    RUN(paste_line_sets_dirty);
    RUN(cut_then_paste_roundtrip);

    SUITE("delete_line");
    RUN(delete_line_removes_content);
    RUN(delete_line_does_not_affect_clipboard);
    RUN(delete_line_updates_line_count);
    RUN(delete_line_sets_dirty);
    RUN(delete_line_cursor_clamped_on_last_line);

    SUITE("move_left / move_right");
    RUN(move_left_decrements_cursor);
    RUN(move_left_at_zero_noop);
    RUN(move_left_over_newline_decrements_line);
    RUN(move_right_increments_cursor);
    RUN(move_right_at_end_noop);
    RUN(move_right_over_newline_increments_line);

    SUITE("move_line_start / move_line_end");
    RUN(move_line_start_on_first_line);
    RUN(move_line_start_on_second_line);
    RUN(move_line_end_on_first_line);
    RUN(move_line_end_on_second_line);
    RUN(move_line_end_empty_line);

    SUITE("move_up / move_down");
    RUN(move_up_from_line_0_noop);
    RUN(move_up_decrements_line);
    RUN(move_up_preserves_column_when_possible);
    RUN(move_up_clamps_to_line_end_on_short_line);
    RUN(move_down_on_last_line_noop);
    RUN(move_down_increments_line);
    RUN(move_down_preserves_column_when_possible);
    RUN(move_down_clamps_to_line_end_on_short_line);

    SUMMARY();
    return g_failed > 0 ? 1 : 0;
}