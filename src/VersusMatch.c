#include "VersusMatch.h"
extern uint8_t true_two_player; // (Game.h: the versus play)

#include <string.h>

#include "Constants.h"
#include "Game.h"
#include "GM_VersusMenu.h"
#include "Level.h"
#include "VersusFlow.h"
#include "VersusStats.h"
#include "GM_SpecialTrack.h"

extern uint8_t two_player_mode; // (SplitScreen.c)

void (*versus_match_show_hook)(const VsFlow *f, VsPage page, const VsPlayerStats *p1, const VsPlayerStats *p2);
static VsFlow flow;
static bool in_match;

// The zone box of the menu for a level (the table of GM_VersusMenu.c: Emerald Hill, Mystic Cave, Casino Night, then the special stage)
static int BoxOf(uint8_t level) {
    switch (LEVEL_ZONE(level)) {
    case ZoneId_MCZ:
        return 1;
    case ZoneId_CNZ:
        return 2;
    default:
        return 0;
    }
}

void VersusMatch_Begin(void) {
    VsFlow_Reset(&flow);
    VsFlow_Choose(&flow, BoxOf(level_id));
    flow.act = LEVEL_ACT(level_id) & 1;
    in_match = true;
}

void VersusMatch_BeginSpecial(void) {
    VsFlow_Reset(&flow);
    VsFlow_Choose(&flow, 3); // (the fourth box of the zone menu: the special stage itself, three stages)
    in_match = true;
    ss4_stage = 7;
    ss4_versus = true;
}

static void StartSpecial(void) { // the next special stage of the match: the stages 7 to 9 of its box, the first for a tie breaker
    ss4_stage = 7 + (flow.zone == 3 ? flow.play : 0);
    ss4_versus = true;
    gamemode = GameMode_Special;
}

void VersusMatch_LevelStart(void) {
    if (two_player_mode || true_two_player) { // (separate counters for each player: the split screen's and the versus')
        VsStats_Begin();
        return;
    }
    vs_stats_active = false;
    vs_act_over = false;
}

static void ShowPage(VsPage page, const VsPlayerStats *p1, const VsPlayerStats *p2) {
    static const uint16_t no_special[3];
    if (versus_match_show_hook != NULL)
        versus_match_show_hook(&flow, page, p1, p2);
    else
        VersusResults_Show(&flow, page, p1, p2, no_special, no_special, 0);
}

static void EndMatch(void) {
    in_match = false;
    ss4_versus = false;
    vs_stats_active = false;
    vs_act_over = false;
    two_player_mode = false;
    true_two_player = false;
    gamemode = GameMode_Title;
}

void VersusMatch_Poll(void) {
    if (!vs_act_over || !in_match)
        return;
    vs_act_over = false;

    VsPlayerStats stats[2];
    VsStats_Read(stats);
    VsFlow_SetEntry(&flow, VsFlow_ActEntry(&stats[0], &stats[1]));
    ShowPage(VS_PAGE_ACT, &stats[0], &stats[1]);

    VsNext next = VsFlow_Continue(&flow, VS_PAGE_ACT);
    // The pages that follow the act's (the zone's winner), until something that is not joined yet
    while (next.what == VSNEXT_PAGE) {
        ShowPage(next.page, &stats[0], &stats[1]);
        next = VsFlow_Continue(&flow, next.page);
    }
    if (next.what == VSNEXT_ACT2) { // the zone's second act: the same level, its act 2
        level_id = LEVEL_ID(LEVEL_ZONE(level_id), 1);
        restart = 1;
        return;
    }
    if (next.what == VSNEXT_SPECIAL) { // a tie: the special stage decides
        StartSpecial();
        return;
    }
    EndMatch();
}

// A versus special stage has ended (its three rounds' rings): its page, then where the match goes
void VersusMatch_SpecialDone(const uint16_t ss1[3], const uint16_t ss2[3]) {
    static const VsPlayerStats none;
    if (!in_match) {
        gamemode = GameMode_Title;
        return;
    }
    VsFlow_SetEntry(&flow, VsFlow_SpecialEntry(ss1, ss2));
    VersusResults_Show(&flow, VS_PAGE_SPECIAL, &none, &none, ss1, ss2, flow.play);
    VsNext next = VsFlow_Continue(&flow, VS_PAGE_SPECIAL);
    while (next.what == VSNEXT_PAGE) {
        VersusResults_Show(&flow, next.page, &none, &none, ss1, ss2, flow.play);
        next = VsFlow_Continue(&flow, next.page);
    }
    if (next.what == VSNEXT_SPECIAL) {
        StartSpecial();
        return;
    }
    EndMatch();
}
