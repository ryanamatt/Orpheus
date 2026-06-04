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
 * @param prompt Prompt string shown before the input area.
 * @param out Buffer to receive the entered string (NUL-terminated).
 * @param max Size of @p out in bytes, including the NUL terminator.
 * @return 1 if the user confirmed a non-empty string, 0 if cancelled or empty.
 */
int mini_input(const char *prompt, char *out, int max) {
    int len = 0;
    int input_row = E.rows - 1;
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
 */
void move_up(Config *cfg_ptr) {
    int ln = E.current_line;
    if (ln == 0) { E.cursor = 0; E.current_line = 0; return; }
    int vcol = cursor_vcol(cfg_ptr);
    int s    = line_start(ln - 1);
    int l    = line_len(ln - 1);
    E.cursor = s + (vcol < l ? vcol : l);
    E.current_line = ln - 1;
}

/**
 * @brief Move the cursor one line down, preserving visual column where possible.
 *
 * Attempts to place the cursor at the same visual column on the next line.
 * If that column exceeds the line's length the cursor is placed at the end of
 * the line.  Does nothing when already on the last line.
 * 
 * @param cfg_ptr A pointer to the Config Instance.
 */
void move_down(Config *cfg_ptr) {
    int ln = E.current_line;
    if (ln >= total_lines() - 1) return;
    int vcol = cursor_vcol(cfg_ptr);
    int s    = line_start(ln + 1);
    int l    = line_len(ln + 1);
    E.cursor = s + (vcol < l ? vcol : l);
    E.current_line = ln + 1;
}

/**
 * @brief Move the cursor one character to the left.
 *
 * Decrements @c E.cursor by one and calls update_current_line_delta() to keep
 * @c E.current_line in sync.  Does nothing at the start of the buffer.
 */
void move_left(void) {
    if (E.cursor > 0) {
        int old = E.cursor;
        E.cursor--;
        update_current_line_delta(old, E.cursor);
    }
}

/**
 * @brief Move the cursor one character to the right.
 *
 * Increments @c E.cursor by one and calls update_current_line_delta() to keep
 * @c E.current_line in sync.  Does nothing at the end of the buffer.
 */
void move_right(void) {
    if (E.cursor < gap_len(&E.text)) {
        int old = E.cursor;
        E.cursor++;
        update_current_line_delta(old, E.cursor);
    }
}

/**
 * @brief Move the cursor to the first character of the current line (Home).
 */
void move_line_start(void) {
    int ln = E.current_line;
    E.cursor = line_start(ln);
}

/**
 * @brief Move the cursor past the last character of the current line (End).
 */
void move_line_end(void) {
    int ln = E.current_line;
    E.cursor = line_start(ln) + line_len(ln);
}

/**
 * @brief Scroll the view up by one full page (Page Up).
 *
 * Calls move_up() once per visible text row, effectively jumping the cursor
 * up by the current viewport height.
 * 
 * @param cfg_ptr A pointer to the Config Instance.
 */
void move_page_up(Config *cfg_ptr) {
    int text_rows = E.rows - (cfg_ptr->show_statusbar ? 2 : 0);
    for (int i = 0; i < text_rows; i++) move_up(cfg_ptr);
}

/**
 * @brief Scroll the view down by one full page (Page Down).
 *
 * Calls move_down() once per visible text row, effectively jumping the cursor
 * down by the current viewport height.
 * 
 * @param cfg_ptr A pointer to the Config Instance.
 */
void move_page_down(Config *cfg_ptr) {
    int text_rows = E.rows - (cfg_ptr->show_statusbar ? 2 : 0);
    for (int i = 0; i < text_rows; i++) move_down(cfg_ptr);
}

// --- Command Operations ---

/**
 * @brief Interactive forward search (Ctrl-F).
 *
 * Prompts the user for a search term via mini_input(). If found, moves the cursor to the first 
 * match at or after the current position (wrapping around the end of the buffer) and updates 
 * @c E.search_term for repeat searches.  Sets @c E.status to indicate success or failure.
 */
void do_find(void) {
    char term[256] = {0};
    if (!mini_input("Find: ", term, sizeof term)) {
        E.status[0] = '\0';
        return;
    }
    strncpy(E.search_term, term, sizeof(E.search_term) - 1);
    E.search_term[sizeof(E.search_term) - 1] = '\0'; // Manually ensure null termination
    E.last_search_pos = E.cursor;

    int len = gap_len(&E.text);
    int tlen = strlen(term);
    int start = (E.cursor + 1) % len;

    for (int i = 0; i < len; i++) {
        int pos = (start + i) % len;
        int match = 1;
        for (int j = 0; j < tlen && match; j++) {
            int p2 = (pos + j) % len;
            if (gap_char(&E.text, p2) != term[j]) match = 0;
        }
        if (match) {
            E.cursor = pos;
            set_status("Found \"%s\"", term);
            E.current_line = pos_to_line(pos);
            return;
        }
    }
    set_status("Not found: \"%s\"", term);
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
 */
void do_replace(void) {
    char term[256]    = {0};
    char rep[256]     = {0};

    if (!mini_input("Replace: ", term, sizeof term))
        return;
    if (!mini_input("With: ", rep, sizeof rep)) {
        // empty replacement is valid — it means "delete the match"
        rep[0] = '\0';
    }

    int tlen = strlen(term);
    int rlen = strlen(rep);
    if (tlen == 0) { set_status("Nothing to replace"); return; }

    move(E.rows - 1, 0);
    attron(COLOR_PAIR(CP_CMDBAR) | A_BOLD);
    clrtoeol();
    printw(" Replace [A]ll / [N]ext / Esc to cancel");
    attroff(COLOR_PAIR(CP_CMDBAR) | A_BOLD);

    refresh();
    int choice = getch();
    if (choice == 27) { set_status("Replace cancelled"); return; }

    int total_len = gap_len(&E.text);

    if (choice == 'a' || choice == 'A') {
        // Replace All — scan from position 0, replace each match.
        int count = 0;
        int pos   = 0;
        while (pos <= gap_len(&E.text) - tlen) {
            int match = 1;
            for (int j = 0; j < tlen && match; j++)
                if (gap_char(&E.text, pos + j) != term[j]) match = 0;

            if (match) {
                // delete tlen chars at pos
                for (int j = 0; j < tlen; j++)
                    gap_delete(&E.text, pos);

                // insert replacement at pos 
                for (int j = 0; j < rlen; j++)
                    gap_insert(&E.text, pos + j, rep[j]);

                pos += rlen;   // skip past what we just inserted
                count++;
                E.dirty = 1;
            } else {
                pos++;
            }
        }
        if (count) {
            rebuild_line_count();
            E.current_line = pos_to_line(E.cursor);
            set_status("Replaced %d occurrence%s", count, count == 1 ? "" : "s");
        } else {
            set_status("Not found: \"%s\"", term);
        }
    } else if (choice == 'n' || choice == 'N') {
        // Replace Next — search forward from cursor (wrapping).
        total_len = gap_len(&E.text);
        int start = E.cursor;
        int found = -1;

        for (int i = 0; i < total_len; i++) {
            int pos = (start + i) % total_len;
            if (pos + tlen > total_len) continue;

            int match = 1;
            for (int j = 0; j < tlen && match; j++)
                if (gap_char(&E.text, pos + j) != term[j]) match = 0;

            if (match) { found = pos; break; }
        }
        if (found < 0) {
            set_status("Not found: \"%s\"", term);
            return;
        }

        for (int j = 0; j < tlen; j++)
            gap_delete(&E.text, found);

        for (int j = 0; j < rlen; j++)
            gap_insert(&E.text, found + j, rep[j]);

        E.cursor = found + rlen;
        E.dirty  = 1;
        rebuild_line_count();
        E.current_line = pos_to_line(E.cursor);
        set_status("Replaced \"%s\" with \"%s\"", term, rep);
    } else {
        set_status("Replace cancelled");
    }
}

/**
 * @brief Cut (copy + delete) the current line into the clipboard (Ctrl-K).
 *
 * Copies the content of @c E.current_line (excluding the newline) into
 * @c E.clipboard, then deletes the line and its trailing newline from the
 * buffer. Rebuilds cached statistics and updates the cursor.
 */
void cut_line(void) {
    int ln = E.current_line;
    int s = line_start(ln);
    int len = line_len(ln);
    int end = s + len;

    // copy to clipboard
    int clen = 0;
    for (int i = s; i < end; i++)
        if (clen < (int)sizeof(E.clipboard) - 1)
            E.clipboard[clen++] = gap_char(&E.text, i);
    E.clipboard[clen] = '\0';
    E.cb_len = clen;

    // delete line content + newline
    int del = len + (end < gap_len(&E.text) ? 1 : 0);
    for (int i = 0; i < del; i++) gap_delete(&E.text, s);

    E.cursor = s;
    E.dirty  = 1;
    rebuild_line_count();
    E.current_line = pos_to_line(E.cursor);
    set_status("Cut line");
}

/**
 * @brief Paste the clipboard contents as a new line above the current line (Ctrl-U).
 *
 * Inserts the text stored in @c E.clipboard at the start of the current line,
 * followed by a newline character. Does nothing and sets a status message if
 * the clipboard is empty. Rebuilds cached statistics after the insertion.
 */
void paste_line(void) {
    if (!E.cb_len) { set_status("Clipboard empty"); return; }
    int ln = E.current_line;
    int s  = line_start(ln);
    // insert clipboard + newline
    for (int i = 0; i < E.cb_len; i++) gap_insert(&E.text, s + i, E.clipboard[i]);

    gap_insert(&E.text, s + E.cb_len, '\n');
    E.cursor = s;
    E.dirty  = 1;
    rebuild_line_count();

    E.current_line = pos_to_line(E.cursor);
    set_status("Pasted");
}

/**
 * @brief Delete the current line without copying it to the clipboard (Ctrl-D).
 *
 * Removes all characters on @c E.current_line plus its trailing newline.
 * The cursor is moved to the start of the same line position (now occupied
 * by the following line). Rebuilds cached statistics.
 */
void delete_line(void) {
    int ln = E.current_line;
    int s = line_start(ln);
    int len = line_len(ln);
    int end = s + len;
    int del = len + (end < gap_len(&E.text) ? 1 : 0);

    for (int i = 0; i < del; i++) gap_delete(&E.text, s);

    E.cursor = s;
    if (E.cursor > gap_len(&E.text)) E.cursor = gap_len(&E.text);
    E.dirty  = 1;
    rebuild_line_count();

    E.current_line = pos_to_line(E.cursor);
    set_status("Deleted line");
}

/**
 * @brief Prompt the user for a line number and jump to it (Ctrl-G).
 *
 * Reads a 1-based line number via mini_input(). The value is clamped to the
 * valid range [1, total_lines()]. The cursor is moved to the start of the
 * target line and @c E.current_line is updated accordingly.
 */
void goto_line(void) {
    char buf[32];
    if (!mini_input("Go to line: ", buf, sizeof buf)) return;
    int ln = atoi(buf) - 1;
    if (ln < 0) ln = 0;
    int tot = total_lines();
    if (ln >= tot) ln = tot - 1;
    int s = line_start(ln);
    E.cursor = s;
    E.current_line = ln;
    set_status("Jumped to line %d", ln + 1);
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
 * @return 1 if the editor should exit, 0 if the quit was cancelled.
 */
int confirm_quit(void) {
    if (!E.dirty) return 1;
    move(E.rows - 1, 0);
    attron(COLOR_PAIR(CP_CMDBAR) | A_BOLD);
    clrtoeol();
    printw(" Unsaved changes! Press Q to quit without saving, S to save, or Esc to cancel.");
    attroff(COLOR_PAIR(CP_CMDBAR) | A_BOLD);
    refresh();

    int c = getch();
    if (c == 'q' || c == 'Q') return 1;
    if (c == 's' || c == 'S') { save_file(); return 1; }
    E.status[0] = '\0';
    return 0;
}