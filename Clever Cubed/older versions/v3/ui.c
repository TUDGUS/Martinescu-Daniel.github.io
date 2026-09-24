#include "ui.h"
#include "clever_cubed.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

int ui_color_pair_for_die(DieColor c) {
    if (c == DIE_COLOR_YELLOW)    return CP_YELLOW;
    if (c == DIE_COLOR_TURQUOISE) return CP_TURQUOISE;
    if (c == DIE_COLOR_BLUE)      return CP_BLUE;
    if (c == DIE_COLOR_BROWN)     return CP_BROWN;
    if (c == DIE_COLOR_PINK)      return CP_PINK;
    if (c == DIE_COLOR_WHITE)     return CP_WHITE_DIE;
    return CP_DEFAULT;
}

const char* ui_color_name(DieColor c) {
    if (c == DIE_COLOR_YELLOW)    return "YEL";
    if (c == DIE_COLOR_TURQUOISE) return "TRQ";
    if (c == DIE_COLOR_BLUE)      return "BLU";
    if (c == DIE_COLOR_BROWN)     return "BRN";
    if (c == DIE_COLOR_PINK)      return "PNK";
    if (c == DIE_COLOR_WHITE)     return "WHT";
    return "???";
}

// ASCII die faces (3-wide)
const char* ui_die_face(int value) {
    switch (value) {
        case 1: return "[1]";
        case 2: return "[2]";
        case 3: return "[3]";
        case 4: return "[4]";
        case 5: return "[5]";
        case 6: return "[6]";
        default: return "[ ]";
    }
}

void ui_set_status(UIState* s, const char* msg, bool is_error) {
    strncpy(s->status_msg, msg, sizeof(s->status_msg) - 1);
    s->status_msg[sizeof(s->status_msg) - 1] = '\0';
    s->status_is_error = is_error;
}

void ui_draw_die(WINDOW* win, int y, int x, DieColor color, int value,
                 bool on_platter, bool highlighted, bool unavailable) {
    int pair;
    if (unavailable) {
        pair = CP_DEFAULT; // dim but visible
    } else if (on_platter) {
        pair = CP_SILVER;
    } else if (highlighted) {
        pair = CP_HIGHLIGHT;
    } else {
        pair = ui_color_pair_for_die(color);
    }

    wattron(win, COLOR_PAIR(pair));
    if (unavailable) wattron(win, A_DIM);
    if (highlighted) wattron(win, A_BOLD);

    // top border
    mvwprintw(win, y,   x, "+-----+");
    // color label row
    mvwprintw(win, y+1, x, "| %s |", ui_color_name(color));
    // always show value so player can see what was rolled/used
    mvwprintw(win, y+2, x, "|  %d  |", value > 0 ? value : 0);
    // bottom border
    mvwprintw(win, y+3, x, "+-----+");

    if (highlighted) wattroff(win, A_BOLD);
    if (unavailable) wattroff(win, A_DIM);
    wattroff(win, COLOR_PAIR(pair));

    // platter indicator below die
    if (on_platter) {
        wattron(win, COLOR_PAIR(CP_SILVER) | A_BOLD);
        mvwprintw(win, y+4, x+1, "PLTR ");
        wattroff(win, COLOR_PAIR(CP_SILVER) | A_BOLD);
    } else if (unavailable) {
        wattron(win, COLOR_PAIR(CP_INACTIVE));
        mvwprintw(win, y+4, x+1, "USED ");
        wattroff(win, COLOR_PAIR(CP_INACTIVE));
    } else {
        mvwprintw(win, y+4, x+1, "     ");
    }
}

// ===============================================================================
//  INIT / TEARDOWN
// ===============================================================================

void ui_init(UIWindows* w, UIState* s) {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    start_color();
    use_default_colors();

    // Define color pairs
    // ncurses color indices: 0=black,1=red,2=green,3=yellow,4=blue,5=magenta,6=cyan,7=white
    init_pair(CP_DEFAULT,    NC_COLOR_WHITE,   NC_COLOR_BLACK);
    init_pair(CP_YELLOW,     NC_COLOR_BLACK,   NC_COLOR_YELLOW);
    init_pair(CP_TURQUOISE,  NC_COLOR_BLACK,   NC_COLOR_CYAN);
    init_pair(CP_BLUE,       NC_COLOR_WHITE,   NC_COLOR_BLUE);
    init_pair(CP_BROWN,      NC_COLOR_WHITE,   NC_COLOR_RED);     // brown approximated as red
    init_pair(CP_PINK,       NC_COLOR_BLACK,   NC_COLOR_MAGENTA);
    init_pair(CP_WHITE_DIE,  NC_COLOR_BLACK,   NC_COLOR_WHITE);
    init_pair(CP_SILVER,     NC_COLOR_BLACK,   NC_COLOR_WHITE);
    init_pair(CP_HEADER,     NC_COLOR_BLACK,   NC_COLOR_YELLOW);
    init_pair(CP_MARKED,     NC_COLOR_WHITE,   NC_COLOR_BLACK);
    init_pair(CP_BONUS,      NC_COLOR_BLACK,   NC_COLOR_GREEN);
    init_pair(CP_INACTIVE,   NC_COLOR_BLACK,   NC_COLOR_BLACK);   // dark/invisible
    init_pair(CP_HIGHLIGHT,  NC_COLOR_BLACK,   NC_COLOR_CYAN);
    init_pair(CP_ACTION,     NC_COLOR_WHITE,   NC_COLOR_BLUE);
    init_pair(CP_STATUS_OK,  NC_COLOR_GREEN,   NC_COLOR_BLACK);
    init_pair(CP_STATUS_ERR, NC_COLOR_RED,     NC_COLOR_BLACK);
    init_pair(CP_TITLE,      NC_COLOR_YELLOW,  NC_COLOR_BLACK);
    init_pair(CP_PLAYER1,    NC_COLOR_WHITE,   NC_COLOR_BLUE);
    init_pair(CP_PLAYER2,    NC_COLOR_WHITE,   NC_COLOR_RED);
    init_pair(CP_PLAYER3,    NC_COLOR_BLACK,   NC_COLOR_GREEN);
    init_pair(CP_PLAYER4,    NC_COLOR_WHITE,   NC_COLOR_MAGENTA);

    getmaxyx(stdscr, w->term_rows, w->term_cols);

    // Layout (from top):
    //  row 0            : top header (3 rows)
    //  row 3            : dice area (6 rows)
    //  row 9            : player sheet (fills rest minus bottom bars)
    //  term_rows-5      : action bar (3 rows)
    //  term_rows-2      : status bar (2 rows)

    int sheet_h = w->term_rows - WIN_TOP_H - WIN_DICE_H - 1 - WIN_ACTIONS_H - WIN_STATUS_H;
    if (sheet_h < 10) sheet_h = 10;

    w->top     = newwin(WIN_TOP_H,       w->term_cols, 0,                                      0);
    w->dice    = newwin(WIN_DICE_H + 1,  w->term_cols, WIN_TOP_H,                              0);
    w->sheet   = newwin(sheet_h,         w->term_cols, WIN_TOP_H + WIN_DICE_H + 1,             0);
    w->actions = newwin(WIN_ACTIONS_H,   w->term_cols, w->term_rows - WIN_STATUS_H - WIN_ACTIONS_H, 0);
    w->status  = newwin(WIN_STATUS_H,    w->term_cols, w->term_rows - WIN_STATUS_H,            0);

    // Initial UIState
    memset(s, 0, sizeof(UIState));
    s->mode            = UIMODE_ACTIVE_ROLL;
    s->cursor_die      = 0;
    s->cursor_row      = 0;
    s->cursor_col      = 0;
    s->cursor_brown    = 0;
    s->status_is_error = false;
    ui_set_status(s, "Welcome to Clever Cubed! Press ROLL (r) to begin.", false);

    refresh();
}

void ui_teardown(UIWindows* w) {
    if (w->top)     { delwin(w->top);     w->top     = NULL; }
    if (w->dice)    { delwin(w->dice);    w->dice    = NULL; }
    if (w->sheet)   { delwin(w->sheet);   w->sheet   = NULL; }
    if (w->actions) { delwin(w->actions); w->actions = NULL; }
    if (w->status)  { delwin(w->status);  w->status  = NULL; }
    endwin();
}

// ===============================================================================
//  TOP HEADER
// ===============================================================================

void ui_render_top(WINDOW* win, GameState* game, UIState* s) {
    werase(win);
    wattron(win, COLOR_PAIR(CP_HEADER) | A_BOLD);

    // Fill background
    for (int c = 0; c < getmaxx(win); c++) {
        mvwaddch(win, 0, c, ' ');
        mvwaddch(win, 1, c, ' ');
        mvwaddch(win, 2, c, ' ');
    }

    // Title
    mvwprintw(win, 0, 2, " CLEVER CUBED ");

    // Round info
    mvwprintw(win, 0, 20, "Round: %d / %d", game->current_round, game->max_rounds);

    // Active player
    mvwprintw(win, 0, 40, "Active: Player %d", game->active_player_index + 1);

    // Number of players
    mvwprintw(win, 0, 62, "Players: %d", game->num_players);

    // Mode description
    const char* mode_str = "";
    switch (s->mode) {
        case UIMODE_ACTIVE_ROLL:          mode_str = "[ SELECT A DIE | r:reroll  a:any-number  n:skip ]"; break;
        case UIMODE_ACTIVE_MARK:          mode_str = "[ MARK YOUR SHEET ]"; break;
        case UIMODE_ACTIVE_TURQUOISE:     mode_str = "[ MARK TURQUOISE ROWS ]"; break;
        case UIMODE_PASSIVE_PICK:         mode_str = "[ PASSIVE: PICK FROM PLATTER ]"; break;
        case UIMODE_PASSIVE_MARK:         mode_str = "[ PASSIVE: MARK YOUR SHEET ]"; break;
        case UIMODE_BONUS_CHOOSE_VALUE:   mode_str = "[ BONUS: CHOOSE A VALUE 1-6 ]"; break;
        case UIMODE_BONUS_CHOOSE_COLOR:   mode_str = "[ BONUS: CHOOSE A COLOR AREA ]"; break;
        case UIMODE_PINK_DECIDE:          mode_str = "[ PINK: BONUS (b) OR POINTS (p)? ]"; break;
        case UIMODE_BLUE_SEVEN:           mode_str = "[ BLUE 7: LEFT (l) OR RIGHT (r)? ]"; break;
        case UIMODE_USE_ACTION:           mode_str = "[ USE ACTION ]"; break;
        case UIMODE_ANY_NUMBER_COLOR:     mode_str = "[ ANY NUMBER: CHOOSE COLOR ]"; break;
        case UIMODE_ANY_NUMBER_VALUE:     mode_str = "[ ANY NUMBER: CHOOSE VALUE ]"; break;
        case UIMODE_EXTRA_DIE_COLOR:      mode_str = "[ EXTRA DIE: CHOOSE COLOR ]"; break;
        case UIMODE_EXTRA_DIE_PROMPT:     mode_str = "[ EXTRA DIE: USE (e) OR SKIP (n) ]"; break;
        case UIMODE_EXTRA_DIE_PICK:       mode_str = "[ EXTRA DIE: SELECT ANY DIE ]"; break;
        case UIMODE_GAME_OVER:            mode_str = "[ GAME OVER -- FINAL SCORES ]"; break;
    }
    mvwprintw(win, 1, 2, "%s", mode_str);

    // Chosen dice slots
    mvwprintw(win, 1, 50, "Die slots: ");
    for (int i = 0; i < 3; i++) {
        if (i < game->chosen_dice_count) {
            Die* d = &game->chosen_dice[i];
            int pair = ui_color_pair_for_die(d->color);
            wattroff(win, COLOR_PAIR(CP_HEADER));
            wattron(win, COLOR_PAIR(pair) | A_BOLD);
            wprintw(win, "[%d]", d->value);
            wattroff(win, COLOR_PAIR(pair) | A_BOLD);
            wattron(win, COLOR_PAIR(CP_HEADER) | A_BOLD);
        } else {
            wprintw(win, "[ ]");
        }
        wprintw(win, " ");
    }

    wattroff(win, COLOR_PAIR(CP_HEADER) | A_BOLD);
    box(win, 0, 0);
    wnoutrefresh(win);
}

