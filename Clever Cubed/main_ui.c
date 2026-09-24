#define _POSIX_C_SOURCE 200809L
#include "clever_cubed.h"
#include "ui.h"
#include "bot.h"
#include <unistd.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>

BotType g_bot_type = BOT_NONE; // global bot selection
extern int g_iroh_move_count;

static int startup_screen() {
    clear();
    attron(COLOR_PAIR(CP_TITLE) | A_BOLD);
    mvprintw(3, 10, "+==============================+");
    mvprintw(4, 10, "|       CLEVER  CUBED          |");
    mvprintw(5, 10, "|   Clever Hoch Drei - TUI     |");
    mvprintw(6, 10, "+==============================+");
    attroff(COLOR_PAIR(CP_TITLE) | A_BOLD);

    attron(COLOR_PAIR(CP_DEFAULT));
    mvprintw(9, 10, "How many players? (1-4):");
    mvprintw(10, 10, "5. Versus Bot");
    refresh();
    attroff(COLOR_PAIR(CP_DEFAULT));

    int ch;
    do { ch = getch(); } while (ch < '1' || ch > '5');

    if (ch == '5') {
        // Bot selection screen
        clear();
        attron(COLOR_PAIR(CP_TITLE) | A_BOLD);
        mvprintw(3, 10, "Choose your opponent:");
        attroff(COLOR_PAIR(CP_TITLE) | A_BOLD);

        attron(COLOR_PAIR(CP_DEFAULT));
        mvprintw(6, 10,  "1.Gollum");
        mvprintw(7, 10,  "2.Midas");
        mvprintw(8, 10,  "3.Iroh");
        mvprintw(10, 10, "Your choice (1-3):");
        refresh();
        attroff(COLOR_PAIR(CP_DEFAULT));

        int bc;
        do { bc = getch(); } while (bc < '1' || bc > '3');
        g_bot_type = (bc == '1') ? BOT_GOLLUM : (bc == '2') ? BOT_MIDAS : BOT_IROH;
        return 2; // always 2 players in vs-bot mode
    }

    g_bot_type = BOT_NONE;
    return ch - '0';
}

