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

#include <ctype.h>
#include <string.h>
#include <stdlib.h>
#include <ncurses.h>
#include "input.h"
#include "buffer.h"
#include "display.h"
#include "fileio.h"

/**
 * @brief Read a single-line string from the user via the command bar.
 *
 * Displays @p prompt on the bottom row and echoes characters as the user types. Backspace removes 
 * the last character. Pressing Enter confirms. Pressing Escape cancels and returns 0.
 *
 * @param edcon The EditorContext Instance.
 * @param prompt Prompt string shown before the input area.
 * @param out Buffer to receive the entered string (NUL-terminated).
 * @param max Size of @p out in bytes, including the NUL terminator.
 * @return 1 if the user confirmed a non-empty string, 0 if cancelled or empty.
 */
int mini_input(EditorContext *edcon, const char *prompt, char *out, int max) {
    int len = 0;
    int input_row = edcon->buffer->rows - 1;
    out[0]  = '\0';
    move(input_row, 0);
    attron(COLOR_PAIR(CP_CMDBAR) | A_BOLD);
    clrtoeol();
    printw(" %s", prompt);
    attroff(A_BOLD);
    refresh();
    curs_set(1);
    int c;
    while ((c = getch()) != '\n' && c != KEY_ENTER) {
        if (c == 27) { attroff(COLOR_PAIR(CP_CMDBAR)); return 0; }
        if ((c == KEY_BACKSPACE || c == 127 || c == '\b') && len > 0) {
            out[--len] = '\0';
        } else if (isprint(c) && len < max - 1) {
            out[len++] = (char)c;
            out[len]   = '\0';
        }
        move(input_row, 0);
        clrtoeol();
        printw(" %s%s", prompt, out);
        refresh();
    }
    attroff(COLOR_PAIR(CP_CMDBAR));
    return len > 0;
}

// --- Navigation ---

/**
 * @brief Move the cursor one line up, preserving visual column where possible.
 *
 * Attempts to place the cursor at the same visual column on the previous line.
 * If that column exceeds the line's length the cursor is placed at the end of
 * the line. Moves to offset 0 when already on line 0.
 * 
 * @param cfg_ptr A pointer to the Config Instance.
 * @param edcon The EditorContext Instance.
 */
void move_up(Config *cfg_ptr, EditorContext *edcon) {
    int ln = edcon->buffer->current_line;
    if (ln == 0) { edcon->buffer->cursor = 0; edcon->buffer->current_line = 0; return; }
    int vcol = cursor_vcol(cfg_ptr, edcon);
    int s = line_start(edcon, ln - 1);
    int l = line_len(edcon, ln - 1);
    edcon->buffer->cursor = s + (vcol < l ? vcol : l);
    edcon->buffer->current_line = ln - 1;
}

/**
 * @brief Move the cursor one line down, preserving visual column where possible.
 *
 * Attempts to place the cursor at the same visual column on the next line.
 * If that column exceeds the line's length the cursor is placed at the end of
 * the line.  Does nothing when already on the last line.
 * 
 * @param cfg_ptr A pointer to the Config Instance.
 * @param edcon The EditorContext Instance.
 */
void move_down(Config *cfg_ptr, EditorContext *edcon) {
    int ln = edcon->buffer->current_line;
    if (ln >= total_lines(edcon) - 1) return;
    int vcol = cursor_vcol(cfg_ptr, edcon);
    int s = line_start(edcon, ln + 1);
    int l = line_len(edcon, ln + 1);
    edcon->buffer->cursor = s + (vcol < l ? vcol : l);
    edcon->buffer->current_line = ln + 1;
}

/**
 * @brief Move the cursor one character to the left.
 *
 * Decrements @c edcon->buffer->cursor by one and calls update_current_line_delta() to keep
 * @c edcon->buffer->current_line in sync.  Does nothing at the start of the buffer.
 */
void move_left(EditorContext *edcon) {
    if (edcon->buffer->cursor > 0) {
        int old = edcon->buffer->cursor;
        edcon->buffer->cursor--;
        update_current_line_delta(edcon, old, edcon->buffer->cursor);
    }
}

/**
 * @brief Move the cursor one character to the right.
 *
 * Increments @c edcon->buffer->cursor by one and calls update_current_line_delta() to keep
 * @c edcon->buffer->current_line in sync.  Does nothing at the end of the buffer.
 * 
 * @param edcon The EditorContext Instance.
 */
