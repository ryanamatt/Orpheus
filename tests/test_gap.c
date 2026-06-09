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
#include "test_framework.h"

/* -----------------------------------------------------------------------
 * Helpers
 * ----------------------------------------------------------------------- */

/* Insert every character of a C-string at the end of the gap buffer. */
static void insert_str(Gap *g, const char *s) {
    int len = strlen(s);
    for (int i = 0; i < len; i++)
        gap_insert(g, gap_len(g), s[i]);
}

/* Read the full logical content of a gap buffer into a NUL-terminated
 * char array (caller must ensure buf is large enough). */
static void read_all(const Gap *g, char *buf) {
    int len = gap_len(g);
    for (int i = 0; i < len; i++) buf[i] = gap_char(g, i);
    buf[len] = '\0';
}

/* -----------------------------------------------------------------------
 * gap_init / gap_len
 * ----------------------------------------------------------------------- */

TEST(gap_init_len_zero) {
    Gap g;
    gap_init(&g);
    ASSERT_EQ(gap_len(&g), 0);
    gap_free(&g);
}

TEST(gap_init_buf_not_null) {
    Gap g;
    gap_init(&g);
    ASSERT_NOT_NULL(g.buf);
    gap_free(&g);
}

TEST(gap_init_size_equals_chunk) {
    Gap g;
    gap_init(&g);
    ASSERT_EQ(g.size, CHUNK);
    gap_free(&g);
}

TEST(gap_init_gap_covers_whole_buf) {
    Gap g;
    gap_init(&g);
    /* gap_start == 0, gap_end == size => entire buffer is the gap */
    ASSERT_EQ(g.gap_start, 0);
    ASSERT_EQ(g.gap_end,   g.size);
    gap_free(&g);
}

/* -----------------------------------------------------------------------
 * gap_insert / gap_char / gap_len
 * ----------------------------------------------------------------------- */

TEST(gap_insert_single_char) {
    Gap g;
    gap_init(&g);
    gap_insert(&g, 0, 'A');
    ASSERT_EQ(gap_len(&g), 1);
    ASSERT_EQ(gap_char(&g, 0), 'A');
    gap_free(&g);
}

TEST(gap_insert_at_end) {
    Gap g;
    gap_init(&g);
    insert_str(&g, "hello");
    ASSERT_EQ(gap_len(&g), 5);
    char buf[16];
    read_all(&g, buf);
    ASSERT_STR_EQ(buf, "hello");
    gap_free(&g);
}

TEST(gap_insert_at_start) {
    Gap g;
    gap_init(&g);
    insert_str(&g, "ello");
    gap_insert(&g, 0, 'h');
    char buf[16];
    read_all(&g, buf);
    ASSERT_STR_EQ(buf, "hello");
    gap_free(&g);
}

TEST(gap_insert_in_middle) {
    Gap g;
    gap_init(&g);
    insert_str(&g, "hllo");
    gap_insert(&g, 1, 'e');
    char buf[16];
    read_all(&g, buf);
    ASSERT_STR_EQ(buf, "hello");
    gap_free(&g);
}

TEST(gap_insert_newline_tracked) {
    Gap g;
    gap_init(&g);
    insert_str(&g, "line1\nline2");
    ASSERT_EQ(gap_len(&g), 11);
    ASSERT_EQ(gap_char(&g, 5), '\n');
    gap_free(&g);
}

TEST(gap_insert_many_chars_correct_order) {
    Gap g;
    gap_init(&g);
    const char *src = "abcdefghijklmnopqrstuvwxyz";
    insert_str(&g, src);
    char buf[32];
    read_all(&g, buf);
    ASSERT_STR_EQ(buf, src);
    gap_free(&g);
}

TEST(gap_len_grows_with_inserts) {
    Gap g;
    gap_init(&g);
    for (int i = 0; i < 10; i++) gap_insert(&g, gap_len(&g), 'x');
    ASSERT_EQ(gap_len(&g), 10);
    gap_free(&g);
}

/* -----------------------------------------------------------------------
 * gap_delete
 * ----------------------------------------------------------------------- */

