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
#include <string.h>
#include <stdlib.h>
#include <unistd.h>   /* mkstemp, close */
#include "../include/gap.h"
#include "../include/config.h"
#include "../include/buffer.h"
#include "../include/fileio.h"
#include "test_framework.h"

/* -----------------------------------------------------------------------
 * Helpers
 * ----------------------------------------------------------------------- */

/* Write content to a uniquely-named temp file. Returns 1 on success. */
static int write_tempfile(char *path_out, const char *content) {
    strcpy(path_out, "/tmp/orpheus_fileio_XXXXXX");
    int fd = mkstemp(path_out);
    if (fd < 0) return 0;
    FILE *f = fdopen(fd, "w");
    if (!f) { close(fd); return 0; }
    fputs(content, f);
    fclose(f);
    return 1;
}

static void cleanup_file(const char *path) { remove(path); }

/*
 * Initialise an EditorContext IN PLACE (pointer, not return-by-value).
 *
 * Returning EditorContext by value copies the Gap struct, which contains a
 * heap pointer (buf). After the copy the caller's Gap.buf is correct, but
 * the EditorContext.buffer pointer stored by switch_buffer() inside
 * new_buffer() points at the CALLEE's stack frame — not the caller's copy.
 * Every subsequent access through e.buffer would be undefined behaviour.
 *
 * Passing a pointer avoids the copy entirely and keeps every internal pointer
 * valid for the lifetime of the caller's local EditorContext.
 */
static void init_edcon(EditorContext *e, const char *filename) {
    memset(e, 0, sizeof(*e));
    new_buffer(e);
    switch_buffer(e, 0);
    if (filename)
        strncpy(e->buffer->filename, filename, sizeof(e->buffer->filename) - 1);
}

static void free_edcon(EditorContext *e) {
    for (int i = 0; i < e->buf_count; i++)
        gap_free(&e->buffers[i].text);
}

/* Read the full gap content into a heap-allocated, NUL-terminated string.
 * Caller must free() the result. */
static char *read_gap(EditorContext *e) {
    int len = gap_len(&e->buffer->text);
    char *buf = malloc(len + 1);
    for (int i = 0; i < len; i++) buf[i] = gap_char(&e->buffer->text, i);
    buf[len] = '\0';
    return buf;
}

/* -----------------------------------------------------------------------
 * load_file
 * ----------------------------------------------------------------------- */

TEST(load_file_returns_1_on_success) {
    char path[64];
    if (!write_tempfile(path, "hello")) { ASSERT_TRUE(0); return; }

    EditorContext e;
    init_edcon(&e, path);
    int result = load_file(&e);
    ASSERT_EQ(result, 1);

    free_edcon(&e);
    cleanup_file(path);
}

TEST(load_file_returns_0_on_missing_file) {
    EditorContext e;
    init_edcon(&e, "/tmp/orpheus_definitely_does_not_exist_xyz");
    int result = load_file(&e);
    ASSERT_EQ(result, 0);
    free_edcon(&e);
}

TEST(load_file_correct_content) {
    char path[64];
    const char *src = "hello, world";
    if (!write_tempfile(path, src)) { ASSERT_TRUE(0); return; }

    EditorContext e;
    init_edcon(&e, path);
    load_file(&e);

    char *got = read_gap(&e);
    ASSERT_STR_EQ(got, src);
    free(got);

    free_edcon(&e);
    cleanup_file(path);
}

TEST(load_file_correct_char_count) {
    char path[64];
    const char *src = "abcde";
    if (!write_tempfile(path, src)) { ASSERT_TRUE(0); return; }

    EditorContext e;
    init_edcon(&e, path);
    load_file(&e);
    ASSERT_EQ(gap_len(&e.buffer->text), (int)strlen(src));

    free_edcon(&e);
    cleanup_file(path);
}

TEST(load_file_multiline_content) {
    char path[64];
    const char *src = "line1\nline2\nline3\n";
    if (!write_tempfile(path, src)) { ASSERT_TRUE(0); return; }

    EditorContext e;
    init_edcon(&e, path);
    load_file(&e);

    char *got = read_gap(&e);
    ASSERT_STR_EQ(got, src);
    free(got);

    free_edcon(&e);
    cleanup_file(path);
}