void move_right(EditorContext *edcon) {
    if (edcon->buffer->cursor < gap_len(&edcon->buffer->text)) {
        int old = edcon->buffer->cursor;
        edcon->buffer->cursor++;
        update_current_line_delta(edcon, old, edcon->buffer->cursor);
    }
}

/**
 * @brief Move the cursor to the first character of the current line (Home).
 * 
 * @param edcon The EditorContext Instance.
 */
void move_line_start(EditorContext *edcon) {
    int ln = edcon->buffer->current_line;
    edcon->buffer->cursor = line_start(edcon, ln);
}

/**
 * @brief Move the cursor past the last character of the current line (End).
 * 
 * @param edcon The EditorContext Instance.
 */
void move_line_end(EditorContext *edcon) {
    int ln = edcon->buffer->current_line;
    edcon->buffer->cursor = line_start(edcon, ln) + line_len(edcon, ln);
}

/**
 * @brief Scroll the view up by one full page (Page Up).
 *
 * Calls move_up() once per visible text row, effectively jumping the cursor
 * up by the current viewport height.
 * 
 * @param cfg_ptr A pointer to the Config Instance.
 * @param edcon The EditorContext Instance.
 */
void move_page_up(Config *cfg_ptr, EditorContext *edcon) {
    int text_rows = edcon->buffer->rows - (cfg_ptr->show_statusbar ? 2 : 0);
    for (int i = 0; i < text_rows; i++) move_up(cfg_ptr, edcon);
}

/**
 * @brief Scroll the view down by one full page (Page Down).
 *
 * Calls move_down() once per visible text row, effectively jumping the cursor
 * down by the current viewport height.
 * 
 * @param cfg_ptr A pointer to the Config Instance.
 * @param edcon The EditorContext Instance.
 */
void move_page_down(Config *cfg_ptr, EditorContext *edcon) {
    int text_rows = edcon->buffer->rows - (cfg_ptr->show_statusbar ? 2 : 0);
    for (int i = 0; i < text_rows; i++) move_down(cfg_ptr, edcon);
}

// --- Command Operations ---

/**
 * @brief Interactive forward search (Ctrl-F).
 *
 * Prompts the user for a search term via mini_input(). If found, moves the cursor to the first 
 * match at or after the current position (wrapping around the end of the buffer) and updates 
 * @c edcon->buffer->search_term for repeat searches. Sets @c edcon->buffer->status to indicate success or failure.
 */
void do_find(EditorContext *edcon) {
    char term[256] = {0};
    if (!mini_input(edcon, "Find: ", term, sizeof term)) {
        edcon->buffer->status[0] = '\0';
        return;
    }
    strncpy(edcon->buffer->search_term, term, sizeof(edcon->buffer->search_term));
    edcon->buffer->search_term[sizeof(edcon->buffer->search_term) - 1] = '\0';
    edcon->buffer->last_search_pos = edcon->buffer->cursor;

    int len = gap_len(&edcon->buffer->text);
    int tlen = strlen(term);
    int start = (edcon->buffer->cursor + 1) % len;

    for (int i = 0; i < len; i++) {
        int pos = (start + i) % len;
        int match = 1;
        for (int j = 0; j < tlen && match; j++) {
            int p2 = (pos + j) % len;
            if (gap_char(&edcon->buffer->text, p2) != term[j]) match = 0;
        }
        if (match) {
            edcon->buffer->cursor = pos;
            set_status(edcon, "Found \"%s\"", term);
            edcon->buffer->current_line = pos_to_line(edcon, pos);
            return;
        }
    }
    set_status(edcon, "Not found: \"%s\"", term);
}

/**
 * @brief Interactive find-and-replace (Ctrl-R).
 *
 * Prompts the user for a search term and a replacement string via
 * mini_input(), then asks whether to replace @b All occurrences or only the
 * @b Next occurrence after the cursor. Pressing Escape at any prompt cancels
 * the operation.
 *
 * - "Replace All" scans from position 0 and replaces every match, reporting
 *   the total count on completion.
 * - "Replace Next" replaces the first match at or after the cursor (wrapping
 *   around the buffer).
 *
 * An empty replacement string is valid and deletes each matched substring.
 * 
 * @param edcon The EditorContext Instance.
 */