void ui_render_dice(WINDOW* win, GameState* game, UIState* s) {
    werase(win);
    wattron(win, COLOR_PAIR(CP_DEFAULT));
    box(win, 0, 0);

    wattron(win, COLOR_PAIR(CP_TITLE) | A_BOLD);
    mvwprintw(win, 0, 2, " DICE ");
    wattroff(win, COLOR_PAIR(CP_TITLE) | A_BOLD);

    // Draw 6 dice side by side; each die cell is 8 wide
    for (int i = 0; i < 6; i++) {
        Die* d = &game->dice[i];
        bool highlighted  = (s->cursor_die == i) &&
                            (s->mode == UIMODE_ACTIVE_ROLL || s->mode == UIMODE_PASSIVE_PICK
                             || s->mode == UIMODE_EXTRA_DIE_PICK);
        bool unavailable  = !d->is_available;
        ui_draw_die(win, 1, 2 + i * 9, d->color, d->value,
                    d->on_silver_platter, highlighted, unavailable);
    }

    // Legend on the right
    int lx = 2 + 6 * 9 + 2;
    wattron(win, COLOR_PAIR(CP_DEFAULT));
    mvwprintw(win, 1, lx, "< >:select  ENTER:pick");
    mvwprintw(win, 2, lx, "r:reroll  a:any-num");
    mvwprintw(win, 3, lx, "n   : skip turn    ^v^: fox");
    mvwprintw(win, 4, lx, "q   : quit          +1: extra die");
    wattroff(win, COLOR_PAIR(CP_DEFAULT));

    wnoutrefresh(win);
}

typedef enum {
    BT_NONE = 0,
    BT_YELLOW,
    BT_TURQUOISE,
    BT_BLUE,
    BT_BROWN,
    BT_PINK,
    BT_WHITE,
    BT_REROLL,
    BT_EXTRA_DIE,
    BT_FOX,
    BT_ANY_NUM,
} BonusType;

static void draw_bonus(WINDOW* win, int y, int x, BonusType bt, bool triggered) {
    int pair;
    const char* sym;
    switch (bt) {
        case BT_YELLOW:    pair = CP_YELLOW;    sym = "[?]";   break;
        case BT_TURQUOISE: pair = CP_TURQUOISE; sym = "[?]";   break;
        case BT_BLUE:      pair = CP_BLUE;      sym = "[?]";   break;
        case BT_BROWN:     pair = CP_BROWN;     sym = "[?]";   break;
        case BT_PINK:      pair = CP_PINK;      sym = "[?]";   break;
        case BT_WHITE:     pair = CP_WHITE_DIE; sym = "[?]";   break;
        case BT_REROLL:    pair = CP_DEFAULT;   sym = "[&]"; break; /* ? */
        case BT_EXTRA_DIE: pair = CP_DEFAULT;   sym = "[+1]";  break;
        case BT_FOX:       pair = CP_TITLE;     sym = "[^v^]"; break;
        case BT_ANY_NUM:   pair = CP_ACTION;    sym = "[AN]";  break;
        default:           return;
    }
    if (triggered) {
        wattron(win, COLOR_PAIR(CP_BONUS) | A_BOLD);
        mvwprintw(win, y, x, "%s", sym);
        wattroff(win, COLOR_PAIR(CP_BONUS) | A_BOLD);
    } else {
        wattron(win, COLOR_PAIR(pair) | A_BOLD);
        mvwprintw(win, y, x, "%s", sym);
        wattroff(win, COLOR_PAIR(pair) | A_BOLD);
    }
}

static const BonusType YB_01[] = {BT_REROLL, BT_ANY_NUM, BT_PINK, BT_EXTRA_DIE, BT_TURQUOISE, BT_FOX};
static const BonusType YB_12[] = {BT_ANY_NUM, BT_TURQUOISE, BT_BLUE, BT_BROWN, BT_YELLOW, BT_EXTRA_DIE};

void ui_render_yellow(WINDOW* win, int y, int x, PlayerSheet* sheet, UIState* s) {
    (void)s;
    int score = 0;
    int row_pts[] = {0, 2, 6, 12, 20, 30, 42};
    for (int r = 0; r < 3; r++) {
        int cnt = 0;
        for (int c = 0; c < 6; c++) if (sheet->yellow_area[r][c]) cnt++;
        score += row_pts[cnt];
    }

    wattron(win, COLOR_PAIR(CP_YELLOW) | A_BOLD);
    mvwprintw(win, y, x, " YELLOW ");
    wattroff(win, COLOR_PAIR(CP_YELLOW) | A_BOLD);
    wattron(win, COLOR_PAIR(CP_DEFAULT));
    mvwprintw(win, y, x + 9, "Score:%3d", score);

    // Column headers: 4 chars wide per cell, starting at x+4
    mvwprintw(win, y+1, x, "    [1] [2] [3] [4] [5] [6]");

    const char* row_labels[] = {"I  ", "II ", "III"};
    for (int r = 0; r < 3; r++) {
        int row_y = y + 2 + r * 2;
        wattron(win, COLOR_PAIR(CP_DEFAULT));
        mvwprintw(win, row_y, x, "%s ", row_labels[r]);
        wattroff(win, COLOR_PAIR(CP_DEFAULT));

        for (int c = 0; c < 6; c++) {
            int cx = x + 4 + c * 4;
            if (sheet->yellow_area[r][c]) {
                wattron(win, COLOR_PAIR(CP_YELLOW) | A_BOLD);
                mvwprintw(win, row_y, cx, "[X]");
                wattroff(win, COLOR_PAIR(CP_YELLOW) | A_BOLD);
            } else {
                // Row I   (r=0): cols 4,5 (values 5,6) are passive gray
                // Row II  (r=1): cols 2,3 (values 3,4) are passive gray
                // Row III (r=2): cols 0,1 (values 1,2) are passive gray
                bool is_passive = (r == 0 && c >= 4) ||
                                  (r == 1 && c >= 2 && c <= 3) ||
                                  (r == 2 && c <= 1);
                if (is_passive) {
                    wattron(win, COLOR_PAIR(CP_DEFAULT) | A_DIM);
                    mvwprintw(win, row_y, cx, "[.]");
                    wattroff(win, COLOR_PAIR(CP_DEFAULT) | A_DIM);
                } else {
                    wattron(win, COLOR_PAIR(CP_DEFAULT));
                    mvwprintw(win, row_y, cx, "[ ]");
                    wattroff(win, COLOR_PAIR(CP_DEFAULT));
                }
            }
        }

        // Bonus row immediately below this row (not after row III)
        if (r < 2) {
            int bon_y = row_y + 1;
            wattron(win, COLOR_PAIR(CP_DEFAULT));
            mvwprintw(win, bon_y, x, "    ");
            wattroff(win, COLOR_PAIR(CP_DEFAULT));
            const BonusType* labels = (r == 0) ? YB_01 : YB_12;
            for (int c = 0; c < 6; c++) {
                bool triggered = sheet->yellow_area[r][c] && sheet->yellow_area[r+1][c];
                draw_bonus(win, bon_y, x + 4 + c * 4, labels[c], triggered);
            }
        }
    }

    // Score table
    wattron(win, COLOR_PAIR(CP_DEFAULT));
    mvwprintw(win, y + 8, x, "pts: 0   2   6  12  20  30  42");
    wattroff(win, COLOR_PAIR(CP_DEFAULT));
}

static const BonusType TQ_COL_BONUS[] = {BT_BROWN, BT_PINK, BT_YELLOW, BT_ANY_NUM, BT_BLUE, BT_REROLL};
static const BonusType TQ_ROW_BONUS[] = {BT_FOX, BT_EXTRA_DIE, BT_BROWN, BT_TURQUOISE, BT_WHITE};
static const int   TQ_ROW_PTS[]   = {0, 1, 3, 6, 10, 15, 21};

void ui_render_turquoise(WINDOW* win, int y, int x, PlayerSheet* sheet, UIState* s) {
    int score = 0;
    for (int r = 0; r < 5; r++) {
        int cnt = 0;
        for (int c = 0; c < 6; c++) if (sheet->turquoise_area[r][c]) cnt++;
        score += TQ_ROW_PTS[cnt];
    }

    wattron(win, COLOR_PAIR(CP_TURQUOISE) | A_BOLD);
    mvwprintw(win, y, x, " TURQUOISE ");
    wattroff(win, COLOR_PAIR(CP_TURQUOISE) | A_BOLD);
    wattron(win, COLOR_PAIR(CP_DEFAULT));
    mvwprintw(win, y, x + 12, "Score:%3d", score);

    // Col headers (values 1-6)
    mvwprintw(win, y+1, x, "     1  2  3  4  5  6  BON");

    for (int r = 0; r < 5; r++) {
        mvwprintw(win, y + 2 + r, x, "R%d  ", r + 1);
        for (int c = 0; c < 6; c++) {
            bool marked   = sheet->turquoise_area[r][c];
            bool can_mark = (s->mode == UIMODE_ACTIVE_TURQUOISE) &&
                            !marked &&
                            !sheet->turquoise_marked_this_turn[r] &&
                            (sheet->turquoise_marks_remaining > 0) &&
                            (s->cursor_row == r);

            if (marked) {
                wattron(win, COLOR_PAIR(CP_TURQUOISE) | A_BOLD);
                mvwprintw(win, y + 2 + r, x + 4 + c * 3, "[X]");
                wattroff(win, COLOR_PAIR(CP_TURQUOISE) | A_BOLD);
            } else if (can_mark) {
                wattron(win, COLOR_PAIR(CP_HIGHLIGHT) | A_BOLD);
                mvwprintw(win, y + 2 + r, x + 4 + c * 3, "[*]");
                wattroff(win, COLOR_PAIR(CP_HIGHLIGHT) | A_BOLD);
            } else {
                wattron(win, COLOR_PAIR(CP_DEFAULT));
                mvwprintw(win, y + 2 + r, x + 4 + c * 3, "[ ]");
                wattroff(win, COLOR_PAIR(CP_DEFAULT));
            }
        }
        // Row bonus
        bool row_done = sheet->turquoise_row_bonus[r];
        draw_bonus(win, y + 2 + r, x + 4 + 6 * 3, TQ_ROW_BONUS[r], row_done);
    }

    // Column bonus row
    mvwprintw(win, y + 7, x, "BON ");
    for (int c = 0; c < 6; c++) {
        bool col_done = sheet->turquoise_column_bonus[c];
        draw_bonus(win, y + 7, x + 4 + c * 3, TQ_COL_BONUS[c], col_done);
    }

    // Score table
    mvwprintw(win, y + 8, x, " 0  1  3  6 10 15 21");
    wattroff(win, COLOR_PAIR(CP_DEFAULT));
}

