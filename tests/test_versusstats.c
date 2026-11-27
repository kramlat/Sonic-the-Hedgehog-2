#include "test.h"

#include "Level.h"
#include "Object/Ring.h"
#include "VersusStats.h"
#include "VersusMatch.h"
extern uint8_t two_player_mode;
extern uint8_t true_two_player;
#include <string.h>
#include "HUD.h"
#include "Object/HUD.h"
#include "TileBank.h"
#include "Constants.h"
extern const uint8_t Art_HUDNum[]; // (HUD.c has it)
#include "Object.h"
#include "Object/Tails.h"
#include "Object/InvincibilityStars.h"
#include "SstLoad.h"

// The versus play's own counters for each player: the second player's set is swapped into the game's variables while his object runs.

static void VsStats_TheSecondPlayersCountersAreHisOwn(void) {
    score = 0;
    rings = 0;
    level_time = (LevelTime){ 0 };
    time_count = 1;
    VsStats_Begin();
    Ring_Collect();                // (the first player)
    Ring_Collect();
    VsStats_Swap();                // (the second's object runs)
    Ring_Collect();
    AddPoints(100);
    CHECK_EQ(rings, 1);
    VsStats_Swap();
    CHECK_EQ(rings, 2);
    CHECK_EQ(score, 0);
    VsPlayerStats s[2];
    VsStats_Read(s);
    CHECK_EQ(s[0].rings, 2);
    CHECK_EQ(s[1].rings, 1);
    CHECK_EQ(s[0].total_rings, 2);
    CHECK_EQ(s[1].total_rings, 1);
    CHECK_EQ(s[1].score, 100);
    vs_stats_active = false;
    ring_collected_hook = NULL;
}

// Beta 4 keeps the shield, the stars and the speed shoes on each character's own object: here the second player's flags are in the swapped set, and what follows a character goes with his own flag
static void VsStats_EachCharactersShieldStarsAndShoesAreHisOwn(void) {
    score = 0;
    rings = 0;
    level_time = (LevelTime){ 0 };
    time_count = 1;
    shield = invincibility = shoes = 0;
    VsStats_Begin();
    CHECK(!VsStats_Has(player, VS_SHIELD));
    CHECK(!VsStats_Has(TAILS_OBJ, VS_SHIELD));
    VsStats_Swap();                // (the second player's object runs: a shield and shoes are his)
    shield = 1;
    shoes = 1;
    CHECK(VsStats_Has(TAILS_OBJ, VS_SHIELD));
    CHECK(!VsStats_Has(player, VS_SHIELD));  // (the first player's are in the other set now)
    VsStats_Swap();
    CHECK(!shield && !shoes);      // (the game's variables are the first player's again)
    CHECK(VsStats_Has(TAILS_OBJ, VS_SHIELD));
    CHECK(VsStats_Has(TAILS_OBJ, VS_SHOES));
    CHECK(!VsStats_Has(player, VS_SHOES));
    invincibility = 1;             // (the first player's stars)
    CHECK(VsStats_Has(player, VS_INVINCIBLE));
    CHECK(!VsStats_Has(TAILS_OBJ, VS_INVINCIBLE));

    // What follows a character: one shield for each, and none twice
    Sst_Reset();
    player->x_pos.f.u = 100;
    TAILS_OBJ->x_pos.f.u = 300;
    invincibility = 0;
    shield = 1;
    ShieldInvincibility_MakeFor(0, player);
    ShieldInvincibility_MakeFor(0, TAILS_OBJ);
    ShieldInvincibility_MakeFor(0, TAILS_OBJ);
    int shields = 0, tails_shields = 0;
    for (int i = SST_FIRST_DYNAMIC; i < SST_COUNT; i++)
        if (sst[i].code != NULL) {
            shields++;
            tails_shields += sst[i].status_tertiary == 1;
        }
    CHECK_EQ(shields, 2);
    CHECK_EQ(tails_shields, 1);
    Sst_Reset();
    vs_stats_active = false;
    ring_collected_hook = NULL;
    shield = invincibility = shoes = 0;
}

