/*
 * Orpheus - a small ncurses text editor
 *
 * Usage: orp [file]
 *
 * Keybindings
 * -----------
 * Arrow keys / PgUp / PgDn / Home / End  - navigation
 * Ctrl-S   save
 * Ctrl-Q   quit (warns on unsaved changes)
 * Ctrl-F   find  (Enter to cycle, Esc to cancel)
 * Ctrl-G   go to line
 * Ctrl-K   cut line
 * Ctrl-U   paste (yank) line
 * Ctrl-D   delete line
 * Ctrl-A   go to start of line
 * Ctrl-E   go to end of line
 * Ctrl+W   Toggle Hiding/Showing the Status Bar
 * 
 * Optional Settings placed in ~/.orpheusrc
 * -----------
 * Works as follows with # as comments:
 * setting=value
 * 
 * tab_width: int 
 *   Sets the number of spaces to tab over
 *  
 * show_line_numbers: int 
 *   If 0, hides the line-number gutter. Any other value shows it. Default: 1.
 * 
 * auto_indent: int
 *   If non-zero, pressing Enter copies the leading whitespace of the current
 *   line to the new line automatically. Default: 1.
 * 
 * show_statusbar: int
 *   If 0, hides the bottom status/command bar, giving one extra row of text.
 *   Default: 1.
 *
 * cursor_style: int
 *   Controls the terminal cursor shape passed to curs_set().
 *     0 = invisible, 1 = normal (default), 2 = very visible / block.
 * 
 * color_scheme: string
 *   Built-in colour theme for the UI chrome.
 *     default  - system default colours (no change)
 *     dark     - white text on dark backgrounds
 *     light    - black text on light backgrounds
 *     mono     - disables colour entirely (A_REVERSE for highlights)
 * 
 * gutter_width: int
 *   The number of spaces for the width of the line number gutter. Default 5.
 * 
 * key_delay: int
 *  The time it takes to wait for escape-sequence processing. Default 50.
 */

#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>

// --- Version ---

#define ORPHEUS_VERSION "0.1.0"

// --- Constants / Settings ---

int TAB_WIDTH = 4;          // spaces per Tab keypress
int SHOW_LINE_NUMBERS = 1;  // show line-number gutter
int AUTO_INDENT = 1;        // copy leading whitespace on Enter
int SHOW_STATUSBAR = 1;     // show the status/command bar row
int CURSOR_STYLE = 1;       // 0=invisible 1=normal 2=block
char COLOR_SCHEME[32] = "default"; // "default" | "dark" | "light" | "mono"
int GUTTER_WIDTH = 5;       // spaces from left to line-number gutter
int KEY_DELAY = 50;         // wait time for escape-sequence processing

#define MAX_STATUS  512
#define CHUNK       64 // gap-buffer growth step (in chars)

// --- Color Pairs ---
#define CP_NORMAL   1
#define CP_STATUS   2
#define CP_CMDBAR   3
#define CP_LNUM     4
#define CP_SEARCH   5

// --- Gap Buffer ---

typedef struct {
    char  *buf;         // raw storage
    int    gap_start;   // index of first gap byte
    int    gap_end;     // index of first byte after gap
    int    size;        // total allocated bytes
} Gap;

static void gap_init(Gap *g) {
    g->size      = CHUNK;
    g->buf       = malloc(g->size);
    g->gap_start = 0;
    g->gap_end   = g->size;
}

static void gap_free(Gap *g) { free(g->buf); }

static int  gap_len(const Gap *g)   { return g->size - (g->gap_end - g->gap_start); }
static char gap_char(const Gap *g, int pos) {
    return pos < g->gap_start ? g->buf[pos] : g->buf[g->gap_end + (pos - g->gap_start)];
}

static void gap_grow(Gap *g, int need) {
    int gap = g->gap_end - g->gap_start;
    if (gap >= need) return;
    int add  = need - gap + CHUNK;
    int nsz  = g->size + add;
    char *nb = realloc(g->buf, nsz);
    // shift tail to make gap continuous at same gap_start
    memmove(nb + g->gap_end + add, nb + g->gap_end, g->size - g->gap_end);
    g->buf     = nb;
    g->gap_end += add;
    g->size     = nsz;
}