void do_replace(EditorContext *edcon) {
    char term[256] = {0};
    char rep[256] = {0};

    if (!mini_input(edcon, "Replace: ", term, sizeof term))
        return;
    if (!mini_input(edcon, "With: ", rep, sizeof rep)) {
        // empty replacement is valid — it means "delete the match"
        rep[0] = '\0';
    }

    int tlen = strlen(term);
    int rlen = strlen(rep);
    if (tlen == 0) { set_status(edcon, "Nothing to replace"); return; }

    move(edcon->buffer->rows - 1, 0);
    attron(COLOR_PAIR(CP_CMDBAR) | A_BOLD);
    clrtoeol();
    printw(" Replace [A]ll / [N]ext / Esc to cancel");
    attroff(COLOR_PAIR(CP_CMDBAR) | A_BOLD);

    refresh();
    int choice = getch();
    if (choice == 27) { set_status(edcon, "Replace cancelled"); return; }

    int total_len = gap_len(&edcon->buffer->text);

    if (choice == 'a' || choice == 'A') {
        // Replace All — scan from position 0, replace each match.
        int count = 0;
        int pos   = 0;
        while (pos <= gap_len(&edcon->buffer->text) - tlen) {
            int match = 1;
            for (int j = 0; j < tlen && match; j++)
                if (gap_char(&edcon->buffer->text, pos + j) != term[j]) match = 0;

            if (match) {
                // delete tlen chars at pos
                for (int j = 0; j < tlen; j++)
                    gap_delete(&edcon->buffer->text, pos);

                // insert replacement at pos 
                for (int j = 0; j < rlen; j++)
                    gap_insert(&edcon->buffer->text, pos + j, rep[j]);

                pos += rlen;   // skip past what we just inserted
                count++;
                edcon->buffer->dirty = 1;
            } else {
                pos++;
            }
        }
        if (count) {
            rebuild_line_count(edcon);
            edcon->buffer->current_line = pos_to_line(edcon, edcon->buffer->cursor);
            set_status(edcon, "Replaced %d occurrence%s", count, count == 1 ? "" : "s");
        } else {
            set_status(edcon, "Not found: \"%s\"", term);
        }
    } else if (choice == 'n' || choice == 'N') {
        // Replace Next — search forward from cursor (wrapping).
        total_len = gap_len(&edcon->buffer->text);
        int start = edcon->buffer->cursor;
        int found = -1;

        for (int i = 0; i < total_len; i++) {
            int pos = (start + i) % total_len;
            if (pos + tlen > total_len) continue;

            int match = 1;
            for (int j = 0; j < tlen && match; j++)
                if (gap_char(&edcon->buffer->text, pos + j) != term[j]) match = 0;

            if (match) { found = pos; break; }
        }
        if (found < 0) {
            set_status(edcon, "Not found: \"%s\"", term);
            return;
        }

        for (int j = 0; j < tlen; j++)
            gap_delete(&edcon->buffer->text, found);

        for (int j = 0; j < rlen; j++)
            gap_insert(&edcon->buffer->text, found + j, rep[j]);

        edcon->buffer->cursor = found + rlen;
        edcon->buffer->dirty  = 1;
        rebuild_line_count(edcon);
        edcon->buffer->current_line = pos_to_line(edcon, edcon->buffer->cursor);
        set_status(edcon, "Replaced \"%s\" with \"%s\"", term, rep);
    } else {
        set_status(edcon, "Replace cancelled");
    }
}

/**
 * @brief Cut (copy + delete) the current line into the clipboard (Ctrl-K).
 *
 * Copies the content of @c edcon->buffer->current_line (excluding the newline) into
 * @c edcon->buffer->clipboard, then deletes the line and its trailing newline from the
 * buffer. Rebuilds cached statistics and updates the cursor.
 */
void cut_line(EditorContext *edcon) {
    int ln = edcon->buffer->current_line;
    int s = line_start(edcon, ln);
    int len = line_len(edcon, ln);
    int end = s + len;

    // copy to clipboard
    int clen = 0;
    for (int i = s; i < end; i++)
        if (clen < (int)sizeof(edcon->buffer->clipboard) - 1)
            edcon->buffer->clipboard[clen++] = gap_char(&edcon->buffer->text, i);
    edcon->buffer->clipboard[clen] = '\0';
    edcon->buffer->cb_len = clen;

    // delete line content + newline
    int del = len + (end < gap_len(&edcon->buffer->text) ? 1 : 0);
    for (int i = 0; i < del; i++) gap_delete(&edcon->buffer->text, s);

    edcon->buffer->cursor = s;
    edcon->buffer->dirty  = 1;
    rebuild_line_count(edcon);
    edcon->buffer->current_line = pos_to_line(edcon, edcon->buffer->cursor);
    set_status(edcon, "Cut line");
}

