#include "clever_cubed.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>

// ==================== BASIC INITIALIZATION FUNCTIONS ====================

void init_game(GameState* game, int num_players) {
    if (num_players<1 || num_players>4) {
        printf("Error: Invalid number of players. Using 2 players.\n");
        num_players=2;
    }
    
    game->current_round=1;
    game->max_rounds=6;
    game->num_players=num_players;
    game->active_player_index=0;
    game->chosen_dice_count=0;
    
    for (int i=0; i<6; i++) {
        game->dice[i].color=DIE_INDEX_TO_COLOR(i);
        game->dice[i].value=0;
        game->dice[i].on_silver_platter=false;
        game->dice[i].is_available=true;
    }
    
    for (int i=0; i<num_players; i++) {
        init_player_sheet(&game->players[i], i);
    }
}

void init_player_sheet(PlayerSheet* sheet, int id) {
    sheet->id=id;
    sheet->score=0;
    
    // Initialize Yellow Area
    for (int i=0; i<3; i++) {
        for (int j=0; j<6; j++) {
            sheet->yellow_area[i][j]=false;
        }
    }
    
    // Initialize Turquoise Area
    for (int i=0; i<5; i++) {
        for (int j=0; j<6; j++) {
            sheet->turquoise_area[i][j]=false;
        }
    }
    for (int i=0; i<6; i++) {
        sheet->turquoise_column_bonus[i]=false;
    }
    for (int i=0; i<5; i++) {
        sheet->turquoise_row_bonus[i]=false;
    }
    sheet->turquoise_marks_remaining=0;
    for (int i=0; i<5; i++) {
        sheet->turquoise_marked_this_turn[i]=false;
    }
    
    // Initialize Blue Area
    for (int i=0; i<13; i++) {
        sheet->blue_area[i]=0;
        sheet->blue_bonus[i]=false;
    }
    sheet->blue_area[6]=7;
    
    // Initialize Brown Area
    for (int i=0; i<12; i++) {
        sheet->brown_area[i]=false;
        sheet->brown_bonus[i]=false;
    }
    sheet->brown_last_marked_index=-1;
    int brown_required[]={1,5,3,4,2,6,4,5,2,1,6,3};
    for (int i=0; i<12; i++) {
        sheet->brown_required_value[i]=brown_required[i];
    }
    
    // Initialize Pink Area
    for (int i=0; i<12; i++) {
        sheet->pink_area[i]=0;
        sheet->pink_bonus_available[i]=true;
    }
    int pink_multipliers[]={0,1,2,2,1,2,2,1,3,2,2,3};
    for (int i=0; i<12; i++) {
        sheet->pink_multipliers[i]=pink_multipliers[i];
    }
    sheet->pink_next_empty_index=0;
    
    // Initialize Actions
    // DEBUG: start with one of each action for testing
    sheet->actions.reroll_charges=0;
    sheet->actions.any_number_charges=0;
    sheet->actions.extra_die_charges=0;
    sheet->actions.foxes_unlocked=0;
    sheet->actions.rerolls_used=0;
    sheet->actions.any_number_used=0;
    sheet->actions.extra_die_used=0;

    // Any-number bar: slots 0-3 require values 3,4,5,6; slots 4-6 are wildcards
    for (int i=0; i<ANY_NUMBER_BAR_SIZE; i++) {
        sheet->actions.any_number_bar_used[i]=false;
    }
}

// ==================== DICE ROLLING FUNCTIONS ====================

void roll_available_dice(GameState* game) {
    for (int i=0; i<6; i++) {
        if (game->dice[i].is_available && !game->dice[i].on_silver_platter) {
            game->dice[i].value=(rand()%6)+1;
        }
    }
}

void roll_specific_dice(GameState* game, int dice_indices[], int count) {
    for (int i=0; i<count; i++) {
        int idx=dice_indices[i];
        if (idx>=0 && idx<6 && game->dice[idx].is_available) {
            game->dice[idx].value=(rand()%6)+1;
            game->dice[idx].on_silver_platter=false;
        }
    }
}