static const int BLUE_SCORES_POS[] = {22, 17, 13, 9, 6, 3, 0, 3, 6, 9, 13, 17, 22};

static const BonusType BLUE_BONUSES[13] = {
    BT_EXTRA_DIE, BT_PINK, BT_NONE, BT_YELLOW, BT_ANY_NUM,
    BT_NONE, BT_NONE, BT_NONE,
    BT_REROLL, BT_BROWN, BT_NONE, BT_TURQUOISE, BT_FOX
};

void ui_render_blue(WINDOW* win, int y, int x, PlayerSheet* sheet, UIState* s) {
    (void)s;
    int score = 0;
    int leftmost = -1, rightmost = -1;
    for (int i = 0; i <= 12; i++) {
        if (sheet->blue_area[i] != 0) {
            if (leftmost == -1) leftmost = i;
            rightmost = i;
        }
    }
    if (leftmost != -1 && leftmost != 6) {
        int d = abs(leftmost - 6);
        int pts[] = {0, 3, 6, 9, 13, 17, 22};
        score += pts[d];
    }
    if (rightmost != -1 && rightmost != 6) {
        int d = abs(rightmost - 6);
        int pts[] = {0, 3, 6, 9, 13, 17, 22};
        score += pts[d];
    }
    for (int i = 0; i <= 12; i++) {
        int v = sheet->blue_area[i];
        if (v != 0 && (v <= 4 || v >= 10)) score += 4;
    }

    wattron(win, COLOR_PAIR(CP_BLUE) | A_BOLD);
    mvwprintw(win, y, x, " BLUE ");
    wattroff(win, COLOR_PAIR(CP_BLUE) | A_BOLD);
    wattron(win, COLOR_PAIR(CP_DEFAULT));
    mvwprintw(win, y, x + 7, "Score:%3d", score);

    // Score row (above)
    mvwprintw(win, y + 1, x, "Pts:");
    for (int i = 0; i < 13; i++) {
        mvwprintw(win, y + 1, x + 4 + i * 4, "%3d ", BLUE_SCORES_POS[i]);
    }

    // Bonus row
    mvwprintw(win, y + 2, x, "Bon:");
    for (int i = 0; i < 13; i++) {
        bool has_bonus = (BLUE_BONUSES[i] != BT_NONE);
        bool triggered  = sheet->blue_bonus[i];
        if (has_bonus) {
            draw_bonus(win, y + 2, x + 4 + i * 4, BLUE_BONUSES[i], triggered);
        } else {
            wattron(win, COLOR_PAIR(CP_DEFAULT));
            mvwprintw(win, y + 2, x + 4 + i * 4, "    ");
            wattroff(win, COLOR_PAIR(CP_DEFAULT));
        }
    }

    // Value cells
    mvwprintw(win, y + 3, x, "Val:");
    for (int i = 0; i < 13; i++) {
        int v = sheet->blue_area[i];
        bool is_center = (i == 6);
        bool is_left   = (i == leftmost && i != 6);
        bool is_right  = (i == rightmost && i != 6);

        if (is_center) {
            wattron(win, COLOR_PAIR(CP_BLUE) | A_BOLD);
        } else if (is_left || is_right) {
            wattron(win, COLOR_PAIR(CP_HIGHLIGHT) | A_BOLD);
        } else {
            wattron(win, COLOR_PAIR(CP_DEFAULT));
        }

        if (v != 0) {
            mvwprintw(win, y + 3, x + 4 + i * 4, "[%2d]", v);
        } else {
            mvwprintw(win, y + 3, x + 4 + i * 4, "[  ]");
        }
        wattroff(win, COLOR_PAIR(CP_BLUE) | A_BOLD);
        wattroff(win, COLOR_PAIR(CP_HIGHLIGHT) | A_BOLD);
        wattroff(win, COLOR_PAIR(CP_DEFAULT));
    }

    // Direction labels
    mvwprintw(win, y + 4, x + 4,        "<--- LEFT (decreasing)");
    mvwprintw(win, y + 4, x + 4 + 7*4,  "RIGHT (increasing) --->");
    wattroff(win, COLOR_PAIR(CP_DEFAULT));
}

static const BonusType BROWN_BONUSES_LBL[] = {
    BT_ANY_NUM, BT_PINK, BT_NONE, BT_REROLL, BT_TURQUOISE,
    BT_NONE, BT_EXTRA_DIE, BT_BLUE, BT_NONE, BT_YELLOW, BT_FOX
};

void ui_render_brown(WINDOW* win, int y, int x, PlayerSheet* sheet, UIState* s) {
    int marked = 0;
    for (int i = 0; i < 12; i++) if (sheet->brown_area[i]) marked++;
    int scores[] = {0, 2, 5, 9, 14, 20, 27, 35, 44, 54, 65, 77, 90};
    int score = scores[marked];

    wattron(win, COLOR_PAIR(CP_BROWN) | A_BOLD);
    mvwprintw(win, y, x, " BROWN ");
    wattroff(win, COLOR_PAIR(CP_BROWN) | A_BOLD);
    wattron(win, COLOR_PAIR(CP_DEFAULT));
    mvwprintw(win, y, x + 8, "Score:%3d", score);

    // Required values row
    mvwprintw(win, y + 1, x, "Req: ");
    for (int i = 0; i < 12; i++) {
        mvwprintw(win, y + 1, x + 5 + i * 4, "[%d] ", sheet->brown_required_value[i]);
    }

    // Marked cells row
    mvwprintw(win, y + 2, x, "Mar: ");
    for (int i = 0; i < 12; i++) {
        bool is_cursor = (s->mode == UIMODE_ACTIVE_MARK || s->mode == UIMODE_PASSIVE_MARK) &&
                         (s->cursor_brown == i);
        if (sheet->brown_area[i]) {
            wattron(win, COLOR_PAIR(CP_BROWN) | A_BOLD);
            mvwprintw(win, y + 2, x + 5 + i * 4, "[X] ");
            wattroff(win, COLOR_PAIR(CP_BROWN) | A_BOLD);
        } else if (is_cursor) {
            wattron(win, COLOR_PAIR(CP_HIGHLIGHT) | A_BOLD);
            mvwprintw(win, y + 2, x + 5 + i * 4, "[?] ");
            wattroff(win, COLOR_PAIR(CP_HIGHLIGHT) | A_BOLD);
        } else {
            wattron(win, COLOR_PAIR(CP_DEFAULT));
            mvwprintw(win, y + 2, x + 5 + i * 4, "[ ] ");
            wattroff(win, COLOR_PAIR(CP_DEFAULT));
        }
    }

    // Bonus row (between adjacent pairs)
    mvwprintw(win, y + 3, x, "Bon: ");
    for (int i = 0; i < 11; i++) {
        bool triggered = sheet->brown_bonus[i];
        if (BROWN_BONUSES_LBL[i] != BT_NONE) {
            draw_bonus(win, y + 3, x + 7 + i * 4, BROWN_BONUSES_LBL[i], triggered);
        } else {
            wattron(win, COLOR_PAIR(CP_DEFAULT));
            mvwprintw(win, y + 3, x + 7 + i * 4, "    ");
            wattroff(win, COLOR_PAIR(CP_DEFAULT));
        }
    }

    // Score table
    mvwprintw(win, y + 4, x, "Pts:  2  5  9 14 20 27 35 44 54 65 77 90");
    wattroff(win, COLOR_PAIR(CP_DEFAULT));
}

static const BonusType PINK_BONUSES_LBL[] = {
    BT_NONE, BT_REROLL, BT_BLUE, BT_EXTRA_DIE, BT_ANY_NUM,
    BT_YELLOW, BT_BROWN, BT_REROLL, BT_FOX, BT_BLUE, BT_TURQUOISE, BT_WHITE
};

void ui_render_pink(WINDOW* win, int y, int x, PlayerSheet* sheet, UIState* s) {
    (void)s;
    int score = 0;
    for (int i = 0; i < sheet->pink_next_empty_index; i++) score += sheet->pink_area[i];

    wattron(win, COLOR_PAIR(CP_PINK) | A_BOLD);
    mvwprintw(win, y, x, " PINK ");
    wattroff(win, COLOR_PAIR(CP_PINK) | A_BOLD);
    wattron(win, COLOR_PAIR(CP_DEFAULT));
    mvwprintw(win, y, x + 7, "Score:%4d", score);

    // Multiplier row
    mvwprintw(win, y + 1, x, "Mul: ");
    for (int i = 0; i < 12; i++) {
        int m = sheet->pink_multipliers[i];
        if (m == 0) {
            mvwprintw(win, y + 1, x + 5 + i * 4, " ?  ");
        } else {
            mvwprintw(win, y + 1, x + 5 + i * 4, "x%d  ", m);
        }
    }

    // Value cells
    mvwprintw(win, y + 2, x, "Val: ");
    for (int i = 0; i < 12; i++) {
        bool is_next = (i == sheet->pink_next_empty_index);
        if (sheet->pink_area[i] != 0) {
            wattron(win, COLOR_PAIR(CP_PINK) | A_BOLD);
            mvwprintw(win, y + 2, x + 5 + i * 4, "[%2d]", sheet->pink_area[i]);
            wattroff(win, COLOR_PAIR(CP_PINK) | A_BOLD);
        } else if (is_next) {
            wattron(win, COLOR_PAIR(CP_HIGHLIGHT));
            mvwprintw(win, y + 2, x + 5 + i * 4, "[ ?]");
            wattroff(win, COLOR_PAIR(CP_HIGHLIGHT));
        } else {
            wattron(win, COLOR_PAIR(CP_DEFAULT));
            mvwprintw(win, y + 2, x + 5 + i * 4, "[  ]");
            wattroff(win, COLOR_PAIR(CP_DEFAULT));
        }
    }

    // Bonus row
    mvwprintw(win, y + 3, x, "Bon: ");
    for (int i = 0; i < 12; i++) {
        bool has_b = (PINK_BONUSES_LBL[i] != BT_NONE);
        bool used   = !sheet->pink_bonus_available[i];
        if (has_b) {
            draw_bonus(win, y + 3, x + 5 + i * 4, PINK_BONUSES_LBL[i], used);
        } else {
            wattron(win, COLOR_PAIR(CP_DEFAULT));
            mvwprintw(win, y + 3, x + 5 + i * 4, "    ");
            wattroff(win, COLOR_PAIR(CP_DEFAULT));
        }
    }
    wattroff(win, COLOR_PAIR(CP_DEFAULT));
}


