#include "test.h"

#include "VersusFlow.h"

// The versus flow's logic (Beta 4's Versus_Mode_Results): the categories of an act, and where each results page leads.

static VsPlayerStats Stats(uint32_t score, uint32_t time, uint16_t rings, uint16_t items, uint16_t total) {
    return (VsPlayerStats){ score, time, rings, items, total };
}

static void VsFlow_ThePlayerWithMoreCategoriesWinsTheAct(void) {
    const VsPlayerStats a = Stats(1000, 3000, 50, 2, 70), b = Stats(500, 2000, 60, 2, 40);
    const uint16_t entry = VsFlow_ActEntry(&a, &b); // P1: score, total rings; P2: time (shorter), rings; the items tie
    CHECK_EQ(entry, (2 << 8) | 2);
    CHECK_EQ(VsFlow_EntryWinner(entry), VS_TIE);
    const VsPlayerStats c = Stats(1000, 1000, 50, 3, 70);
    CHECK_EQ(VsFlow_EntryWinner(VsFlow_ActEntry(&c, &b)), VS_P1);
    CHECK_EQ(VsFlow_EntryWinner(VsFlow_ActEntry(&b, &c)), VS_P2);
    CHECK_EQ(VsFlow_EntryWinner(VS_UNPLAYED), VS_TIE);
}

static void VsFlow_AZoneIsPlayedInTwoActsAndAWinnerGoesToTheLevelResults(void) {
    VsFlow f;
    VsFlow_Reset(&f);
    VsFlow_Choose(&f, 1);
    CHECK(!VsFlow_ZonePlayed(&f, 1));
    VsFlow_SetEntry(&f, (3 << 8) | 2); // act 1: P1
    VsNext n = VsFlow_Continue(&f, VS_PAGE_ACT);
    CHECK_EQ(n.what, VSNEXT_ACT2);
    CHECK_EQ(f.play, 1);
    VsFlow_SetEntry(&f, (4 << 8) | 1); // act 2: P1
    n = VsFlow_Continue(&f, VS_PAGE_ACT);
    CHECK_EQ(n.what, VSNEXT_PAGE);
    CHECK_EQ(n.page, VS_PAGE_LEVEL);
    CHECK(VsFlow_ZonePlayed(&f, 1));
    n = VsFlow_Continue(&f, VS_PAGE_LEVEL);
    CHECK_EQ(n.what, VSNEXT_ZONE_MENU); // (the other zones are still to be played)
}

static void VsFlow_ATiedZoneGoesToTheSpecialStageAndThenToTheLevelResults(void) {
    VsFlow f;
    VsFlow_Reset(&f);
    VsFlow_Choose(&f, 0);
    VsFlow_SetEntry(&f, (3 << 8) | 2); // P1
    VsFlow_Continue(&f, VS_PAGE_ACT);
    VsFlow_SetEntry(&f, (2 << 8) | 3); // P2: one each
    VsNext n = VsFlow_Continue(&f, VS_PAGE_ACT);
    CHECK_EQ(n.what, VSNEXT_SPECIAL);
    CHECK_EQ(f.play, 2);
    VsFlow_SetEntry(&f, (2 << 8) | 1);
    n = VsFlow_Continue(&f, VS_PAGE_SPECIAL);
    CHECK_EQ(n.what, VSNEXT_PAGE);
    CHECK_EQ(n.page, VS_PAGE_LEVEL);
}

static void VsFlow_TheSpecialStageBoxHasThreeRoundsAndAllOfThemResults(void) {
    VsFlow f;
    VsFlow_Reset(&f);
    VsFlow_Choose(&f, 3);
    for (int round = 0; round < 3; round++) {
        VsFlow_SetEntry(&f, (1 << 8) | 0);
        const VsNext n = VsFlow_Continue(&f, VS_PAGE_SPECIAL);
        if (round < 2) {
            CHECK_EQ(n.what, VSNEXT_SPECIAL);
            CHECK_EQ(f.play, round + 1);
        } else {
            CHECK_EQ(n.what, VSNEXT_PAGE);
            CHECK_EQ(n.page, VS_PAGE_ALL_SPECIAL);
        }
    }
    CHECK(VsFlow_ZonePlayed(&f, 3));
}

static void VsFlow_WhenNothingIsLeftTheGlobalResultsAndTheSegaScreenFollow(void) {
    VsFlow f;
    VsFlow_Reset(&f);
    for (int z = 0; z < VS_ZONES; z++)
        for (int p = 0; p < (z == 3 ? VS_PLAYS : 2); p++)
            f.results[z][p] = (uint16_t)((1 << 8) | 0);
    VsNext n = VsFlow_Continue(&f, VS_PAGE_LEVEL);
    CHECK_EQ(n.what, VSNEXT_PAGE);
    CHECK_EQ(n.page, VS_PAGE_GLOBAL);
    n = VsFlow_Continue(&f, VS_PAGE_GLOBAL);
    CHECK_EQ(n.what, VSNEXT_SEGA);
}

void RegisterVersusFlowTests(void) {
    RUN_TEST(VsFlow_ThePlayerWithMoreCategoriesWinsTheAct);
    RUN_TEST(VsFlow_AZoneIsPlayedInTwoActsAndAWinnerGoesToTheLevelResults);
    RUN_TEST(VsFlow_ATiedZoneGoesToTheSpecialStageAndThenToTheLevelResults);
    RUN_TEST(VsFlow_TheSpecialStageBoxHasThreeRoundsAndAllOfThemResults);
    RUN_TEST(VsFlow_WhenNothingIsLeftTheGlobalResultsAndTheSegaScreenFollow);
}