/* 
 * lightweight single-character gap shifts.
 * gap_shift_right: moves gap one position to the right (copies 1 byte)
 * gap_shift_left:  moves gap one position to the left  (copies 1 byte)
 *
 * Defined before gap_move so gap_move can call them inline for ±1 moves.
 */
static void gap_shift_right(Gap *g) {
    if (g->gap_end >= g->size) return;
    g->buf[g->gap_start] = g->buf[g->gap_end];
    g->gap_start++;
    g->gap_end++;
}
 
static void gap_shift_left(Gap *g) {
    if (g->gap_start <= 0) return;
    g->gap_end--;
    g->gap_start--;
    g->buf[g->gap_end] = g->buf[g->gap_start];
}

static void gap_move(Gap *g, int pos) {
    if (pos == g->gap_start) return;
    if (pos < g->gap_start) {
        if (g->gap_start - pos == 1) {
            gap_shift_left(g);
            return;
        }
        int n = g->gap_start - pos;
        memmove(g->buf + g->gap_end - n, g->buf + pos, n);
        g->gap_start -= n;
        g->gap_end   -= n;
    } else {
        if (pos - g->gap_start == 1) {
            gap_shift_right(g);
            return;
        }
        int n = pos - g->gap_start;
        memmove(g->buf + g->gap_start, g->buf + g->gap_end, n);
        g->gap_start += n;
        g->gap_end   += n;
    }
}

static void gap_insert(Gap *g, int pos, char c) {
    gap_grow(g, 1);
    gap_move(g, pos);
    g->buf[g->gap_start++] = c;
}

static void gap_delete(Gap *g, int pos) {
    gap_move(g, pos);
    if (g->gap_end < g->size) g->gap_end++;
}

// --- Load Configurations ---

void load_config(void) {
    char path[512];
    snprintf(path, sizeof(path), "%s/.orpheusrc", getenv("HOME"));

    FILE *pfile = fopen(path, "r");
    if (!pfile) return; // no config file use defaults

    char line[128];
    while (fgets(line, sizeof(line), pfile)) {
        if (line[0] == '#' || line[0] == '\n') continue;

        char *key = strtok(line, "=");
        char *val = strtok(NULL, "=");

        if (key && val) {
            if (strcmp(key, "tab_width") == 0) {
                TAB_WIDTH = atoi(val);
            }

            else if (strcmp(key, "show_line_numbers") == 0) {
                SHOW_LINE_NUMBERS = atoi(val);
            }

            else if (strcmp(key, "auto_indent") == 0) {
                AUTO_INDENT = atoi(val);
            }

            else if (strcmp(key, "show_statusbar") == 0) {
                SHOW_STATUSBAR = atoi(val);
            }

            else if (strcmp(key, "cursor_style") == 0) {
                CURSOR_STYLE = atoi(val);
            }

            else if (strcmp(key, "color_scheme") == 0) {
                val[strcspn(val, "\r\n")] = 0;
                strncpy(COLOR_SCHEME, val, sizeof(COLOR_SCHEME - 1));
                COLOR_SCHEME[sizeof(COLOR_SCHEME) - 1] = '\0';
            }

            else if (strcmp(key, "gutter_width") == 0) {
                GUTTER_WIDTH = atoi(val);
            }

            else if (strcmp(key, "key_delay") == 0) {
                KEY_DELAY = atoi(val);
            }
        }
    }
}

// --- Editor State ---

typedef struct {
    Gap   text;
    int   cursor;               // logical char offset                         
    int   row_off;              // first visible row                           
    int   col_off;              // first visible column                        
    int   rows, cols;           // terminal size                               
    int   dirty;                // unsaved changes flag                        
    char  filename[256];
    char  status[MAX_STATUS];
    char  clipboard[4096];      // cut/paste buffer                            
    int   cb_len;
    int   last_search_pos;      // for repeated Ctrl-F search                  
    char  search_term[256];

    int   line_count;           // total newlines + 1 (kept incrementally)
    int   word_count;           // kept incrementally via stats_dirty
    int   stats_dirty;          // non-zero -> word_count needs a full rescan
} Editor;