void ui_render_sheet(WINDOW* win, PlayerSheet* sheet, UIState* s) {
    werase(win);
    wattron(win, COLOR_PAIR(CP_DEFAULT));
    box(win, 0, 0);

    wattron(win, COLOR_PAIR(CP_TITLE) | A_BOLD);
    mvwprintw(win, 0, 2, " PLAYER %d SHEET ", sheet->id + 1);
    wattroff(win, COLOR_PAIR(CP_TITLE) | A_BOLD);

    int rows, cols;
    getmaxyx(win, rows, cols);
    (void)rows;

    int top_y    = 1;
    int bottom_y = top_y + 10;  // each top area is ~9 rows tall

    // -- TOP ROW --------------------------------------------------------------
    if (cols > 22)  ui_render_yellow(win,    top_y, 1,  sheet, s);
    if (cols > 55)  ui_render_turquoise(win, top_y, 31, sheet, s);
    if (cols > 93)  ui_render_blue(win,      top_y, 60, sheet, s);

    // Separators
    if (cols > 30) {
        for (int r = top_y; r < top_y + 9; r++) mvwaddch(win, r, 30, ACS_VLINE);
    }
    if (cols > 59) {
        for (int r = top_y; r < top_y + 9; r++) mvwaddch(win, r, 59, ACS_VLINE);
    }

    // -- DIVIDER --------------------------------------------------------------
    if (bottom_y < rows - 1) {
        for (int c = 1; c < cols - 1; c++) mvwaddch(win, top_y + 9, c, ACS_HLINE);
    }

    // -- BOTTOM ROW -----------------------------------------------------------
    if (cols > 50)  ui_render_brown(win, bottom_y, 1,  sheet, s);
    if (cols > 100) ui_render_pink(win,  bottom_y, 55, sheet, s);

    // Separators bottom
    if (cols > 54) {
        for (int r = bottom_y; r < bottom_y + 6; r++) mvwaddch(win, r, 54, ACS_VLINE);
    }

    // -- FOXES & ACTIONS SUMMARY ----------------------------------------------
    int fox_y = bottom_y + 6;
    if (fox_y < rows - 1) {
        wattron(win, COLOR_PAIR(CP_TITLE) | A_BOLD);
        mvwprintw(win, fox_y, 1, "Foxes: %d", sheet->actions.foxes_unlocked);
        wattroff(win, COLOR_PAIR(CP_TITLE) | A_BOLD);
        wattron(win, COLOR_PAIR(CP_DEFAULT));
        mvwprintw(win, fox_y, 15, "Re-roll:%d  AnyNum:%d  ExtraDie:%d",
                  sheet->actions.reroll_charges,
                  sheet->actions.any_number_charges,
                  sheet->actions.extra_die_charges);
        wattroff(win, COLOR_PAIR(CP_DEFAULT));
    }

    wnoutrefresh(win);
}

void ui_render_actions_bar(WINDOW* win, PlayerSheet* sheet, UIState* s, int player_id) {
    static const int player_pairs[] = {CP_PLAYER1, CP_PLAYER2, CP_PLAYER3, CP_PLAYER4};
    int bar_pair = player_pairs[player_id % 4];
    werase(win);
    wattron(win, COLOR_PAIR(bar_pair));
    for (int r = 0; r < WIN_ACTIONS_H; r++)
        for (int c = 0; c < getmaxx(win); c++)
            mvwaddch(win, r, c, ' ');

    wattron(win, A_BOLD);
    mvwprintw(win, 0, 1, "PLAYER %d", player_id + 1);
    wattroff(win, A_BOLD);

    // Context-sensitive keybindings
    switch (s->mode) {
        case UIMODE_ACTIVE_ROLL:
        case UIMODE_PASSIVE_PICK:
            mvwprintw(win, 0, 12, "< >:move  ENTER:pick  a:actions  q:quit");
            break;
        case UIMODE_ACTIVE_MARK:
            mvwprintw(win, 0, 12, "? ?:navigate  ENTER:mark  ESC:cancel");
            break;
        case UIMODE_ACTIVE_TURQUOISE:
            mvwprintw(win, 0, 12, "? ?:row  ENTER:mark row (%d left)  ESC:done",
                sheet->turquoise_marks_remaining);
            break;
        case UIMODE_BONUS_CHOOSE_VALUE:
            mvwprintw(win, 0, 12, "1-6: choose value for bonus");
            break;
        case UIMODE_BONUS_CHOOSE_COLOR:
            mvwprintw(win, 0, 12, "y:Yellow  t:Turquoise  b:Blue  r:Brown  p:Pink");
            break;
        case UIMODE_PINK_DECIDE:
            mvwprintw(win, 0, 12, "b:bonus(half)  p:points(multiply)");
            break;
        case UIMODE_BLUE_SEVEN:
            mvwprintw(win, 0, 12, "l:place 7 LEFT   r:place 7 RIGHT");
            break;
        case UIMODE_USE_ACTION:
            mvwprintw(win, 0, 12, "(unused)");
            break;
        case UIMODE_ANY_NUMBER_VALUE:
            mvwprintw(win, 0, 12, "1-6: pick value for any-number bar  ESC:cancel");
            break;
        case UIMODE_EXTRA_DIE_COLOR:
            mvwprintw(win, 0, 12, "y:Yellow  t:Turquoise  b:Blue  r:Brown  p:Pink  w:White  ESC:cancel");
            break;
        case UIMODE_EXTRA_DIE_PROMPT:
            mvwprintw(win, 0, 12, "e:use extra die   n:skip to passive");
            break;
        case UIMODE_EXTRA_DIE_PICK:
            mvwprintw(win, 0, 12, "< >:select any die (incl. used)   ENTER:use it   ESC:back");
            break;
        case UIMODE_GAME_OVER:
            mvwprintw(win, 0, 12, "q:quit");
            break;
        default:
            mvwprintw(win, 0, 12, "q:quit");
            break;
    }

    // -- Row 1: Charge counts + usage trackers --------------------------------
    int x = 1;
    // Reroll
    wattroff(win, COLOR_PAIR(bar_pair));
    draw_bonus(win, 1, x, BT_REROLL, false);
    wattron(win, COLOR_PAIR(bar_pair));
    x += 4;
    mvwprintw(win, 1, x, ":%d  ", sheet->actions.reroll_charges);
    x += 4;
    // Track rerolls used (max 7, 7->fox)
    mvwprintw(win, 1, x, "used:");
    x += 5;
    for (int i = 0; i < 7; i++) {
        if (i < sheet->actions.rerolls_used) {
            wattron(win, A_BOLD);
            mvwprintw(win, 1, x + i * 2, "X ");
            wattroff(win, A_BOLD);
        } else {
            mvwprintw(win, 1, x + i * 2, "o ");
        }
    }
    x += 7 * 2 + 2;
    // Fox earned indicator
    if (sheet->actions.rerolls_used >= 7) {
        wattroff(win, COLOR_PAIR(bar_pair));
        draw_bonus(win, 1, x, BT_FOX, true);
        wattron(win, COLOR_PAIR(bar_pair));
    }
    x += 6;

    // Extra die
    wattroff(win, COLOR_PAIR(bar_pair));
    draw_bonus(win, 1, x, BT_EXTRA_DIE, false);
    wattron(win, COLOR_PAIR(bar_pair));
    x += 4;
    mvwprintw(win, 1, x, ":%d  ", sheet->actions.extra_die_charges);
    x += 4;
    mvwprintw(win, 1, x, "used:");
    x += 5;
    for (int i = 0; i < 7; i++) {
        if (i < sheet->actions.extra_die_used) {
            wattron(win, A_BOLD);
            mvwprintw(win, 1, x + i * 2, "X ");
            wattroff(win, A_BOLD);
        } else {
            mvwprintw(win, 1, x + i * 2, "o ");
        }
    }
    x += 7 * 2 + 2;
    if (sheet->actions.extra_die_used >= 7) {
        wattroff(win, COLOR_PAIR(bar_pair));
        draw_bonus(win, 1, x, BT_BROWN, true);
        wattron(win, COLOR_PAIR(bar_pair));
    }
    x += 6;

    // Foxes
    wattroff(win, COLOR_PAIR(bar_pair));
    draw_bonus(win, 1, x, BT_FOX, false);
    wattron(win, COLOR_PAIR(bar_pair));
    x += 6;
    mvwprintw(win, 1, x, ":%d", sheet->actions.foxes_unlocked);

    // -- Row 2: Any-number bar -------------------------------------------------
    x = 1;
    // Label
    wattroff(win, COLOR_PAIR(bar_pair));
    draw_bonus(win, 2, x, BT_ANY_NUM, false);
    wattron(win, COLOR_PAIR(bar_pair));
    x += 5;
    mvwprintw(win, 2, x, ":%d bar[", sheet->actions.any_number_charges);
    x += 7;

    // Draw the 7 bar slots: 3,4,5,6,?,?,?
    static const int bar_vals[ANY_NUMBER_BAR_SIZE] = {3, 4, 5, 6, 0, 0, 0};
    for (int i = 0; i < ANY_NUMBER_BAR_SIZE; i++) {
        bool used = sheet->actions.any_number_bar_used[i];
        if (used) {
            wattron(win, A_BOLD);
            mvwprintw(win, 2, x, "[X]");
            wattroff(win, A_BOLD);
        } else if (bar_vals[i] == 0) {
            wattroff(win, COLOR_PAIR(bar_pair));
            draw_bonus(win, 2, x, BT_WHITE, false);
            wattron(win, COLOR_PAIR(bar_pair));
        } else {
            mvwprintw(win, 2, x, "[%d]", bar_vals[i]);
        }
        x += 4;
    }
    mvwprintw(win, 2, x, "]");
    x += 2;
    mvwprintw(win, 2, x, "used:%d", sheet->actions.any_number_used);
    x += 8;
    if (sheet->actions.any_number_used >= 7) {
        wattroff(win, COLOR_PAIR(bar_pair));
        draw_bonus(win, 2, x, BT_PINK, true);
        wattron(win, COLOR_PAIR(bar_pair));
    }

    wattroff(win, COLOR_PAIR(bar_pair));
    wnoutrefresh(win);
}


void ui_render_status(WINDOW* win, UIState* s) {
    werase(win);
    int pair = s->status_is_error ? CP_STATUS_ERR : CP_STATUS_OK;
    wattron(win, COLOR_PAIR(pair) | A_BOLD);
    mvwprintw(win, 0, 1, "%s", s->status_msg);
    wattroff(win, COLOR_PAIR(pair) | A_BOLD);
    wnoutrefresh(win);
}