void move_lower_dice_to_platter(GameState* game, int chosen_value) {
    for (int i=0; i<6; i++) {
        if (game->dice[i].is_available && 
            !game->dice[i].on_silver_platter && 
            game->dice[i].value<chosen_value) {
            game->dice[i].on_silver_platter=true;
        }
    }
}

// ==================== DICE PICKING MECHANISMS ====================

bool pick_die(GameState* game, int die_index, Die* picked_die) {
    if (die_index<0 || die_index>=6) {
        return false;
    }
    
    if (!game->dice[die_index].is_available) {
        return false;
    }
    
    if (game->chosen_dice_count>=3) {
        return false;
    }
    
    picked_die->color=game->dice[die_index].color;
    picked_die->value=game->dice[die_index].value;
    picked_die->on_silver_platter=game->dice[die_index].on_silver_platter;
    picked_die->is_available=true;
    
    game->dice[die_index].is_available=false;
    
    game->chosen_dice[game->chosen_dice_count]=*picked_die;
    game->chosen_dice_count++;
    
    return true;
}

void reset_chosen_dice(GameState* game) {
    game->chosen_dice_count=0;
}

// ==================== COLOR BONUS HANDLER ====================

void use_color_bonus(PlayerSheet* sheet, int bonus_type, int chosen_value) {
    if (chosen_value<1 || chosen_value>6) {
        printf("Invalid number! Bonus lost.\n");
        return;
    }
    
    switch(bonus_type) {
        case BONUS_PINK:
            mark_pink(sheet, chosen_value, false);
            break;
        case BONUS_TURQUOISE:
            setup_turquoise_marks(sheet, NULL, chosen_value, false);
            mark_turquoise(sheet, chosen_value, 0, false);
            break;
        case BONUS_BLUE:
            mark_blue(sheet, chosen_value, true);
            break;
        case BONUS_BROWN:
            for (int i=0; i<12; i++) {
                if (!sheet->brown_area[i]) {
                    mark_brown(sheet, chosen_value, i);
                    break;
                }
            }
            break;
        case BONUS_YELLOW:
            mark_yellow(sheet, chosen_value, 0, false);
            break;
    }
}

// ==================== ACTION FUNCTIONS ====================

// Returns the index of the first available any-number bar slot for the given value.
// Slots 0-3 require values 3,4,5,6 respectively. Slots 4-6 are wildcards (any value).
// Returns -1 if no valid slot is available.
int any_number_bar_slot_for_value(PlayerSheet* sheet, int value) {
    static const int slot_values[ANY_NUMBER_BAR_SIZE] = {3, 4, 5, 6, 0, 0, 0};
    // First try to find a specific slot matching this value
    for (int i=0; i<4; i++) {
        if (!sheet->actions.any_number_bar_used[i] && slot_values[i]==value) {
            return i;
        }
    }
    // Then fall back to a wildcard slot
    for (int i=4; i<ANY_NUMBER_BAR_SIZE; i++) {
        if (!sheet->actions.any_number_bar_used[i]) {
            return i;
        }
    }
    return -1;
}

// Give a round bonus to all players at the start of the given round.
void give_round_bonus(GameState* game, int round) {
    for (int i=0; i<game->num_players; i++) {
        PlayerSheet* sheet = &game->players[i];
        switch (round) {
            case 1: sheet->actions.reroll_charges++;     break;
            case 2: sheet->actions.extra_die_charges++;  break;
            case 3: sheet->actions.any_number_charges++; break;
            case 4: /* white ? bonus -- handled by UI asking player to choose */ break;
            default: break;
        }
    }
}

