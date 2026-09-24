#ifndef CLEVER_CUBED_H
#define CLEVER_CUBED_H

#include <stdbool.h>
#include <stdint.h>

// --- ENUMS & DEFINITIONS ---

// IMPORTANT: Values start at 10 to avoid conflicts with ncurses COLOR_* macros
// (ncurses defines COLOR_BLACK=0 through COLOR_WHITE=7). Do not include
// ncurses.h before this header in any translation unit that uses DieColor.
typedef enum {
    DIE_COLOR_YELLOW    = 10,
    DIE_COLOR_TURQUOISE = 11,
    DIE_COLOR_BLUE      = 12,
    DIE_COLOR_BROWN     = 13,
    DIE_COLOR_PINK      = 14,
    DIE_COLOR_WHITE     = 15,
    DIE_COLOR_NONE      = 16
} DieColor;

// Helper: convert 0-based die slot index to DieColor
#define DIE_INDEX_TO_COLOR(i) ((DieColor)(DIE_COLOR_YELLOW + (i)))


// --- DATA STRUCTURES ---

typedef struct {
    DieColor color;
    int value;
    bool on_silver_platter;
    bool is_available;
} Die;

// The any-number bar has 7 fixed slots: values 3,4,5,6 then three wildcards '?'.
// any_number_bar_used[i] = true means that slot has been crossed out.
// Slot values: indices 0-3 require values 3,4,5,6; indices 4-6 are wildcards (0).
#define ANY_NUMBER_BAR_SIZE 7

typedef struct {
    int  reroll_charges;
    int  any_number_charges;   // available uses (capped by uncrossed slots)
    int  extra_die_charges;
    int  foxes_unlocked;

    // Any-number bar slots
    bool any_number_bar_used[ANY_NUMBER_BAR_SIZE];

    // Usage counters for end-of-track bonuses
    int  rerolls_used;       // 7 -> fox charge
    int  any_number_used;    // 7 -> pink ? bonus
    int  extra_die_used;     // 7 -> brown ? bonus
} Actions;

typedef struct {
    int id;
    int score;
    
    bool yellow_area[3][6];

    bool turquoise_area[5][6];
    bool turquoise_column_bonus[6];
    bool turquoise_row_bonus[5];
    int turquoise_marks_remaining;
    bool turquoise_marked_this_turn[5];
 
    int blue_area[13];
    bool blue_bonus[13];

    bool brown_area[12];
    int brown_required_value[12];
    int brown_last_marked_index;
    bool brown_bonus[11];
    
    int pink_area[12];
    int pink_multipliers[12];
    bool pink_bonus_available[12];
    int pink_next_empty_index;
    
    Actions actions;

    // Set by mark_yellow when a ? bonus is triggered; UI reads and clears this.
    int pending_bonus_type; // 0 = none, else BONUS_* constant
    
} PlayerSheet;

typedef struct {
    int current_round;
    int max_rounds;
    int num_players;
    int active_player_index;
    Die dice[6];
    Die chosen_dice[3];
    int chosen_dice_count;
    PlayerSheet players[4];
} GameState;

// Bonus type constants
#define BONUS_PINK 1
#define BONUS_TURQUOISE 2
#define BONUS_BLUE 3
#define BONUS_BROWN 4
#define BONUS_YELLOW 5

// --- FUNCTION DECLARATIONS ---

// Basic initialization
void init_game(GameState* game, int num_players);
void init_player_sheet(PlayerSheet* sheet, int id);

// Dice management
void roll_available_dice(GameState* game);
void roll_specific_dice(GameState* game, int dice_indices[], int count);
void move_lower_dice_to_platter(GameState* game, int chosen_value);

// Dice picking
bool pick_die(GameState* game, int die_index, Die* picked_die);
void reset_chosen_dice(GameState* game);

// Color bonus handler
void use_color_bonus(PlayerSheet* sheet, int bonus_type, int chosen_value);

// Yellow Area
bool mark_yellow(PlayerSheet* sheet, int value, int roll_number, bool is_active_player);
int calculate_yellow_score(PlayerSheet* sheet);

// Turquoise Area
int count_turquoise_dice_of_value(GameState* game, PlayerSheet* sheet, int value, bool is_active_player);
void reset_turquoise_turn(PlayerSheet* sheet);
void setup_turquoise_marks(PlayerSheet* sheet, GameState* game, int value, bool is_active_player);
void activate_turquoise_bonus(PlayerSheet* sheet, int index, bool is_column);
bool mark_turquoise(PlayerSheet* sheet, int value, int row, bool is_active_player);
int calculate_turquoise_score(PlayerSheet* sheet);

// Blue Area
void check_and_activate_blue_bonus(PlayerSheet* sheet, int position);
bool mark_blue_seven(PlayerSheet* sheet);
bool mark_blue(PlayerSheet* sheet, int total_value, bool is_active_player);
int get_blue_outermost_points(int position);
int calculate_blue_score(PlayerSheet* sheet);

// Brown Area
void activate_brown_bonus(PlayerSheet* sheet, int bonus_index);
bool mark_brown(PlayerSheet* sheet, int value, int target_index);
int calculate_brown_score(PlayerSheet* sheet);

// Pink Area
void activate_pink_bonus(PlayerSheet* sheet, int position);
bool mark_pink(PlayerSheet* sheet, int value, bool take_bonus);
int calculate_pink_score(PlayerSheet* sheet);

// Actions
int  any_number_bar_slot_for_value(PlayerSheet* sheet, int value);
void give_round_bonus(GameState* game, int round);
bool use_reroll_action(PlayerSheet* sheet, GameState* game);
bool use_any_number_action(PlayerSheet* sheet, Die* die, int desired_value);
bool use_extra_die_action(PlayerSheet* sheet, GameState* game, DieColor color);

// Turn Flow
void start_round(GameState* game);
void play_active_turn(GameState* game, int player_index);
void play_passive_turn(GameState* game);
bool check_game_end(GameState* game);
void calculate_final_scores(GameState* game);

#endif // CLEVER_CUBED_H