static void ui_render_game_over(UIWindows* w, GameState* game) {
    calculate_final_scores(game);

    WINDOW* ov = w->sheet;
    werase(ov);
    wattron(ov, COLOR_PAIR(CP_TITLE) | A_BOLD);
    mvwprintw(ov, 1, 2, "=== GAME OVER -- FINAL SCORES ===");
    wattroff(ov, COLOR_PAIR(CP_TITLE) | A_BOLD);

    int best_score = -1, winner = 0;
    for (int i = 0; i < game->num_players; i++) {
        if (game->players[i].score > best_score) {
            best_score = game->players[i].score;
            winner = i;
        }
    }

    static const char* RATINGS[] = {
        "Lets talk about something else",
        "It was just bad luck",
        "You can still improve",
        "Not too shabby",
        "It's going up",
        "You are a prodigy",
        "That's pretty clever!",
        "Reaching for the stars",
        "Hello Mr. Hawking!",
        "Beautiful AND clever!",
        "Clever Cubed!"
    };

    for (int i = 0; i < game->num_players; i++) {
        PlayerSheet* sh = &game->players[i];
        int y   = calculate_yellow_score(sh);
        int t   = calculate_turquoise_score(sh);
        int bl  = calculate_blue_score(sh);
        int br  = calculate_brown_score(sh);
        int pk  = calculate_pink_score(sh);
        int fox = sh->actions.foxes_unlocked;
        int min_area = y;
        if (t  < min_area) min_area = t;
        if (bl < min_area) min_area = bl;
        if (br < min_area) min_area = br;
        if (pk < min_area) min_area = pk;
        int fox_pts = fox * min_area;
        int total   = y + t + bl + br + pk + fox_pts;
        sh->score   = total;

        int row = 3 + i * 5;
        bool is_winner = (i == winner);

        if (is_winner) wattron(ov, COLOR_PAIR(CP_BONUS) | A_BOLD);
        else           wattron(ov, COLOR_PAIR(CP_DEFAULT) | A_BOLD);

        mvwprintw(ov, row, 2, "Player %d %s", i + 1, is_winner ? " *** WINNER ***" : "");
        wattroff(ov, COLOR_PAIR(CP_BONUS) | A_BOLD);
        wattroff(ov, COLOR_PAIR(CP_DEFAULT) | A_BOLD);

        wattron(ov, COLOR_PAIR(CP_DEFAULT));
        mvwprintw(ov, row+1, 4,
            "Yellow:%3d  Turquoise:%3d  Blue:%3d  Brown:%3d  Pink:%4d  Foxes:%dx%d=%3d",
            y, t, bl, br, pk, fox, min_area, fox_pts);
        mvwprintw(ov, row+2, 4, "TOTAL: %d", total);

        // Rating
        int ridx = 0;
        if (total < 179)      ridx = 0;
        else if (total < 210) ridx = 1;
        else if (total < 240) ridx = 2;
        else if (total < 270) ridx = 3;
        else if (total < 300) ridx = 4;
        else if (total < 330) ridx = 5;
        else if (total < 360) ridx = 6;
        else if (total < 390) ridx = 7;
        else if (total < 420) ridx = 8;
        else if (total < 450) ridx = 9;
        else                   ridx = 10;

        wattron(ov, COLOR_PAIR(CP_TITLE) | A_BOLD);
        mvwprintw(ov, row+3, 4, "Rating: %s", RATINGS[ridx]);
        wattroff(ov, COLOR_PAIR(CP_TITLE) | A_BOLD);
        wattroff(ov, COLOR_PAIR(CP_DEFAULT));
    }

    box(ov, 0, 0);
    wnoutrefresh(ov);
}

// ===============================================================================
//  MASTER RENDER
// ===============================================================================

void ui_render_all(UIWindows* w, UIState* s, GameState* game, int viewer_player) {
    if (s->mode == UIMODE_GAME_OVER) {
        ui_render_top(w->top,    game, s);
        ui_render_game_over(w, game);
        ui_render_actions_bar(w->actions, &game->players[viewer_player], s, viewer_player);
        ui_render_status(w->status, s);
    } else {
        ui_render_top(w->top,     game, s);
        ui_render_dice(w->dice,   game, s);
        ui_render_sheet(w->sheet, &game->players[viewer_player], s);
        ui_render_actions_bar(w->actions, &game->players[viewer_player], s, viewer_player);
        ui_render_status(w->status, s);
    }
    doupdate();
}

// ===============================================================================
//  INPUT HANDLING
// ===============================================================================

// Helper: find next available die index in direction dir (+1 or -1)
static int next_available_die(GameState* game, int cur, int dir, bool platter_only) {
    for (int step = 1; step <= 6; step++) {
        int idx = (cur + dir * step + 6) % 6;
        Die* d = &game->dice[idx];
        if (platter_only) {
            if (d->on_silver_platter) return idx;
        } else {
            if (d->is_available && !d->on_silver_platter) return idx;
        }
    }
    return cur; // no other available die
}

// Count dice still available to pick this roll (not picked, not on platter)
static int count_pickable_dice(GameState* game) {
    int n = 0;
    for (int i = 0; i < 6; i++)
        if (game->dice[i].is_available && !game->dice[i].on_silver_platter) n++;
    return n;
}

// Restore a die to available (undo a pick that couldn't be applied)
static void unpick_die(GameState* game, int die_index) {
    game->dice[die_index].is_available = true;
    if (game->chosen_dice_count > 0) game->chosen_dice_count--;
}


static void try_end_active_turn(UIState* s, GameState* game, PlayerSheet* sheet) {
    // Move ALL remaining available dice to the silver platter
    // (official rule: at end of active turn, everything goes to platter)
    for (int i = 0; i < 6; i++) {
        if (game->dice[i].is_available && !game->dice[i].on_silver_platter) {
            game->dice[i].on_silver_platter = true;
        }
    }
    if (sheet->actions.extra_die_charges > 0) {
        s->mode = UIMODE_EXTRA_DIE_PROMPT;
        char buf[128];
        snprintf(buf, sizeof(buf),
            "Active turn done. %d extra die charge%s. Use extra die (e) or skip (n).",
            sheet->actions.extra_die_charges,
            sheet->actions.extra_die_charges == 1 ? "" : "s");
        ui_set_status(s, buf, false);
    } else {
        s->mode = UIMODE_PASSIVE_PICK;
    }
}


static void apply_bonus_as_die(UIState* s, GameState* game, PlayerSheet* sheet,
                                int bonus_type, int value, bool is_white_die) {
    char buf[128];

    if (bonus_type == BONUS_YELLOW) {
        // chosen_dice_count already includes the die just picked.
        // Subtract 1 to get the roll number for THIS pick.
        int roll = game->chosen_dice_count - 1;
        if (roll < 0) roll = 0;
        if (roll > 2) roll = 2;
        bool ok = mark_yellow(sheet, value, roll, true);
        snprintf(buf, sizeof(buf), ok ? "Bonus: Yellow %d on row %d." :
            "Bonus: Can't mark Yellow %d on row %d!", value, roll + 1);
        ui_set_status(s, buf, !ok);
        s->mode = (game->chosen_dice_count >= 3 || count_pickable_dice(game) == 0)
                  ? UIMODE_PASSIVE_PICK : UIMODE_ACTIVE_ROLL;

    } else if (bonus_type == BONUS_TURQUOISE) {
        // Use same logic as real turquoise die: is_active=true counts chosen_dice
        // and available dice. The white die is already in chosen_dice but with its
        // original value, NOT the chosen turquoise value, so it won't be double-counted.
        s->cursor_col = value; // store so ACTIVE_TURQUOISE can use it when chosen_dice_count==0
        setup_turquoise_marks(sheet, game, value, true);
        // +1 for ? bonus only: the bonus die isn't on the board so isn't counted.
        // White die is already "used" as the acting die - no extra +1 needed.
        if (!is_white_die && sheet->turquoise_marks_remaining < 3)
            sheet->turquoise_marks_remaining++;
        s->mode = UIMODE_ACTIVE_TURQUOISE;
        snprintf(buf, sizeof(buf), "Bonus: Turquoise %d, %d row(s) to mark.",
                 value, sheet->turquoise_marks_remaining);
        ui_set_status(s, buf, false);

    } else if (bonus_type == BONUS_BLUE) {
        // Blue bonus: the player chose a value (e.g. 4), add white die.
        // But if the white die WAS the die played (pending_bonus_value path),
        // the white die is already in chosen_dice and we must NOT add it again.
        // We find the white die only among AVAILABLE (not-yet-picked) dice.
        int white_val = 0;
        for (int i = 0; i < 6; i++) {
            if (game->dice[i].color == DIE_COLOR_WHITE &&
                game->dice[i].is_available &&
                !game->dice[i].on_silver_platter) {
                white_val = game->dice[i].value;
                break;
            }
        }
        int total = value + white_val;
        if (total == 7) {
            s->mode = UIMODE_BLUE_SEVEN;
            ui_set_status(s, "Bonus: Blue+White=7 -- LEFT (l) or RIGHT (r)?", false);
        } else {
            bool ok = mark_blue(sheet, total, true);
            snprintf(buf, sizeof(buf), ok ? "Bonus: Blue %d+%d=%d." :
                "Bonus: Blue %d+%d=%d no valid neighbor!", value, white_val, total);
            ui_set_status(s, buf, !ok);
            s->mode = (game->chosen_dice_count >= 3 || count_pickable_dice(game) == 0)
                      ? UIMODE_PASSIVE_PICK : UIMODE_ACTIVE_ROLL;
        }

    } else if (bonus_type == BONUS_BROWN) {
        s->mode = UIMODE_ACTIVE_MARK;
        s->cursor_col = value;
        int last_b = sheet->brown_last_marked_index;
        s->cursor_brown = (last_b >= 0 && last_b < 11) ? last_b + 1 : 0;
        snprintf(buf, sizeof(buf), "Bonus: Brown %d -- < > navigate, ENTER.", value);
        ui_set_status(s, buf, false);

    } else if (bonus_type == BONUS_PINK) {
        if (sheet->pink_next_empty_index == 0) {
            mark_pink(sheet, value, true);
            ui_set_status(s, "Bonus: Pink first slot (half-value).", false);
            s->mode = (game->chosen_dice_count >= 3 || count_pickable_dice(game) == 0)
                      ? UIMODE_PASSIVE_PICK : UIMODE_ACTIVE_ROLL;
        } else {
            s->cursor_row = value;
            s->mode = UIMODE_PINK_DECIDE;
            snprintf(buf, sizeof(buf),
                "Bonus: Pink %d -- BONUS=half(%d) or POINTS=x%d(%d)? b/p",
                value, (value+1)/2,
                sheet->pink_multipliers[sheet->pink_next_empty_index],
                value * sheet->pink_multipliers[sheet->pink_next_empty_index]);
            ui_set_status(s, buf, false);
        }
    } else {
        ui_set_status(s, "Unknown bonus type.", true);
        s->mode = UIMODE_ACTIVE_ROLL;
    }
}