static Editor E;

// --- helpers ---

// count newlines before logical position pos  ->  line number (0-based)
static int pos_to_line(int pos) {
    int ln = 0;
    for (int i = 0; i < pos; i++)
        if (gap_char(&E.text, i) == '\n') ln++;
    return ln;
}

// return logical position of start of line ln
static int line_start(int ln) {
    int p = 0, len = gap_len(&E.text);
    for (int l = 0; l < ln && p < len; p++)
        if (gap_char(&E.text, p) == '\n') l++;
    return p;
}

// length of line ln (not counting newline)
static int line_len(int ln) {
    int s = line_start(ln), e = s, len = gap_len(&E.text);
    while (e < len && gap_char(&E.text, e) != '\n') e++;
    return e - s;
}

// total number of lines — O(1): return cached value
static int total_lines(void) {
    return E.line_count;
}

// Count total characters (including newlines)
static int count_chars(void) {
    return gap_len(&E.text);
}

// Full O(n) word rescan — only called when stats_dirty is set
static int count_words_full(void) {
    int words = 0, in_word = 0, len = gap_len(&E.text);
    for (int i = 0; i < len; i++) {
        char c = gap_char(&E.text, i);
        if (isspace(c)) in_word = 0;
        else if (!in_word) { in_word = 1; words++; }
    }
    return words;
}

// Return cached word count, rescanning only when the buffer changed
static int count_words(void) {
    if (E.stats_dirty) {
        E.word_count  = count_words_full();
        E.stats_dirty = 0;
    }
    return E.word_count;
}

/*
 * Incremental stat update called on every insert/delete.
 *
 * c        : the character that was inserted (+1) or deleted (-1 delta).
 * delta    : +1 for insert, -1 for delete.
 *
 * line_count is updated exactly here so total_lines() is always O(1).
 * word_count is harder to do perfectly incrementally (word boundaries
 * depend on neighbours), so we set stats_dirty and let count_words()
 * do one full rescan on the next draw — still only once per frame.
 */
static void update_stats(char c, int delta) {
    if (c == '\n') E.line_count += delta;
    E.stats_dirty = 1;
}

/*
 * Rebuild line_count from scratch.  Called after load_file() and
 * whenever a bulk edit (cut/paste/delete-line) is done.
 */
static void rebuild_line_count(void) {
    int n = 1, len = gap_len(&E.text);
    for (int i = 0; i < len; i++)
        if (gap_char(&E.text, i) == '\n') n++;
    E.line_count  = n;
    E.stats_dirty = 1; // force word rescan on next draw
}

// visual column for cursor on its line (tabs expanded)
static int cursor_vcol(void) {
    int ln  = pos_to_line(E.cursor);
    int s   = line_start(ln);
    int col = 0;
    for (int i = s; i < E.cursor; i++) {
        char c = gap_char(&E.text, i);
        if (c == '\t') col = (col / TAB_WIDTH + 1) * TAB_WIDTH;
        else           col++;
    }
    return col;
}

static void set_status(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(E.status, sizeof E.status, fmt, ap);
    va_end(ap);
}

// --- File I/O ---

static int load_file(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    int c;
    while ((c = fgetc(f)) != EOF)
        gap_insert(&E.text, gap_len(&E.text), (char)c);
    fclose(f);
    rebuild_line_count();   // initialise cached line_count + stats_dirty
    return 1;
}

static int save_file(void) {
    if (!E.filename[0]) {
        set_status("No filename - use Ctrl-W to set one");
        return 0;
    }
    FILE *f = fopen(E.filename, "w");
    if (!f) { set_status("Cannot write: %s", strerror(errno)); return 0; }
    int len = gap_len(&E.text);
    for (int i = 0; i < len; i++) fputc(gap_char(&E.text, i), f);
    fclose(f);
    E.dirty = 0;
    set_status("Saved \"%s\"  (%d bytes)", E.filename, len);
    return 1;
}

// --- display ---

