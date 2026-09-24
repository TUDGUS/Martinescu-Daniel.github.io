#ifndef UI_H
#define UI_H

#include "clever_cubed.h"

// Include ncurses AFTER our game header to avoid macro conflicts.
// We provide NC_COLOR_* aliases for ncurses color constants so they
// don't collide with the DieColor enum values (which start at 10).
#include <ncurses.h>

// Safe ncurses color aliases
#define NC_COLOR_BLACK   COLOR_BLACK
#define NC_COLOR_RED     COLOR_RED
#define NC_COLOR_GREEN   COLOR_GREEN
#define NC_COLOR_YELLOW  COLOR_YELLOW
#define NC_COLOR_BLUE    COLOR_BLUE
#define NC_COLOR_MAGENTA COLOR_MAGENTA
#define NC_COLOR_CYAN    COLOR_CYAN
#define NC_COLOR_WHITE   COLOR_WHITE

// ─── Color pair IDs ───────────────────────────────────────────────────────────
#define CP_DEFAULT      1   // white on black
#define CP_YELLOW       2   // black on yellow
#define CP_TURQUOISE    3   // black on cyan
#define CP_BLUE         4   // white on blue
#define CP_BROWN        5   // white on dark-red (approximated)
#define CP_PINK         6   // black on magenta
#define CP_WHITE_DIE    7   // black on white
#define CP_SILVER       8   // black on bright-white (silver platter)
#define CP_HEADER       9   // black on yellow  (top bar)
#define CP_MARKED       10  // bright-white on black (crossed out fields)
#define CP_BONUS        11  // black on green (bonus cells)
#define CP_INACTIVE     12  // dark on black (unavailable)
#define CP_HIGHLIGHT    13  // black on bright-cyan (selection highlight)
#define CP_ACTION       14  // white on dark-blue (action bar)
#define CP_STATUS_OK    15  // bright-green on black
#define CP_STATUS_ERR   16  // bright-red on black
#define CP_TITLE        17  // bright-yellow on black

// ─── Window layout constants ──────────────────────────────────────────────────
// Minimum recommended terminal: 120 cols × 42 rows

#define WIN_TOP_H       3
#define WIN_DICE_H      5
#define WIN_DICE_W      60
#define WIN_ACTIONS_H   3
#define WIN_STATUS_H    2

// Sub-windows for each area (relative to main sheet window)
#define YELLOW_COLS     20
#define TURQUOISE_COLS  22
#define BLUE_COLS       32
#define BROWN_COLS      30
#define PINK_COLS       28

// ─── UI state ─────────────────────────────────────────────────────────────────
typedef enum {
    UIMODE_ACTIVE_ROLL,         // active player: choose which die to pick
    UIMODE_ACTIVE_MARK,         // active player: choose where to mark (area-specific)
    UIMODE_ACTIVE_TURQUOISE,    // active player: choose rows for turquoise
    UIMODE_PASSIVE_PICK,        // passive player: choose die from silver platter
    UIMODE_PASSIVE_MARK,        // passive player: mark their sheet
    UIMODE_BONUS_CHOOSE_VALUE,  // any player: pick value 1-6 for a ?-bonus
    UIMODE_BONUS_CHOOSE_COLOR,  // for black ? bonus: pick color area
    UIMODE_PINK_DECIDE,         // active/passive: bonus or points for pink
    UIMODE_BLUE_SEVEN,          // place 7 on left or right
    UIMODE_USE_ACTION,          // choose which action to use
    UIMODE_ANY_NUMBER_VALUE,    // pick value for any-number bar
    UIMODE_EXTRA_DIE_COLOR,     // pick color for extra die action
    UIMODE_EXTRA_DIE_PROMPT,    // after active turn: use extra die or continue
    UIMODE_EXTRA_DIE_PICK,      // pick any die from board for extra die
    UIMODE_GAME_OVER,           // final scores displayed
} UIMode;

typedef struct {
    UIMode      mode;
    int         cursor_die;         // which die index is highlighted (0-5)
    int         cursor_row;         // generic row cursor for area grids
    int         cursor_col;         // generic col cursor for area grids
    int         cursor_brown;       // cursor for brown area (0-11)
    char        status_msg[256];    // message shown in status bar
    bool        status_is_error;    // red vs green status
    int         pending_bonus_type; // which bonus is pending (BONUS_*)
    int         pending_bonus_value; // value to use with the bonus (0 = ask player)
    int         pending_roll_num;    // roll number when white die was picked (for yellow row)
    bool        pending_is_active;   // true if bonus comes from active player turn
    int         selected_die_index; // die index chosen for this mark step
    bool        pink_take_bonus;    // user's pink decision
    int         extra_die_color;    // color chosen for extra die action
} UIState;

// ─── Main windows ─────────────────────────────────────────────────────────────
typedef struct {
    WINDOW* top;        // header: round / player / phase
    WINDOW* dice;       // dice display + silver platter
    WINDOW* sheet;      // full player sheet (all 5 areas)
    WINDOW* actions;    // action charge bar
    WINDOW* status;     // status / prompt line
    int     term_rows;
    int     term_cols;
} UIWindows;

// ─── Public API ───────────────────────────────────────────────────────────────

// Lifecycle
void ui_init(UIWindows* w, UIState* s);
void ui_teardown(UIWindows* w);
void ui_resize(UIWindows* w);

// Rendering
void ui_render_all(UIWindows* w, UIState* s, GameState* game, int viewer_player);
void ui_render_top(WINDOW* win, GameState* game, UIState* s);
void ui_render_dice(WINDOW* win, GameState* game, UIState* s);
void ui_render_sheet(WINDOW* win, PlayerSheet* sheet, UIState* s);
void ui_render_actions_bar(WINDOW* win, PlayerSheet* sheet, UIState* s);
void ui_render_status(WINDOW* win, UIState* s);

// Area sub-renderers (called from ui_render_sheet)
void ui_render_yellow(WINDOW* win, int y, int x, PlayerSheet* sheet, UIState* s);
void ui_render_turquoise(WINDOW* win, int y, int x, PlayerSheet* sheet, UIState* s);
void ui_render_blue(WINDOW* win, int y, int x, PlayerSheet* sheet, UIState* s);
void ui_render_brown(WINDOW* win, int y, int x, PlayerSheet* sheet, UIState* s);
void ui_render_pink(WINDOW* win, int y, int x, PlayerSheet* sheet, UIState* s);

// Input handling (returns false when game should quit)
bool ui_handle_input(UIWindows* w, UIState* s, GameState* game, int viewer_player);

// Helpers
void ui_set_status(UIState* s, const char* msg, bool is_error);
void ui_prompt_value(UIWindows* w, UIState* s, const char* prompt, int* out_value);
int  ui_color_pair_for_die(DieColor c);
const char* ui_color_name(DieColor c);
const char* ui_die_face(int value);
void ui_draw_die(WINDOW* win, int y, int x, DieColor color, int value,
                 bool on_platter, bool highlighted, bool unavailable);

#endif // UI_H