bool use_reroll_action(PlayerSheet* sheet, GameState* game) {
    if (sheet->actions.reroll_charges<=0) {
        return false;
    }
    
    int reroll_indices[6];
    int count=0;
    
    for (int i=0; i<6; i++) {
        if (game->dice[i].is_available && !game->dice[i].on_silver_platter) {
            reroll_indices[count++]=i;
        }
    }
    
    if (count==0) {
        return false;
    }
    
    roll_specific_dice(game, reroll_indices, count);
    sheet->actions.reroll_charges--;
    sheet->actions.rerolls_used++;

    // After 7 total rerolls used -> earn a fox charge
    if (sheet->actions.rerolls_used==7) {
        sheet->actions.foxes_unlocked++;
    }
    return true;
}

// Returns true and crosses the appropriate bar slot if a valid slot exists.
// The die value is NOT changed here -- caller must apply the value to the die.
bool use_any_number_action(PlayerSheet* sheet, Die* die, int desired_value) {
    if (sheet->actions.any_number_charges<=0) {
        return false;
    }
    if (desired_value<1 || desired_value>6) {
        return false;
    }
    int slot = any_number_bar_slot_for_value(sheet, desired_value);
    if (slot < 0) {
        return false; // no valid slot for this value
    }
    sheet->actions.any_number_bar_used[slot] = true;
    die->value = desired_value;
    sheet->actions.any_number_charges--;
    sheet->actions.any_number_used++;

    // After 7 any-number uses -> pink ? bonus (handled by caller checking this)
    // We signal completion by returning true; caller checks any_number_used==7
    return true;
}

bool use_extra_die_action(PlayerSheet* sheet, GameState* game, DieColor color) {
    if (sheet->actions.extra_die_charges<=0) {
        return false;
    }
    
    // Prefer a slot that is currently unavailable (already used this turn)
    for (int i=0; i<6; i++) {
        if (!game->dice[i].is_available) {
            game->dice[i].color=color;
            game->dice[i].value=(rand()%6)+1;
            game->dice[i].on_silver_platter=false;
            game->dice[i].is_available=true;
            sheet->actions.extra_die_charges--;
            sheet->actions.extra_die_used++;
            if (sheet->actions.extra_die_used==7) {
                // 7 extra dice used -> brown ? bonus (caller checks extra_die_used==7)
            }
            return true;
        }
    }
    
    // Fallback: reuse any available slot
    for (int i=0; i<6; i++) {
        if (game->dice[i].is_available) {
            game->dice[i].color=color;
            game->dice[i].value=(rand()%6)+1;
            game->dice[i].on_silver_platter=false;
            game->dice[i].is_available=true;
            sheet->actions.extra_die_charges--;
            sheet->actions.extra_die_used++;
            if (sheet->actions.extra_die_used==7) {
                // 7 extra dice used -> brown ? bonus (caller checks extra_die_used==7)
            }
            return true;
        }
    }
    
    return false;
}

// ==================== YELLOW AREA FUNCTIONS ====================