int main(void) {
    srand((unsigned)time(NULL));

    // -- Init ncurses (minimal, enough for startup screen) --------------------
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    start_color();
    use_default_colors();

    init_pair(CP_DEFAULT,    NC_COLOR_WHITE,   COLOR_BLACK);
    init_pair(CP_YELLOW,     NC_COLOR_BLACK,   NC_COLOR_YELLOW);
    init_pair(CP_TURQUOISE,  NC_COLOR_BLACK,   NC_COLOR_CYAN);
    init_pair(CP_BLUE,       NC_COLOR_WHITE,   NC_COLOR_BLUE);
    init_pair(CP_BROWN,      NC_COLOR_WHITE,   NC_COLOR_RED);
    init_pair(CP_PINK,       NC_COLOR_BLACK,   NC_COLOR_MAGENTA);
    init_pair(CP_WHITE_DIE,  NC_COLOR_BLACK,   NC_COLOR_WHITE);
    init_pair(CP_SILVER,     NC_COLOR_BLACK,   NC_COLOR_WHITE);
    init_pair(CP_HEADER,     NC_COLOR_BLACK,   NC_COLOR_YELLOW);
    init_pair(CP_MARKED,     NC_COLOR_WHITE,   NC_COLOR_BLACK);
    init_pair(CP_BONUS,      NC_COLOR_BLACK,   NC_COLOR_GREEN);
    init_pair(CP_INACTIVE,   NC_COLOR_BLACK,   NC_COLOR_BLACK);
    init_pair(CP_HIGHLIGHT,  NC_COLOR_BLACK,   NC_COLOR_CYAN);
    init_pair(CP_ACTION,     NC_COLOR_WHITE,   NC_COLOR_BLUE);
    init_pair(CP_STATUS_OK,  NC_COLOR_GREEN,   NC_COLOR_BLACK);
    init_pair(CP_STATUS_ERR, NC_COLOR_RED,     NC_COLOR_BLACK);
    init_pair(CP_TITLE,      NC_COLOR_YELLOW,  NC_COLOR_BLACK);

    int num_players = startup_screen();
    endwin(); // tear down minimal ncurses, let ui_init rebuild windows properly

    // -- Init game ------------------------------------------------------------
    GameState game;
    init_game(&game, num_players);
    game.max_rounds = (num_players == 4) ? 4 : (num_players == 3) ? 5 : 6; // 4 rounds for 4p, 5 for 3p, 6 for 1-2p
    roll_available_dice(&game);

    // Player names for vs-bot mode
    const char* player_names[4] = {"Player", bot_name(g_bot_type), "Player 3", "Player 4"};
    (void)player_names; // used in status messages below

    // -- Init UI --------------------------------------------------------------
    UIWindows w;
    UIState   s;
    ui_init(&w, &s);

    // Reset Iroh move counter for new game
    g_iroh_move_count = 0;

    // Give round 1 bonus to all players before the first turn
    give_round_bonus(&game, game.current_round);

    s.mode = UIMODE_ACTIVE_ROLL;
    char buf[256];


    // Turn tracking
    // Each round: every player takes one active turn (in order 0..num_players-1),
    // each followed immediately by all OTHER players doing a passive pick.
    // After all players have been active, the round advances.
    int passive_queue[4];
    int passive_count        = 0;
    int passive_done         = 0;
    int current_passive      = -1;
    int viewer               = 0;
    int active_turns_this_round = 0; // how many players have been active this round

    // -------------------------------------------------------------------------
    #define RESET_DICE_FOR_ACTIVE() do {         for (int _i = 0; _i < 6; _i++) {             game.dice[_i].is_available     = true;             game.dice[_i].on_silver_platter = false;             game.dice[_i].value            = 0;         }         game.chosen_dice_count = 0;         roll_available_dice(&game);     } while(0)

    #define START_ACTIVE_TURN() do {         RESET_DICE_FOR_ACTIVE();         current_passive = -1;         passive_count   = 0;         passive_done    = 0;         viewer = game.active_player_index;         if (game.current_round == 4) {             s.pending_bonus_value = 0;             s.mode = UIMODE_BONUS_CHOOSE_COLOR;             snprintf(buf, sizeof(buf),                 "Round 4: WHITE ? bonus for Player %d -- choose area: y t b r p",                 game.active_player_index + 1);             ui_set_status(&s, buf, false);         } else {             s.mode = UIMODE_ACTIVE_ROLL;             snprintf(buf, sizeof(buf),                 "Round %d | Player %d active -- pick a die.",                 game.current_round, game.active_player_index + 1);             ui_set_status(&s, buf, false);         }     } while(0)

    #define START_NEXT_ROUND() do {         game.current_round++;         active_turns_this_round = 0;         game.active_player_index = 0;         if (check_game_end(&game)) {             s.mode = UIMODE_GAME_OVER;             ui_set_status(&s, "Game over! Press q to exit.", false);             goto game_over;         }         give_round_bonus(&game, game.current_round);         {             const char* _bn[] = {"","[&] reroll","+1 extra die","[AN] any-number","[?] white"};             int _r = game.current_round;             snprintf(buf, sizeof(buf), "Round %d! All players get: %s",                 _r, (_r >= 1 && _r <= 4) ? _bn[_r] : "");             ui_set_status(&s, buf, false);         }         START_ACTIVE_TURN();     } while(0)

    active_turns_this_round = 0;
    START_ACTIVE_TURN();

    // -- Main loop ------------------------------------------------------------
    while (true) {
        ui_render_all(&w, &s, &game, viewer);

        // ── Bot turn: execute automatically, skip human input ────────────────
        bool bot_turn = (g_bot_type != BOT_NONE && game.active_player_index == 1
                         && s.mode == UIMODE_ACTIVE_ROLL);
        bool bot_passive = (g_bot_type != BOT_NONE && current_passive == 1
                            && s.mode == UIMODE_PASSIVE_PICK);
        // Only intercept bonuses when it's actually the bot's turn (not human passive)
        bool bot_is_acting = (g_bot_type != BOT_NONE && game.active_player_index == 1
                              && current_passive != 0);
        bool bot_round4 = (bot_is_acting && s.mode == UIMODE_BONUS_CHOOSE_COLOR);
        bool bot_bonus_value = (bot_is_acting && s.mode == UIMODE_BONUS_CHOOSE_VALUE);
        bool bot_extra_skip = (g_bot_type != BOT_NONE && game.active_player_index == 1
                               && s.mode == UIMODE_EXTRA_DIE_PROMPT);

        if (bot_round4 || bot_bonus_value) {
            bot_handle_white_bonus(g_bot_type, &game, &game.players[1]);
            // After bonus, handle any sub-modes that may have been triggered
            // (turquoise row selection, brown mark, pink decide, blue-7)
            if (s.mode == UIMODE_ACTIVE_TURQUOISE) {
                // Bot marks turquoise rows one by one
                while (game.players[1].turquoise_marks_remaining > 0) {
                    int trq_val = s.cursor_col > 0 ? s.cursor_col : 1;
                    for (int _r = 0; _r < 5; _r++) {
                        if (mark_turquoise(&game.players[1], trq_val, _r, true)) break;
                    }
                    if (game.players[1].turquoise_marks_remaining <= 0) break;
                }
                s.cursor_col = 0;
                s.mode = UIMODE_ACTIVE_ROLL;
            } else if (s.mode == UIMODE_ACTIVE_MARK) {
                // Bot marks brown at next valid slot
                int bv = s.cursor_col > 0 ? s.cursor_col : 1;
                for (int _idx = 0; _idx < 12; _idx++)
                    if (mark_brown(&game.players[1], bv, _idx)) break;
                s.cursor_col = 0;
                s.mode = UIMODE_ACTIVE_ROLL;
            } else if (s.mode == UIMODE_PINK_DECIDE) {
                int pv = s.cursor_row > 0 ? s.cursor_row : 1;
                mark_pink(&game.players[1], pv, false); // always take points
                s.cursor_row = 0;
                s.mode = UIMODE_ACTIVE_ROLL;
            } else if (s.mode == UIMODE_BLUE_SEVEN) {
                // Place on left
                for (int _i = 5; _i >= 0; _i--)
                    if (game.players[1].blue_area[_i] == 0) {
                        game.players[1].blue_area[_i] = 7;
                        check_and_activate_blue_bonus(&game.players[1], _i); break;
                    }
                s.mode = UIMODE_ACTIVE_ROLL;
            } else if (s.mode == UIMODE_BONUS_CHOOSE_COLOR || s.mode == UIMODE_BONUS_CHOOSE_VALUE) {
                s.mode = UIMODE_ACTIVE_ROLL;
            }
            // Clear any status messages set by the bonus handler
            ui_set_status(&s, "", false);
            { struct timespec _ts = {0, 500000000L}; nanosleep(&_ts, NULL); };
            continue;
        }

        if (bot_extra_skip) {
            PlayerSheet* bsheet = &game.players[1];
            bool used = bot_use_extra_die(g_bot_type, &game, bsheet);
            if (used) {
                ui_render_all(&w, &s, &game, viewer);
                { struct timespec _ts = {0, 500000000L}; nanosleep(&_ts, NULL); };
            }
            // If more extra die charges remain, loop again; otherwise go to passive
            if (bsheet->actions.extra_die_charges > 0) continue;
            s.mode = UIMODE_PASSIVE_PICK;
            continue;
        }

        if (bot_turn) {
            PlayerSheet* bsheet = &game.players[1];

            // Handle reroll and any-number before picks
            bool rerolled = false, an_used = false;
            bot_prepare_turn(g_bot_type, &game, bsheet, &rerolled, &an_used);
            if (rerolled || an_used) {
                ui_render_all(&w, &s, &game, viewer);
                { struct timespec _ts = {0, 500000000L}; nanosleep(&_ts, NULL); };
            }

            // Pick up to 3 dice
            int picks = 0;
            while (picks < 3) {
                bool moved = bot_do_active_turn(g_bot_type, &game, bsheet, picks);
                if (!moved) break;
                picks++;
                ui_render_all(&w, &s, &game, viewer);
                { struct timespec _ts = {0, 500000000L}; nanosleep(&_ts, NULL); };
                { int _n=0; for(int _i=0;_i<6;_i++) if(game.dice[_i].is_available&&!game.dice[_i].on_silver_platter) _n++; if(_n==0) break; }
            }
            // End active turn: move remaining to platter
            for (int _i = 0; _i < 6; _i++)
                if (game.dice[_i].is_available && !game.dice[_i].on_silver_platter)
                    game.dice[_i].on_silver_platter = true;
            snprintf(buf, sizeof(buf), "%s finished their active turn.", bot_name(g_bot_type));
            ui_set_status(&s, buf, false);
            s.mode = UIMODE_PASSIVE_PICK;
            ui_render_all(&w, &s, &game, viewer);
            { struct timespec _ts = {0, 500000000L}; nanosleep(&_ts, NULL); };
            continue;
        }

        if (bot_passive) {
            bot_do_passive_turn(g_bot_type, &game, &game.players[1]);
            snprintf(buf, sizeof(buf), "%s made their passive pick.", bot_name(g_bot_type));
            ui_set_status(&s, buf, false);
            s.mode = UIMODE_ACTIVE_ROLL;
            ui_render_all(&w, &s, &game, viewer);
            { struct timespec _ts = {0, 500000000L}; nanosleep(&_ts, NULL); };
            continue;
        }

        bool running = ui_handle_input(&w, &s, &game, viewer);
        if (!running) break;

        // ── Active turn complete: enter passive phase ────────────────────────
        // Triggered when ui_handle_input sets mode to PASSIVE_PICK
        if (s.mode == UIMODE_PASSIVE_PICK && current_passive == -1) {
            // Build the passive queue: all players except the active one, in order
            passive_count = 0;
            passive_done  = 0;
            for (int i = 1; i < game.num_players; i++) {
                int pi = (game.active_player_index + i) % game.num_players;
                passive_queue[passive_count++] = pi;
            }

            if (passive_count == 0) {
                // Single player: one passive pick then advance
                current_passive = game.active_player_index;
                viewer = current_passive;
                snprintf(buf, sizeof(buf),
                    "Round %d | Active done. Make your passive pick from the platter.",
                    game.current_round);
                ui_set_status(&s, buf, false);
            } else {
                // Multiplayer: first passive player's turn
                current_passive = passive_queue[passive_done];
                viewer = current_passive;
                snprintf(buf, sizeof(buf),
                    "Round %d | %s: make your passive pick from the platter.",
                    game.current_round,
                    (g_bot_type != BOT_NONE && current_passive == 1)
                        ? bot_name(g_bot_type) : player_names[current_passive]);
                ui_set_status(&s, buf, false);
            }
        }

        // ── Passive player just made their pick (mode -> ACTIVE_ROLL) ────────
        // ui_handle_input sets mode back to ACTIVE_ROLL after a passive mark
        // or when 'n' is pressed (skip passive pick).
        if (current_passive != -1 && s.mode == UIMODE_ACTIVE_ROLL) {
            passive_done++;

            bool all_done = (passive_done >= passive_count) ||
                            (game.num_players == 1 && passive_done >= 1);

            if (!all_done) {
                // Next passive player
                current_passive = passive_queue[passive_done];
                viewer = current_passive;
                s.mode = UIMODE_PASSIVE_PICK;
                snprintf(buf, sizeof(buf),
                    "Round %d | %s: make your passive pick from the platter.",
                    game.current_round,
                    (g_bot_type != BOT_NONE && current_passive == 1)
                        ? bot_name(g_bot_type) : player_names[current_passive]);
                ui_set_status(&s, buf, false);
            } else {
                // All passive picks done for this active player.
                // Check if more players still need to be active this round.
                active_turns_this_round++;
                if (active_turns_this_round < game.num_players) {
                    // Next player's active turn
                    game.active_player_index = active_turns_this_round;
                    START_ACTIVE_TURN();
                } else {
                    // All players have been active -- start next round
                    START_NEXT_ROUND();
                }
            }
        }

        // ── Fallback game end check ──────────────────────────────────────────
        if (check_game_end(&game) && s.mode != UIMODE_GAME_OVER) {
            s.mode = UIMODE_GAME_OVER;
            ui_set_status(&s, "Game over! Press q to exit.", false);
        }
    }

game_over:
    #undef RESET_DICE_FOR_ACTIVE
    #undef START_ACTIVE_TURN
    #undef START_NEXT_ROUND

    ui_teardown(&w);

    // Final score printout to terminal (fallback)
    calculate_final_scores(&game);
    printf("\n=== FINAL SCORES ===\n");
    for (int i = 0; i < game.num_players; i++) {
        if (g_bot_type != BOT_NONE && i == 1)
            printf("%s: %d points\n", bot_name(g_bot_type), game.players[i].score);
        else
            printf("Player %d: %d points\n", i + 1, game.players[i].score);
    }
    printf("Thanks for playing Clever Cubed!\n");

    return 0;
    //gcc -Wall -std=c11 -o clever_cubed_ui cv.c ui.c main_ui.c -lncurses
}