static void adjust_scroll(void) {
    int cur_line = pos_to_line(E.cursor);
    int vcol = cursor_vcol();
    int text_rows = E.rows - (SHOW_STATUSBAR ? 2 : 0);   // status + command bar

    if (cur_line < E.row_off)               E.row_off = cur_line;
    if (cur_line >= E.row_off + text_rows)  E.row_off = cur_line - text_rows + 1;
    if (vcol < E.col_off)                   E.col_off = vcol;
    if (vcol >= E.col_off + E.cols - 6)     E.col_off = vcol - (E.cols - 6) + 1;
}

static void draw_rows(void) {
    int total = total_lines();
    int text_rows = E.rows - (SHOW_STATUSBAR ? 2 : 0);

    char fmt[16];
    snprintf(fmt, sizeof(fmt), "%%%dd ", GUTTER_WIDTH - 1);

    for (int y = 0; y < text_rows; y++) {
        int ln = y + E.row_off;
        move(y, 0);

        // line number gutter
        attron(COLOR_PAIR(CP_LNUM));
        if ((ln < total) && SHOW_LINE_NUMBERS) {
            printw(fmt, ln + 1);
        } else {
            for(int i=0; i < GUTTER_WIDTH - 1; i++) addch(' ');
            addch('~');
            addch(' ');
        }
        attroff(COLOR_PAIR(CP_LNUM));

        attron(COLOR_PAIR(CP_NORMAL));
        if (ln < total) {
            int s = line_start(ln);
            int end = s + line_len(ln);
            int col = 0;
            for (int i = s; i < end; i++) {
                char c = gap_char(&E.text, i);
                if (c == '\t') {
                    int next = (col / TAB_WIDTH + 1) * TAB_WIDTH;
                    while (col < next && col - E.col_off < E.cols - 5) {
                        if (col >= E.col_off) addch(' ');
                        col++;
                    }
                } else {
                    if (col >= E.col_off && col - E.col_off < E.cols - 5)
                        addch((unsigned char)c);
                    col++;
                }
            }
        }
        clrtoeol();
        attroff(COLOR_PAIR(CP_NORMAL));
    }
}

static void draw_statusbar(void) {
    attron(COLOR_PAIR(CP_STATUS) | A_BOLD);
    move(E.rows - 2, 0);
    int ln = pos_to_line(E.cursor);
    int col = cursor_vcol();

    int chars = count_chars();
    int words = count_words();

    char left[128], right[128];

    snprintf(left,  sizeof left,  " %.40s%s",
             E.filename[0] ? E.filename : "[No Name]",
             E.dirty ? " [+]" : "");

    snprintf(right, sizeof right, "Ln %d, Col %d | %d lines | %d words %d chars ",
             ln + 1, col + 1, total_lines(), words, chars);

    int pad = E.cols - (int)strlen(left) - (int)strlen(right);
    printw("%s", left);
    for (int i = 0; i < pad && i < E.cols; i++) addch(' ');
    printw("%s", right);
    attroff(COLOR_PAIR(CP_STATUS) | A_BOLD);
}

static void draw_cmdbar(void) {
    attron(COLOR_PAIR(CP_CMDBAR));
    move(E.rows - 1, 0);
    printw(" ^S Save  ^Q Quit  ^F Find  ^G Go-To  ^K Cut  ^U Paste  ^D Del-Ln  ^W Hide");
    clrtoeol();
    attroff(COLOR_PAIR(CP_CMDBAR));

    // show one-shot status message on right side
    if (E.status[0]) {
        int slen = strlen(E.status);
        int x    = E.cols - slen - 2;
        if (x < 0) x = 0;
        attron(COLOR_PAIR(CP_CMDBAR) | A_BOLD);
        mvprintw(E.rows - 1, x, " %s ", E.status);
        attroff(COLOR_PAIR(CP_CMDBAR) | A_BOLD);
    }
}

static void refresh_screen(void) {
    adjust_scroll();
    int ln = pos_to_line(E.cursor);
    int vcol = cursor_vcol();

    draw_rows();
    if (SHOW_STATUSBAR) {
        draw_statusbar();
        draw_cmdbar();
    }

    // position real cursor
    move(ln - E.row_off, GUTTER_WIDTH + vcol - E.col_off);
    wnoutrefresh(stdscr);
    doupdate();
}