bool mark_yellow(PlayerSheet* sheet, int value, int roll_number, bool is_active_player) {
    if (value<1 || value>6) {
        return false;
    }
    
    int row;
    
    if (is_active_player) {
        if (roll_number<0 || roll_number>2) {
            return false;
        }
        row=roll_number;
    } else {
        if (value==5 || value==6) {
            row=0;
        } else if (value==3 || value==4) {
            row=1;
        } else if (value==1 || value==2) {
            row=2;
        } else {
            return false;
        }
    }
    
    if (sheet->yellow_area[row][value-1]) {
        return false;
    }
    
    sheet->yellow_area[row][value-1]=true;
    
    if ((row==0 || row==1) && sheet->yellow_area[0][value-1] && sheet->yellow_area[1][value-1]) {
        int bonus_col=value-1;
        switch(bonus_col) {
            case 0: sheet->actions.reroll_charges++; break;
            case 1: sheet->actions.any_number_charges++; break;
            case 2: 
                printf("Pink bonus! Choose number (1-6): "); 
                int p; 
                scanf("%d", &p); 
                use_color_bonus(sheet, BONUS_PINK, p); 
                break;
            case 3: sheet->actions.extra_die_charges++; break;
            case 4: 
                printf("Turquoise bonus! Choose number (1-6): "); 
                int t; 
                scanf("%d", &t); 
                use_color_bonus(sheet, BONUS_TURQUOISE, t); 
                break;
            case 5: sheet->actions.foxes_unlocked++; break;
        }
    }
    
    if ((row==1 || row==2) && sheet->yellow_area[1][value-1] && sheet->yellow_area[2][value-1]) {
        int bonus_col=value-1;
        switch(bonus_col) {
            case 0: sheet->actions.any_number_charges++; break;
            case 1: 
                printf("Turquoise bonus! Choose number (1-6): "); 
                int t; 
                scanf("%d", &t); 
                use_color_bonus(sheet, BONUS_TURQUOISE, t); 
                break;
            case 2: 
                printf("Blue bonus! Choose number (1-6): "); 
                int b; 
                scanf("%d", &b); 
                use_color_bonus(sheet, BONUS_BLUE, b); 
                break;
            case 3: 
                printf("Brown bonus! Choose number (1-6): "); 
                int br; 
                scanf("%d", &br); 
                use_color_bonus(sheet, BONUS_BROWN, br); 
                break;
            case 4: 
                printf("Yellow bonus! Choose number (1-6): "); 
                int y; 
                scanf("%d", &y); 
                use_color_bonus(sheet, BONUS_YELLOW, y); 
                break;
            case 5: sheet->actions.extra_die_charges++; break;
        }
    }
    
    return true;
}

int calculate_yellow_score(PlayerSheet* sheet) {
    int score=0;
    int row_points[]={0,2,6,12,20,30,42};
    
    for (int row=0; row<3; row++) {
        int marked_count=0;
        for (int col=0; col<6; col++) {
            if (sheet->yellow_area[row][col]) {
                marked_count++;
            }
        }
        score+=row_points[marked_count];
    }
    
    return score;
}

// ==================== TURQUOISE AREA FUNCTIONS ====================

int count_turquoise_dice_of_value(GameState* game, PlayerSheet* sheet, int value, bool is_active_player) {
    // Count ALL dice currently visible on the board with the same value.
    // No +1: the turquoise die itself is already on the board and gets counted.
    // For active: chosen_dice (die fields) + remaining available dice.
    // For passive: platter dice (turquoise die is among them).
    (void)sheet;
    int count = 0;
    if (game == NULL) return 1;

    if (is_active_player) {
        // Count dice already placed on die fields this turn
        for (int i = 0; i < game->chosen_dice_count; i++) {
            if (game->chosen_dice[i].value == value) count++;
        }
        // Count remaining available dice on the table (not yet picked, not on platter)
        for (int i = 0; i < 6; i++) {
            if (game->dice[i].is_available && !game->dice[i].on_silver_platter
                    && game->dice[i].value == value) count++;
        }
    } else {
        // Passive: count all platter dice with this value
        for (int i = 0; i < 6; i++) {
            if (game->dice[i].on_silver_platter && game->dice[i].value == value) count++;
        }
    }

    if (count < 1) count = 1; // always at least 1 for the die itself
    if (count > 3) count = 3; // rules cap at 3
    return count;
}

void reset_turquoise_turn(PlayerSheet* sheet) {
    sheet->turquoise_marks_remaining=0;
    for (int i=0; i<5; i++) sheet->turquoise_marked_this_turn[i]=false;
}

void setup_turquoise_marks(PlayerSheet* sheet, GameState* game, int value, bool is_active_player) {
    reset_turquoise_turn(sheet);
    int dice_count=count_turquoise_dice_of_value(game, sheet, value, is_active_player);
    sheet->turquoise_marks_remaining=dice_count;
    
    int column=value-1;
    int empty_cells=0;
    for (int row=0; row<5; row++) {
        if (!sheet->turquoise_area[row][column]) empty_cells++;
    }
    if (sheet->turquoise_marks_remaining>empty_cells) {
        sheet->turquoise_marks_remaining=empty_cells;
    }
}

