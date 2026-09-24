#ifndef BOT_H
#define BOT_H

#include "clever_cubed.h"
#include <stdbool.h>

// Bot personality identifiers
typedef enum {
    BOT_NONE   = 0,
    BOT_GOLLUM = 1,  // Greedy: max immediate points
    BOT_MIDAS  = 2,  // Greedy: action-first, then max points
    BOT_IROH   = 3,  // Balanced: diminishing returns per area + dynamic action values
} BotType;

// Names for display
static inline const char* bot_name(BotType t) {
    switch (t) {
        case BOT_GOLLUM: return "Gollum";
        case BOT_MIDAS:  return "Midas";
        case BOT_IROH:   return "Iroh";
        default:         return "Bot";
    }
}

// Result of a bot decision
typedef struct {
    int      die_index;
    int      area;
    int      param;
    bool     take_bonus;
    bool     go_left;
} BotMove;

// Area maximum scores (used by Iroh for diminishing returns)
#define IROH_YELLOW_MAX    126
#define IROH_TURQUOISE_MAX 105
#define IROH_BLUE_MAX       76
#define IROH_BROWN_MAX      90
#define IROH_PINK_MAX      129

// Action helpers
void bot_prepare_turn(BotType bot, GameState* game, PlayerSheet* sheet, bool* rerolled_out, bool* any_number_used_out);
bool bot_should_reroll(BotType bot, int plan_pts, int plan_acts, double plan_iroh);
bool bot_find_any_number(BotType bot, GameState* game, PlayerSheet* sheet, int* best_color_out, int* best_value_out);

// Extra die
bool bot_use_extra_die(BotType bot, GameState* game, PlayerSheet* sheet);

// Main entry points
bool bot_do_active_turn(BotType bot, GameState* game, PlayerSheet* sheet, int roll_num);
bool bot_do_passive_turn(BotType bot, GameState* game, PlayerSheet* sheet);
void bot_handle_white_bonus(BotType bot, GameState* game, PlayerSheet* sheet);

// Iroh move counter
extern int g_iroh_move_count;

#endif // BOT_H