// --- Mini Input Line (search / goto) ---

static int mini_input(const char *prompt, char *out, int max) {
    int len = 0;
    out[0]  = '\0';
    move(E.rows - 1, 0);
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
        move(E.rows - 1, 0);
        clrtoeol();
        printw(" %s%s", prompt, out);
        refresh();
    }
    attroff(COLOR_PAIR(CP_CMDBAR));
    return len > 0;
}

// --- Search ---

static void do_find(void) {
    char term[256] = {0};
    if (!mini_input("Find: ", term, sizeof term)) {
        E.status[0] = '\0';
        return;
    }
    strncpy(E.search_term, term, sizeof E.search_term - 1);
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
            return;
        }
    }
    set_status("Not found: \"%s\"", term);
}

// --- Cut & Paste ---

static void cut_line(void) {
    int ln = pos_to_line(E.cursor);
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
    set_status("Cut line");
}

static void paste_line(void) {
    if (!E.cb_len) { set_status("Clipboard empty"); return; }
    int ln = pos_to_line(E.cursor);
    int s  = line_start(ln);
    // insert clipboard + newline
    for (int i = 0; i < E.cb_len; i++) gap_insert(&E.text, s + i, E.clipboard[i]);
    gap_insert(&E.text, s + E.cb_len, '\n');
    E.cursor = s;
    E.dirty  = 1;
    rebuild_line_count();
    set_status("Pasted");
}

static void delete_line(void) {
    int ln = pos_to_line(E.cursor);
    int s = line_start(ln);
    int len = line_len(ln);
    int end = s + len;
    int del = len + (end < gap_len(&E.text) ? 1 : 0);
    for (int i = 0; i < del; i++) gap_delete(&E.text, s);
    E.cursor = s;
    if (E.cursor > gap_len(&E.text)) E.cursor = gap_len(&E.text);
    E.dirty  = 1;
    rebuild_line_count();
    set_status("Deleted line");
}

static void toggle_status(void) {
    SHOW_STATUSBAR = !SHOW_STATUSBAR;
    refresh_screen();
}

// --- Movement ---

static void move_up(void) {
    int ln = pos_to_line(E.cursor);
    if (ln == 0) { E.cursor = 0; return; }
    int vcol = cursor_vcol();
    int s    = line_start(ln - 1);
    int l    = line_len(ln - 1);
    E.cursor = s + (vcol < l ? vcol : l);
}

static void move_down(void) {
    int ln = pos_to_line(E.cursor);
    if (ln >= total_lines() - 1) return;
    int vcol = cursor_vcol();
    int s    = line_start(ln + 1);
    int l    = line_len(ln + 1);
    E.cursor = s + (vcol < l ? vcol : l);
}

static void move_left(void) {
    if (E.cursor > 0) E.cursor--;
}

static void move_right(void) {
    if (E.cursor < gap_len(&E.text)) E.cursor++;
}

static void move_line_start(void) {
    int ln = pos_to_line(E.cursor);
    E.cursor = line_start(ln);
}

static void move_line_end(void) {
    int ln = pos_to_line(E.cursor);
    E.cursor = line_start(ln) + line_len(ln);
}

static void move_page_up(void) {
    int text_rows = E.rows - (SHOW_STATUSBAR ? 2 : 0);
    for (int i = 0; i < text_rows; i++) move_up();
}

static void move_page_down(void) {
    int text_rows = E.rows - (SHOW_STATUSBAR ? 2 : 0);
    for (int i = 0; i < text_rows; i++) move_down();
}

static void goto_line(void) {
    char buf[32];
    if (!mini_input("Go to line: ", buf, sizeof buf)) return;
    int ln = atoi(buf) - 1;
    if (ln < 0) ln = 0;
    int tot = total_lines();
    if (ln >= tot) ln = tot - 1;
    int s = line_start(ln);
    E.cursor = s;
    set_status("Jumped to line %d", ln + 1);
}

// --- Quit Confirmation ---