void activate_turquoise_bonus(PlayerSheet* sheet, int index, bool is_column) {
    if (is_column) {
        switch(index) {
            case 0: printf("Brown bonus! Choose number: "); int br; scanf("%d", &br); use_color_bonus(sheet, BONUS_BROWN, br); break;
            case 1: printf("Pink bonus! Choose number: "); int p; scanf("%d", &p); use_color_bonus(sheet, BONUS_PINK, p); break;
            case 2: printf("Yellow bonus! Choose number: "); int y; scanf("%d", &y); use_color_bonus(sheet, BONUS_YELLOW, y); break;
            case 3: sheet->actions.any_number_charges++; break;
            case 4: printf("Blue bonus! Choose number: "); int b; scanf("%d", &b); use_color_bonus(sheet, BONUS_BLUE, b); break;
            case 5: sheet->actions.reroll_charges++; break;
        }
    } else {
        switch(index) {
            case 0: sheet->actions.foxes_unlocked++; break;
            case 1: sheet->actions.extra_die_charges++; break;
            case 2: printf("Brown bonus! Choose number: "); int br; scanf("%d", &br); use_color_bonus(sheet, BONUS_BROWN, br); break;
            case 3: printf("Turquoise bonus! Choose number: "); int t; scanf("%d", &t); use_color_bonus(sheet, BONUS_TURQUOISE, t); break;
            case 4: break;
        }
    }
}

bool mark_turquoise(PlayerSheet* sheet, int value, int row, bool is_active_player) {
    if (value<1 || value>6) return false;
    if (row<0 || row>4) return false;
    
    int column=value-1;
    
    if (sheet->turquoise_marks_remaining<=0) return false;
    if (sheet->turquoise_marked_this_turn[row]) return false;
    if (sheet->turquoise_area[row][column]) return false;
    
    sheet->turquoise_area[row][column]=true;
    sheet->turquoise_marks_remaining--;
    sheet->turquoise_marked_this_turn[row]=true;
    
    bool column_complete=true;
    for (int r=0; r<5; r++) {
        if (!sheet->turquoise_area[r][column]) { column_complete=false; break; }
    }
    if (column_complete && !sheet->turquoise_column_bonus[column]) {
        sheet->turquoise_column_bonus[column]=true;
        activate_turquoise_bonus(sheet, column, true);
    }
    
    bool row_complete=true;
    for (int c=0; c<6; c++) {
        if (!sheet->turquoise_area[row][c]) { row_complete=false; break; }
    }
    if (row_complete && !sheet->turquoise_row_bonus[row]) {
        sheet->turquoise_row_bonus[row]=true;
        activate_turquoise_bonus(sheet, row, false);
    }
    
    return true;
}

void handle_turquoise_pick(GameState* game, PlayerSheet* sheet, int die_index, bool is_active_player) {
    Die chosen_die = game->dice[die_index];
    int value = chosen_die.value;
    
    setup_turquoise_marks(sheet, game, value, is_active_player);

    for (int i=0;i<sheet->turquoise_marks_remaining;i++) {
        printf("Choose row to mark (0-4): ");
        int row;
        scanf("%d", &row);
        mark_turquoise(sheet,value,row,is_active_player);
    }
}

int calculate_turquoise_score(PlayerSheet* sheet) {
    int score=0;
    int row_points[]={0,1,3,6,10,15,21};
    
    for (int row=0; row<5; row++) {
        int marked_count=0;
        for (int col=0; col<6; col++) {
            if (sheet->turquoise_area[row][col]) {
                marked_count++;
            }
        }
        score+=row_points[marked_count];
    }
    
    return score;
}

// ==================== BLUE AREA FUNCTIONS ====================

