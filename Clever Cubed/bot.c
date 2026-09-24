#include "bot.h"
#include "clever_cubed.h"
#include <string.h>
#include <limits.h>
#include <math.h>

int g_iroh_move_count = 0;


// ─── Iroh area helpers ────────────────────────────────────────────────────────
static int iroh_area_max(int area) {
    switch (area) {
        case BONUS_YELLOW:    return IROH_YELLOW_MAX;
        case BONUS_TURQUOISE: return IROH_TURQUOISE_MAX;
        case BONUS_BLUE:      return IROH_BLUE_MAX;
        case BONUS_BROWN:     return IROH_BROWN_MAX;
        case BONUS_PINK:      return IROH_PINK_MAX;
        default:              return 100;
    }
}
static int iroh_current_score(PlayerSheet* sheet, int area) {
    switch (area) {
        case BONUS_YELLOW:    return calculate_yellow_score(sheet);
        case BONUS_TURQUOISE: return calculate_turquoise_score(sheet);
        case BONUS_BLUE:      return calculate_blue_score(sheet);
        case BONUS_BROWN:     return calculate_brown_score(sheet);
        case BONUS_PINK:      return calculate_pink_score(sheet);
        default:              return 0;
    }
}
static int iroh_fox_value(PlayerSheet* sheet) {
    int scores[5] = {
        calculate_yellow_score(sheet), calculate_turquoise_score(sheet),
        calculate_blue_score(sheet),   calculate_brown_score(sheet),
        calculate_pink_score(sheet)
    };
    int lowest = scores[0];
    for (int i = 1; i < 5; i++) if (scores[i] < lowest) lowest = scores[i];
    int base = 48 + lowest - g_iroh_move_count * 2;
    return (base < lowest) ? lowest : base;
}
static int iroh_action_value(PlayerSheet* before, PlayerSheet* after) {
    int val = 0;
    val += (after->actions.reroll_charges     - before->actions.reroll_charges)     * 10;
    val += (after->actions.extra_die_charges  - before->actions.extra_die_charges)  * 10;
    val += (after->actions.any_number_charges - before->actions.any_number_charges) * 10;
    val += (after->actions.foxes_unlocked     - before->actions.foxes_unlocked)     * iroh_fox_value(before);
    return val;
}
static double iroh_adjusted_score(PlayerSheet* before, PlayerSheet* after, int area, int pts) {
    int raw = pts + iroh_action_value(before, after);
    int mx  = iroh_area_max(area);
    int cur = iroh_current_score(before, area);
    int rem = mx - cur; if (rem < 0) rem = 0;
    return (double)raw * (double)rem / (double)mx;
}

// ─────────────────────────────────────────────────────────────────────────────
// SIMULATION HELPERS
// Score a sheet without modifying it

// Total action charges on a sheet
static int total_actions(PlayerSheet* s) {
    return s->actions.reroll_charges
         + s->actions.extra_die_charges
         + s->actions.any_number_charges
         + s->actions.foxes_unlocked;
}

// ─────────────────────────────────────────────────────────────────────────────
// EVALUATE: what score gain does placing die_value in each area give?
// Returns the gain (can be 0 if placement fails).
// Also returns whether it grants at least one new action charge (for Midas).

typedef struct {
    int    score_gain;
    bool   grants_action;
    bool   valid;
    double iroh_score;
    int    area;
    int    param;
    bool   take_bonus;
    bool   go_left;
} Eval;

static Eval eval_yellow(PlayerSheet* sheet, int value, int roll_num) {
    Eval e = {0};
    PlayerSheet copy = *sheet;
    int before = calculate_yellow_score(&copy);
    int ab = total_actions(&copy);
    bool ok = mark_yellow(&copy, value, roll_num, true);
    if (!ok) return e;
    e.valid = true;
    e.score_gain = calculate_yellow_score(&copy) - before;
    e.grants_action = (total_actions(&copy) > ab);
    e.iroh_score = iroh_adjusted_score(sheet, &copy, BONUS_YELLOW, e.score_gain);
    e.area = BONUS_YELLOW; e.param = roll_num;
    return e;
}

