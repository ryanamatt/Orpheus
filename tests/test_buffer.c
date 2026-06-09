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

#include <string.h>
#include "../include/gap.h"
#include "../include/config.h"
#include "../include/buffer.h"
#include "test_framework.h"

/* -----------------------------------------------------------------------
 * Helpers
 * ----------------------------------------------------------------------- */

/*
 * Initialise an EditorContext IN PLACE and load text from `src`.
 *
 * EditorContext must never be returned by value: switch_buffer() stores a
 * pointer to buffers[i] inside the struct itself. Returning by value copies
 * the struct but the internal pointer still points at the callee's stack,
 * making every subsequent e.buffer-> access undefined behaviour.
 */
static void init_edcon(EditorContext *e, const char *src) {
    memset(e, 0, sizeof(*e));
    new_buffer(e);
    switch_buffer(e, 0);
    int len = strlen(src);
    for (int i = 0; i < len; i++)
        gap_insert(&e->buffer->text, i, src[i]);
    rebuild_line_count(e);
    e->buffer->cursor = 0;
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

/* -----------------------------------------------------------------------
 * new_buffer / switch_buffer
 * ----------------------------------------------------------------------- */

TEST(new_buffer_first_slot) {
    EditorContext e;
    memset(&e, 0, sizeof(e));
    int idx = new_buffer(&e);
    ASSERT_EQ(idx, 0);
    ASSERT_EQ(e.buf_count, 1);
    gap_free(&e.buffers[0].text);
}

TEST(new_buffer_second_slot) {
    EditorContext e;
    memset(&e, 0, sizeof(e));
    new_buffer(&e);
    int idx = new_buffer(&e);
    ASSERT_EQ(idx, 1);
    ASSERT_EQ(e.buf_count, 2);
    for (int i = 0; i < e.buf_count; i++) gap_free(&e.buffers[i].text);
}

TEST(new_buffer_returns_minus1_at_max) {
    EditorContext e;
    memset(&e, 0, sizeof(e));
    for (int i = 0; i < MAX_BUFFERS; i++) new_buffer(&e);
    int idx = new_buffer(&e);
    ASSERT_EQ(idx, -1);
    for (int i = 0; i < e.buf_count; i++) gap_free(&e.buffers[i].text);
}

TEST(new_buffer_line_count_starts_at_1) {
    EditorContext e;
    memset(&e, 0, sizeof(e));
    int idx = new_buffer(&e);
    ASSERT_EQ(e.buffers[idx].line_count, 1);
    gap_free(&e.buffers[idx].text);
}

TEST(switch_buffer_sets_active) {
    EditorContext e;
    memset(&e, 0, sizeof(e));
    new_buffer(&e);
    new_buffer(&e);
    switch_buffer(&e, 1);
    ASSERT_EQ(e.cur_buf, 1);
    ASSERT_TRUE(e.buffer == &e.buffers[1]);
    for (int i = 0; i < e.buf_count; i++) gap_free(&e.buffers[i].text);
}

TEST(switch_buffer_wraps_negative) {
    EditorContext e;
    memset(&e, 0, sizeof(e));
    new_buffer(&e);
    new_buffer(&e);
    switch_buffer(&e, -1);
    /* -1 should wrap to last buffer (index 1) */
    ASSERT_EQ(e.cur_buf, 1);
    for (int i = 0; i < e.buf_count; i++) gap_free(&e.buffers[i].text);
}

TEST(switch_buffer_wraps_past_end) {
    EditorContext e;
    memset(&e, 0, sizeof(e));
    new_buffer(&e);
    new_buffer(&e);
    switch_buffer(&e, 2);   /* only indices 0 and 1 exist */
    ASSERT_EQ(e.cur_buf, 0);
    for (int i = 0; i < e.buf_count; i++) gap_free(&e.buffers[i].text);
}

/* -----------------------------------------------------------------------
 * line_start / line_len
 * ----------------------------------------------------------------------- */

TEST(line_start_empty_buffer) {
    EditorContext e; init_edcon(&e, "");
    ASSERT_EQ(line_start(&e, 0), 0);
    free_edcon(&e);
}

TEST(line_start_single_line) {
    EditorContext e; init_edcon(&e, "hello");
    ASSERT_EQ(line_start(&e, 0), 0);
    free_edcon(&e);
}

TEST(line_start_second_line) {
    EditorContext e; init_edcon(&e, "hello\nworld");
    ASSERT_EQ(line_start(&e, 1), 6);   /* "hello\n" is 6 chars */
    free_edcon(&e);
}

TEST(line_start_third_line) {
    EditorContext e; init_edcon(&e, "a\nbb\nccc");
    /* line 0: "a\n" -> 2 chars, line 1: "bb\n" -> 3 chars, line 2 starts at 5 */
    ASSERT_EQ(line_start(&e, 2), 5);
    free_edcon(&e);
}

TEST(line_len_empty_buffer) {
    EditorContext e; init_edcon(&e, "");
    ASSERT_EQ(line_len(&e, 0), 0);
    free_edcon(&e);
}

TEST(line_len_single_line_no_newline) {
    EditorContext e; init_edcon(&e, "hello");
    ASSERT_EQ(line_len(&e, 0), 5);
    free_edcon(&e);
}

TEST(line_len_first_of_two_lines) {
    EditorContext e; init_edcon(&e, "hello\nworld");
    ASSERT_EQ(line_len(&e, 0), 5);
    free_edcon(&e);
}

TEST(line_len_second_of_two_lines) {
    EditorContext e; init_edcon(&e, "hello\nworld");
    ASSERT_EQ(line_len(&e, 1), 5);
    free_edcon(&e);
}

TEST(line_len_empty_first_line) {
    EditorContext e; init_edcon(&e, "\nhello");
    ASSERT_EQ(line_len(&e, 0), 0);
    ASSERT_EQ(line_len(&e, 1), 5);
    free_edcon(&e);
}

/* -----------------------------------------------------------------------
 * total_lines / rebuild_line_count
 * ----------------------------------------------------------------------- */

TEST(total_lines_empty_buffer) {
    EditorContext e; init_edcon(&e, "");
    ASSERT_EQ(total_lines(&e), 1);
    free_edcon(&e);
}

TEST(total_lines_single_line) {
    EditorContext e; init_edcon(&e, "hello");
    ASSERT_EQ(total_lines(&e), 1);
    free_edcon(&e);
}

TEST(total_lines_two_lines) {
    EditorContext e; init_edcon(&e, "hello\nworld");
    ASSERT_EQ(total_lines(&e), 2);
    free_edcon(&e);
}

TEST(total_lines_trailing_newline) {
    /* "hello\n" has a newline at the end: line 0 = "hello", line 1 = "" */
    EditorContext e; init_edcon(&e, "hello\n");
    ASSERT_EQ(total_lines(&e), 2);
    free_edcon(&e);
}

TEST(rebuild_line_count_matches_newlines) {
    EditorContext e; init_edcon(&e, "a\nb\nc\nd");
    /* 3 newlines -> 4 lines */
    ASSERT_EQ(e.buffer->line_count, 4);
    free_edcon(&e);
}

TEST(rebuild_line_count_sets_stats_dirty) {
    EditorContext e; init_edcon(&e, "hello");
    e.buffer->stats_dirty = 0;
    rebuild_line_count(&e);
    ASSERT_EQ(e.buffer->stats_dirty, 1);
    free_edcon(&e);
}

/* -----------------------------------------------------------------------
 * count_chars / count_words / count_words_full
 * ----------------------------------------------------------------------- */

TEST(count_chars_empty) {
    EditorContext e; init_edcon(&e, "");
    ASSERT_EQ(count_chars(&e), 0);
    free_edcon(&e);
}

TEST(count_chars_simple) {
    EditorContext e; init_edcon(&e, "hello");
    ASSERT_EQ(count_chars(&e), 5);
    free_edcon(&e);
}

TEST(count_chars_includes_newlines) {
    EditorContext e; init_edcon(&e, "a\nb");
    ASSERT_EQ(count_chars(&e), 3);
    free_edcon(&e);
}

TEST(count_words_empty) {
    EditorContext e; init_edcon(&e, "");
    ASSERT_EQ(count_words(&e), 0);
    free_edcon(&e);
}

TEST(count_words_one_word) {
    EditorContext e; init_edcon(&e, "hello");
    ASSERT_EQ(count_words(&e), 1);
    free_edcon(&e);
}

TEST(count_words_multiple) {
    EditorContext e; init_edcon(&e, "the quick brown fox");
    ASSERT_EQ(count_words(&e), 4);
    free_edcon(&e);
}

TEST(count_words_leading_trailing_spaces) {
    EditorContext e; init_edcon(&e, "  hello world  ");
    ASSERT_EQ(count_words(&e), 2);
    free_edcon(&e);
}

TEST(count_words_whitespace_only) {
    EditorContext e; init_edcon(&e, "   \n\t  ");
    ASSERT_EQ(count_words(&e), 0);
    free_edcon(&e);
}

TEST(count_words_uses_cache_when_clean) {
    EditorContext e; init_edcon(&e, "hello world");
    /* Prime the cache. */
    int w1 = count_words(&e);
    ASSERT_EQ(e.buffer->stats_dirty, 0);
    /* Call again — should return same value without rescan. */
    int w2 = count_words(&e);
    ASSERT_EQ(w1, w2);
    free_edcon(&e);
}

TEST(count_words_rescans_when_dirty) {
    EditorContext e; init_edcon(&e, "hello world");
    count_words(&e);              /* fill cache */
    e.buffer->stats_dirty = 1;   /* force rescan */
    int w = count_words(&e);
    ASSERT_EQ(w, 2);
    ASSERT_EQ(e.buffer->stats_dirty, 0);
    free_edcon(&e);
}

/* -----------------------------------------------------------------------
 * update_stats
 * ----------------------------------------------------------------------- */

TEST(update_stats_insert_newline_increments_line_count) {
    EditorContext e; init_edcon(&e, "hello");
    int before = e.buffer->line_count;
    update_stats(&e, '\n', +1);
    ASSERT_EQ(e.buffer->line_count, before + 1);
    free_edcon(&e);
}

TEST(update_stats_delete_newline_decrements_line_count) {
    EditorContext e; init_edcon(&e, "hello\nworld");
    int before = e.buffer->line_count;  /* 2 */
    update_stats(&e, '\n', -1);
    ASSERT_EQ(e.buffer->line_count, before - 1);
    free_edcon(&e);
}

TEST(update_stats_non_newline_does_not_change_line_count) {
    EditorContext e; init_edcon(&e, "hello");
    int before = e.buffer->line_count;
    update_stats(&e, 'x', +1);
    ASSERT_EQ(e.buffer->line_count, before);
    free_edcon(&e);
}

TEST(update_stats_always_sets_stats_dirty) {
    EditorContext e; init_edcon(&e, "hello");
    e.buffer->stats_dirty = 0;
    update_stats(&e, 'x', +1);
    ASSERT_EQ(e.buffer->stats_dirty, 1);
    free_edcon(&e);
}

/* -----------------------------------------------------------------------
 * pos_to_line / update_current_line_delta
 * ----------------------------------------------------------------------- */

TEST(pos_to_line_position_zero) {
    EditorContext e; init_edcon(&e, "hello\nworld");
    ASSERT_EQ(pos_to_line(&e, 0), 0);
    free_edcon(&e);
}

TEST(pos_to_line_on_second_line) {
    EditorContext e; init_edcon(&e, "hello\nworld");
    /* pos 6 is 'w', which is on line 1 */
    ASSERT_EQ(pos_to_line(&e, 6), 1);
    free_edcon(&e);
}

TEST(pos_to_line_at_newline) {
    EditorContext e; init_edcon(&e, "hello\nworld");
    /* pos 5 is the '\n' itself — still counted as line 0 */
    ASSERT_EQ(pos_to_line(&e, 5), 0);
    free_edcon(&e);
}

TEST(pos_to_line_multiline) {
    EditorContext e; init_edcon(&e, "a\nb\nc");
    /* 'a'=0, '\n'=1, 'b'=2, '\n'=3, 'c'=4 */
    ASSERT_EQ(pos_to_line(&e, 4), 2);
    free_edcon(&e);
}

TEST(update_current_line_delta_step_right_over_newline) {
    EditorContext e; init_edcon(&e, "hello\nworld");
    e.buffer->cursor = 5;
    e.buffer->current_line = 0;
    /* step right from pos 5 ('\n') to pos 6 */
    update_current_line_delta(&e, 5, 6);
    ASSERT_EQ(e.buffer->current_line, 1);
    free_edcon(&e);
}

TEST(update_current_line_delta_step_left_over_newline) {
    EditorContext e; init_edcon(&e, "hello\nworld");
    e.buffer->cursor = 6;
    e.buffer->current_line = 1;
    /* step left from pos 6 to pos 5 ('\n') */
    update_current_line_delta(&e, 6, 5);
    ASSERT_EQ(e.buffer->current_line, 0);
    free_edcon(&e);
}

TEST(update_current_line_delta_step_right_no_newline) {
    EditorContext e; init_edcon(&e, "hello\nworld");
    e.buffer->current_line = 0;
    update_current_line_delta(&e, 0, 1);
    ASSERT_EQ(e.buffer->current_line, 0);
    free_edcon(&e);
}

TEST(update_current_line_delta_large_jump_uses_full_scan) {
    EditorContext e; init_edcon(&e, "a\nb\nc\nd");
    e.buffer->current_line = 0;
    /* jump from 0 to pos 6 ('c') */
    update_current_line_delta(&e, 0, 6);
    ASSERT_EQ(e.buffer->current_line, pos_to_line(&e, 6));
    free_edcon(&e);
}

/* -----------------------------------------------------------------------
 * cursor_vcol
 * ----------------------------------------------------------------------- */

TEST(cursor_vcol_at_start) {
    Config cfg = default_cfg();
    EditorContext e; init_edcon(&e, "hello");
    e.buffer->cursor = 0;
    ASSERT_EQ(cursor_vcol(&cfg, &e), 0);
    free_edcon(&e);
}

TEST(cursor_vcol_mid_line) {
    Config cfg = default_cfg();
    EditorContext e; init_edcon(&e, "hello");
    e.buffer->cursor = 3;
    ASSERT_EQ(cursor_vcol(&cfg, &e), 3);
    free_edcon(&e);
}

TEST(cursor_vcol_tab_expansion) {
    Config cfg = default_cfg();  /* tab_width = 4 */
    EditorContext e; init_edcon(&e, "\thello");
    /* Tab at col 0 expands to col 4; cursor at pos 1 ('h') -> vcol 4 */
    e.buffer->cursor = 1;
    ASSERT_EQ(cursor_vcol(&cfg, &e), 4);
    free_edcon(&e);
}

TEST(cursor_vcol_tab_at_col_4_expands_to_8) {
    Config cfg = default_cfg();  /* tab_width = 4 */
    /* four spaces then a tab: visual col after spaces is 4,
       tab bumps to next multiple of 4 = 8 */
    EditorContext e; init_edcon(&e, "    \t");
    e.buffer->cursor = 5;
    ASSERT_EQ(cursor_vcol(&cfg, &e), 8);
    free_edcon(&e);
}

/* -----------------------------------------------------------------------
 * set_status
 * ----------------------------------------------------------------------- */

TEST(set_status_basic_string) {
    EditorContext e; init_edcon(&e, "");
    set_status(&e, "hello");
    ASSERT_STR_EQ(e.buffer->status, "hello");
    free_edcon(&e);
}

TEST(set_status_format_args) {
    EditorContext e; init_edcon(&e, "");
    set_status(&e, "line %d of %d", 3, 10);
    ASSERT_STR_EQ(e.buffer->status, "line 3 of 10");
    free_edcon(&e);
}

TEST(set_status_overwrites_previous) {
    EditorContext e; init_edcon(&e, "");
    set_status(&e, "first");
    set_status(&e, "second");
    ASSERT_STR_EQ(e.buffer->status, "second");
    free_edcon(&e);
}

/* -----------------------------------------------------------------------
 * Test runner
 * ----------------------------------------------------------------------- */

int main(void) {
    SUITE("new_buffer / switch_buffer");
    RUN(new_buffer_first_slot);
    RUN(new_buffer_second_slot);
    RUN(new_buffer_returns_minus1_at_max);
    RUN(new_buffer_line_count_starts_at_1);
    RUN(switch_buffer_sets_active);
    RUN(switch_buffer_wraps_negative);
    RUN(switch_buffer_wraps_past_end);

    SUITE("line_start / line_len");
    RUN(line_start_empty_buffer);
    RUN(line_start_single_line);
    RUN(line_start_second_line);
    RUN(line_start_third_line);
    RUN(line_len_empty_buffer);
    RUN(line_len_single_line_no_newline);
    RUN(line_len_first_of_two_lines);
    RUN(line_len_second_of_two_lines);
    RUN(line_len_empty_first_line);

    SUITE("total_lines / rebuild_line_count");
    RUN(total_lines_empty_buffer);
    RUN(total_lines_single_line);
    RUN(total_lines_two_lines);
    RUN(total_lines_trailing_newline);
    RUN(rebuild_line_count_matches_newlines);
    RUN(rebuild_line_count_sets_stats_dirty);

    SUITE("count_chars / count_words");
    RUN(count_chars_empty);
    RUN(count_chars_simple);
    RUN(count_chars_includes_newlines);
    RUN(count_words_empty);
    RUN(count_words_one_word);
    RUN(count_words_multiple);
    RUN(count_words_leading_trailing_spaces);
    RUN(count_words_whitespace_only);
    RUN(count_words_uses_cache_when_clean);
    RUN(count_words_rescans_when_dirty);

    SUITE("update_stats");
    RUN(update_stats_insert_newline_increments_line_count);
    RUN(update_stats_delete_newline_decrements_line_count);
    RUN(update_stats_non_newline_does_not_change_line_count);
    RUN(update_stats_always_sets_stats_dirty);

    SUITE("pos_to_line / update_current_line_delta");
    RUN(pos_to_line_position_zero);
    RUN(pos_to_line_on_second_line);
    RUN(pos_to_line_at_newline);
    RUN(pos_to_line_multiline);
    RUN(update_current_line_delta_step_right_over_newline);
    RUN(update_current_line_delta_step_left_over_newline);
    RUN(update_current_line_delta_step_right_no_newline);
    RUN(update_current_line_delta_large_jump_uses_full_scan);

    SUITE("cursor_vcol");
    RUN(cursor_vcol_at_start);
    RUN(cursor_vcol_mid_line);
    RUN(cursor_vcol_tab_expansion);
    RUN(cursor_vcol_tab_at_col_4_expands_to_8);

    SUITE("set_status");
    RUN(set_status_basic_string);
    RUN(set_status_format_args);
    RUN(set_status_overwrites_previous);

    SUMMARY();
    return g_failed > 0 ? 1 : 0;
}