void check_and_activate_blue_bonus(PlayerSheet* sheet, int position) {
    if (sheet->blue_bonus[position]) return;
    
    switch(position) {
        case 0: sheet->actions.extra_die_charges++; break;
        case 1: printf("Pink bonus! Choose number: "); int p; scanf("%d", &p); use_color_bonus(sheet, BONUS_PINK, p); break;
        case 3: printf("Yellow bonus! Choose number: "); int y; scanf("%d", &y); use_color_bonus(sheet, BONUS_YELLOW, y); break;
        case 4: sheet->actions.any_number_charges++; break;
        case 8: sheet->actions.reroll_charges++; break;
        case 9: printf("Brown bonus! Choose number: "); int br; scanf("%d", &br); use_color_bonus(sheet, BONUS_BROWN, br); break;
        case 11: printf("Turquoise bonus! Choose number: "); int t; scanf("%d", &t); use_color_bonus(sheet, BONUS_TURQUOISE, t); break;
        case 12: sheet->actions.foxes_unlocked++; break;
        default: return;
    }
    sheet->blue_bonus[position]=true;
}

bool mark_blue_seven(PlayerSheet* sheet) {
    printf("Place 7 on (1-left, 2-right): ");
    int choice;
    scanf("%d", &choice);
    
    if (choice==1) {
        for (int i=5; i>=0; i--) {
            if (sheet->blue_area[i]==0) {
                sheet->blue_area[i]=7;
                check_and_activate_blue_bonus(sheet, i);
                return true;
            }
        }
        printf("No empty spots on left side!\n");
        return false;
    } else if (choice==2) {
        for (int i=7; i<=12; i++) {
            if (sheet->blue_area[i]==0) {
                sheet->blue_area[i]=7;
                check_and_activate_blue_bonus(sheet, i);
                return true;
            }
        }
        printf("No empty spots on right side!\n");
        return false;
    }
    
    return false;
}

bool mark_blue(PlayerSheet* sheet, int total_value, bool is_active_player) {
    if (total_value<2 || total_value>12) {
        return false;
    }
    
    if (total_value==7) {
        return mark_blue_seven(sheet);
    }
    
    int left_empty=-1;
    for (int i=5; i>=0; i--) {
        if (sheet->blue_area[i]==0) {
            left_empty=i;
            break;
        }
    }
    
    if (left_empty!=-1) {
        int right_neighbor=sheet->blue_area[left_empty+1];
        if (right_neighbor==total_value+1) {
            sheet->blue_area[left_empty]=total_value;
            check_and_activate_blue_bonus(sheet, left_empty);
            return true;
        }
    }
    
    int right_empty=-1;
    for (int i=7; i<=12; i++) {
        if (sheet->blue_area[i]==0) {
            right_empty=i;
            break;
        }
    }
    
    if (right_empty!=-1) {
        int left_neighbor=sheet->blue_area[right_empty-1];
        if (left_neighbor==total_value-1) {
            sheet->blue_area[right_empty]=total_value;
            check_and_activate_blue_bonus(sheet, right_empty);
            return true;
        }
    }
    
    return false;
}

int get_blue_outermost_points(int position) {
    int distance=abs(position-6);
    switch(distance) {
        case 1: return 3;
        case 2: return 6;
        case 3: return 9;
        case 4: return 13;
        case 5: return 17;
        case 6: return 22;
        default: return 0;
    }
}

int calculate_blue_score(PlayerSheet* sheet) {
    int score=0;
    int leftmost=-1, rightmost=-1;
    
    for (int i=0; i<=12; i++) {
        if (sheet->blue_area[i]!=0) {
            if (leftmost==-1) leftmost=i;
            rightmost=i;
        }
    }
    
    if (leftmost!=-1 && leftmost!=6) score+=get_blue_outermost_points(leftmost);
    if (rightmost!=-1 && rightmost!=6) score+=get_blue_outermost_points(rightmost);
    
    for (int i=0; i<=12; i++) {
        if (sheet->blue_area[i]!=0 && (sheet->blue_area[i]<=4 || sheet->blue_area[i]>=10)) {
            score+=4;
        }
    }
    return score;
}