static Eval eval_turquoise(PlayerSheet* sheet, GameState* game, int value) {
    // Try each row, take the one with most marks remaining (greedy: use all marks)
    Eval best = {0};
    int before_score = calculate_turquoise_score(sheet);
    int before_actions = total_actions(sheet);

    // Find how many marks we'd get
    PlayerSheet tmp_setup = *sheet;
    setup_turquoise_marks(&tmp_setup, game, value, true);
    int marks = tmp_setup.turquoise_marks_remaining;
    if (marks <= 0) return best;

    // Simulate marking 'marks' rows greedily (top to bottom)
    PlayerSheet copy = *sheet;
    setup_turquoise_marks(&copy, game, value, true);
    int marked = 0;
    for (int row = 0; row < 5 && marked < marks; row++) {
        if (mark_turquoise(&copy, value, row, true)) marked++;
    }
    if (marked == 0) return best;

    best.valid = true;
    best.score_gain = calculate_turquoise_score(&copy) - before_score;
    best.grants_action = (total_actions(&copy) > before_actions);
    best.iroh_score = iroh_adjusted_score(sheet, &copy, BONUS_TURQUOISE, best.score_gain);
    best.area = BONUS_TURQUOISE; best.param = 0;
    return best;
}

static Eval eval_blue(PlayerSheet* sheet, int die_value, int white_value) {
    Eval e = {0};
    int total = die_value + white_value;
    PlayerSheet copy = *sheet;
    int before = calculate_blue_score(&copy);
    int ab = total_actions(&copy);

    if (total == 7) {
        // Try left first, then right
        PlayerSheet cl = *sheet;
        bool placed = false;
        for (int i = 5; i >= 0; i--) {
            if (cl.blue_area[i] == 0) { cl.blue_area[i] = 7; check_and_activate_blue_bonus(&cl, i); placed = true; e.go_left = true; break; }
        }
        if (!placed) {
            for (int i = 7; i <= 12; i++) {
                if (cl.blue_area[i] == 0) { cl.blue_area[i] = 7; check_and_activate_blue_bonus(&cl, i); placed = true; e.go_left = false; break; }
            }
        }
        if (!placed) return e;
        e.valid = true;
        e.score_gain = calculate_blue_score(&cl) - before;
        e.grants_action = (total_actions(&cl) > ab);
        e.iroh_score = iroh_adjusted_score(sheet, &cl, BONUS_BLUE, e.score_gain);
    } else {
        bool ok = mark_blue(&copy, total, true);
        if (!ok) return e;
        e.valid = true;
        e.score_gain = calculate_blue_score(&copy) - before;
        e.grants_action = (total_actions(&copy) > ab);
        e.iroh_score = iroh_adjusted_score(sheet, &copy, BONUS_BLUE, e.score_gain);
    }
    e.area = BONUS_BLUE;
    return e;
}

static Eval eval_brown(PlayerSheet* sheet, int value) {
    Eval best = {0};
    int before = calculate_brown_score(sheet);
    int ab = total_actions(sheet);
    // Try each valid index
    int start = (sheet->brown_last_marked_index >= 0) ? sheet->brown_last_marked_index + 1 : 0;
    for (int idx = start; idx < 12; idx++) {
        PlayerSheet copy = *sheet;
        if (mark_brown(&copy, value, idx)) {
            int gain = calculate_brown_score(&copy) - before;
            bool grants = (total_actions(&copy) > ab);
            // Iroh: penalise skipped slots (-8 per skip, bonuses lost forever)
            int skips = idx - ((sheet->brown_last_marked_index >= 0)
                               ? sheet->brown_last_marked_index + 1 : 0);
            double raw_iroh = iroh_adjusted_score(sheet, &copy, BONUS_BROWN, gain);
            double adjusted_iroh = raw_iroh - skips * 8.0;
            if (!best.valid || gain > best.score_gain ||
                (!best.grants_action && grants)) {
                best.valid = true;
                best.score_gain = gain;
                best.grants_action = grants;
                best.iroh_score = adjusted_iroh;
                best.area = BONUS_BROWN; best.param = idx;
            }
            break;
        }
    }
    return best;
}

