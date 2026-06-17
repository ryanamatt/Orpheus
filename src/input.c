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
#include "logger.h"
#include "input.h"
#include "buffer.h"
#include "display.h"
#include "fileio.h"
#include "logger.h"

/**
 * @brief Handles the Main Input.
 * 
 * This is where all keys for typing characters, navigation and everything in between are 
 * handled.
 * 
 * @param cfg_ptr A pointer to the Config Instance.
 * @param edcon The EditorContext Instance.
 * @return 0, False to stop the main programming if user quits. 1 to continue the programming
 */
int main_input(Config *cfg_ptr, EditorContext *edcon) {
    int c = getch();

    switch (c) {

    case KEY_MOUSE:
        handle_mouse(cfg_ptr, edcon);
        break;

    // Quit
    case ('q' & 0x1f): // Ctrl-Q
        log_debug("main_input: Ctrl-Q quit requested");
        if (confirm_quit(edcon)) return 0;
        break;

    // Save
    case ('s' & 0x1f): // Ctrl-S
        log_debug("main_input: Ctrl-S save requested (filename='%s')",
                  edcon->buffer->filename[0] ? edcon->buffer->filename : "");
        if (!edcon->buffer->filename[0]) {
            char buf[256];
            if (mini_input(edcon, "Save as: ", buf, sizeof buf)) {
                strncpy(edcon->buffer->filename, buf, sizeof(edcon->buffer->filename));
                edcon->buffer->search_term[sizeof(edcon->buffer->search_term) - 1] = '\0';
                log_debug("main_input: save-as '%s'", edcon->buffer->filename);
                save_file(edcon);
            }
        } else save_file(edcon);
        break;

    // Find
    case ('f' & 0x1f): // Ctrl-F
        log_debug("main_input: Ctrl-F find invoked");
        do_find(edcon);
        break;

    // Replace
    case ('r' & 0x1f): // Ctrl-R
        log_debug("main_input: Ctrl-R replace invoked");
        do_replace(edcon);
        break;

    // Go to Line
    case ('g' & 0x1f): // Ctrl-G
        log_debug("main_input: Ctrl-G goto-line invoked");
        goto_line(edcon);
        break;

    // Cut / Paste / Delete
    case ('k' & 0x1f): cut_line(edcon);   break;
    case ('u' & 0x1f): paste_line(edcon); break;
    case ('d' & 0x1f): delete_line(edcon); break;

    // Toggle Status
    case ('w' & 0x1f):
        log_debug("main_input: Ctrl-W toggle statusbar");
        toggle_status(cfg_ptr, edcon);
        break;

    // Toggle Focus / Typewriter mode
    case ('t' & 0x1f):
        log_debug("main_input: Ctrl-T toggle focus mode");
        toggle_focus(cfg_ptr, edcon);
        break;

    case ('o' & 0x1f): { // Ctrl-O - new empty buffer
        log_debug("main_input: Ctrl-O open/new buffer invoked");
        char fname[256];
        if (mini_input(edcon, "Open file (blank for new): ", fname, sizeof fname) && fname[0]) {
            int idx = new_buffer(edcon);
            if (idx < 0) {
                log_error("main_input: Ctrl-O - too many buffers (%d max)", MAX_BUFFERS);
                set_status(edcon, "Too many buffers open (max %d)", MAX_BUFFERS);
            } 
            
            else {
                switch_buffer(edcon, idx);
                strncpy(edcon->buffer->filename, fname, sizeof(edcon->buffer->filename));
                edcon->buffer->filename[sizeof(edcon->buffer->filename) - 1] = '\0';

                if (!load_file(edcon)) {
                    set_status(edcon, "New file: \"%s\"", edcon->buffer->filename);
                    log_debug("main_input: Ctrl-O - new file '%s'", fname);
                } 
                
                else {
                    set_status(edcon, "Opened \"%s\"", edcon->buffer->filename);
                    log_debug("main_input: Ctrl-O - opened existing file '%s'", fname);
                }
            }
        }
        else {
            // blank input - open empty unnamed buffer.
            int idx = new_buffer(edcon);
            if (idx < 0) {
                log_error("main_input: Ctrl-O (blank) - too many buffers (%d max)", MAX_BUFFERS);
                set_status(edcon, "Too many buffers open (max %d)", MAX_BUFFERS);
            } 
            
            else {
                switch_buffer(edcon, idx);
                log_debug("main_input: Ctrl-O - new unnamed buffer[%d]", idx);
                set_status(edcon, "New buffer %d/%d  [No Name]", edcon->cur_buf + 1, edcon->buf_count);
            }
        break;
        }
    }

    case ('n' & 0x1f): // Ctrl-N - next buffer
        if (edcon->buf_count > 1) {
            switch_buffer(edcon, edcon->cur_buf + 1);
            log_debug("main_input: Ctrl-N - switched to buffer %d/%d '%s'",
                      edcon->cur_buf + 1, edcon->buf_count,
                      edcon->buffer->filename[0] ? edcon->buffer->filename : "[No Name]");
            set_status(edcon, "Buffer %d/%d: %s",
                        edcon->cur_buf + 1, edcon->buf_count,
                        edcon->buffer->filename[0] ? edcon->buffer->filename : "[No Name]");
        } 
        
        else {
            log_debug("main_input: Ctrl-N - only one buffer open");
            set_status(edcon, "Only one buffer open");
        }
        break;

    case ('p' & 0x1f): // Ctrl-P - previous buffer
        if (edcon->buf_count > 1) {
            switch_buffer(edcon, edcon->cur_buf - 1);
            log_debug("main_input: Ctrl-P - switched to buffer %d/%d '%s'",
                      edcon->cur_buf + 1, edcon->buf_count,
                      edcon->buffer->filename[0] ? edcon->buffer->filename : "[No Name]");
            set_status(edcon, "Buffer %d/%d: %s",
                        edcon->cur_buf + 1, edcon->buf_count,
                        edcon->buffer->filename[0] ? edcon->buffer->filename : "[No Name]");
        } 
        
        else {
            log_debug("main_input: Ctrl-P - only one buffer open");
            set_status(edcon, "Only one buffer open");
        }
        break;

    // Movement
    case KEY_UP:         move_up(cfg_ptr, edcon);          break;
    case KEY_DOWN:       move_down(cfg_ptr, edcon);        break;
    case KEY_LEFT:       move_left(edcon);              break;
    case KEY_RIGHT:      move_right(edcon);             break;
    case KEY_HOME:
    case ('a' & 0x1f):   move_line_start(edcon);        break;
    case KEY_END:
    case ('e' & 0x1f):   move_line_end(edcon);          break;
    case KEY_PPAGE:      move_page_up(cfg_ptr, edcon);     break;
    case KEY_NPAGE:      move_page_down(cfg_ptr, edcon);   break;

    // Editing
    case KEY_BACKSPACE:
    case 127:
    case '\b':
        if (edcon->buffer->cursor > 0) {
            char deleted = gap_char(&edcon->buffer->text, edcon->buffer->cursor - 1);
            gap_delete(&edcon->buffer->text, edcon->buffer->cursor - 1);
            int old = edcon->buffer->cursor;
            edcon->buffer->cursor--;

            if (deleted == '\n') {
                update_current_line_delta(edcon, old, edcon->buffer->cursor);
            }
            
            update_stats(edcon, deleted, -1);
            edcon->buffer->dirty = 1;
        }
        break;

    case KEY_DC: // Delete Key
        if (edcon->buffer->cursor < gap_len(&edcon->buffer->text)) {
            gap_delete(&edcon->buffer->text, edcon->buffer->cursor);
            edcon->buffer->dirty = 1;
        }
        break;

    case '\t':
        for (int i = 0; i < cfg_ptr->tab_width; i++) {
            gap_insert(&edcon->buffer->text, edcon->buffer->cursor, ' ');
            edcon->buffer->cursor++;
            update_stats(edcon, ' ', +1);
        }
        edcon->buffer->dirty = 1;
        break;

    case '\n':
    case KEY_ENTER:
        gap_insert(&edcon->buffer->text, edcon->buffer->cursor, '\n');
        edcon->buffer->cursor++;
        update_stats(edcon, '\n', +1);
        edcon->buffer->current_line++;

        if (cfg_ptr->auto_indent) {
            int ln = edcon->buffer->current_line - 1;
            int s = line_start(edcon, ln);
            int end = s + line_len(edcon, ln);
            int ws = s;
            while (ws < end && (gap_char(&edcon->buffer->text, ws) == ' ' 
                || gap_char(&edcon->buffer->text, ws) == '\t'))
                ws++;
            int indent = ws - s;
            for (int i = 0; i < indent; i++) {
                char wc = gap_char(&edcon->buffer->text, s + i);
                gap_insert(&edcon->buffer->text, edcon->buffer->cursor, wc);
                edcon->buffer->cursor++;
                update_stats(edcon, wc, +1);
            }
        }
        edcon->buffer->dirty = 1;
        break;

    default:
        if (c >= 32 && c < 256 && c != 127) {
            gap_insert(&edcon->buffer->text, edcon->buffer->cursor, (char)c);
            edcon->buffer->cursor++;
            update_stats(edcon, (char)c, +1);
            edcon->buffer->dirty = 1;
        }
        break;
    }
    return 1;
}

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