// ==================== BROWN AREA FUNCTIONS ====================

void activate_brown_bonus(PlayerSheet* sheet, int bonus_index) {
    if (sheet->brown_bonus[bonus_index]) return;
    
    switch(bonus_index) {
        case 0: sheet->actions.any_number_charges++; break;
        case 1: printf("Pink bonus! Choose number: "); int p; scanf("%d", &p); use_color_bonus(sheet, BONUS_PINK, p); break;
        case 2: break;
        case 3: sheet->actions.reroll_charges++; break;
        case 4: printf("Turquoise bonus! Choose number: "); int t; scanf("%d", &t); use_color_bonus(sheet, BONUS_TURQUOISE, t); break;
        case 5: break;
        case 6: sheet->actions.extra_die_charges++; break;
        case 7: printf("Blue bonus! Choose number: "); int b; scanf("%d", &b); use_color_bonus(sheet, BONUS_BLUE, b); break;
        case 8: break;
        case 9: printf("Yellow bonus! Choose number: "); int y; scanf("%d", &y); use_color_bonus(sheet, BONUS_YELLOW, y); break;
        case 10: sheet->actions.foxes_unlocked++; break;
    }
    sheet->brown_bonus[bonus_index]=true;
}

bool mark_brown(PlayerSheet* sheet, int value, int target_index) {
    if (value<1 || value>6) return false;
    if (target_index<0 || target_index>11) return false;
    if (sheet->brown_area[target_index]) return false;
    if (sheet->brown_required_value[target_index]!=value) return false;
    if (target_index<sheet->brown_last_marked_index) return false;
    
    sheet->brown_area[target_index]=true;
    
    if (target_index>0 && sheet->brown_area[target_index-1]) {
        activate_brown_bonus(sheet, target_index-1);
    }
    
    if (target_index>sheet->brown_last_marked_index) {
        sheet->brown_last_marked_index=target_index;
    }
    
    return true;
}

int calculate_brown_score(PlayerSheet* sheet) {
    int marked_count=0;
    for (int i=0; i<12; i++) {
        if (sheet->brown_area[i]) marked_count++;
    }
    int brown_scores[]={0,2,5,9,14,20,27,35,44,54,65,77,90};
    return brown_scores[marked_count];
}

// ==================== PINK AREA FUNCTIONS ====================

void activate_pink_bonus(PlayerSheet* sheet, int position) {
    if (!sheet->pink_bonus_available[position]) return;
    
    switch(position) {
        case 0: break;
        case 1: sheet->actions.reroll_charges++; break;
        case 2: printf("Blue bonus! Choose number: "); int b; scanf("%d", &b); use_color_bonus(sheet, BONUS_BLUE, b); break;
        case 3: sheet->actions.extra_die_charges++; break;
        case 4: sheet->actions.any_number_charges++; break;
        case 5: printf("Yellow bonus! Choose number: "); int y; scanf("%d", &y); use_color_bonus(sheet, BONUS_YELLOW, y); break;
        case 6: printf("Brown bonus! Choose number: "); int br; scanf("%d", &br); use_color_bonus(sheet, BONUS_BROWN, br); break;
        case 7: sheet->actions.reroll_charges++; break;
        case 8: sheet->actions.foxes_unlocked++; break;
        case 9: printf("Blue bonus! Choose number: "); int b2; scanf("%d", &b2); use_color_bonus(sheet, BONUS_BLUE, b2); break;
        case 10: printf("Turquoise bonus! Choose number: "); int t; scanf("%d", &t); use_color_bonus(sheet, BONUS_TURQUOISE, t); break;
        case 11:
            printf("White bonus! Choose number: "); int w; scanf("%d", &w);
            printf("Use on (1-Yellow,2-Turquoise,3-Blue,4-Brown,5-Pink): ");
            int area; scanf("%d", &area);
            switch(area) {
                case 1: mark_yellow(sheet, w, 0, false); break;
                case 2: setup_turquoise_marks(sheet, NULL, w, false); mark_turquoise(sheet, w, 0, false); break;
                case 3: mark_blue(sheet, w, true); break;
                case 4: for (int i=0; i<12; i++) { if (!sheet->brown_area[i]) { mark_brown(sheet, w, i); break; } } break;
                case 5: mark_pink(sheet, w, false); break;
            }
            break;
    }
    sheet->pink_bonus_available[position]=false;
}

