#include "test.h"

#include "Level.h"
#include "PLC.h"

// The prototype loads the signpost's art when the camera reaches the end of the level, except in the levels its Test_End_Level_Art_Load lists: the bosses' acts (Emerald Hill 2, Hill Top 2, Oil Ocean 2, Mystic Cave 2, Casino Night 2, Chemical Plant 2, Neo Green Hill 2 and the like) and the zones without a sign.
// Loading it clears the queue, and with it the boss's art that is still being loaded.

static bool SignpostQueued(uint16_t level) {
    level_id = level;
    debug_use = 0;
    time_count = 1;
    limit_right2 = 0x2000;
    limit_left2 = 0;
    scrpos_x.f.u = (int16_t)(limit_right2 - 0x100 - SCREEN_WIDEADD2 + 0x10);
    ClearPLC();
    SignpostArtLoad();
    return plc_buffer[0].art != NULL;
}

static void Signpost_TheActsWithoutABossLoadTheirArt(void) {
    CHECK(SignpostQueued(LEVEL_ID(ZoneId_EHZ, 0)));
    CHECK(SignpostQueued(LEVEL_ID(ZoneId_CPZ, 0)));
    CHECK(SignpostQueued(LEVEL_ID(ZoneId_HTZ, 0)));
    CHECK(SignpostQueued(LEVEL_ID(ZoneId_CNZ, 0)));
}

static void Signpost_TheBossActsLoadNothing(void) {
    CHECK(!SignpostQueued(LEVEL_ID(ZoneId_EHZ, 1)));
    CHECK(!SignpostQueued(LEVEL_ID(ZoneId_HTZ, 1)));
    CHECK(!SignpostQueued(LEVEL_ID(ZoneId_OOZ, 1)));
    CHECK(!SignpostQueued(LEVEL_ID(ZoneId_MCZ, 1)));
    CHECK(!SignpostQueued(LEVEL_ID(ZoneId_CNZ, 1))); // (its boss's art would be cleared: the ship, the body, the explosions)
    CHECK(!SignpostQueued(LEVEL_ID(ZoneId_CPZ, 1)));
    CHECK(!SignpostQueued(LEVEL_ID(ZoneId_ARZ, 1)));
}

static void Signpost_WaitsForTheEndOfTheLevel(void) {
    level_id = LEVEL_ID(ZoneId_CPZ, 0);
    debug_use = 0;
    time_count = 1;
    limit_right2 = 0x2000;
    limit_left2 = 0;
    scrpos_x.f.u = 0x400; // far from the end
    ClearPLC();
    SignpostArtLoad();
    CHECK(plc_buffer[0].art == NULL);
}

void RegisterSignpostTests(void) {
    RUN_TEST(Signpost_TheActsWithoutABossLoadTheirArt);
    RUN_TEST(Signpost_TheBossActsLoadNothing);
    RUN_TEST(Signpost_WaitsForTheEndOfTheLevel);
}
