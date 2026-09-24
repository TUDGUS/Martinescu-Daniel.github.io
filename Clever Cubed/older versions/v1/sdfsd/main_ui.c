#include "clever_cubed.h"
#include "ui.h"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>

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
    refresh();
    attroff(COLOR_PAIR(CP_DEFAULT));

    int ch;
    do {
        ch = getch();
    } while (ch < '1' || ch > '4');

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
    game.max_rounds = (num_players == 4) ? 4 : (num_players == 3) ? 5 : 6;
    roll_available_dice(&game);

    // -- Init UI --------------------------------------------------------------
    UIWindows w;
    UIState   s;
    ui_init(&w, &s);

    // Give round 1 bonus to all players before the first turn
    give_round_bonus(&game, game.current_round);

    s.mode = UIMODE_ACTIVE_ROLL;
    char buf[256];

    // How many players have been active so far this round
    int active_turns_this_round = 0;

    // Passive phase:
    // We cycle through each passive player one at a time (hot-seat).
    // passive_queue[i] = player index, passive_count = how many left to go.
    int passive_queue[4];
    int passive_count    = 0;  // passive players still to pick
    int passive_done     = 0;  // how many have already picked
    int current_passive  = -1; // player currently doing passive pick (-1 = none)
    int viewer           = 0;  // whose sheet is shown

    // -------------------------------------------------------------------------
    #define RESET_DICE_FOR_ACTIVE() do {         for (int _i = 0; _i < 6; _i++) {             game.dice[_i].is_available     = true;             game.dice[_i].on_silver_platter = false;             game.dice[_i].value            = 0;         }         game.chosen_dice_count = 0;         roll_available_dice(&game);     } while(0)

    #define START_ACTIVE_TURN() do {         RESET_DICE_FOR_ACTIVE();         current_passive = -1;         passive_count   = 0;         passive_done    = 0;         s.mode = UIMODE_ACTIVE_ROLL;         viewer = game.active_player_index;         snprintf(buf, sizeof(buf),             "Round %d | Player %d active -- pick a die.",             game.current_round, game.active_player_index + 1);         ui_set_status(&s, buf, false);     } while(0)

    #define START_NEXT_ROUND() do {         active_turns_this_round = 0;         game.current_round++;         if (check_game_end(&game)) {             s.mode = UIMODE_GAME_OVER;             ui_set_status(&s, "Game over! Press q to exit.", false);             goto game_over;         }         give_round_bonus(&game, game.current_round);         if (game.current_round == 4) {             s.pending_bonus_value = 0;             s.mode = UIMODE_BONUS_CHOOSE_COLOR;             snprintf(buf, sizeof(buf),                 "Round 4 bonus: WHITE ? for Player %d -- choose color: y t b r p",                 game.active_player_index + 1);             ui_set_status(&s, buf, false);         } else {             const char* _bn[] = {"","[&] reroll","+1 extra die","[AN] any-number","[?] white"};             int _r = game.current_round;             snprintf(buf, sizeof(buf), "Round %d! All players get: %s",                 _r, (_r >= 1 && _r <= 4) ? _bn[_r] : "");             ui_set_status(&s, buf, false);             START_ACTIVE_TURN();         }     } while(0)

    START_ACTIVE_TURN();

    // -- Main loop ------------------------------------------------------------
    while (true) {
        ui_render_all(&w, &s, &game, viewer);

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
                    "Round %d | Player %d: make your passive pick from the platter.",
                    game.current_round, current_passive + 1);
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
                    "Round %d | Player %d: make your passive pick from the platter.",
                    game.current_round, current_passive + 1);
                ui_set_status(&s, buf, false);
            } else {
                // All passive picks done -- advance to next active player
                active_turns_this_round++;
                game.active_player_index =
                    (game.active_player_index + 1) % game.num_players;

                if (active_turns_this_round >= game.num_players) {
                    START_NEXT_ROUND();
                } else {
                    START_ACTIVE_TURN();
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
        printf("Player %d: %d points\n", i + 1, game.players[i].score);
    }
    printf("Thanks for playing Clever Cubed!\n");

    return 0;
    //gcc -Wall -std=c11 -o clever_cubed_ui cv.c ui.c main_ui.c -lncurses
}