TEST(load_file_sets_line_count) {
    char path[64];
    /* 3 newlines -> 4 lines */
    const char *src = "a\nb\nc\nd";
    if (!write_tempfile(path, src)) { ASSERT_TRUE(0); return; }

    EditorContext e;
    init_edcon(&e, path);
    load_file(&e);
    ASSERT_EQ(e.buffer->line_count, 4);

    free_edcon(&e);
    cleanup_file(path);
}

TEST(load_file_resets_cursor_to_zero) {
    char path[64];
    if (!write_tempfile(path, "hello")) { ASSERT_TRUE(0); return; }

    EditorContext e;
    init_edcon(&e, path);
    e.buffer->cursor = 99;   /* simulate leftover state */
    load_file(&e);
    ASSERT_EQ(e.buffer->cursor, 0);

    free_edcon(&e);
    cleanup_file(path);
}

TEST(load_file_empty_file) {
    char path[64];
    if (!write_tempfile(path, "")) { ASSERT_TRUE(0); return; }

    EditorContext e;
    init_edcon(&e, path);
    int result = load_file(&e);
    ASSERT_EQ(result, 1);
    ASSERT_EQ(gap_len(&e.buffer->text), 0);

    free_edcon(&e);
    cleanup_file(path);
}

/* -----------------------------------------------------------------------
 * save_file
 * ----------------------------------------------------------------------- */

TEST(save_file_returns_0_when_no_filename) {
    EditorContext e;
    init_edcon(&e, NULL);   /* filename left as "" */
    int result = save_file(&e);
    ASSERT_EQ(result, 0);
    free_edcon(&e);
}

TEST(save_file_sets_status_when_no_filename) {
    EditorContext e;
    init_edcon(&e, NULL);
    save_file(&e);
    ASSERT_TRUE(e.buffer->status[0] != '\0');
    free_edcon(&e);
}

TEST(save_file_returns_1_on_success) {
    char path[64];
    strcpy(path, "/tmp/orpheus_save_XXXXXX");
    int fd = mkstemp(path);
    if (fd < 0) { ASSERT_TRUE(0); return; }
    close(fd);

    EditorContext e;
    init_edcon(&e, path);
    const char *src = "saved content";
    for (int i = 0; i < (int)strlen(src); i++)
        gap_insert(&e.buffer->text, i, src[i]);

    int result = save_file(&e);
    ASSERT_EQ(result, 1);

    free_edcon(&e);
    cleanup_file(path);
}

TEST(save_file_clears_dirty_flag) {
    char path[64];
    strcpy(path, "/tmp/orpheus_save_XXXXXX");
    int fd = mkstemp(path);
    if (fd < 0) { ASSERT_TRUE(0); return; }
    close(fd);

    EditorContext e;
    init_edcon(&e, path);
    e.buffer->dirty = 1;
    save_file(&e);
    ASSERT_EQ(e.buffer->dirty, 0);

    free_edcon(&e);
    cleanup_file(path);
}

TEST(save_file_written_bytes_match) {
    char path[64];
    strcpy(path, "/tmp/orpheus_save_XXXXXX");
    int fd = mkstemp(path);
    if (fd < 0) { ASSERT_TRUE(0); return; }
    close(fd);

    EditorContext e;
    init_edcon(&e, path);
    const char *src = "hello, file";
    for (int i = 0; i < (int)strlen(src); i++)
        gap_insert(&e.buffer->text, i, src[i]);
    save_file(&e);

    /* Read the file back with fgetc and compare. */
    FILE *f = fopen(path, "r");
    ASSERT_NOT_NULL(f);
    char buf[64];
    int n = 0, c;
    while ((c = fgetc(f)) != EOF) buf[n++] = (char)c;
    buf[n] = '\0';
    fclose(f);

    ASSERT_EQ(n, (int)strlen(src));
    ASSERT_STR_EQ(buf, src);

    free_edcon(&e);
    cleanup_file(path);
}