static int confirm_quit(void) {
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

// --- Main Loop ---

static void init_ncurses(void) {
    initscr();
    raw();
    noecho();
    keypad(stdscr, TRUE);
    set_escdelay(KEY_DELAY);

    if (has_colors() && strcmp(COLOR_SCHEME, "mono") != 0) {
        start_color();
        use_default_colors();
 
        if (strcmp(COLOR_SCHEME, "dark") == 0) {
            init_pair(CP_NORMAL, COLOR_WHITE,  COLOR_BLACK);
            init_pair(CP_STATUS, COLOR_BLACK,  COLOR_WHITE);
            init_pair(CP_CMDBAR, COLOR_BLACK,  COLOR_CYAN);
            init_pair(CP_LNUM,   COLOR_YELLOW, COLOR_BLACK);
            init_pair(CP_SEARCH, COLOR_BLACK,  COLOR_YELLOW);
        } else if (strcmp(COLOR_SCHEME, "light") == 0) {
            init_pair(CP_NORMAL, COLOR_BLACK,  COLOR_WHITE);
            init_pair(CP_STATUS, COLOR_WHITE,  COLOR_BLUE);
            init_pair(CP_CMDBAR, COLOR_WHITE,  COLOR_BLUE);
            init_pair(CP_LNUM,   COLOR_BLUE,   COLOR_WHITE);
            init_pair(CP_SEARCH, COLOR_WHITE,  COLOR_RED);
        } else {
            // default: use terminal's own colours
            init_pair(CP_NORMAL, -1,           -1);
            init_pair(CP_STATUS, -1,           -1);
            init_pair(CP_CMDBAR, -1,           -1);
            init_pair(CP_LNUM,   COLOR_YELLOW, -1);
            init_pair(CP_SEARCH, COLOR_BLACK,  COLOR_YELLOW);
        }
    }

    curs_set(CURSOR_STYLE);
}

// Handles Args. Returns 1 if not opening text editor, otherwise 0.
int handle_args(int argc, char *argv[]) {
    if (argc < 2) {
        set_status("orpheus - no file. Ctrl-S to save, Ctrl-Q to quit.");
        return 0;
    }

    // orp [-h | --help]
    if (argc == 2 && (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0)) {
        printf("Orpheus version %s\n", ORPHEUS_VERSION);
        printf("Usage: orp [file]\n\n");
        printf("Options:\n");
        printf("  -h, --help        Show this help message\n");
        printf("  -v, --version     Show version information\n\n");
        printf("Keybindings:\n");
        printf("Arrow keys          navigation Move 1 char in direction of arrow\n");
        printf("PgUp/PgDn Home/End  Navigation Top/Bottom Front of Line/End of Line\n");
        printf(" Ctrl-S             save\n");
        printf(" Ctrl-Q             quit (warns on unsaved changes)\n");
        printf(" Ctrl-F             find  (Enter to cycle, Esc to cancel)\n");
        printf(" Ctrl-G             go to line\n");
        printf(" Ctrl-K             cut line\n");
        printf(" Ctrl-U             paste (yank) line\n");
        printf(" Ctrl-D             delete line\n");
        printf(" Ctrl-A             go to start of line\n");
        printf(" Ctrl-E             go to end of line\n");
        printf(" Ctrl+W             Toggle Hiding/Showing the Status Bar\n");
        printf("\n");
        return 1;
    }

    // orp [-v | --version]
    if (argc == 2 && (strcmp(argv[1], "-v") == 0 || strcmp(argv[1], "--version") == 0)) {
        printf("Orpheus version %s\n", ORPHEUS_VERSION);
        return 1;
    }

    // orp [filename]
    else if (argc == 2) {
        strncpy(E.filename, argv[1], sizeof E.filename - 1);
        if (!load_file(E.filename))
            set_status("New file: \"%s\"", E.filename);
        else
            set_status("Opened \"%s\"", E.filename);
        return 0;
    }

    printf("Incorrect Usage. Try: \n" \
        "orp\n" \
        "orp [--help | --version]\n"
        "orp [filename]\n");
    return 1;
}

int main(int argc, char *argv[]) {
    memset(&E, 0, sizeof E);
    load_config();
    gap_init(&E.text);
    // Initialise cached statistics (empty buffer = 1 line, 0 words)
    E.line_count   = 1;
    E.word_count   = 0;
    E.stats_dirty  = 0;

    // If handle args True exit early as it a flag to not enter text mode.
    if (handle_args(argc, argv)) return 0; 

    init_ncurses();
    getmaxyx(stdscr, E.rows, E.cols);

    while (1) {
        getmaxyx(stdscr, E.rows, E.cols);
        refresh_screen();
        E.status[0] = '\0'; // clear status after one frame

        int c = getch();

        switch (c) {
        // Quit
        case ('q' & 0x1f): // Ctrl-Q
            if (confirm_quit()) goto done;
            break;

        // Save
        case ('s' & 0x1f): // Ctrl-S
            if (!E.filename[0]) {
                char buf[256];
                if (mini_input("Save as: ", buf, sizeof buf)) {
                    strncpy(E.filename, buf, sizeof E.filename - 1);
                    save_file();
                }
            } else save_file();
            break;

        // Find
        case ('f' & 0x1f): // Ctrl-F
            do_find();
            break;

        // Go to Line
        case ('g' & 0x1f): // Ctrl-G
            goto_line();
            break;

        // Cut / Paste / Delete
        case ('k' & 0x1f): cut_line();   break;
        case ('u' & 0x1f): paste_line(); break;
        case ('d' & 0x1f): delete_line(); break;

        // Toggle Status
        case ('w' & 0x1f): toggle_status(); break;

        // Movement
        case KEY_UP:         move_up();        break;
        case KEY_DOWN:       move_down();      break;
        case KEY_LEFT:       move_left();      break;
        case KEY_RIGHT:      move_right();     break;
        case KEY_HOME:
        case ('a' & 0x1f):   move_line_start(); break;
        case KEY_END:
        case ('e' & 0x1f):   move_line_end();   break;
        case KEY_PPAGE:      move_page_up();     break;
        case KEY_NPAGE:      move_page_down();   break;

        // Editing
        case KEY_BACKSPACE:
        case 127:
        case '\b':
            if (E.cursor > 0) {
                char deleted = gap_char(&E.text, E.cursor - 1);
                // if gap is already at cursor, shift left by 1 byte
                if (E.text.gap_start == E.cursor)
                    gap_shift_left(&E.text);
                else
                    gap_delete(&E.text, E.cursor - 1);
                E.cursor--;
                update_stats(deleted, -1);
                E.dirty = 1;
            }
            break;

        case KEY_DC: // Delete Key
            if (E.cursor < gap_len(&E.text)) {
                gap_delete(&E.text, E.cursor);
                E.dirty = 1;
            }
            break;

        case '\t':
            for (int i = 0; i < TAB_WIDTH; i++) {
                gap_insert(&E.text, E.cursor, ' ');
                E.cursor++;
                update_stats(' ', +1);
            }
            E.dirty = 1;
            break;

        case '\n':
        case KEY_ENTER:
            gap_insert(&E.text, E.cursor, '\n');
            E.cursor++;
            update_stats('\n', +1);

            if (AUTO_INDENT) {
                int ln = pos_to_line(E.cursor - 1);
                int s = line_start(ln);
                int end = s + line_len(ln);
                // Count leading whitespace/tabs on the line that was just left
                int ws = s;
                while (ws < end && (gap_char(&E.text, ws) == ' ' || gap_char(&E.text, ws) == '\t'))
                    ws++;
                int indent = ws - s;
                for (int i = 0; i < indent; i++) {
                    char wc = gap_char(&E.text, s + i);
                    gap_insert(&E.text, E.cursor, wc);
                    E.cursor++;
                    update_stats(wc, +1);
                }
            }
            E.dirty = 1;
            break;

        default:
            if (c >= 32 && c < 256 && c != 127) {
                gap_insert(&E.text, E.cursor, (char)c);
                E.cursor++;
                update_stats((char)c, +1);
                E.dirty = 1;
            }
            break;
        }
    }

done:
    endwin();
    gap_free(&E.text);
    return 0;
}