// When a sub-mode completes: if we came from passive pick, just do what 'n' does.
// Otherwise use normal active-turn end logic.
static void end_sub_mode(UIState* s, GameState* game, PlayerSheet* sheet) {
    if (s->passive_context) {
        s->passive_context = false;
        s->mode = UIMODE_ACTIVE_ROLL; // same as pressing 'n' in passive
    } else {
        if (game->chosen_dice_count >= 3 || count_pickable_dice(game) == 0)
            try_end_active_turn(s, game, sheet);
        else
            s->mode = UIMODE_ACTIVE_ROLL;
    }
}

bool ui_handle_input(UIWindows* w, UIState* s, GameState* game, int viewer_player) {
    int ch = wgetch(stdscr);
    (void)w;
    PlayerSheet* sheet = &game->players[viewer_player];

    // Universal quit
    if (ch == 'q' && s->mode != UIMODE_GAME_OVER) {
        return false;
    }

    // Universal ESC: always clears error state and returns to a safe base mode.
    // This prevents the game getting stuck after any error message.
    if (ch == 27) { // ESC
        bool in_sub_mode =
            s->mode == UIMODE_ACTIVE_MARK     ||
            s->mode == UIMODE_ACTIVE_TURQUOISE||
            s->mode == UIMODE_BONUS_CHOOSE_VALUE ||
            s->mode == UIMODE_BONUS_CHOOSE_COLOR ||
            s->mode == UIMODE_PINK_DECIDE     ||
            s->mode == UIMODE_BLUE_SEVEN      ||
            s->mode == UIMODE_ANY_NUMBER_VALUE||
            s->mode == UIMODE_EXTRA_DIE_COLOR;
        if (in_sub_mode) {
            // Return to the appropriate base mode
            { if (game->chosen_dice_count >= 3 || count_pickable_dice(game) == 0) try_end_active_turn(s, game, sheet); else s->mode = UIMODE_ACTIVE_ROLL; }
            s->status_is_error = false;
            ui_set_status(s, "Cancelled. Pick a die or use an action.", false);
            return true;
        }
    }

    switch (s->mode) {

    // -- SELECT DIE (active roll) ---------------------------------------------
    case UIMODE_ACTIVE_ROLL: {
        if (ch == KEY_LEFT || ch == 'h') {
            s->cursor_die = next_available_die(game, s->cursor_die, -1, false);
        } else if (ch == KEY_RIGHT || ch == 'l') {
            s->cursor_die = next_available_die(game, s->cursor_die, +1, false);
        } else if (ch == '\n' || ch == KEY_ENTER) {
            Die* d = &game->dice[s->cursor_die];
            if (!d->is_available || d->on_silver_platter) {
                ui_set_status(s, "That die is not available. Choose another.", true);
            } else {
                Die picked;
                if (pick_die(game, s->cursor_die, &picked)) {
                    // Blue: defer platter move until mark succeeds (sum may be invalid)
                    DieColor eff_color = picked.color;
                    if (eff_color != DIE_COLOR_BLUE) {
                        move_lower_dice_to_platter(game, picked.value);
                    }
                    s->selected_die_index = s->cursor_die;
                    bool no_dice_left = (count_pickable_dice(game) == 0);
                    if (eff_color == DIE_COLOR_WHITE) {
                        // White die: use the rolled value, just ask for color area
                        s->pending_bonus_value = picked.value;
                        s->mode = UIMODE_BONUS_CHOOSE_COLOR;
                        char wbuf[64];
                        snprintf(wbuf, sizeof(wbuf),
                            "White die (joker) rolled %d! Choose area: y t b r p", picked.value);
                        ui_set_status(s, wbuf, false);
                    } else if (eff_color == DIE_COLOR_TURQUOISE) {
                        setup_turquoise_marks(sheet, game, picked.value, true);
                        s->mode = UIMODE_ACTIVE_TURQUOISE;
                        char buf[128];
                        snprintf(buf, sizeof(buf),
                            "Turquoise %d: pick %d row(s) to mark (?? to move, ENTER to mark)",
                            picked.value, sheet->turquoise_marks_remaining);
                        ui_set_status(s, buf, false);
                    } else if (eff_color == DIE_COLOR_BLUE) {
                        int white_val = 0;
                        for (int i = 0; i < 6; i++) {
                            if (game->dice[i].color == DIE_COLOR_WHITE) {
                                white_val = game->dice[i].value;
                                break;
                            }
                        }
                        int total = picked.value + white_val;
                        if (total == 7) {
                            s->mode = UIMODE_BLUE_SEVEN;
                            ui_set_status(s, "Blue+White=7: place 7 LEFT (l) or RIGHT (r)?", false);
                        } else {
                            bool ok = mark_blue(sheet, total, true);
                            if (ok) {
                                move_lower_dice_to_platter(game, picked.value);
                                no_dice_left = (count_pickable_dice(game) == 0);
                                char buf[128];
                                snprintf(buf, sizeof(buf),
                                    "Blue: placed %d+%d=%d.", picked.value, white_val, total);
                                ui_set_status(s, buf, false);
                                if (game->chosen_dice_count >= 3 || no_dice_left) { try_end_active_turn(s, game, sheet); }
                            } else {
                                // Mark failed: undo the pick, platter untouched
                                unpick_die(game, s->selected_die_index);
                                char buf[128];
                                snprintf(buf, sizeof(buf),
                                    "Blue %d+%d=%d can't be placed (no valid neighbor). Pick another die.",
                                    picked.value, white_val, total);
                                ui_set_status(s, buf, true);
                            }
                        }
                    } else if (eff_color == DIE_COLOR_PINK) {
                        if (sheet->pink_next_empty_index == 0) {
                            // first slot always takes bonus
                            mark_pink(sheet, picked.value, true);
                            ui_set_status(s, "Pink: first slot always takes half-value bonus.", false);
                            if (game->chosen_dice_count >= 3 || no_dice_left) try_end_active_turn(s, game, sheet);
                        } else {
                            s->mode = UIMODE_PINK_DECIDE;
                            char buf[64];
                            snprintf(buf, sizeof(buf),
                                "Pink %d: BONUS=half (%d) or POINTS=x%d (%d)?  b / p",
                                picked.value,
                                (picked.value + 1) / 2,
                                sheet->pink_multipliers[sheet->pink_next_empty_index],
                                picked.value * sheet->pink_multipliers[sheet->pink_next_empty_index]);
                            ui_set_status(s, buf, false);
                        }
                    } else if (eff_color == DIE_COLOR_YELLOW) {
                        int roll_num = game->chosen_dice_count - 1;
                        bool ok = mark_yellow(sheet, picked.value, roll_num, true);
                        if (ok) {
                            char buf[64];
                            snprintf(buf, sizeof(buf), "Yellow %d marked on row %d.", picked.value, roll_num + 1);
                            ui_set_status(s, buf, false);
                            if (game->chosen_dice_count >= 3 || no_dice_left) try_end_active_turn(s, game, sheet);
                        } else {
                            ui_set_status(s, "Can't mark that yellow field!", true);
                        }
                    } else if (eff_color == DIE_COLOR_BROWN) {
                        s->mode = UIMODE_ACTIVE_MARK;
                        char buf[128];
                        snprintf(buf, sizeof(buf),
                            "Brown %d: use ?? to choose position, ENTER to mark",
                            picked.value);
                        ui_set_status(s, buf, false);
                    }
                } else {
                    ui_set_status(s, "Could not pick that die.", true);
                }
            }
        } else if (ch == 'n') {
            try_end_active_turn(s, game, sheet);
        } else if (ch == 'r' || ch == 'R') {
            bool ok = use_reroll_action(sheet, game);
            ui_set_status(s, ok ? "Re-rolled! Choose a die." : "No re-roll charges left.", !ok);
        } else if (ch == 'a') {
            if (sheet->actions.any_number_charges > 0) {
                s->mode = UIMODE_ANY_NUMBER_VALUE;
                ui_set_status(s, "Any Number: pick value 1-6 (must match an open bar slot):", false);
            } else {
                ui_set_status(s, "No Any Number charges.", true);
            }
        } else if (ch == 'q') {
            return false;
        }
        break;
    }

    // -- MARK BROWN (navigate target index) ----------------------------------
    case UIMODE_ACTIVE_MARK: {
        // cursor_col > 0: value from bonus/passive path (stored by apply_bonus_as_die)
        // cursor_col == 0: active path, read from last chosen die
        int brown_value;
        if (s->cursor_col > 0) {
            brown_value = s->cursor_col;
        } else {
            Die* last = (game->chosen_dice_count > 0)
                        ? &game->chosen_dice[game->chosen_dice_count - 1]
                        : NULL;
            if (!last) { s->mode = UIMODE_ACTIVE_ROLL; break; }
            brown_value = last->value;
        }
        int brown_min = (sheet->brown_last_marked_index >= 0)
                        ? sheet->brown_last_marked_index + 1 : 0;
        if (ch == KEY_LEFT || ch == 'h') {
            if (s->cursor_brown > brown_min) s->cursor_brown--;
        } else if (ch == KEY_RIGHT || ch == 'l') {
            if (s->cursor_brown < 11) s->cursor_brown++;
        } else if (ch == '\n' || ch == KEY_ENTER) {
            bool ok = mark_brown(sheet, brown_value, s->cursor_brown);
            if (ok) {
                s->cursor_col = 0;
                char buf[64];
                snprintf(buf, sizeof(buf), "Brown: marked index %d (value %d).",
                         s->cursor_brown, brown_value);
                ui_set_status(s, buf, false);
                { if (game->chosen_dice_count >= 3 || count_pickable_dice(game) == 0) try_end_active_turn(s, game, sheet); else s->mode = UIMODE_ACTIVE_ROLL; }
            } else {
                ui_set_status(s, "Can't mark there (wrong value, already marked, or out of order).", true);
            }
        } else if (ch == 27) { // ESC
            s->cursor_col = 0;
            s->mode = UIMODE_ACTIVE_ROLL;
            ui_set_status(s, "Brown mark cancelled.", false);
        }
        break;
    }

    // -- TURQUOISE ROW SELECTION ----------------------------------------------
    case UIMODE_ACTIVE_TURQUOISE: {
        // cursor_col > 0: value from bonus path (stored by apply_bonus_as_die)
        // cursor_col == 0: read from last chosen die
        int trq_value;
        if (s->cursor_col > 0) {
            trq_value = s->cursor_col;
        } else {
            Die* last = (game->chosen_dice_count > 0)
                        ? &game->chosen_dice[game->chosen_dice_count - 1]
                        : NULL;
            if (!last) { s->mode = UIMODE_ACTIVE_ROLL; break; }
            trq_value = last->value;
        }

        if (ch == KEY_UP || ch == 'k') {
            if (s->cursor_row > 0) s->cursor_row--;
        } else if (ch == KEY_DOWN || ch == 'j') {
            if (s->cursor_row < 4) s->cursor_row++;
        } else if (ch == '\n' || ch == KEY_ENTER) {
            bool ok = mark_turquoise(sheet, trq_value, s->cursor_row, true);
            if (ok) {
                char buf[64];
                snprintf(buf, sizeof(buf), "Turquoise row %d marked. %d left.",
                         s->cursor_row + 1, sheet->turquoise_marks_remaining);
                ui_set_status(s, buf, false);
                if (sheet->turquoise_marks_remaining <= 0) {
                    s->cursor_col = 0;
                    end_sub_mode(s, game, sheet);
                }
            } else {
                ui_set_status(s, "Can't mark that row (already marked this turn or field full).", true);
            }
        } else if (ch == 27) {
            s->cursor_col = 0;
            end_sub_mode(s, game, sheet);
            ui_set_status(s, "Turquoise marking done.", false);
        }
        break;
    }

    // -- PASSIVE PICK FROM PLATTER --------------------------------------------
    case UIMODE_PASSIVE_PICK: {
        if (ch == KEY_LEFT || ch == 'h') {
            s->cursor_die = next_available_die(game, s->cursor_die, -1, true);
        } else if (ch == KEY_RIGHT || ch == 'l') {
            s->cursor_die = next_available_die(game, s->cursor_die, +1, true);
        } else if (ch == '\n' || ch == KEY_ENTER) {
            Die* d = &game->dice[s->cursor_die];
            if (!d->on_silver_platter) {
                ui_set_status(s, "That die is not on the platter.", true);
            } else {
                // For passive players we don't remove the die; just mark on sheet
                // Using active-player-style branching but with is_active=false
                DieColor c = d->color;
                if (c == DIE_COLOR_YELLOW) {
                    bool ok = mark_yellow(sheet, d->value, 0, false);
                    ui_set_status(s, ok ? "Yellow marked (passive)." : "Can't mark that yellow field!", !ok);
                    s->mode = UIMODE_ACTIVE_ROLL; // auto-advance after pick
                } else if (c == DIE_COLOR_TURQUOISE) {
                    setup_turquoise_marks(sheet, game, d->value, false);
                    s->cursor_col = d->value;
                    s->passive_context = true;
                    s->mode = UIMODE_ACTIVE_TURQUOISE;
                    char buf[64];
                    snprintf(buf, sizeof(buf), "Turquoise (passive): %d row(s) -- up/down ENTER",
                             sheet->turquoise_marks_remaining);
                    ui_set_status(s, buf, false);
                } else if (c == DIE_COLOR_BLUE) {
                    int white_val = 0;
                    for (int i = 0; i < 6; i++) {
                        if (game->dice[i].color == DIE_COLOR_WHITE) white_val = game->dice[i].value;
                    }
                    int total = d->value + white_val;
                    if (total == 7) {
                        s->passive_context = true;
                        s->mode = UIMODE_BLUE_SEVEN;
                        ui_set_status(s, "Blue passive: place 7 LEFT (l) or RIGHT (r)?", false);
                    } else {
                        bool ok = mark_blue(sheet, total, false);
                        ui_set_status(s, ok ? "Blue marked (passive)." : "Invalid blue value!", !ok);
                        s->mode = UIMODE_ACTIVE_ROLL; // auto-advance after pick
                    }
                } else if (c == DIE_COLOR_BROWN) {
                    s->mode = UIMODE_ACTIVE_MARK;
                    s->cursor_col = d->value;
                    s->passive_context = true;
                    int last_b = sheet->brown_last_marked_index;
                    s->cursor_brown = (last_b >= 0 && last_b < 11) ? last_b + 1 : 0;
                    char buf[64];
                    snprintf(buf, sizeof(buf), "Brown %d (passive): < > navigate, ENTER to mark", d->value);
                    ui_set_status(s, buf, false);
                } else if (c == DIE_COLOR_PINK) {
                    if (sheet->pink_next_empty_index == 0) {
                        mark_pink(sheet, d->value, true);
                        ui_set_status(s, "Pink (passive): first slot always half-value.", false);
                        s->mode = UIMODE_ACTIVE_ROLL; // auto-advance after pick
                    } else {
                        s->cursor_row = d->value;
                        s->passive_context = true;
                        s->mode = UIMODE_PINK_DECIDE;
                        char buf[64];
                        snprintf(buf, sizeof(buf),
                            "Pink %d (passive): bonus=half(%d) or points=x%d(%d)?  b/p",
                            d->value, (d->value+1)/2,
                            sheet->pink_multipliers[sheet->pink_next_empty_index],
                            d->value * sheet->pink_multipliers[sheet->pink_next_empty_index]);
                        ui_set_status(s, buf, false);
                    }
                } else if (c == DIE_COLOR_WHITE) {
                    s->pending_bonus_value = d->value;
                    s->passive_context = true;
                    s->mode = UIMODE_BONUS_CHOOSE_COLOR;
                    char wbuf[64];
                    snprintf(wbuf, sizeof(wbuf),
                        "White die (joker) %d (passive): choose area y t b r p", d->value);
                    ui_set_status(s, wbuf, false);
                }
            }
        } else if (ch == 'a') {
            if (sheet->actions.any_number_charges > 0) {
                s->mode = UIMODE_ANY_NUMBER_VALUE;
                ui_set_status(s, "Any Number: pick value 1-6 (must match an open bar slot):", false);
            } else {
                ui_set_status(s, "No Any Number charges.", true);
            }
        } else if (ch == 'n') {
            // 'n' = passive phase done, signal main loop to advance turn
            s->mode = UIMODE_ACTIVE_ROLL;
            ui_set_status(s, "Passive phase complete. Advancing to next player...", false);
        }
        break;
    }

    // -- PINK DECISION --------------------------------------------------------
    case UIMODE_PINK_DECIDE: {
        Die* last = (game->chosen_dice_count > 0)
                    ? &game->chosen_dice[game->chosen_dice_count - 1]
                    : NULL;
        int val = last ? last->value : 1;

        if (ch == 'b' || ch == 'B') {
            mark_pink(sheet, val, true);
            ui_set_status(s, "Pink: took bonus (half-value).", false);
            { if (game->chosen_dice_count >= 3 || count_pickable_dice(game) == 0) try_end_active_turn(s, game, sheet); else s->mode = UIMODE_ACTIVE_ROLL; }
        } else if (ch == 'p' || ch == 'P') {
            mark_pink(sheet, val, false);
            ui_set_status(s, "Pink: took points (multiplied).", false);
            { if (game->chosen_dice_count >= 3 || count_pickable_dice(game) == 0) try_end_active_turn(s, game, sheet); else s->mode = UIMODE_ACTIVE_ROLL; }
        }
        break;
    }

    // -- BLUE SEVEN -----------------------------------------------------------
    case UIMODE_BLUE_SEVEN: {
        if (ch == 'l' || ch == 'L') {
            // Place 7 on left side
            bool placed = false;
            for (int i = 5; i >= 0; i--) {
                if (sheet->blue_area[i] == 0) {
                    sheet->blue_area[i] = 7;
                    check_and_activate_blue_bonus(sheet, i);
                    placed = true;
                    break;
                }
            }
            if (placed) {
                Die* lp = (game->chosen_dice_count > 0) ? &game->chosen_dice[game->chosen_dice_count-1] : NULL;
                if (lp && !s->passive_context) move_lower_dice_to_platter(game, lp->value);
                ui_set_status(s, "Blue: 7 placed on left.", false);
                end_sub_mode(s, game, sheet);
            } else { ui_set_status(s, "No space on left!", true); }
        } else if (ch == 'r' || ch == 'R') {
            bool placed = false;
            for (int i = 7; i <= 12; i++) {
                if (sheet->blue_area[i] == 0) {
                    sheet->blue_area[i] = 7;
                    check_and_activate_blue_bonus(sheet, i);
                    placed = true;
                    break;
                }
            }
            if (placed) {
                Die* lp = (game->chosen_dice_count > 0) ? &game->chosen_dice[game->chosen_dice_count-1] : NULL;
                if (lp && !s->passive_context) move_lower_dice_to_platter(game, lp->value);
                ui_set_status(s, "Blue: 7 placed on right.", false);
                end_sub_mode(s, game, sheet);
            } else { ui_set_status(s, "No space on right!", true); }
        }
        break;
    }

    // -- BONUS: CHOOSE VALUE --------------------------------------------------
    case UIMODE_BONUS_CHOOSE_VALUE: {
        if (ch >= '1' && ch <= '6') {
            int val = ch - '0';
            apply_bonus_as_die(s, game, sheet, s->pending_bonus_type, val, false);
            if (s->passive_context && s->mode == UIMODE_ACTIVE_ROLL) s->passive_context = false;
        } else if (ch == 27) {
            s->passive_context = false;
            s->mode = UIMODE_ACTIVE_ROLL;
            ui_set_status(s, "Bonus cancelled.", false);
        }
        break;
    }

    // -- ANY NUMBER BAR: CHOOSE VALUE -----------------------------------------
    case UIMODE_ANY_NUMBER_VALUE: {
        if (ch >= '1' && ch <= '6') {
            int val = ch - '0';
            // Check bar slot availability first
            int slot = any_number_bar_slot_for_value(sheet, val);
            if (slot < 0) {
                char buf[128];
                snprintf(buf, sizeof(buf),
                    "Value %d has no open bar slot! Use a ? slot for 1,2 or repeats.", val);
                ui_set_status(s, buf, true);
            } else {
                // Apply: mark bar slot, decrement charge, apply to a dummy die
                // The value is used as a color-bonus call (caller specifies color separately)
                Die dummy; dummy.value = 0;
                bool ok = use_any_number_action(sheet, &dummy, val);
                if (ok) {
                    char buf[128];
                    // Check for completion bonus
                    if (sheet->actions.any_number_used == 7) {
                        snprintf(buf, sizeof(buf),
                            "Any Number %d used! Bar complete -- PINK ? bonus earned!", val);
                        // Trigger pink bonus immediately
                        s->pending_bonus_type = BONUS_PINK;
                        s->mode = UIMODE_BONUS_CHOOSE_VALUE;
                    } else {
                        snprintf(buf, sizeof(buf), "Any Number: value %d applied.", val);
                        { if (game->chosen_dice_count >= 3 || count_pickable_dice(game) == 0) try_end_active_turn(s, game, sheet); else s->mode = UIMODE_ACTIVE_ROLL; }
                    }
                    ui_set_status(s, buf, false);
                } else {
                    ui_set_status(s, "Could not apply any-number action.", true);
                    s->mode = UIMODE_ACTIVE_ROLL;
                }
            }
        } else if (ch == 27) {
            s->mode = UIMODE_ACTIVE_ROLL;
            ui_set_status(s, "Any Number cancelled.", false);
        }
        break;
    }

    // -- BONUS: CHOOSE COLOR (white die / black ? bonus) ----------------------
    case UIMODE_BONUS_CHOOSE_COLOR: {
        int bonus_type = -1;
        if (ch == 'y') bonus_type = BONUS_YELLOW;
        else if (ch == 't') bonus_type = BONUS_TURQUOISE;
        else if (ch == 'b') bonus_type = BONUS_BLUE;
        else if (ch == 'r') bonus_type = BONUS_BROWN;
        else if (ch == 'p') bonus_type = BONUS_PINK;

        if (bonus_type != -1) {
            s->pending_bonus_type = bonus_type;
            if (s->pending_bonus_value > 0) {
                // White die: value known, route through proper UI logic
                apply_bonus_as_die(s, game, sheet, bonus_type, s->pending_bonus_value, true);
                s->pending_bonus_value = 0;
                // If passive and apply set ACTIVE_ROLL (immediate completion), clear context
                if (s->passive_context && s->mode == UIMODE_ACTIVE_ROLL) s->passive_context = false;
            } else {
                // ? bonus: ask player for value first
                s->mode = UIMODE_BONUS_CHOOSE_VALUE;
                ui_set_status(s, "Choose value 1-6 for the bonus:", false);
            }
        }
        break;
    }

    // -- EXTRA DIE: CHOOSE COLOR -----------------------------------------------
    case UIMODE_EXTRA_DIE_COLOR: {
        DieColor dc = DIE_COLOR_NONE;
        if (ch == 'y') dc = DIE_COLOR_YELLOW;
        else if (ch == 't') dc = DIE_COLOR_TURQUOISE;
        else if (ch == 'b') dc = DIE_COLOR_BLUE;
        else if (ch == 'r') dc = DIE_COLOR_BROWN;
        else if (ch == 'p') dc = DIE_COLOR_PINK;
        else if (ch == 'w') dc = DIE_COLOR_WHITE;
        else if (ch == 27) { s->mode = UIMODE_ACTIVE_ROLL; ui_set_status(s, "Extra die cancelled.", false); break; }

        if (dc != DIE_COLOR_NONE) {
            bool ok = use_extra_die_action(sheet, game, dc);
            if (ok) {
                char buf[128];
                if (sheet->actions.extra_die_used == 7) {
                    snprintf(buf, sizeof(buf),
                        "Extra die used! 7 total -- BROWN ? bonus earned!");
                    s->pending_bonus_type = BONUS_BROWN;
                    s->mode = UIMODE_BONUS_CHOOSE_VALUE;
                } else {
                    snprintf(buf, sizeof(buf), "Extra die added (%s).", ui_color_name(dc));
                    s->mode = UIMODE_ACTIVE_ROLL;
                }
                ui_set_status(s, buf, false);
            } else {
                ui_set_status(s, "Could not add extra die.", true);
                s->mode = UIMODE_ACTIVE_ROLL;
            }
        }
        break;
    }

    // -- USE ACTION -----------------------------------------------------------
    case UIMODE_USE_ACTION: {
        if (ch == 'r' || ch == 'R') {
            bool ok = use_reroll_action(sheet, game);
            ui_set_status(s, ok ? "Re-rolled! Choose a die." : "No re-roll charges left.", !ok);
            s->mode = UIMODE_ACTIVE_ROLL;
        } else if (ch == 'n' || ch == 'N') {
            if (sheet->actions.any_number_charges > 0) {
                s->mode = UIMODE_ANY_NUMBER_VALUE;
                ui_set_status(s, "Any Number: pick value 1-6 (must match an open bar slot):", false);
            } else {
                ui_set_status(s, "No Any Number charges.", true);
                s->mode = UIMODE_ACTIVE_ROLL;
            }
        } else if (ch == 'e' || ch == 'E') {
            if (sheet->actions.extra_die_charges > 0) {
                s->extra_die_color = -1; // will be set in BONUS_CHOOSE_COLOR
                s->mode = UIMODE_EXTRA_DIE_COLOR;
                ui_set_status(s, "Extra Die: choose color y/t/b/r/p", false);
            } else {
                ui_set_status(s, "No Extra Die charges.", true);
                s->mode = UIMODE_ACTIVE_ROLL;
            }
        } else if (ch == 27) { // ESC
            s->mode = UIMODE_ACTIVE_ROLL;
            ui_set_status(s, "Action cancelled.", false);
        }
        break;
    }

    // -- EXTRA DIE PROMPT: after active turn, player decides ─────────────────
    case UIMODE_EXTRA_DIE_PROMPT: {
        if (ch == 'e' || ch == 'E') {
            if (sheet->actions.extra_die_charges > 0) {
                s->cursor_die = 0;
                s->mode = UIMODE_EXTRA_DIE_PICK;
                ui_set_status(s, "Extra die: < > select ANY die (incl. used/platter), ENTER to use.", false);
            }
        } else if (ch == 'n') {
            s->mode = UIMODE_PASSIVE_PICK;
            ui_set_status(s, "Skipping extra die. Moving to passive phase.", false);
        }
        break;
    }

    // -- EXTRA DIE PICK: pick any die, apply using same logic as active pick --
    case UIMODE_EXTRA_DIE_PICK: {
        if (ch == KEY_LEFT || ch == 'h') {
            s->cursor_die = (s->cursor_die + 5) % 6;
        } else if (ch == KEY_RIGHT || ch == 'l') {
            s->cursor_die = (s->cursor_die + 1) % 6;
        } else if (ch == '\n' || ch == KEY_ENTER) {
            Die* d = &game->dice[s->cursor_die];
            DieColor dc = d->color;
            int dv = d->value;
            char buf[128];

            // Consume one extra die charge
            sheet->actions.extra_die_charges--;
            sheet->actions.extra_die_used++;

            // Use EXACT same logic as active die pick for each color
            if (dc == DIE_COLOR_WHITE) {
                s->pending_bonus_value = dv;
                s->mode = UIMODE_BONUS_CHOOSE_COLOR;
                snprintf(buf, sizeof(buf), "Extra die: white %d -- choose area y t b r p", dv);
                ui_set_status(s, buf, false);
            } else if (dc == DIE_COLOR_TURQUOISE) {
                setup_turquoise_marks(sheet, game, dv, true);
                s->mode = UIMODE_ACTIVE_TURQUOISE;
                snprintf(buf, sizeof(buf),
                    "Extra die: Turquoise %d: pick %d row(s) (up/down ENTER)",
                    dv, sheet->turquoise_marks_remaining);
                ui_set_status(s, buf, false);
            } else if (dc == DIE_COLOR_BLUE) {
                int white_val = 0;
                for (int i = 0; i < 6; i++) {
                    if (game->dice[i].color == DIE_COLOR_WHITE) { white_val = game->dice[i].value; break; }
                }
                int total = dv + white_val;
                if (total == 7) {
                    s->mode = UIMODE_BLUE_SEVEN;
                    ui_set_status(s, "Extra die: Blue+White=7 -- LEFT (l) or RIGHT (r)?", false);
                } else {
                    bool ok = mark_blue(sheet, total, true);
                    if (ok) {
                        snprintf(buf, sizeof(buf), "Extra die: Blue %d+%d=%d.", dv, white_val, total);
                        ui_set_status(s, buf, false);
                        if (sheet->actions.extra_die_charges > 0) s->mode = UIMODE_EXTRA_DIE_PROMPT;
                        else s->mode = UIMODE_PASSIVE_PICK;
                    } else {
                        snprintf(buf, sizeof(buf), "Extra die: Blue %d+%d=%d no valid neighbor!", dv, white_val, total);
                        ui_set_status(s, buf, true);
                        sheet->actions.extra_die_charges++; sheet->actions.extra_die_used--;
                        s->mode = UIMODE_EXTRA_DIE_PICK;
                    }
                }
            } else if (dc == DIE_COLOR_PINK) {
                if (sheet->pink_next_empty_index == 0) {
                    mark_pink(sheet, dv, true);
                    ui_set_status(s, "Extra die: Pink first slot (half-value).", false);
                    if (sheet->actions.extra_die_charges > 0) s->mode = UIMODE_EXTRA_DIE_PROMPT;
                    else s->mode = UIMODE_PASSIVE_PICK;
                } else {
                    s->mode = UIMODE_PINK_DECIDE;
                    snprintf(buf, sizeof(buf),
                        "Extra die: Pink %d -- BONUS=half(%d) or POINTS=x%d(%d)? b/p",
                        dv, (dv+1)/2,
                        sheet->pink_multipliers[sheet->pink_next_empty_index],
                        dv * sheet->pink_multipliers[sheet->pink_next_empty_index]);
                    ui_set_status(s, buf, false);
                }
            } else if (dc == DIE_COLOR_YELLOW) {
                // Yellow: use chosen_dice_count as roll_num (same as active pick)
                int roll_num = (game->chosen_dice_count > 0) ? game->chosen_dice_count - 1 : 2;
                if (roll_num > 2) roll_num = 2;
                bool ok = mark_yellow(sheet, dv, roll_num, true);
                if (ok) {
                    snprintf(buf, sizeof(buf), "Extra die: Yellow %d on row %d.", dv, roll_num + 1);
                    ui_set_status(s, buf, false);
                    if (sheet->actions.extra_die_charges > 0) s->mode = UIMODE_EXTRA_DIE_PROMPT;
                    else s->mode = UIMODE_PASSIVE_PICK;
                } else {
                    ui_set_status(s, "Extra die: Can't mark that yellow field!", true);
                    sheet->actions.extra_die_charges++; sheet->actions.extra_die_used--;
                    s->mode = UIMODE_EXTRA_DIE_PICK;
                }
            } else if (dc == DIE_COLOR_BROWN) {
                // Brown: same as active pick - enter ACTIVE_MARK with cursor at last+1
                s->mode = UIMODE_ACTIVE_MARK;
                s->cursor_col = 0; // 0 = use chosen_dice value... but we need extra die value
                // Store value in cursor_col (passive path reuse - cursor_col>0 means use it)
                s->cursor_col = dv;
                int last_b = sheet->brown_last_marked_index;
                s->cursor_brown = (last_b >= 0 && last_b < 11) ? last_b + 1 : 0;
                snprintf(buf, sizeof(buf), "Extra die: Brown %d -- < > navigate, ENTER.", dv);
                ui_set_status(s, buf, false);
            }
        } else if (ch == 27) {
            s->mode = UIMODE_EXTRA_DIE_PROMPT;
            ui_set_status(s, "Cancelled. Use extra die (e) or skip (n).", false);
        }
        break;
    }

    // -- GAME OVER ------------------------------------------------------------
    case UIMODE_GAME_OVER: {
        if (ch == 'q') return false;
        break;
    }

    default:
        break;
    }

    return true; // keep running
}


void ui_prompt_value(UIWindows* w, UIState* s, const char* prompt, int* out_value) {
    ui_set_status(s, prompt, false);
    ui_render_status(w->status, s);
    doupdate();

    // Read a digit
    int ch;
    do {
        ch = wgetch(stdscr);
    } while (ch < '1' || ch > '6');

    *out_value = ch - '0';
}