// --- Mouse Input ---

/**
 * @brief Convert a clicked screen column to a logical character offset within a line.
 *
 * Walks the characters on @p ln from the line start, expanding tabs using @c cfg_ptr->tab_width,
 * until the accumulated visual column meets or passes @p target_vcol. Accounts for the
 * horizontal scroll offset (@c col_off).
 *
 * @param cfg_ptr A pointer to the Config Instance.
 * @param edcon   The EditorContext Instance.
 * @param ln      Zero-based line number to search within.
 * @param target_vcol  Visual column the user clicked (after subtracting gutter width and col_off).
 * @return Logical character offset into the buffer.
 */
int vcol_to_pos(Config *cfg_ptr, EditorContext *edcon, int ln, int target_vcol) {
    int s = line_start(edcon, ln);
    int end = s + line_len(edcon, ln);
    int col = 0;
    for (int i = s; i < end; i++) {
        if (col >= target_vcol) return i;
        char c = gap_char(&edcon->buffer->text, i);
        if (c == '\t')
            col = (col / cfg_ptr->tab_width + 1) * cfg_ptr->tab_width;
        else
            col++;
    }
    return end; // past end of line
}

/**
 * @brief Handle a mouse event from ncurses.
 *
 * Reads the pending MEVENT and dispatches:
 * - BUTTON1_PRESSED in the tab bar row: switch to the clicked buffer tab.
 * - BUTTON1_PRESSED in the text area: move the cursor to the clicked position,
 *   clamping to the nearest valid character.
 * - BUTTON4_PRESSED (scroll up) / BUTTON5_PRESSED (scroll down): scroll the
 *   viewport by three lines.
 *
 * The gutter width, tab-bar row, status-bar rows, and horizontal scroll offset
 * are all accounted for when translating screen coordinates to buffer positions.
 *
 * @param cfg_ptr A pointer to the Config Instance.
 * @param edcon   The EditorContext Instance.
 */
