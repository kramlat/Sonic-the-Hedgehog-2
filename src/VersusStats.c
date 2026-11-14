#include "VersusStats.h"
#include "Level.h"
#include "Object/Ring.h"
#include <string.h>

bool vs_stats_active;

typedef struct {
    uint32_t score;
    LevelTime time;
    uint16_t rings;
    uint8_t time_count;
} Set;

static Set other;              // the set that is not in the game's variables
static bool second_in;         // whether it is the second player's that is in them
static uint16_t items[2], total_rings[2];

void VsStats_Begin(void) {
    memset(&other, 0, sizeof(other));
    other.time_count = 1;
    second_in = false;
    memset(items, 0, sizeof(items));
    memset(total_rings, 0, sizeof(total_rings));
    vs_stats_active = true;
    ring_collected_hook = VsStats_RingCollected;
}

void VsStats_Swap(void) {
    Set now = { score, level_time, rings, time_count };
    score = other.score;
    level_time = other.time;
    rings = other.rings;
    time_count = other.time_count;
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