TEST(save_file_sets_status_message) {
    char path[64];
    strcpy(path, "/tmp/orpheus_save_XXXXXX");
    int fd = mkstemp(path);
    if (fd < 0) { ASSERT_TRUE(0); return; }
    close(fd);

    EditorContext e;
    init_edcon(&e, path);
    save_file(&e);
    ASSERT_TRUE(e.buffer->status[0] != '\0');

    free_edcon(&e);
    cleanup_file(path);
}

/* -----------------------------------------------------------------------
 * round-trip: load then save, verify byte-for-byte identity
 * ----------------------------------------------------------------------- */

TEST(roundtrip_load_save_identical) {
    char src_path[64], dst_path[64];
    const char *content = "the quick brown fox\njumped over\nthe lazy dog\n";

    if (!write_tempfile(src_path, content)) { ASSERT_TRUE(0); return; }

    strcpy(dst_path, "/tmp/orpheus_rt_XXXXXX");
    int fd = mkstemp(dst_path);
    if (fd < 0) { cleanup_file(src_path); ASSERT_TRUE(0); return; }
    close(fd);

    EditorContext e;
    init_edcon(&e, src_path);
    load_file(&e);

    strncpy(e.buffer->filename, dst_path, sizeof(e.buffer->filename) - 1);
    save_file(&e);

    FILE *f = fopen(dst_path, "r");
    ASSERT_NOT_NULL(f);
    char buf[256];
    int n = 0, c;
    while ((c = fgetc(f)) != EOF) buf[n++] = (char)c;
    buf[n] = '\0';
    fclose(f);

    ASSERT_EQ(n, (int)strlen(content));
    ASSERT_STR_EQ(buf, content);

    free_edcon(&e);
    cleanup_file(src_path);
    cleanup_file(dst_path);
}

TEST(roundtrip_binary_safe_all_printable_ascii) {
    /* All printable ASCII 32-126 in one block. */
    char content[100];
    int len = 0;
    for (int ch = 32; ch <= 126; ch++) content[len++] = (char)ch;
    content[len] = '\0';

    char src_path[64], dst_path[64];
    if (!write_tempfile(src_path, content)) { ASSERT_TRUE(0); return; }

    strcpy(dst_path, "/tmp/orpheus_rt2_XXXXXX");
    int fd = mkstemp(dst_path);
    if (fd < 0) { cleanup_file(src_path); ASSERT_TRUE(0); return; }
    close(fd);

    EditorContext e;
    init_edcon(&e, src_path);
    load_file(&e);
    strncpy(e.buffer->filename, dst_path, sizeof(e.buffer->filename) - 1);
    save_file(&e);

    FILE *f = fopen(dst_path, "r");
    ASSERT_NOT_NULL(f);
    char buf[200];
    int n = 0, c;
    while ((c = fgetc(f)) != EOF) buf[n++] = (char)c;
    buf[n] = '\0';
    fclose(f);

    ASSERT_EQ(n, len);
    ASSERT_STR_EQ(buf, content);

    free_edcon(&e);
    cleanup_file(src_path);
    cleanup_file(dst_path);
}

/* -----------------------------------------------------------------------
 * Test runner
 * ----------------------------------------------------------------------- */

int main(void) {
    SUITE("load_file");
    RUN(load_file_returns_1_on_success);
    RUN(load_file_returns_0_on_missing_file);
    RUN(load_file_correct_content);
    RUN(load_file_correct_char_count);
    RUN(load_file_multiline_content);
    RUN(load_file_sets_line_count);
    RUN(load_file_resets_cursor_to_zero);
    RUN(load_file_empty_file);

    SUITE("save_file");
    RUN(save_file_returns_0_when_no_filename);
    RUN(save_file_sets_status_when_no_filename);
    RUN(save_file_returns_1_on_success);
    RUN(save_file_clears_dirty_flag);
    RUN(save_file_written_bytes_match);
    RUN(save_file_sets_status_message);

    SUITE("round-trip");
    RUN(roundtrip_load_save_identical);
    RUN(roundtrip_binary_safe_all_printable_ascii);

    SUMMARY();
    return g_failed > 0 ? 1 : 0;
}