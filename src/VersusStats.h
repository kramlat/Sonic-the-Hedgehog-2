#ifndef _VERSUSSTATS_H
#define _VERSUSSTATS_H

#include <stdbool.h>
#include "VersusFlow.h"

// The two players' own counters in the versus play. The game keeps one set (`score`, `rings`, `level_time`, `time_count`): those are the first player's, and the second player's are in here, swapped into those for
// the time the second player's object runs (Obj_Tails), so everything that adds to them (rings, monitors, points) needs no change. Beta 4 has separate variables for each ($FE20/$FED0 and the rest).
extern bool vs_stats_active;

void VsStats_Begin(void);                       // a match's act starts: both sets are zero
void VsStats_Swap(void);                        // the second player's set in, or out again
void VsStats_Tick(void);                        // the second player's clock (the first's is the HUD's)
void VsStats_RingCollected(void);               // the total of the player whose set is in
void VsStats_ItemBroken(void);                  // an item box
void VsStats_Read(VsPlayerStats out[2]);        // both, for the results
void VsStats_StopClock(int player);             // the player has passed the end: his time stops

#endif //_VERSUSSTATS_H