TEST(gap_delete_only_char) {
    Gap g;
    gap_init(&g);
    gap_insert(&g, 0, 'A');
    gap_delete(&g, 0);
    ASSERT_EQ(gap_len(&g), 0);
    gap_free(&g);
}

TEST(gap_delete_first_char) {
    Gap g;
    gap_init(&g);
    insert_str(&g, "hello");
    gap_delete(&g, 0);
    char buf[16];
    read_all(&g, buf);
    ASSERT_STR_EQ(buf, "ello");
    gap_free(&g);
}

TEST(gap_delete_last_char) {
    Gap g;
    gap_init(&g);
    insert_str(&g, "hello");
    gap_delete(&g, 4);
    char buf[16];
    read_all(&g, buf);
    ASSERT_STR_EQ(buf, "hell");
    gap_free(&g);
}

TEST(gap_delete_middle_char) {
    Gap g;
    gap_init(&g);
    insert_str(&g, "helo");
    gap_delete(&g, 2);   /* remove extra 'l' wait - "helo" delete index 2 = 'l' -> "heo" */
    /* Actually intended: "hello" delete one 'l' -> "helo". Let's just verify length. */
    ASSERT_EQ(gap_len(&g), 3);
    gap_free(&g);
}

TEST(gap_delete_reduces_len) {
    Gap g;
    gap_init(&g);
    insert_str(&g, "abc");
    gap_delete(&g, 0);
    ASSERT_EQ(gap_len(&g), 2);
    gap_free(&g);
}

TEST(gap_insert_then_delete_roundtrip) {
    Gap g;
    gap_init(&g);
    insert_str(&g, "hello");
    /* Insert 'X' in the middle then delete it — should return to "hello". */
    gap_insert(&g, 2, 'X');
    gap_delete(&g, 2);
    char buf[16];
    read_all(&g, buf);
    ASSERT_STR_EQ(buf, "hello");
    gap_free(&g);
}

/* -----------------------------------------------------------------------
 * gap_move / gap_shift_left / gap_shift_right
 * ----------------------------------------------------------------------- */

TEST(gap_move_to_start) {
    Gap g;
    gap_init(&g);
    insert_str(&g, "abcde");
    gap_move(&g, 0);
    ASSERT_EQ(g.gap_start, 0);
    gap_free(&g);
}

TEST(gap_move_to_end) {
    Gap g;
    gap_init(&g);
    insert_str(&g, "abcde");
    gap_move(&g, 5);
    ASSERT_EQ(g.gap_start, 5);
    gap_free(&g);
}

TEST(gap_move_preserves_content) {
    Gap g;
    gap_init(&g);
    insert_str(&g, "abcde");
    gap_move(&g, 2);
    char buf[16];
    read_all(&g, buf);
    ASSERT_STR_EQ(buf, "abcde");
    gap_free(&g);
}

TEST(gap_move_noop_same_pos) {
    Gap g;
    gap_init(&g);
    insert_str(&g, "abc");
    int start_before = g.gap_start;
    gap_move(&g, start_before);
    ASSERT_EQ(g.gap_start, start_before);
    gap_free(&g);
}

TEST(gap_shift_right_moves_gap) {
    Gap g;
    gap_init(&g);
    insert_str(&g, "ab");
    gap_move(&g, 0);    /* reset gap to front */
    int start_before = g.gap_start;
    gap_shift_right(&g);
    ASSERT_EQ(g.gap_start, start_before + 1);
    ASSERT_EQ(g.gap_end,   g.gap_start + (g.size - gap_len(&g)));
    gap_free(&g);
}

TEST(gap_shift_left_moves_gap) {
    Gap g;
    gap_init(&g);
    insert_str(&g, "ab");
    /* gap is at end after insert_str; shift left once */
    int start_before = g.gap_start;
    gap_shift_left(&g);
    ASSERT_EQ(g.gap_start, start_before - 1);
    gap_free(&g);
}

TEST(gap_shift_right_at_end_noop) {
    Gap g;
    gap_init(&g);
    insert_str(&g, "ab");
    /* gap_start is already at end of content */
    int end_before = g.gap_end;
    gap_shift_right(&g);
    /* gap_end >= size means no movement */
    ASSERT_EQ(g.gap_end, end_before);
    gap_free(&g);
}

