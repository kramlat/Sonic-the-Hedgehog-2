#ifndef _VERSUSMATCH_H
#define _VERSUSMATCH_H

#include <stdint.h>
#include "VersusFlow.h"

// The two player versus play joined to the game: the match's state (VersusFlow.h) between its acts, each player's own counters (VersusStats.h) from the start of an act, and the results page when both players have
// passed the end panel (Beta 4's Versus_Mode_Results: the act's page, then where it leads). Built on the pieces of docs/Sonic2M5.md; the zone menu's way back and the special stage tie breaker are not joined yet (the match
// then ends at the title screen).

// A match begins (the zone menu has chosen the level in `level_id`)
void VersusMatch_Begin(void);
// A level starts: in the versus play both players' counters are zero (Sonic's are the game's variables, Miles' the second set), otherwise nothing is kept
void VersusMatch_LevelStart(void);
// The special stage box of the zone menu was chosen: the match's flow stands on it (three stages)
void VersusMatch_BeginSpecial(void);
// A versus special stage has ended: the rings of its three rounds (Sonic's and Miles'); shows its results and starts what follows
void VersusMatch_SpecialDone(const uint16_t ss1[3], const uint16_t ss2[3]);

// Every frame of the level: when the end panel says the act is over, the results page of the act and what follows it
void VersusMatch_Poll(void);

// The tests replace the page's screen (which waits for Start) with this
extern void (*versus_match_show_hook)(const VsFlow *f, VsPage page, const VsPlayerStats *p1, const VsPlayerStats *p2);

#endif //_VERSUSMATCH_H
