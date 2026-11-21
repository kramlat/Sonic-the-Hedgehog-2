#include "test.h"

#include "Level.h"
#include "Object/Ring.h"
#include "VersusStats.h"
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
    RUN_TEST(VsStats_EachCharactersShieldStarsAndShoesAreHisOwn);
}
