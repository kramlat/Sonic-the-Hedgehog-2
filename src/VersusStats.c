#include "VersusStats.h"
#include "Level.h"
#include "Object/Ring.h"
#include "Sst.h"
#include "ObjectSst.h"
#include "Object.h"
#include "Object/Tails.h"
#include <string.h>

bool vs_stats_active;
bool vs_act_over;
static bool passed[2];

typedef struct {
    uint32_t score;
    LevelTime time;
    uint16_t rings;
    uint8_t time_count;
    uint8_t lives, life_count;
    uint8_t shield, invincibility, shoes;
} Set;

static Set other;              // the set that is not in the game's variables
static bool second_in;         // whether it is the second player's that is in them
static uint16_t items[2], total_rings[2];

void VsStats_Begin(void) {
    memset(&other, 0, sizeof(other));
    other.time_count = 1;
    other.lives = lives;
    second_in = false;
    memset(items, 0, sizeof(items));
    memset(total_rings, 0, sizeof(total_rings));
    vs_stats_active = true;
    vs_act_over = false;
    memset(passed, 0, sizeof(passed));
    ring_collected_hook = VsStats_RingCollected;
}

void VsStats_Swap(void) {
    Set now = { score, level_time, rings, time_count, lives, life_count, shield, invincibility, shoes };
    score = other.score;
    level_time = other.time;
    rings = other.rings;
    time_count = other.time_count;
    lives = other.lives;
    life_count = other.life_count;
    shield = other.shield;
    invincibility = other.invincibility;
    shoes = other.shoes;
    other = now;
    second_in = !second_in;
}

static LevelTime *SecondTime(void) {
    return second_in ? &level_time : &other.time;
}

void VsStats_Tick(void) {
    // (the first player's clock runs in the HUD; this one is the second's, whichever set is in)
    uint8_t *running = second_in ? &time_count : &other.time_count;
    if (!*running || pause_state)
        return;
    LevelTime *t = SecondTime();
    if (++t->frame >= 60) {
        t->frame = 0;
        if (++t->sec >= 60) {
            t->sec = 0;
            if (++t->min > 9)
                t->min = 9;
        }
    }
}

void VsStats_RingCollected(void) {
    if (vs_stats_active)
        total_rings[second_in ? 1 : 0]++;
}

void VsStats_AddPointsTo(const Sst *chr, uint16_t points) {
    const bool for_second = vs_stats_active && chr == TAILS_OBJ;
    if (for_second != second_in && vs_stats_active) {
        VsStats_Swap();
        AddPoints(points);
        VsStats_Swap();
    } else {
        AddPoints(points);
    }
}

void VsStats_SecondHud(uint32_t *score_out, uint16_t *rings_out, uint8_t *min, uint8_t *sec) {
    const bool in_game = !vs_stats_active || second_in;
    *score_out = in_game ? score : other.score;
    *rings_out = in_game ? rings : other.rings;
    const LevelTime *t = in_game ? &level_time : &other.time;
    *min = t->min;
    *sec = t->sec;
}

bool VsStats_Has(const Sst *chr, int what) {
    const bool in_game = !vs_stats_active || (chr == TAILS_OBJ) == second_in; // (whose set is in the game's variables)
    const Set *set = in_game ? NULL : &other;
    switch (what) {
    case VS_SHIELD:
        return set ? set->shield : shield;
    case VS_INVINCIBLE:
        return set ? set->invincibility : invincibility;
    default:
        return set ? set->shoes : shoes;
    }
}

void VsStats_ItemBroken(void) {
    if (vs_stats_active)
        items[second_in ? 1 : 0]++;
}

void VsStats_StopClock(int player) {
    if ((player == 1) == second_in)
        time_count = 0;
    else
        other.time_count = 0;
}

void VsStats_Read(VsPlayerStats out[2]) {
    const bool swapped = second_in;
    if (swapped)
        VsStats_Swap(); // (the first player's in the variables again)
    out[0] = (VsPlayerStats){ score, (uint32_t)level_time.min * 3600 + (uint32_t)level_time.sec * 60 + level_time.frame, rings, items[0], total_rings[0] };
    out[1] = (VsPlayerStats){ other.score, (uint32_t)other.time.min * 3600 + (uint32_t)other.time.sec * 60 + other.time.frame, other.rings, items[1], total_rings[1] };
    if (swapped)
        VsStats_Swap();
}

bool VsEndPanel_Update(Sst *panel) {
    const bool none_before = !passed[0] && !passed[1];
    bool any = false;
    for (int p = 0; p < 2; p++) {
        const Sst *o = p ? TAILS_OBJ : player;
        if (!passed[p] && o->code != sst_object_codes[ObjId_Null] && (uint16_t)(o->x_pos.f.u - panel->x_pos.f.u) < 0x20) {
            passed[p] = true;
            VsStats_StopClock(p);
            any = true;
        }
    }
    return none_before && any;
}

bool VsEndPanel_BothPassed(void) {
    return passed[0] && passed[1];
}