void handle_mouse(Config *cfg_ptr, EditorContext *edcon) {
    MEVENT ev;
    if (getmouse(&ev) != OK) return;

    int tab_rows  = (edcon->buf_count > 1) ? 1 : 0;
    int stat_rows = cfg_ptr->show_statusbar ? 2 : 0;
    int text_rows = edcon->buffer->rows - stat_rows - tab_rows;
    int tab_row   = edcon->buffer->rows - stat_rows - 1; // row where the tab bar sits

    // Tab bar click -> switch buffer
    if (tab_rows && ev.y == tab_row && (ev.bstate & BUTTON1_PRESSED)) {
        int x = 0;
        for (int i = 0; i < edcon->buf_count; i++) {
            const char *name = edcon->buffers[i].filename[0]
                               ? edcon->buffers[i].filename : "[No Name]";
            const char *base = strrchr(name, '/');
            base = base ? base + 1 : name;
            char tab[64];
            int tlen = snprintf(tab, sizeof tab, " %s%s ",
                                base, edcon->buffers[i].dirty ? "+" : "");
            if (ev.x >= x && ev.x < x + tlen) {
                switch_buffer(edcon, i);
                log_debug("handle_mouse: tab-click -> buffer %d '%s'", i,
                          edcon->buffer->filename[0] ? edcon->buffer->filename : "[No Name]");
                set_status(edcon, "Buffer %d/%d: %s", i + 1, edcon->buf_count,
                           edcon->buffer->filename[0] ? edcon->buffer->filename : "[No Name]");
                return;
            }
            x += tlen;
        }
        return; // click was in tab bar padding, ignore
    }

    // Scroll wheel -> 3 lines at a time
    if (ev.bstate & BUTTON4_PRESSED) { // scroll up
        for (int i = 0; i < 3; i++) move_up(cfg_ptr, edcon);
        log_debug("handle_mouse: scroll up");
        return;
    }
    if (ev.bstate & BUTTON5_PRESSED) { // scroll down
        for (int i = 0; i < 3; i++) move_down(cfg_ptr, edcon);
        log_debug("handle_mouse: scroll down");
        return;
    }
 
    // Left click in text area -> position cursor
    if ((ev.bstate & BUTTON1_PRESSED) && ev.y < text_rows) {
        int clicked_line = ev.y + edcon->buffer->row_off;
        int total = total_lines(edcon);
        if (clicked_line >= total) clicked_line = total - 1;
 
        // ev.x includes the gutter; subtract it to get the visual column,
        // then add the horizontal scroll offset to get the true vcol.
        int clicked_vcol = (ev.x - cfg_ptr->gutter_width) + edcon->buffer->col_off;
        if (clicked_vcol < 0) clicked_vcol = 0;
 
        int new_pos = vcol_to_pos(cfg_ptr, edcon, clicked_line, clicked_vcol);
        edcon->buffer->cursor = new_pos;
        edcon->buffer->current_line = clicked_line;
        log_debug("handle_mouse: click row=%d col=%d -> line=%d pos=%d (was %d)",
                  ev.y, ev.x, clicked_line, new_pos, edcon->buffer->cursor);
        return;
    }
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
    log_debug("do_find: searching for '%s' from cursor=%d", term, edcon->buffer->cursor);
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
            log_debug("do_find: found '%s' at pos=%d line=%d", term, pos, pos_to_line(edcon, pos));
            set_status(edcon, "Found \"%s\"", term);
            edcon->buffer->current_line = pos_to_line(edcon, pos);
            return;
        }
    }
    log_debug("do_find: '%s' not found", term);
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
        // empty replacement is valid - it means "delete the match"
        rep[0] = '\0';
    }

    int tlen = strlen(term);
    int rlen = strlen(rep);
    if (tlen == 0) { set_status(edcon, "Nothing to replace"); return; }

    log_debug("do_replace: term='%s' rep='%s'", term, rep);

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
        // Replace All - scan from position 0, replace each match.
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
            log_debug("do_replace: replaced all - %d occurrence(s) of '%s'", count, term);
            set_status(edcon, "Replaced %d occurrence%s", count, count == 1 ? "" : "s");
        } else {
            log_debug("do_replace: replace-all - '%s' not found", term);
            set_status(edcon, "Not found: \"%s\"", term);
        }
    } else if (choice == 'n' || choice == 'N') {
        // Replace Next - search forward from cursor (wrapping).
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
            log_debug("do_replace: replace-next - '%s' not found", term);
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
        log_debug("do_replace: replaced next '%s' -> '%s' at pos=%d", term, rep, found);
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
    log_debug("cut_line: cut line %d (%d chars) from '%s'", ln + 1, clen,
              edcon->buffer->filename[0] ? edcon->buffer->filename : "[No Name]");
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
    log_debug("paste_line: pasted %d chars at line %d", edcon->buffer->cb_len, ln + 1);
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
    log_debug("delete_line: deleted line %d (%d chars) from '%s'", ln + 1, len,
              edcon->buffer->filename[0] ? edcon->buffer->filename : "[No Name]");
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
    log_debug("goto_line: jumped to line %d (pos=%d)", ln + 1, s);
    set_status(edcon, "Jumped to line %d", ln + 1);
}