static Eval eval_pink(PlayerSheet* sheet, int value) {
    Eval best = {0};
    if (sheet->pink_next_empty_index >= 12) return best;
    int before = calculate_pink_score(sheet);
    int ab = total_actions(sheet);

    // Try take_bonus=false (points, multiplied)
    if (sheet->pink_next_empty_index > 0) {
        PlayerSheet cp = *sheet;
        if (mark_pink(&cp, value, false)) {
            int gain = calculate_pink_score(&cp) - before;
            bool grants = (total_actions(&cp) > ab);
            best.valid = true; best.score_gain = gain; best.grants_action = grants;
            best.iroh_score = iroh_adjusted_score(sheet, &cp, BONUS_PINK, gain);
            best.area = BONUS_PINK; best.take_bonus = false;
        }
        PlayerSheet cb = *sheet;
        if (mark_pink(&cb, value, true)) {
            bool grants = (total_actions(&cb) > ab);
            int gain = calculate_pink_score(&cb) - before;
            if (!best.valid || (grants && !best.grants_action)) {
                best.valid = true; best.score_gain = gain; best.grants_action = grants;
                best.iroh_score = iroh_adjusted_score(sheet, &cb, BONUS_PINK, gain);
                best.area = BONUS_PINK; best.take_bonus = true;
            }
        }
    } else {
        // First slot: always take bonus
        PlayerSheet cp = *sheet;
        if (mark_pink(&cp, value, true)) {
            best.valid = true;
            best.score_gain = calculate_pink_score(&cp) - before;
            best.grants_action = (total_actions(&cp) > ab);
            best.iroh_score = iroh_adjusted_score(sheet, &cp, BONUS_PINK, best.score_gain);
            best.area = BONUS_PINK; best.take_bonus = true;
        }
    }
    return best;
}

