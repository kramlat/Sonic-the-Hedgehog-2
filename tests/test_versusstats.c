#include "test.h"

#include "Level.h"
#include "Object/Ring.h"
#include "VersusStats.h"

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
}