bool mark_pink(PlayerSheet* sheet, int value, bool take_bonus) {
    if (value<1 || value>6) return false;
    if (sheet->pink_next_empty_index>=12) return false;
    
    int position=sheet->pink_next_empty_index;
    int entered_value;
    
    if (position==0) take_bonus=true;
    
    if (take_bonus) {
        entered_value=(value+1)/2;
        if (sheet->pink_bonus_available[position]) {
            activate_pink_bonus(sheet, position);
        }
    } else {
        if (sheet->pink_multipliers[position]==0) {
            entered_value=(value+1)/2;
            if (sheet->pink_bonus_available[position]) {
                activate_pink_bonus(sheet, position);
            }
        } else {
            entered_value=value*sheet->pink_multipliers[position];
            sheet->pink_bonus_available[position]=false;
        }
    }
    
    sheet->pink_area[position]=entered_value;
    sheet->pink_next_empty_index++;
    return true;
}

int calculate_pink_score(PlayerSheet* sheet) {
    int score=0;
    for (int i=0; i<sheet->pink_next_empty_index; i++) {
        score+=sheet->pink_area[i];
    }
    return score;
}

// ==================== TURN FLOW FUNCTIONS ====================

void start_round(GameState* game) {
    printf("=== Starting Round %d ===\n", game->current_round);
    for (int i=0; i<6; i++) {
        game->dice[i].is_available=true;
        game->dice[i].on_silver_platter=false;
        game->dice[i].value=0;
    }
    game->chosen_dice_count=0;
    roll_available_dice(game);
}

void play_active_turn(GameState* game, int player_index) {
    printf("\n--- Player %d's Active Turn ---\n", player_index+1);
    for (int i=0; i<6; i++) {
        if (game->dice[i].is_available) {
            printf("Die %d: Color %d, Value %d%s\n", i, game->dice[i].color, game->dice[i].value,
                   game->dice[i].on_silver_platter ? " (Silver Platter)" : "");
        }
    }
}

void play_passive_turn(GameState* game) {
    for (int i=0; i<game->num_players; i++) {
        if (i!=game->active_player_index) {
            printf("\n--- Player %d's Passive Turn ---\n", i+1);
        }
    }
}

bool check_game_end(GameState* game) {
    if (game->current_round>game->max_rounds) return true;
    for (int i=0; i<game->num_players; i++) {
        if (game->players[i].actions.foxes_unlocked>=6) return true;
    }
    return false;
}

void calculate_final_scores(GameState* game) {
    for (int i=0; i<game->num_players; i++) {
        PlayerSheet* sheet=&game->players[i];
        int yellow    = calculate_yellow_score(sheet);
        int turquoise = calculate_turquoise_score(sheet);
        int blue      = calculate_blue_score(sheet);
        int brown     = calculate_brown_score(sheet);
        int pink      = calculate_pink_score(sheet);

        // Foxes: each fox scores points equal to the lowest area score
        int min_area = yellow;
        if (turquoise < min_area) min_area = turquoise;
        if (blue      < min_area) min_area = blue;
        if (brown     < min_area) min_area = brown;
        if (pink      < min_area) min_area = pink;
        int fox_score = sheet->actions.foxes_unlocked * min_area;

        sheet->score = yellow + turquoise + blue + brown + pink + fox_score;
    }
}