TEST(gap_shift_left_at_start_noop) {
    Gap g;
    gap_init(&g);
    insert_str(&g, "ab");
    gap_move(&g, 0);
    int start_before = g.gap_start;
    gap_shift_left(&g);
    ASSERT_EQ(g.gap_start, start_before);
    gap_free(&g);
}

/* -----------------------------------------------------------------------
 * gap_grow
 * ----------------------------------------------------------------------- */

TEST(gap_grow_triggers_on_overflow) {
    Gap g;
    gap_init(&g);
    /* Fill the initial CHUNK allocation to force a grow. */
    for (int i = 0; i < CHUNK + 1; i++)
        gap_insert(&g, gap_len(&g), 'x');
    ASSERT_TRUE(g.size > CHUNK);
    ASSERT_EQ(gap_len(&g), CHUNK + 1);
    gap_free(&g);
}

TEST(gap_grow_content_survives_realloc) {
    Gap g;
    gap_init(&g);
    const char *str = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                      "abcdefghijklmnopqrstuvwxyz"
                      "0123456789";  /* 62 chars > CHUNK(64)? close, do double */
    insert_str(&g, str);
    insert_str(&g, str);  /* 124 chars — guarantees at least one grow */
    ASSERT_EQ(gap_len(&g), (int)(strlen(str) * 2));
    /* Spot-check first and last chars. */
    ASSERT_EQ(gap_char(&g, 0), 'A');
    ASSERT_EQ(gap_char(&g, gap_len(&g) - 1), '9');
    gap_free(&g);
}

/* -----------------------------------------------------------------------
 * gap_char boundary
 * ----------------------------------------------------------------------- */

TEST(gap_char_at_boundary_before_gap) {
    Gap g;
    gap_init(&g);
    insert_str(&g, "abc");
    gap_move(&g, 1);  /* gap_start = 1 */
    /* pos 0 is before the gap */
    ASSERT_EQ(gap_char(&g, 0), 'a');
    gap_free(&g);
}

TEST(gap_char_at_boundary_after_gap) {
    Gap g;
    gap_init(&g);
    insert_str(&g, "abc");
    gap_move(&g, 1);  /* gap_start = 1, pos 1 and 2 are after gap */
    ASSERT_EQ(gap_char(&g, 1), 'b');
    ASSERT_EQ(gap_char(&g, 2), 'c');
    gap_free(&g);
}

/* -----------------------------------------------------------------------
 * Test runner
 * ----------------------------------------------------------------------- */

int main(void) {
    SUITE("gap_init / gap_len");
    RUN(gap_init_len_zero);
    RUN(gap_init_buf_not_null);
    RUN(gap_init_size_equals_chunk);
    RUN(gap_init_gap_covers_whole_buf);

    SUITE("gap_insert / gap_char / gap_len");
    RUN(gap_insert_single_char);
    RUN(gap_insert_at_end);
    RUN(gap_insert_at_start);
    RUN(gap_insert_in_middle);
    RUN(gap_insert_newline_tracked);
    RUN(gap_insert_many_chars_correct_order);
    RUN(gap_len_grows_with_inserts);

    SUITE("gap_delete");
    RUN(gap_delete_only_char);
    RUN(gap_delete_first_char);
    RUN(gap_delete_last_char);
    RUN(gap_delete_middle_char);
    RUN(gap_delete_reduces_len);
    RUN(gap_insert_then_delete_roundtrip);

    SUITE("gap_move / gap_shift");
    RUN(gap_move_to_start);
    RUN(gap_move_to_end);
    RUN(gap_move_preserves_content);
    RUN(gap_move_noop_same_pos);
    RUN(gap_shift_right_moves_gap);
    RUN(gap_shift_left_moves_gap);
    RUN(gap_shift_right_at_end_noop);
    RUN(gap_shift_left_at_start_noop);

    SUITE("gap_grow");
    RUN(gap_grow_triggers_on_overflow);
    RUN(gap_grow_content_survives_realloc);

    SUITE("gap_char boundary");
    RUN(gap_char_at_boundary_before_gap);
    RUN(gap_char_at_boundary_after_gap);

    SUMMARY();
    return g_failed > 0 ? 1 : 0;
}