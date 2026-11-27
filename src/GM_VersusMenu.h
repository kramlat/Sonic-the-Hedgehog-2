#ifndef _GM_VERSUSMENU_H
#define _GM_VERSUSMENU_H

#include <stdbool.h>

// The 2 player versus menus of Beta 4 (M5; docs/Sonic2M5.md): the session menu (LOCAL SPLITSCREEN, HOST SESSION, JOIN SESSION), the code screens, the wait, loading, refused and time out screens, and then
// the zone menu (to come: for now the first zone is taken). All play the Level select / Options theme. Returns true with `level_id` set and the two player flags on when a match is to start, false when the player
// went back (B on the session menu). A session (HOST or JOIN) that has connected is ended here for now: it is not connected to the game loop yet (the netplay milestone).
bool VersusMenu_Run(void);
// After a true VersusMenu_Run: the special stage box was chosen (level_id is not a level then)
bool VersusMenu_ChoseSpecialStage(void);
// After a true VersusMenu_Run: the match is in the split screen (LOCAL SPLITSCREEN); a session online (HOST, JOIN) is the true 2 player mode alone, with the one picture
bool VersusMenu_ChoseLocal(void);

#include "VersusFlow.h"

// The zone menu again, in the middle of a match: `f`'s played zones show the red X and cannot be picked. Sets level_id (or the special stage's choice: VersusMenu_ChoseSpecialStage) and returns true
bool VersusMenu_PickZone(const VsFlow *f);

// One of the five results pages of a match (Beta 4's Versus_Mode_Results): the stats of the act just played for `p1` and `p2` (page VS_PAGE_ACT), the three numbers each of the special stage for `ss1` and `ss2`
// (VS_PAGE_SPECIAL: `round` its number), the rest from `f`. Shows it with the banner, plays the results theme and waits for Start
void VersusResults_Show(const VsFlow *f, VsPage page, const VsPlayerStats *p1, const VsPlayerStats *p2, const uint16_t ss1[3], const uint16_t ss2[3], int round);

#endif //_GM_VERSUSMENU_H
