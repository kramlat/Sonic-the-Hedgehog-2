#ifndef _VERSUSSTATS_H
#define _VERSUSSTATS_H

#include <stdbool.h>
#include <stdint.h>
#include "VersusFlow.h"

struct Sst;
// The two players' own counters in the versus play. The game keeps one set (`score`, `rings`, `level_time`, `time_count`): those are the first player's, and the second player's are in here, swapped into those for
// the time the second player's object runs (Obj_Tails), so everything that adds to them (rings, monitors, points) needs no change. Beta 4 has separate variables for each ($FE20/$FED0 and the rest).
extern bool vs_stats_active;

void VsStats_Begin(void);                       // a match's act starts: both sets are zero
void VsStats_Swap(void);                        // the second player's set in, or out again
void VsStats_Tick(void);                        // the second player's clock (the first's is the HUD's)
void VsStats_RingCollected(void);               // the total of the player whose set is in
void VsStats_AddPointsTo(const struct Sst *chr, uint16_t points); // points to the character's own score (Beta 4's AddPoints_1, which takes the character in a3)
void VsStats_ItemBroken(void);                  // an item box
// A character's shield, invincibility and speed shoes (the flags are on his own object in Beta 4, `$2B` bits 0 to 2): the game keeps one set (`shield`, `invincibility`, `shoes`) that goes with the counters, so the second
// player's are in `other` while the first player's object runs. Outside the versus play the one set is both characters' (Sonic's, which is what Tails has always seen).
enum { VS_SHIELD, VS_INVINCIBLE, VS_SHOES };
bool VsStats_Has(const struct Sst *chr, int what);
void VsStats_Read(VsPlayerStats out[2]);        // both, for the results
void VsStats_StopClock(int player);             // the player has passed the end: his time stops

// The versus end panel: each player passes it on his own. Update looks at both and is true on the frame the first of them has; BothPassed when the second has too; `vs_act_over` is set by the panel a moment after.
struct Sst;
bool VsEndPanel_Update(struct Sst *panel);
bool VsEndPanel_BothPassed(void);
extern bool vs_act_over;

#endif //_VERSUSSTATS_H