// --- Quit Confirmation ---

/**
 * @brief Ask the user to confirm quitting when there are unsaved changes.
 *
 * If the active buffer is clean (E.dirty == 0) the function returns 1
 * immediately.  Otherwise it renders an inline prompt and waits for a keypress:
 * - @c Q - quit without saving, returns 1.
 * - @c S - save then quit, returns 1.
 * - Any other key - cancel, returns 0.
 * 
 * @param edcon The EditorContext Instance.
 * @return 1 if the editor should exit, 0 if the quit was cancelled.
 */
int confirm_quit(EditorContext *edcon) {
    if (!edcon->buffer->dirty) {
        log_debug("confirm_quit: buffer clean - exiting");
        return 1;
    }
    move(edcon->buffer->rows - 1, 0);
    attron(COLOR_PAIR(CP_CMDBAR) | A_BOLD);
    clrtoeol();
    printw(" Unsaved changes! Press Q to quit without saving, S to save, or Esc to cancel.");
    attroff(COLOR_PAIR(CP_CMDBAR) | A_BOLD);
    refresh();

    int c = getch();
    if (c == 'q' || c == 'Q') {
        log_debug("confirm_quit: user chose quit without saving ('%s')",
                  edcon->buffer->filename[0] ? edcon->buffer->filename : "[No Name]");
        return 1;
    }
    if (c == 's' || c == 'S') {
        log_debug("confirm_quit: user chose save-and-quit ('%s')",
                  edcon->buffer->filename[0] ? edcon->buffer->filename : "[No Name]");
        save_file(edcon);
        return 1;
    }
    log_debug("confirm_quit: quit cancelled");
    edcon->buffer->status[0] = '\0';
    return 0;
}