// The second view's HUD numbers: Miles' rings and time are written into the bank the second sprite draws from, in the HUD's digits
static void VsStats_TheSecondViewsHudShowsHisOwnNumbers(void) {
    score = 0;
    rings = 0;
    level_time = (LevelTime){ 0 };
    time_count = 1;
    VsStats_Begin();
    VsStats_Swap();                // (his object runs: 25 rings and 1:07 on his clock)
    rings = 25;
    level_time.min = 1;
    level_time.sec = 7;
    VsStats_Swap();
    tilebank_t *bank = TileBank_Create(ArtTile_HUD + 0x3A);
    CHECK(bank != NULL);
    if (bank == NULL)
        return;
    hud_second_bank = bank;
    HUD_Base();
    HUD_Update();
    CHECK_EQ(memcmp(bank->patterns + (ArtTile_HUDRings + 2) * 32, Art_HUDNum + (2 << 6), 64), 0); // (the 2 of 25)
    CHECK_EQ(memcmp(bank->patterns + (ArtTile_HUDRings + 4) * 32, Art_HUDNum + (5 << 6), 64), 0); // (and the 5)
    CHECK_EQ(memcmp(bank->patterns + ArtTile_HUDTimeMins * 32, Art_HUDNum + (1 << 6), 64), 0);    // (1:)
    CHECK_EQ(memcmp(bank->patterns + (ArtTile_HUDTimeSecs + 2) * 32, Art_HUDNum + (7 << 6), 64), 0); // (07)
    CHECK(!hud_second_rings_zero);
    hud_second_bank = NULL;
    TileBank_Free(bank);
    vs_stats_active = false;
    ring_collected_hook = NULL;
}

// The match joined to the game: both players' scores go to the act's results page when the end panel says the act is over, and the first act leads to the second
static int match_pages;
static VsPlayerStats match_p1, match_p2;
static void MatchShow(const VsFlow *f, VsPage page, const VsPlayerStats *p1, const VsPlayerStats *p2) {
    (void)f;
    (void)page;
    match_pages++;
    match_p1 = *p1;
    match_p2 = *p2;
}

static void VsMatch_TheActsResultsHaveBothPlayersScoresAndLeadToTheSecondAct(void) {
    two_player_mode = 1; // (a split screen game: both flags are set; the versus stuff follows true_two_player)
    true_two_player = 1;
    level_id = LEVEL_ID(ZoneId_EHZ, 0);
    restart = 0;
    score = 0;
    rings = 0;
    level_time = (LevelTime){ 0 };
    time_count = 1;
    VersusMatch_Begin();
    VersusMatch_LevelStart();
    CHECK(vs_stats_active);
    score = 50;                    // (Sonic's)
    VsStats_Swap();
    score = 100;                   // (Miles')
    VsStats_Swap();
    versus_match_show_hook = MatchShow;
    VersusMatch_Poll();            // (the act is not over)
    CHECK_EQ(match_pages, 0);
    vs_act_over = true;
    VersusMatch_Poll();
    CHECK_EQ(match_pages, 1);
    CHECK_EQ(match_p1.score, 50);
    CHECK_EQ(match_p2.score, 100);
    CHECK_EQ(LEVEL_ACT(level_id), 1);
    CHECK(restart);
    versus_match_show_hook = NULL;
    two_player_mode = 0;
    true_two_player = 0;
    vs_stats_active = false;
    ring_collected_hook = NULL;
    restart = 0;
}

static void VsStats_TheSecondPlayersClockStopsOnItsOwn(void) {
    score = 0;
    rings = 0;
    level_time = (LevelTime){ 0 };
    time_count = 1;
    VsStats_Begin();
    for (int i = 0; i < 90; i++)
        VsStats_Tick();
    VsStats_StopClock(1);
    for (int i = 0; i < 30; i++)
        VsStats_Tick();
    VsPlayerStats s[2];
    VsStats_Read(s);
    CHECK_EQ(s[1].time, 90);
    CHECK_EQ(time_count, 1); // (the first player's clock is not stopped)
    vs_stats_active = false;
    ring_collected_hook = NULL;
}

void RegisterVersusStatsTests(void) {
    RUN_TEST(VsStats_TheSecondPlayersCountersAreHisOwn);
    RUN_TEST(VsStats_TheSecondPlayersClockStopsOnItsOwn);
    RUN_TEST(VsMatch_TheActsResultsHaveBothPlayersScoresAndLeadToTheSecondAct);
    RUN_TEST(VsStats_TheSecondViewsHudShowsHisOwnNumbers);
    RUN_TEST(VsStats_EachCharactersShieldStarsAndShoesAreHisOwn);
}