/**
 * @brief Paste the clipboard contents as a new line above the current line (Ctrl-U).
 *
 * Inserts the text stored in @c edcon->buffer->clipboard at the start of the current line,
 * followed by a newline character. Does nothing and sets a status message if
 * the clipboard is empty. Rebuilds cached statistics after the insertion.
 * 
 * @param edcon The EditorContext Instance.
 */
void paste_line(EditorContext *edcon) {
    if (!edcon->buffer->cb_len) { set_status(edcon, "Clipboard empty"); return; }
    int ln = edcon->buffer->current_line;
    int s  = line_start(edcon, ln);
    // insert clipboard + newline
    for (int i = 0; i < edcon->buffer->cb_len; i++) 
        gap_insert(&edcon->buffer->text, s + i, edcon->buffer->clipboard[i]);

    gap_insert(&edcon->buffer->text, s + edcon->buffer->cb_len, '\n');
    edcon->buffer->cursor = s;
    edcon->buffer->dirty  = 1;
    rebuild_line_count(edcon);

    edcon->buffer->current_line = pos_to_line(edcon, edcon->buffer->cursor);
    set_status(edcon, "Pasted");
}

/**
 * @brief Delete the current line without copying it to the clipboard (Ctrl-D).
 *
 * Removes all characters on @c edcon->buffer->current_line plus its trailing newline.
 * The cursor is moved to the start of the same line position (now occupied
 * by the following line). Rebuilds cached statistics.
 * 
 * @param edcon The EditorContext Instance.
 */
void delete_line(EditorContext *edcon) {
    int ln = edcon->buffer->current_line;
    int s = line_start(edcon, ln);
    int len = line_len(edcon, ln);
    int end = s + len;
    int del = len + (end < gap_len(&edcon->buffer->text) ? 1 : 0);

    for (int i = 0; i < del; i++) gap_delete(&edcon->buffer->text, s);

    edcon->buffer->cursor = s;
    if (edcon->buffer->cursor > gap_len(&edcon->buffer->text)) 
        edcon->buffer->cursor = gap_len(&edcon->buffer->text);
    edcon->buffer->dirty  = 1;
    rebuild_line_count(edcon);

    edcon->buffer->current_line = pos_to_line(edcon, edcon->buffer->cursor);
    set_status(edcon, "Deleted line");
}

/**
 * @brief Prompt the user for a line number and jump to it (Ctrl-G).
 *
 * Reads a 1-based line number via mini_input(). The value is clamped to the
 * valid range [1, total_lines()]. The cursor is moved to the start of the
 * target line and @c edcon->buffer->current_line is updated accordingly.
 * 
 * @param edcon The EditorContext Instance.
 */
void goto_line(EditorContext *edcon) {
    char buf[32];
    if (!mini_input(edcon, "Go to line: ", buf, sizeof buf)) return;
    int ln = atoi(buf) - 1;
    if (ln < 0) ln = 0;
    int tot = total_lines(edcon);
    if (ln >= tot) ln = tot - 1;
    int s = line_start(edcon, ln);
    edcon->buffer->cursor = s;
    edcon->buffer->current_line = ln;
    set_status(edcon, "Jumped to line %d", ln + 1);
}

// --- Quit Confirmation ---

/**
 * @brief Ask the user to confirm quitting when there are unsaved changes.
 *
 * If the active buffer is clean (E.dirty == 0) the function returns 1
 * immediately.  Otherwise it renders an inline prompt and waits for a keypress:
 * - @c Q — quit without saving, returns 1.
 * - @c S — save then quit, returns 1.
 * - Any other key — cancel, returns 0.
 * 
 * @param edcon The EditorContext Instance.
 * @return 1 if the editor should exit, 0 if the quit was cancelled.
 */
int confirm_quit(EditorContext *edcon) {
    if (!edcon->buffer->dirty) return 1;
    move(edcon->buffer->rows - 1, 0);
    attron(COLOR_PAIR(CP_CMDBAR) | A_BOLD);
    clrtoeol();
    printw(" Unsaved changes! Press Q to quit without saving, S to save, or Esc to cancel.");
    attroff(COLOR_PAIR(CP_CMDBAR) | A_BOLD);
    refresh();

    int c = getch();
    if (c == 'q' || c == 'Q') return 1;
    if (c == 's' || c == 'S') { save_file(edcon); return 1; }
    edcon->buffer->status[0] = '\0';
    return 0;
}