// ─────────────────────────────────────────────────────────────────────────────
// APPLY: actually perform a bot's chosen mark on the real sheet
static void apply_eval(Eval* e, int die_value, PlayerSheet* sheet, GameState* game, int roll_num) {
    if (!e->valid) return;
    switch (e->area) {
        case BONUS_YELLOW:
            mark_yellow(sheet, die_value, roll_num, true);
            break;
        case BONUS_TURQUOISE: {
            setup_turquoise_marks(sheet, game, die_value, true);
            int marks = sheet->turquoise_marks_remaining;
            for (int row = 0; row < 5 && marks > 0; row++) {
                if (mark_turquoise(sheet, die_value, row, true)) marks--;
            }
            break;
        }
        case BONUS_BLUE: {
            int white_val = 0;
            for (int i = 0; i < 6; i++)
                if (game->dice[i].color == DIE_COLOR_WHITE) white_val = game->dice[i].value;
            int total = die_value + white_val;
            if (total == 7) {
                if (e->go_left) {
                    for (int i = 5; i >= 0; i--)
                        if (sheet->blue_area[i] == 0) { sheet->blue_area[i] = 7; check_and_activate_blue_bonus(sheet, i); break; }
                } else {
                    for (int i = 7; i <= 12; i++)
                        if (sheet->blue_area[i] == 0) { sheet->blue_area[i] = 7; check_and_activate_blue_bonus(sheet, i); break; }
                }
            } else {
                mark_blue(sheet, total, true);
            }
            break;
        }
        case BONUS_BROWN:
            mark_brown(sheet, die_value, e->param);
            break;
        case BONUS_PINK:
            mark_pink(sheet, die_value, e->take_bonus);
            break;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// BEST EVAL: given a die, find the best placement for Gollum (max points)
// or Midas (action first, then max points)
static Eval best_eval_for_die(BotType bot, GameState* game, PlayerSheet* sheet,
                               int die_index, int roll_num) {
    Die* d = &game->dice[die_index];
    int value = d->value;
    DieColor color = d->color;

    // Find white die value for blue calculation
    int white_val = 0;
    for (int i = 0; i < 6; i++)
        if (game->dice[i].color == DIE_COLOR_WHITE) white_val = game->dice[i].value;

    Eval candidates[6] = {0};
    int nc = 0;

    if (color == DIE_COLOR_WHITE) {
        // White die: evaluate as each color
        for (int c = BONUS_YELLOW; c <= BONUS_PINK; c++) {
            Eval e = {0};
            switch (c) {
                case BONUS_YELLOW:    e = eval_yellow(sheet, value, roll_num); break;
                case BONUS_TURQUOISE: e = eval_turquoise(sheet, game, value); break;
                case BONUS_BLUE:      e = eval_blue(sheet, value, white_val); break;
                case BONUS_BROWN:     e = eval_brown(sheet, value); break;
                case BONUS_PINK:      e = eval_pink(sheet, value); break;
            }
            if (e.valid) candidates[nc++] = e;
        }
    } else {
        Eval e = {0};
        switch (color) {
            case DIE_COLOR_YELLOW:    e = eval_yellow(sheet, value, roll_num); break;
            case DIE_COLOR_TURQUOISE: e = eval_turquoise(sheet, game, value); break;
            case DIE_COLOR_BLUE:      e = eval_blue(sheet, value, white_val); break;
            case DIE_COLOR_BROWN:     e = eval_brown(sheet, value); break;
            case DIE_COLOR_PINK:      e = eval_pink(sheet, value); break;
            default: break;
        }
        if (e.valid) candidates[nc++] = e;
    }

    if (nc == 0) { Eval none = {0}; return none; }

    if (bot == BOT_IROH) {
        Eval best = {0}; double bs = -1e9;
        for (int i = 0; i < nc; i++)
            if (candidates[i].iroh_score > bs) { best = candidates[i]; bs = candidates[i].iroh_score; }
        return best;
    } else if (bot == BOT_MIDAS) {
        Eval ba = {0}; int bas = -1; Eval bp = {0}; int bps = -1;
        for (int i = 0; i < nc; i++) {
            if (candidates[i].grants_action && candidates[i].score_gain > bas) { ba = candidates[i]; bas = candidates[i].score_gain; }
            if (candidates[i].score_gain > bps) { bp = candidates[i]; bps = candidates[i].score_gain; }
        }
        return (bas >= 0) ? ba : bp;
    } else {
        Eval best = {0}; int bs = -1;
        for (int i = 0; i < nc; i++)
            if (candidates[i].score_gain > bs) { best = candidates[i]; bs = candidates[i].score_gain; }
        return best;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// PUBLIC: BOT ACTIVE TURN
// Picks the best die+placement, applies it, returns true if a move was made.
// ─── Plan-based active turn ──────────────────────────────────────────────────
typedef struct { int die_index; int white_as; Eval eval; } PlanSlot;

static bool plan_better(BotType bot, int ap, int aa, double ai, int bp, int ba, double bi) {
    if (bot == BOT_IROH)  return bi > ai;
    if (bot == BOT_MIDAS) return (ba != aa) ? ba > aa : bp > ap;
    return bp > ap;
}

static Eval eval_slot(BotType bot, GameState* game, PlayerSheet* sheet,
                      int die_index, int as_color, int roll_num) {
    int wv = 0;
    for (int i = 0; i < 6; i++) if (game->dice[i].color == DIE_COLOR_WHITE) wv = game->dice[i].value;
    if (die_index >= 0) return best_eval_for_die(bot, game, sheet, die_index, roll_num);
    Eval e = {0};
    switch (as_color) {
        case BONUS_YELLOW:    e = eval_yellow(sheet, wv, roll_num); break;
        case BONUS_TURQUOISE: e = eval_turquoise(sheet, game, wv); break;
        case BONUS_BLUE:      e = eval_blue(sheet, wv, 0); break;
        case BONUS_BROWN:     e = eval_brown(sheet, wv); break;
        case BONUS_PINK:      e = eval_pink(sheet, wv); break;
    }
    return e;
}

static int find_best_plan(BotType bot, GameState* game, PlayerSheet* sheet, int roll_num,
                           PlanSlot plan_out[3]) {
    typedef struct { int di; int ac; int val; } Cand;
    Cand cands[32]; int nc = 0;
    int wv = 0;
    for (int i = 0; i < 6; i++) {
        if (!game->dice[i].is_available || game->dice[i].on_silver_platter) continue;
        if (game->dice[i].color == DIE_COLOR_WHITE) wv = game->dice[i].value;
        else cands[nc++] = (Cand){i, 0, game->dice[i].value};
    }
    // Add white die as each color
    if (wv > 0) for (int c = BONUS_YELLOW; c <= BONUS_PINK; c++) cands[nc++] = (Cand){-1, c, wv};
    if (nc == 0) return 0;

    int bn = 0; PlanSlot bp[3] = {0};
    int bpts = -1, bacts = -1; double biroh = -1e9; bool found = false;

    // Try all 1-slot combos
    for (int i = 0; i < nc; i++) {
        int combo[1] = {i};
        // Sort not needed for 1
        PlayerSheet sim = *sheet;
        Eval e = eval_slot(bot, game, &sim, cands[i].di, cands[i].ac, roll_num);
        if (!e.valid) continue;
        int av = (cands[i].di >= 0) ? game->dice[cands[i].di].value : wv;
        apply_eval(&e, av, &sim, game, roll_num);
        int tp = e.score_gain, ta = e.grants_action ? 1 : 0; double ti = e.iroh_score;
        PlanSlot slots[1] = {{cands[i].di, cands[i].ac, e}};
        if (!found || plan_better(bot, bpts, bacts, biroh, tp, ta, ti)) {
            found = true; bpts = tp; bacts = ta; biroh = ti; bn = 1; bp[0] = slots[0];
        }
        (void)combo;
    }

    // Try all 2-slot combos
    for (int i = 0; i < nc; i++) {
    for (int j = 0; j < nc; j++) {
        if (j == i) continue;
        // At most one white-as-color slot
        int wc = (cands[i].di < 0 ? 1 : 0) + (cands[j].di < 0 ? 1 : 0);
        if (wc > 1) continue;
        // Sort by value ascending
        int ci = i, cj = j;
        if (cands[ci].val > cands[cj].val) { int t = ci; ci = cj; cj = t; }
        PlayerSheet sim = *sheet;
        bool valid = true; int tp = 0, ta = 0; double ti = 0.0;
        PlanSlot slots[2] = {0};
        int idx[2] = {ci, cj};
        for (int s = 0; s < 2; s++) {
            Cand* c = &cands[idx[s]];
            Eval e = eval_slot(bot, game, &sim, c->di, c->ac, roll_num + s);
            if (!e.valid) { valid = false; break; }
            int av = (c->di >= 0) ? game->dice[c->di].value : wv;
            apply_eval(&e, av, &sim, game, roll_num + s);
            tp += e.score_gain; ta += e.grants_action ? 1 : 0; ti += e.iroh_score;
            slots[s] = (PlanSlot){c->di, c->ac, e};
        }
        if (valid && (!found || plan_better(bot, bpts, bacts, biroh, tp, ta, ti))) {
            found = true; bpts = tp; bacts = ta; biroh = ti; bn = 2;
            bp[0] = slots[0]; bp[1] = slots[1];
        }
    }}

    // Try all 3-slot combos
    for (int i = 0; i < nc; i++) {
    for (int j = 0; j < nc; j++) { if (j == i) continue;
    for (int k = 0; k < nc; k++) {
        if (k == i || k == j) continue;
        int wc = (cands[i].di<0?1:0)+(cands[j].di<0?1:0)+(cands[k].di<0?1:0);
        if (wc > 1) continue;
        // Sort by value ascending
        int idx[3] = {i, j, k};
        for (int a = 0; a < 2; a++)
            for (int b2 = a+1; b2 < 3; b2++)
                if (cands[idx[a]].val > cands[idx[b2]].val) { int t=idx[a]; idx[a]=idx[b2]; idx[b2]=t; }
        PlayerSheet sim = *sheet;
        bool valid = true; int tp = 0, ta = 0; double ti = 0.0;
        PlanSlot slots[3] = {0};
        for (int s = 0; s < 3; s++) {
            Cand* c = &cands[idx[s]];
            Eval e = eval_slot(bot, game, &sim, c->di, c->ac, roll_num + s);
            if (!e.valid) { valid = false; break; }
            int av = (c->di >= 0) ? game->dice[c->di].value : wv;
            apply_eval(&e, av, &sim, game, roll_num + s);
            tp += e.score_gain; ta += e.grants_action ? 1 : 0; ti += e.iroh_score;
            slots[s] = (PlanSlot){c->di, c->ac, e};
        }
        if (valid && (!found || plan_better(bot, bpts, bacts, biroh, tp, ta, ti))) {
            found = true; bpts = tp; bacts = ta; biroh = ti; bn = 3;
            bp[0] = slots[0]; bp[1] = slots[1]; bp[2] = slots[2];
        }
    }}}

    for (int s = 0; s < bn; s++) plan_out[s] = bp[s];
    return bn;
}

// Reroll decision
bool bot_should_reroll(BotType bot, int pts, int acts, double iroh) {
    if (bot == BOT_GOLLUM) return pts  < 15;
    if (bot == BOT_MIDAS)  return acts == 0;
    if (bot == BOT_IROH)   return iroh < 18.0;
    return false;
}

// Any-number: find best color+value
bool bot_find_any_number(BotType bot, GameState* game, PlayerSheet* sheet,
                          int* col_out, int* val_out) {
    int bp=-1,ba=-1; double bi=-1e9; int bc=-1,bv=-1;
    for (int area=BONUS_YELLOW;area<=BONUS_PINK;area++) {
        for (int v=1;v<=6;v++) {
            PlayerSheet tmp=*sheet; Die d; d.value=0;
            if (!use_any_number_action(&tmp,&d,v)) continue;
            Eval e={0};
            switch(area){
                case BONUS_YELLOW:e=eval_yellow(sheet,v,0);break;
                case BONUS_TURQUOISE:e=eval_turquoise(sheet,game,v);break;
                case BONUS_BLUE:e=eval_blue(sheet,v,0);break;
                case BONUS_BROWN:e=eval_brown(sheet,v);break;
                case BONUS_PINK:e=eval_pink(sheet,v);break;
            }
            if(!e.valid) continue;
            bool pref=false;
            if(bot==BOT_IROH) pref=e.iroh_score>bi;
            else if(bot==BOT_MIDAS) pref=(e.grants_action&&ba==0)||(e.grants_action==(ba>0)&&e.score_gain>bp);
            else pref=e.score_gain>bp;
            if(pref){bp=e.score_gain;ba=e.grants_action?1:0;bi=e.iroh_score;bc=area;bv=v;}
        }
    }
    if(bc<0) return false;
    *col_out=bc; *val_out=bv; return true;
}

// Prepare turn: apply any-number then reroll if needed
void bot_prepare_turn(BotType bot, GameState* game, PlayerSheet* sheet,
                      bool* rerolled, bool* an_used) {
    *rerolled=false; *an_used=false;
    PlanSlot tmp[3]; int n=find_best_plan(bot,game,sheet,0,tmp);
    int pts=0,acts=0; double iroh=0.0;
    for(int i=0;i<n;i++){pts+=tmp[i].eval.score_gain;acts+=tmp[i].eval.grants_action?1:0;iroh+=tmp[i].eval.iroh_score;}
    if(sheet->actions.any_number_charges>0){
        int ac=-1,av=-1;
        if(bot_find_any_number(bot,game,sheet,&ac,&av)){
            Die d; d.value=0;
            if(use_any_number_action(sheet,&d,av)){
                move_lower_dice_to_platter(game,av);
                Eval e={0};
                switch(ac){
                    case BONUS_YELLOW:e=eval_yellow(sheet,av,0);break;
                    case BONUS_TURQUOISE:e=eval_turquoise(sheet,game,av);break;
                    case BONUS_BLUE:e=eval_blue(sheet,av,0);break;
                    case BONUS_BROWN:e=eval_brown(sheet,av);break;
                    case BONUS_PINK:e=eval_pink(sheet,av);break;
                }
                if(e.valid) apply_eval(&e,av,sheet,game,0);
                *an_used=true; g_iroh_move_count++;
                n=find_best_plan(bot,game,sheet,0,tmp);
                pts=0;acts=0;iroh=0.0;
                for(int i=0;i<n;i++){pts+=tmp[i].eval.score_gain;acts+=tmp[i].eval.grants_action?1:0;iroh+=tmp[i].eval.iroh_score;}
            }
        }
    }
    if(sheet->actions.reroll_charges>0&&bot_should_reroll(bot,pts,acts,iroh)){
        use_reroll_action(sheet,game); *rerolled=true;
    }
}

static PlanSlot g_plan[3];
static int g_plan_n = 0;
static int g_plan_step = 0;

bool bot_do_active_turn(BotType bot, GameState* game, PlayerSheet* sheet, int roll_num) {
    if (roll_num == 0 || g_plan_step >= g_plan_n) {
        g_plan_n = find_best_plan(bot, game, sheet, roll_num, g_plan);
        g_plan_step = 0;
    }
    if (g_plan_step >= g_plan_n) return false;
    PlanSlot* slot = &g_plan[g_plan_step++];
    int wv = 0;
    for (int i=0;i<6;i++) if(game->dice[i].color==DIE_COLOR_WHITE) wv=game->dice[i].value;
    int die_idx = slot->die_index;
    if (die_idx < 0) {
        for (int i=0;i<6;i++)
            if(game->dice[i].color==DIE_COLOR_WHITE&&game->dice[i].is_available&&!game->dice[i].on_silver_platter)
                {die_idx=i;break;}
    }
    if (die_idx < 0) return false;
    Die picked; pick_die(game, die_idx, &picked);
    int av = (slot->die_index >= 0) ? picked.value : wv;
    if (slot->eval.area != BONUS_BLUE) move_lower_dice_to_platter(game, av);
    apply_eval(&slot->eval, av, sheet, game, roll_num);
    if (slot->eval.area == BONUS_BLUE) move_lower_dice_to_platter(game, av);
    g_iroh_move_count++;
    return true;
}


// ─────────────────────────────────────────────────────────────────────────────
// PUBLIC: BOT EXTRA DIE
// Picks and applies the best available die using bot criteria.
// Returns true if a die was used.
bool bot_use_extra_die(BotType bot, GameState* game, PlayerSheet* sheet) {
    int best_die = -1;
    Eval best = {0};
    double best_iroh = -1e9;
    int best_score = -1;
    bool best_grants = false;

    for (int i = 0; i < 6; i++) {
        if (!game->dice[i].is_available) continue;
        Eval e = eval_slot(bot, game, sheet, i, 0, 0);
        if (!e.valid) continue;
        bool pref = false;
        if (bot == BOT_IROH) pref = e.iroh_score > best_iroh;
        else if (bot == BOT_MIDAS) {
            if (e.grants_action && !best_grants) pref = true;
            else if ((e.grants_action?1:0)==(best_grants?1:0) && e.score_gain > best_score) pref = true;
        } else pref = e.score_gain > best_score;

        if (pref) {
            best_die = i; best = e;
            best_score = e.score_gain; best_iroh = e.iroh_score; best_grants = e.grants_action;
        }
    }

    if (best_die < 0) return false;
    apply_eval(&best, game->dice[best_die].value, sheet, game, 0);
    sheet->actions.extra_die_charges--;
    g_iroh_move_count++;
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// PUBLIC: BOT PASSIVE TURN
bool bot_do_passive_turn(BotType bot, GameState* game, PlayerSheet* sheet) {
    int best_die = -1;
    Eval best = {0};
    double best_iroh = -1e9;
    int best_score = -1;
    bool best_grants = false;

    for (int i = 0; i < 6; i++) {
        if (!game->dice[i].on_silver_platter) continue;
        Eval e = best_eval_for_die(bot, game, sheet, i, 0);
        if (!e.valid) continue;

        bool prefer = false;
        if (bot == BOT_IROH) prefer = e.iroh_score > best_iroh;
        else if (bot == BOT_MIDAS) {
            if (e.grants_action && !best_grants) prefer = true;
            else if (e.grants_action == best_grants && e.score_gain > best_score) prefer = true;
        } else prefer = e.score_gain > best_score;

        if (prefer) {
            best_die = i; best = e;
            best_score = e.score_gain; best_iroh = e.iroh_score; best_grants = e.grants_action;
        }
    }

    if (best_die < 0) return false;
    Die* d = &game->dice[best_die];
    apply_eval(&best, d->value, sheet, game, 0);
    g_iroh_move_count++;
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// PUBLIC: ROUND 4 WHITE BONUS
// Bot picks the best color + value for the white ? bonus.
void bot_handle_white_bonus(BotType bot, GameState* game, PlayerSheet* sheet) {
    Eval best = {0};
    int best_score = -1;
    bool best_grants = false;

    for (int v = 1; v <= 6; v++) {
        for (int area = BONUS_YELLOW; area <= BONUS_PINK; area++) {
            Eval e = {0};
            int white_val = 0; // no white die in bonus context
            switch (area) {
                case BONUS_YELLOW:    e = eval_yellow(sheet, v, game->chosen_dice_count); break;
                case BONUS_TURQUOISE: e = eval_turquoise(sheet, game, v); break;
                case BONUS_BLUE:      e = eval_blue(sheet, v, white_val); break;
                case BONUS_BROWN:     e = eval_brown(sheet, v); break;
                case BONUS_PINK:      e = eval_pink(sheet, v); break;
            }
            if (!e.valid) continue;

            bool prefer = false;
            if (bot == BOT_MIDAS) {
                if (e.grants_action && !best_grants) prefer = true;
                else if (e.grants_action == best_grants && e.score_gain > best_score) prefer = true;
            } else {
                if (e.score_gain > best_score) prefer = true;
            }

            if (prefer) {
                best = e; best_score = e.score_gain; best_grants = e.grants_action;
            }
        }
    }

    if (best.valid) {
        // Find the value that produced best
        // Re-run to find the value (we stored area but not value, so re-scan)
        int best_v = 1;
        int bs2 = -1;
        bool bg2 = false;
        for (int v = 1; v <= 6; v++) {
            Eval e = {0};
            int white_val = 0;
            switch (best.area) {
                case BONUS_YELLOW:    e = eval_yellow(sheet, v, game->chosen_dice_count); break;
                case BONUS_TURQUOISE: e = eval_turquoise(sheet, game, v); break;
                case BONUS_BLUE:      e = eval_blue(sheet, v, white_val); break;
                case BONUS_BROWN:     e = eval_brown(sheet, v); break;
                case BONUS_PINK:      e = eval_pink(sheet, v); break;
            }
            if (!e.valid) continue;
            bool prefer = false;
            if (bot == BOT_MIDAS) {
                if (e.grants_action && !bg2) prefer = true;
                else if (e.grants_action == bg2 && e.score_gain > bs2) prefer = true;
            } else {
                if (e.score_gain > bs2) prefer = true;
            }
            if (prefer) { best_v = v; bs2 = e.score_gain; bg2 = e.grants_action; best = e; }
        }
        apply_eval(&best, best_v, sheet, game, game->chosen_dice_count);
    }
}
