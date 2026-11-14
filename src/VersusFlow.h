#ifndef _VERSUSFLOW_H
#define _VERSUSFLOW_H

#include <stdbool.h>
#include <stdint.h>

// The two player versus flow of Sonic 2 Beta 4: what is kept between the acts of a match and where each results page leads (the logic of `Versus_Mode_Results` at Sonic2B4.asm ~8145, `loc_8A3A` to `loc_8B48`, and the
// comparisons of `Vs_Mode_Current_Act_Results`). The screens and the game modes that use it are built on this (docs/Sonic2M5.md, "The versus flow").
//
//   zone menu -> [Please Wait] -> title card -> act 1 -> [Please Wait] -> act results -> [Please Wait] -> title card -> act 2 -> [Please Wait] -> act results
//     -> the zone's winner: level results -> zone menu; or a tie: the special stage as the tie breaker -> its results -> level results -> zone menu
//     -> when no zone is left: [Please Wait] -> global results -> the Sega screen
#define VS_ZONES 4 // the boxes of the zone menu (the fourth is the special stage itself)
#define VS_PLAYS 3 // what each has: act 1, act 2 and the special stage tie breaker (the special stage box: its three rounds)

#define VS_UNPLAYED 0xFFFF // (an entry: the categories P1 won in the high byte, P2 in the low one)
#define VS_GAME_OVER 0xFFFE

typedef enum { VS_PAGE_ACT, VS_PAGE_LEVEL, VS_PAGE_GLOBAL, VS_PAGE_SPECIAL, VS_PAGE_ALL_SPECIAL } VsPage;

typedef struct {
    uint32_t score;
    uint32_t time;      // frames (the shorter wins)
    uint16_t rings;     // the rings of the act
    uint16_t items;     // the item boxes broken
    uint16_t total_rings; // all the rings collected, lost ones too
} VsPlayerStats;

typedef enum { VS_TIE, VS_P1, VS_P2 } VsWinner;

// The five categories (score, time, rings, items, total rings): how many each player won, as the byte pair of a results entry
uint16_t VsFlow_ActEntry(const VsPlayerStats *p1, const VsPlayerStats *p2);
// The special stage's three categories (the higher wins each)
uint16_t VsFlow_SpecialEntry(const uint16_t p1[3], const uint16_t p2[3]);
VsWinner VsFlow_EntryWinner(uint16_t entry);

typedef struct {
    uint16_t results[VS_ZONES][VS_PLAYS]; // Results_Data_2P
    int zone;                             // Current_Zone_2P: the box chosen
    int play;                             // $FF89: which act (or special stage round) is being played
    int act;                              // $FE11: the act of the level, 0 or 1
} VsFlow;

typedef enum {
    VSNEXT_ACT2,        // the second act of the zone
    VSNEXT_SPECIAL,     // a special stage: the tie breaker, or the next round of the special stage box
    VSNEXT_PAGE,        // another results page
    VSNEXT_ZONE_MENU,   // back to the zone menu
    VSNEXT_SEGA,        // the match is over
} VsNextWhat;

typedef struct {
    VsNextWhat what;
    VsPage page; // (VSNEXT_PAGE)
} VsNext;

void VsFlow_Reset(VsFlow *f);
void VsFlow_Choose(VsFlow *f, int zone);                                     // a box was chosen on the zone menu: the first act
void VsFlow_SetEntry(VsFlow *f, uint16_t entry);                             // what the act (or round) that has just been played gave
// Start on a results page: where it leads (and the state moves on: the act, the round)
VsNext VsFlow_Continue(VsFlow *f, VsPage page);
// Whether a zone box has been played to the end (its results make it unavailable on the zone menu)
bool VsFlow_ZonePlayed(const VsFlow *f, int zone);

#endif //_VERSUSFLOW_H
