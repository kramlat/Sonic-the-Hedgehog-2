#include "test.h"

#include <string.h>

#include "Level.h"
#include "HTZQuake.h"
#include "SstLoad.h"
#include "Object.h"
#include "Object/DHZBadniks.h"
#include "Object/DHZBoss.h"
#include "Object/CNZBoss.h"
#include "Object/OOZBoss.h"
#include "Object/MTZBoss.h"
#include "Object/CNZObjects.h"
#include "Object/BossCommon.h"
#include "Object/CharControl.h"
#include "Object/DHZObjects.h"
#include "Object/DHZVines.h"
#include "Object/Sonic.h"
#include "Object/Tails.h"
#include "Solid.h"
#include "EnginePLC.h"
#include "Oscillatory Routines.h"

// Dust Hill's objects (and the collapsing platform it shares with Oil Ocean): the platform that breaks, the stomper, the switch and the drawbridge it lowers, the boxes on their path, the chain of spiked balls and
// the platform with spikes at its sides.

void Obj_CollapsingPlatform(Sst *obj);
void Obj_OOZBreakBoost(Sst *self);
void Obj_Monitor(Sst *self);
signed int ReactToItem(Sst *self);
void Obj_MonitorItem(Sst *self);
void Obj_Explosion(Sst *self);
void Obj_OOZFan(Sst *self);
void Obj_Octus(Sst *self);
void Obj_Aquis(Sst *self);
void Spikes_HurtCharacter(Sst *self, Sst *chr, int who);
extern const uint8_t Mappings_DHZVines[]; // (the resources define their tables: a test names them from here)
extern const uint8_t Mappings_SFZHookChain[];

// (the slots below $20 are taken, so that the objects a test makes get the slots the old tests named)
static void SstTestFiller(Sst *self) { (void)self; }

static void Reset(int zone) {
    Sst_Reset();
    sst_load_everything_in_range = true;
    for (int i = 4; i < 0x20; i++)
        sst[i].code = SstTestFiller;
    memset(f_switch, 0, sizeof(f_switch));
    level_id = LEVEL_ID(zone, 0);
    scrpos_x.v = scrpos_y.v = 0;
    scrpos_x.f.u = 0;
    limit_btm2 = 0x720;
    player->routine = 2;
    player->y_radius = 19;
    player->x_pos.f.u = 0x1000;
    player->y_pos.f.u = 0x1000;
    {
        static const SstObjectIndex index_ = {sst_object_codes, 0x100};
        SstLoadCamera camera = {(int16_t)scrpos_x.f.u, (int16_t)scrpos_y.f.u, false, 0xFFF};
        SstLoad_Init(&index_, NULL, false, &camera);
    }
}

static Sst *Spawn(uint8_t type, uint8_t subtype, int16_t x, int16_t y) {
    SstLoad_Seek((int16_t)(x - 0x100)); // (the loader keeps what is near the camera: the camera is where the object is)
    Sst *obj = Sst_Spawn(type, x, y);
    obj->subtype = subtype;
    return obj;
}

// Sonic standing on a flat top at `top`
static void StandOn(int16_t x, int16_t top) {
    player->x_pos.f.u = x;
    player->y_pos.f.u = (int16_t)(top - player->y_radius);
    player->status &= (uint8_t)~SST_PSTATUS_IN_AIR;
    player->status |= SST_PSTATUS_OBJECT_STAND;
}

static int CountOf(uint8_t type, uint8_t routine) {
    int n = 0;
    for (int i = 0x20; i < 0x60; i++)
        n += sst[i].code == sst_object_codes[type] && sst[i].routine == routine;
    return n;
}

// --- 1F ---

static void DHZ_TheCollapsingPlatformBreaksIntoSixPiecesOnceStoodOn(void) {
    Reset(ZoneId_MCZ);
    Sst *obj = Spawn(0x1F, 0, 200, 400);
    Obj_CollapsingPlatform(obj);
    CHECK_EQ(obj->width_pixels, 0x20);
    CHECK_EQ(obj->art_tile & 0x7FF, 0x3F4);
    for (int i = 0; i < 2400; i++) { // (nobody on it: it stays)
        Obj_CollapsingPlatform(obj);
        CHECK_EQ(obj->routine, 2);
    }
    for (int i = 0; i < 30 && obj->routine != 6; i++) {
        StandOn(200, 400 - 0x10);
        Obj_CollapsingPlatform(obj);
    }
    CHECK_EQ(obj->routine, 6); // (it has let go)
    CHECK_EQ(obj->mapping_frame, 1);
    CHECK_EQ(CountOf(0x1F, 6), 6); // (the platform itself is the first piece)
    CHECK_EQ(obj->own.u8[0x10] <= 0x1A, true);
}

static void DHZ_TheCollapsingPlatformOfOilOceanIsWiderAndHasSevenPieces(void) {
    Reset(ZoneId_OOZ);
    Sst *obj = Spawn(0x1F, 0, 200, 400);
    Obj_CollapsingPlatform(obj);
    CHECK_EQ(obj->width_pixels, 0x40);
    CHECK_EQ(obj->art_tile & 0x7FF, 0x39D);
    for (int i = 0; i < 30 && obj->routine != 6; i++) {
        StandOn(200, 400 - 0x10);
        Obj_CollapsingPlatform(obj);
    }
    CHECK_EQ(CountOf(0x1F, 6), 7);
}

// The platform keeps the facing its layout gave it (the prototype ORs the level flag in), and so do its pieces
static void DHZ_TheCollapsingPlatformKeepsItsFacingAndItsPiecesTakeIt(void) {
    for (int zone = 0; zone < 2; zone++) {
        Reset(zone ? ZoneId_OOZ : ZoneId_MCZ);
        Sst *obj = Spawn(0x1F, 0, 200, 400);
        obj->render_flags |= SST_RENDER_X_FLIP;
        obj->status |= SST_STATUS_X_FLIP;
        Obj_CollapsingPlatform(obj);
        CHECK((obj->render_flags & SST_RENDER_X_FLIP));
        CHECK((obj->render_flags & SST_RENDER_LEVEL));
        for (int i = 0; i < 30 && obj->routine != 6; i++) {
            StandOn(200, 400 - 0x10);
            Obj_CollapsingPlatform(obj);
        }
        int pieces = 0;
        for (int i = 0x20; i < 0x60; i++)
            if (sst[i].code == sst_object_codes[0x1F] && sst[i].routine == 6) {
                pieces++;
                CHECK((sst[i].render_flags & SST_RENDER_X_FLIP));
            }
        CHECK(pieces >= 6);
    }
}

// A piece falls when its delay has gone: the others wait
static void DHZ_PiecesFallOneAfterAnother(void) {
    Reset(ZoneId_MCZ);
    Sst *obj = Spawn(0x1F, 0, 200, 400);
    Obj_CollapsingPlatform(obj);
    for (int i = 0; i < 30 && obj->routine != 6; i++) {
        StandOn(200, 400 - 0x10);
        Obj_CollapsingPlatform(obj);
    }
    int16_t y[0x60];
    for (int i = 0x20; i < 0x60; i++)
        y[i] = sst[i].y_pos.f.u;
    for (int frame = 0; frame < 14; frame++)
        for (int i = 0x20; i < 0x60; i++)
            if (sst[i].code == sst_object_codes[0x1F] && sst[i].routine == 6) {
                sst[i].render_flags |= SST_RENDER_ON_SCREEN; // (a piece that has fallen out of sight is gone: they are in sight here)
                Obj_CollapsingPlatform(&sst[i]);
            }
    int fallen = 0;
    for (int i = 0x21; i < 0x60; i++)
        fallen += sst[i].code == sst_object_codes[0x1F] && sst[i].y_pos.f.u > y[i];
    CHECK(fallen >= 1 && fallen < 5); // (only the piece with the shortest delay, $02, has begun to fall by now: $0A has not got going)
}

// --- 2A ---

static void DHZ_TheStomperRisesAPixelAFrameAndDropsEight(void) {
    Reset(ZoneId_MCZ);
    Sst *obj = Spawn(0x2A, 0, 200, 500);
    Obj_DHZStomper(obj);
    CHECK_EQ(obj->y_pos.f.u, 499);
    for (int i = 1; i < 0x60; i++)
        Obj_DHZStomper(obj);
    CHECK_EQ(obj->y_pos.f.u, 500 - 0x60); // (at the top)
    Obj_DHZStomper(obj);
    CHECK_EQ(obj->y_pos.f.u, 500 - 0x58); // (and dropping)
    for (int i = 0; i < 11; i++)
        Obj_DHZStomper(obj);
    CHECK_EQ(obj->y_pos.f.u, 500); // (12 frames to the bottom)
    Obj_DHZStomper(obj);
    CHECK_EQ(obj->y_pos.f.u, 499); // (rising again)
}

// --- 47 and 77 ---

static void DHZ_TheSwitchIsDownWhileSomeoneStandsOnIt(void) {
    Reset(ZoneId_MCZ);
    Sst *obj = Spawn(0x47, 3, 200, 400);
    Obj_Switch(obj);
    CHECK_EQ(obj->y_pos.f.u, 404); // (4 lower than where it is placed)
    obj->render_flags |= SST_RENDER_ON_SCREEN;
    StandOn(200, 404 - 5);
    Obj_Switch(obj);
    CHECK_EQ(f_switch[3], 1);
    CHECK_EQ(obj->mapping_frame, 1);
    player->x_pos.f.u = 0x1000; // (steps off)
    player->status |= SST_PSTATUS_IN_AIR;
    player->status &= (uint8_t)~SST_PSTATUS_OBJECT_STAND;
    Obj_Switch(obj);
    CHECK_EQ(f_switch[3], 0);
    CHECK_EQ(obj->mapping_frame, 0);
}

static void DHZ_ASwitchWithBitSixSetsTheTopBit(void) {
    Reset(ZoneId_MCZ);
    Sst *obj = Spawn(0x47, 0x42, 200, 400);
    Obj_Switch(obj);
    obj->render_flags |= SST_RENDER_ON_SCREEN;
    StandOn(200, 404 - 5);
    Obj_Switch(obj);
    CHECK_EQ(f_switch[2], 0x80);
}

static void DHZ_TheDrawbridgeLowersAtStartAndRaisesOnceItsSwitchIsDown(void) {
    Reset(ZoneId_MCZ);
    Sst *obj = Spawn(0x77, 5, 200, 400);
    for (int i = 0; i < 20; i++)
        Obj_DHZGate(obj);
    CHECK_EQ(obj->mapping_frame, 0); // (flat)
    f_switch[5] = 1;
    for (int i = 0; i < 20; i++)
        Obj_DHZGate(obj);
    CHECK_EQ(obj->anim, 1);
    CHECK_EQ(obj->mapping_frame, 4); // (up: the alpha has five frames)
    f_switch[5] = 0;
    for (int i = 0; i < 20; i++)
        Obj_DHZGate(obj);
    CHECK_EQ(obj->mapping_frame, 4); // (only once)
}

static void DHZ_ARaisedDrawbridgeLetsGoOfWhoeverStandsOnIt(void) {
    Reset(ZoneId_MCZ);
    Sst *obj = Spawn(0x77, 5, 200, 400);
    f_switch[5] = 1;
    for (int i = 0; i < 20; i++)
        Obj_DHZGate(obj);
    obj->status |= 1 << 3;
    player->status |= SST_PSTATUS_OBJECT_STAND;
    Obj_DHZGate(obj);
    CHECK(!((player->status & SST_PSTATUS_OBJECT_STAND) != 0));
    CHECK_EQ(obj->status & 0x18, 0);
}

// --- 6A ---

static void DHZ_TheBoxesMakeTwoMoreAndMoveOnAPath(void) {
    Reset(ZoneId_MCZ);
    Sst *obj = Spawn(0x6A, 0x18, 200, 400);
    Obj_RotatingBoxes(obj);
    CHECK_EQ(CountOf(0x6A, 0), 2); // (the two others wait to start)
    CHECK_EQ(sst[0x21].x_pos.f.u, 200 + 0x40);
    CHECK_EQ(sst[0x21].y_pos.f.u, 400 + 0x40);
    CHECK_EQ(sst[0x22].x_pos.f.u, 200 - 0x40);
    CHECK_EQ(sst[0x21].subtype, 6);
    CHECK_EQ(sst[0x22].subtype, 0xC);
    CHECK_EQ(obj->x_vel, 0x100); // (the parent starts on the step after the cycle's last: to the right)
    for (int i = 0; i < 0x40; i++)
        Obj_RotatingBoxes(obj);
    CHECK_EQ(obj->x_pos.f.u, 200 + 0x40); // (a pixel a frame for the step's $40 frames)
    CHECK_EQ(obj->y_pos.f.u, 400);
    CHECK_EQ(obj->y_vel, 0x100); // (the first step: down)
    CHECK_EQ(obj->x_vel, 0);
}

static void DHZ_FlippedBoxesTurnTheOtherWay(void) {
    Reset(ZoneId_MCZ);
    Sst *obj = Spawn(0x6A, 0x18, 200, 400);
    obj->status |= SST_STATUS_X_FLIP;
    Obj_RotatingBoxes(obj);
    CHECK_EQ(obj->x_vel, -0x100);
    CHECK_EQ(sst[0x21].subtype, 0xC); // (the one on the right swaps with the left)
    CHECK_EQ(sst[0x22].subtype, 6);
}

// --- 75 ---

static void DHZ_TheSpikedBallIsAtTheEndOfItsChain(void) {
    Reset(ZoneId_MCZ);
    Sst *obj = Spawn(0x75, 0x16, 200, 400); // 6 links, speed $80
    Obj_SpikeballChain(obj);
    CHECK_EQ(obj->collision_flags, 0x9A);
    CHECK_EQ(sst[0x21].child_count, 6);
    CHECK((sst[0x21].render_flags & SST_RENDER_MULTI_DRAW));
    CHECK_EQ(obj->x_pos.f.u, 200 + 6 * 16); // (the angle starts at 0: to the right)
    CHECK_EQ(obj->y_pos.f.u, 400);
    CHECK_EQ(sst[0x21].children[0].x, 200);
    CHECK_EQ(sst[0x21].children[5].x, 200 + 5 * 16);
    CHECK_EQ(sst[0x21].children[5].frame, 1);
}

static void DHZ_TheFlipOfTheObjectGivesTheStartingAngle(void) {
    Reset(ZoneId_MCZ);
    Sst *obj = Spawn(0x75, 0x16, 200, 400);
    obj->status = 1; // x flip: ror #2 puts bit 0 at bit 6: a quarter turn
    Obj_SpikeballChain(obj);
    CHECK_EQ(obj->x_pos.f.u, 200); // (sine at $40: straight down)
    CHECK_EQ(obj->y_pos.f.u, 400 + 6 * 16);
}

static void DHZ_TheChainTurnsBySpeedTimesEightAFrame(void) {
    Reset(ZoneId_MCZ);
    Sst *obj = Spawn(0x75, 0x16, 200, 400);
    Obj_SpikeballChain(obj);
    for (int i = 0; i < 128; i++) // 128 frames of $80 is a quarter of the angle's range, and the first frame's: straight down
        Obj_SpikeballChain(obj);
    CHECK_EQ(obj->x_pos.f.u, 200);
    CHECK_EQ(obj->y_pos.f.u, 400 + 6 * 16);
    for (int i = 0; i < 128; i++)
        Obj_SpikeballChain(obj);
    CHECK_EQ(obj->x_pos.f.u, 200 - 6 * 16); // (and then to the left)
    CHECK_EQ(obj->y_pos.f.u, 400);
}

static void DHZ_ASubtypeOfLowNibbleFIsASolidBlock(void) {
    Reset(ZoneId_MCZ);
    Sst *obj = Spawn(0x75, 0x0F, 200, 400);
    Obj_SpikeballChain(obj);
    CHECK_EQ(obj->mapping_frame, 2);
    CHECK_EQ(obj->routine, 4);
    CHECK_EQ(obj->collision_flags, 0);
    CHECK(sst[0x21].code == NULL); // (no chain)
}

// --- 76 ---

static void DHZ_ThePlatformWithSpikesSlidesAwayWhenSomeoneComesNear(void) {
    Reset(ZoneId_MCZ);
    Sst *obj = Spawn(0x76, 0, 300, 400);
    Obj_PlatformSpikes(obj);
    CHECK_EQ(obj->width_pixels, 0x40);
    for (int i = 0; i < 5; i++)
        Obj_PlatformSpikes(obj);
    CHECK_EQ(obj->x_pos.f.u, 300); // (nobody near)
    player->x_pos.f.u = 300 - 0x80; // within the strip $C0 to $41 to the left, at its height, on the ground
    player->y_pos.f.u = 400;
    player->status &= (uint8_t)~SST_PSTATUS_IN_AIR;
    Obj_PlatformSpikes(obj);
    Obj_PlatformSpikes(obj);
    CHECK_EQ(obj->own.u8[0], 2);
    for (int i = 0; i < 0x7F; i++)
        Obj_PlatformSpikes(obj);
    CHECK_EQ(obj->x_pos.f.u, 300 - 0x80); // (it has gone $80 to the left and stops)
    Obj_PlatformSpikes(obj);
    CHECK_EQ(obj->x_pos.f.u, 300 - 0x80);
}

static void DHZ_ACharacterInTheAirDoesNotSetItOff(void) {
    Reset(ZoneId_MCZ);
    Sst *obj = Spawn(0x76, 0, 300, 400);
    Obj_PlatformSpikes(obj);
    player->x_pos.f.u = 300 - 0x80;
    player->y_pos.f.u = 400;
    player->status |= SST_PSTATUS_IN_AIR;
    for (int i = 0; i < 10; i++)
        Obj_PlatformSpikes(obj);
    CHECK_EQ(obj->own.u8[0], 0);
}

static void DHZ_AFlippedPlatformWithSpikesSlidesRight(void) {
    Reset(ZoneId_MCZ);
    Sst *obj = Spawn(0x76, 0, 300, 400);
    obj->status |= SST_STATUS_X_FLIP;
    Obj_PlatformSpikes(obj);
    player->x_pos.f.u = 300 + 0x80 - 0x100 + 0x80 - 0x40; // (the strip is the same one turned round: $40 to $C0 to the right)
    player->x_pos.f.u = 300 + 0x80;
    player->y_pos.f.u = 400;
    player->status &= (uint8_t)~SST_PSTATUS_IN_AIR;
    for (int i = 0; i < 12; i++)
        Obj_PlatformSpikes(obj);
    CHECK(obj->x_pos.f.u > 300);
}

// --- 15, the swinging platform (Dust Hill's Newton's cradle: three side by side, the first swinging out to the left only, the middle hanging still, the third out to the right only) ---

#include "Object/NGHZObjects.h"
#include "SstLoad.h"

static void RunSwing(Sst *obj, int frames, int16_t *min_x, int16_t *max_x, int16_t *min_y, int16_t *max_y) {
    *min_x = *min_y = 0x7FFF;
    *max_x = *max_y = -0x7FFF;
    for (int i = 0; i < frames; i++) {
        player->routine = 2;
        OscillateNumDo();
        Obj_SwingDispatch(obj);
        if (obj->x_pos.f.u < *min_x) *min_x = obj->x_pos.f.u;
        if (obj->x_pos.f.u > *max_x) *max_x = obj->x_pos.f.u;
        if (obj->y_pos.f.u < *min_y) *min_y = obj->y_pos.f.u;
        if (obj->y_pos.f.u > *max_y) *max_y = obj->y_pos.f.u;
    }
}

static void DHZ_TheCradleSwingsOutOnOneSideOnlyAndTheMiddleHangsStill(void) {
    Reset(ZoneId_MCZ);
    OscillateNumInit();
    Sst_Reset();
    scrpos_x.f.u = 0x80;
    static const SstObjectIndex index_ = {sst_object_codes, 0x100};
    SstLoadCamera camera = {0x80, 0x500, false, 0xFFF};
    SstLoad_Init(&index_, NULL, false, &camera);
    Sst *left = Sst_Spawn(0x15, 0x100, 0x600);
    left->subtype = 0x18;
    Sst *mid = Sst_Spawn(0x15, 0x130, 0x600);
    mid->subtype = 0x28;
    Sst *right = Sst_Spawn(0x15, 0x160, 0x600);
    right->subtype = 0x38;
    int16_t lx0, lx1, ly0, ly1, mx0, mx1, my0, my1, rx0, rx1, ry0, ry1;
    // (the platform hangs 8 links of 16, and 8, below its hub: at rest it is straight under it)
    Obj_SwingDispatch(left); Obj_SwingDispatch(mid); Obj_SwingDispatch(right);
    OscillateNumInit();
    for (int i = 0; i < 300; i++) {
        OscillateNumDo();
        Obj_SwingDispatch(left); Obj_SwingDispatch(mid); Obj_SwingDispatch(right);
        if (i == 0) { lx0 = lx1 = left->x_pos.f.u; ly0 = ly1 = left->y_pos.f.u; mx0 = mx1 = mid->x_pos.f.u; my0 = my1 = mid->y_pos.f.u; rx0 = rx1 = right->x_pos.f.u; ry0 = ry1 = right->y_pos.f.u; }
        if (left->x_pos.f.u < lx0) lx0 = left->x_pos.f.u;
        if (left->x_pos.f.u > lx1) lx1 = left->x_pos.f.u;
        if (right->x_pos.f.u < rx0) rx0 = right->x_pos.f.u;
        if (right->x_pos.f.u > rx1) rx1 = right->x_pos.f.u;
        if (mid->x_pos.f.u < mx0) mx0 = mid->x_pos.f.u;
        if (mid->x_pos.f.u > mx1) mx1 = mid->x_pos.f.u;
        (void)ly0; (void)ly1; (void)my0; (void)my1; (void)ry0; (void)ry1;
    }
    CHECK_EQ(mx0, mx1); // (the middle one does not move)
    CHECK(lx0 < 0x100 - 0x40); // (the left one swings out to the left)
    CHECK(lx1 <= 0x100 + 1);   // (and not to the right)
    CHECK(rx1 > 0x160 + 0x40); // (the right one swings out to the right)
    CHECK(rx0 >= 0x160 - 1);   // (and not to the left)
}

// --- 9E, A3 and 57 (the alpha's) ---

static void DHZ_TheCrawltonWaitsUntilSonicIsNearThenTurnsToHimAndCrawls(void) {
    Reset(ZoneId_MCZ);
    Sst *head = Spawn(0x9E, 0x22, 200, 400);
    player->x_pos.f.u = 200 + 0x100; // (far)
    player->y_pos.f.u = 400;
    Obj_Crawlton(head); // the set up: its body is made
    CHECK(sst[0x21].code == sst_object_codes[0x9E]);
    CHECK_EQ(sst[0x21].child_count, 7);
    CHECK((sst[0x21].render_flags & SST_RENDER_MULTI_DRAW));
    for (int i = 0; i < 5; i++)
        Obj_Crawlton(head);
    CHECK_EQ(head->x_vel, 0);
    player->x_pos.f.u = 250;
    Obj_Crawlton(head); // he is near: it turns
    CHECK((head->render_flags & SST_RENDER_X_FLIP));
    CHECK_EQ(head->x_vel, 0x100); // (50 pixels away: 50 * 8 = $190, less its low byte)
    for (int i = 0; i < 0x11 + 0x1C; i++)
        Obj_Crawlton(head);
    CHECK(head->x_pos.f.u > 200); // it has crawled towards him
}

static void DHZ_TheCrawltonBodyFollowsTheHeadWhileItCrawls(void) {
    Reset(ZoneId_MCZ);
    Sst *head = Spawn(0x9E, 0x22, 200, 400);
    player->x_pos.f.u = 250;
    player->y_pos.f.u = 400;
    Obj_Crawlton(head);
    Sst *body = &sst[0x21];
    for (int i = 0; i < 3; i++) {
        Obj_Crawlton(head);
        Obj_Crawlton(body);
    }
    // gathering, then crawling
    for (int i = 0; i < 0x11 + 0x1C; i++) {
        Obj_Crawlton(head);
        Obj_Crawlton(body);
    }
    CHECK(body->children[0].x > 200); // the first piece has moved on first
    CHECK(body->children[0].x >= body->children[6].x);
}

static void DHZ_TheFlasherDriftsFlashesAndGlowsDangerously(void) {
    Reset(ZoneId_MCZ);
    Sst *obj = Spawn(0xA3, 0x2C, 300, 300);
    Obj_Flasher(obj);
    CHECK_EQ(obj->collision_flags, 0x06);
    for (int i = 0; i < 0x41; i++)
        Obj_Flasher(obj);
    CHECK_EQ(obj->routine, 4); // drifting
    for (int i = 0; i < 0x81; i++)
        Obj_Flasher(obj);
    CHECK_EQ(obj->routine, 6);       // it stops to flash
    CHECK_EQ(obj->collision_flags, 0x86);   // and hurts
    for (int i = 0; i < 0x100 && obj->routine != 8; i++)
        Obj_Flasher(obj);
    CHECK_EQ(obj->routine, 8);       // the glow
    for (int i = 0; i < 0x200 && obj->routine != 4; i++)
        Obj_Flasher(obj);
    CHECK_EQ(obj->routine, 4);       // and it drifts again
    CHECK_EQ(obj->collision_flags, 0x06);
}

// The pre-beta's Dust Hill boss: it starts at ($21A0,$560) already going down (rocks come loose above Y $620), digs on to $660, charges along the floor at the side Sonic is on (the drill out, and its box in front
// hurts) to the wall, and comes up to $560 on the other side
static void DHZ_TheBossDigsDropsRocksAndChargesAlongTheFloor(void) {
    const uint16_t kept_frame = frame_count; // (restored: the zones' background checks run off it)
    const uint32_t kept_seed = random_seed.v; // (restored as well)
    const uint16_t kept_limits[6] = { limit_left2, limit_right2, limit_top2, limit_btm1, limit_btm2, limit_right1 }; // (restored at the end: the zones' background checks read them)
    Reset(ZoneId_MCZ);
    level_id = LEVEL_ID(ZoneId_MCZ, 1);
    boss_flag = 5;
    Sst *boss = Spawn(0x57, 0, 0, 0);
    boss->x_pos.f.u = 0;
    Obj_DHZBoss(boss);
    CHECK_EQ(boss->child_count, 4);
    CHECK((boss->render_flags & SST_RENDER_MULTI_DRAW));
    CHECK_EQ(boss->x_pos.f.u, 0x21A0);
    CHECK_EQ(boss->y_pos.f.u, 0x560);
    CHECK_EQ(boss->collision_property, 8);
    player->x_pos.f.u = 0x2180;
    int rocks = 0, dangerous = 0;
    int16_t lowest = 0, left = 0x7FFF, right = 0;
    bool box = false;
    for (int i = 0; i < 2400; i++) {
        scrpos_x.f.u = 0x20F0;
        scrpos_y.f.u = 0x5C0;
        frame_count++;
        Obj_DHZBoss(boss);
        if (boss->y_pos.f.u > lowest)
            lowest = boss->y_pos.f.u;
        if (boss->y_pos.f.u >= 0x660) {
            if (boss->x_pos.f.u < left)
                left = boss->x_pos.f.u;
            if (boss->x_pos.f.u > right)
                right = boss->x_pos.f.u;
        }
        int16_t cx, cy, hw, hh;
        for (int b = 0; DHZBoss_Box(boss, b, &cx, &cy, &hw, &hh); b++)
            box |= hh == 4 && cy == boss->y_pos.f.u + 4;
        int now = 0;
        for (int j = 0x21; j < 0x60; j++) {
            if (sst[j].code != sst_object_codes[0x57] || sst[j].routine != 4)
                continue;
            now++;
            dangerous += sst[j].collision_flags == 0xAD;
            Obj_DHZBoss(&sst[j]);
        }
        rocks += now;
    }
    CHECK(rocks > 0);       // (they come loose while it digs)
    CHECK(dangerous > 0);   // (and some hurt)
    CHECK(box);             // (the drill out)
    CHECK_EQ(lowest, 0x660);
    CHECK(left <= 0x2120 + 8 && right >= 0x2200 - 8); // (it charges to either wall)
    ClearPLC();
    boss_flag = 0;
    dhz_shake = 0; // (the screen shake is a global the zones' background checks read)
    frame_count = kept_frame;
    random_seed.v = kept_seed;
    Sst_Reset();
    limit_left2 = kept_limits[0]; limit_right2 = kept_limits[1]; limit_top2 = kept_limits[2]; limit_btm1 = kept_limits[3]; limit_btm2 = kept_limits[4]; limit_right1 = kept_limits[5];
}

// DynResize_DHz (act 2) of the pre-beta: X $2080 fixes the left edge and the lower limit ($5D0); X $20F0 shuts the arena, Boss_Flag 5; then the top is held at $5C8 and, $5A frames on, the boss is made
static void DHZ_TheEventsShutTheArenaAndMakeTheBossLast(void) {
    const uint16_t kept_limits[6] = { limit_left2, limit_right2, limit_top2, limit_btm1, limit_btm2, limit_right1 }; // (restored at the end: the zones' background checks read them)
    Reset(ZoneId_MCZ);
    level_id = LEVEL_ID(ZoneId_MCZ, 1);
    dle_routine = 0;
    scrpos_x.f.u = 0x2080 - SCREEN_WIDEADD2 - 1;
    DHZBoss_Events();
    CHECK_EQ(dle_routine, 0);
    scrpos_x.f.u = 0x2080 - SCREEN_WIDEADD2;
    DHZBoss_Events();
    CHECK_EQ(dle_routine, 2);
    CHECK_EQ(limit_btm1, 0x5D0 - SCREEN_TALLADD);
    CHECK_EQ(boss_flag, 0);
    scrpos_x.f.u = 0x20F0 - SCREEN_WIDEADD2;
    DHZBoss_Events();
    CHECK_EQ(dle_routine, 4);
    CHECK_EQ(boss_flag, 5);
    CHECK_EQ(limit_left2, 0x20F0 - SCREEN_WIDEADD2);
    CHECK_EQ(limit_right2, 0x20F0 + SCREEN_WIDEADD2);
    { int made = 0; for (int j = 1; j < 0x60; j++) made += sst[j].code == sst_object_codes[0x57]; CHECK_EQ(made, 0); }
    scrpos_y.f.u = 0x5C8 - SCREEN_TALLADD;
    for (int i = 0; i < 0x59; i++)
        DHZBoss_Events();
    CHECK_EQ(limit_top2, 0x5C8 - SCREEN_TALLADD);
    CHECK_EQ(dle_routine, 4);
    DHZBoss_Events();
    CHECK_EQ(dle_routine, 6);
    { int made = 0; for (int j = 1; j < 0x60; j++) made += sst[j].code == sst_object_codes[0x57]; CHECK_EQ(made, 1); }
    ClearPLC();
    boss_flag = 0;
    dle_routine = 0;
    limit_left2 = kept_limits[0]; limit_right2 = kept_limits[1]; limit_top2 = kept_limits[2]; limit_btm1 = kept_limits[3]; limit_btm2 = kept_limits[4]; limit_right1 = kept_limits[5];
}


// --- Casino Night (the alpha's) ---

static void CNZ_TheRoundBumperThrowsWhoeverTouchesIt(void) {
    Reset(ZoneId_CNZ);
    Sst *bumper = Spawn(0x44, 0, 300, 300);
    player->x_pos.f.u = 300;
    player->y_pos.f.u = 330; // below it
    Obj_CNZBumper(bumper); // set up
    CHECK_EQ(bumper->collision_flags, 0xD7);
    CHECK_EQ(player->y_vel, 0);
    bumper->collision_property = 1; // (the touch response counts it)
    Obj_CNZBumper(bumper);
    CHECK(player->y_vel > 0x500);                   // thrown away from it: straight down, at $700 or so
    CHECK(((player->status & SST_PSTATUS_IN_AIR) != 0));
    CHECK_EQ(bumper->collision_property, 0);
    CHECK_EQ(bumper->anim, 1);
}

// The pre-beta's Casino Night boss: it starts at ($29D0,$590) and is on its track at Y $63A at once; it goes to and fro between $28A0 and $29E0, opens its catchers (and lets a ball fall from one side), and the ball
// falls, rolls and breaks away
static void CNZ_TheBossGoesToAndFroAndLetsTheBallFall(void) {
    Reset(ZoneId_CNZ);
    level_id = LEVEL_ID(ZoneId_CNZ, 1);
    boss_flag = 6;
    Sst *boss = Spawn(0x51, 0, 0, 0);
    Obj_CNZBoss(boss);
    CHECK_EQ(boss->child_count, 4);
    CHECK_EQ(boss->collision_property, 2);
    CHECK_EQ(boss->x_pos.f.u, 0x29D0);
    CHECK_EQ(boss->y_pos.f.u, 0x590);
    player->x_pos.f.u = 0x2800;
    int balls = 0;
    int16_t min_x = 0x7FFF, max_x = 0;
    bool open = false;
    for (int i = 0; i < 1500; i++) {
        scrpos_x.f.u = 0x2700;
        scrpos_y.f.u = 0x500;
        frame_count++;
        Obj_CNZBoss(boss);
        if (boss->x_pos.f.u < min_x)
            min_x = boss->x_pos.f.u;
        if (boss->x_pos.f.u > max_x)
            max_x = boss->x_pos.f.u;
        {
            int16_t cx, cy, hw, hh;
            open |= CNZBoss_CatcherBox(boss, &cx, &cy, &hw, &hh);
        }
        int now = 0;
        for (int j = 0x21; j < 0x60; j++) {
            if (sst[j].code == sst_object_codes[0x51] && sst[j].routine == 4) {
                now++;
                Obj_CNZBoss(&sst[j]);
            }
        }
        if (now > balls)
            balls = now;
    }
    CHECK(balls > 0);                           // (it lets a ball fall)
    CHECK(open);                                // (its catchers hurt while out)
    CHECK(min_x <= 0x28A0 + 4);                 // (the left end of its track)
    CHECK(max_x >= 0x29E0 - 4 && max_x <= 0x29E0 + 8); // (and the right)
    CHECK(boss->y_pos.f.u <= 0x63A + 6 && boss->y_pos.f.u >= 0x63A - 6);
    ClearPLC();
    boss_flag = 0;
}

// DynResize_CNz (act 2) of the pre-beta: X $27C0 fixes the left edge and the lower limit; X $2890 shuts the arena between $2860 and $28E0, Boss_Flag 6; then the top is held at $4E0 and, $5A frames on, the boss is made
static void CNZ_TheEventsShutTheArenaAndMakeTheBossLast(void) {
    const uint16_t kept_limits[6] = { limit_left2, limit_right2, limit_top2, limit_btm1, limit_btm2, limit_right1 }; // (restored at the end: the zones' background checks read them)
    Reset(ZoneId_CNZ);
    dle_routine = 0;
    level_id = LEVEL_ID(ZoneId_CNZ, 1);
    scrpos_x.f.u = 0x27C0 - SCREEN_WIDEADD2 - 1;
    CNZBoss_Events();
    CHECK_EQ(dle_routine, 0);
    scrpos_x.f.u = 0x27C0 - SCREEN_WIDEADD2;
    CNZBoss_Events();
    CHECK_EQ(dle_routine, 2);
    CHECK_EQ(limit_btm1, 0x062E - SCREEN_TALLADD);
    CHECK_EQ(boss_flag, 0);
    scrpos_x.f.u = 0x2890 - SCREEN_WIDEADD2;
    CNZBoss_Events();
    CHECK_EQ(dle_routine, 4);
    CHECK_EQ(boss_flag, 6);
    CHECK_EQ(limit_left2, 0x2860 - SCREEN_WIDEADD2);
    CHECK_EQ(limit_right2, 0x28E0 + SCREEN_WIDEADD2);
    { int made = 0; for (int j = 1; j < 0x60; j++) made += sst[j].code == sst_object_codes[0x51]; CHECK_EQ(made, 0); } // (the boss is made later)
    scrpos_y.f.u = 0x4E0 - SCREEN_TALLADD;
    for (int i = 0; i < 0x59; i++)
        CNZBoss_Events();
    CHECK_EQ(limit_top2, 0x4E0 - SCREEN_TALLADD);
    CHECK_EQ(dle_routine, 4);
    CNZBoss_Events();
    CHECK_EQ(dle_routine, 6);
    { int made = 0; for (int j = 1; j < 0x60; j++) made += sst[j].code == sst_object_codes[0x51]; CHECK_EQ(made, 1); }
    ClearPLC();
    boss_flag = 0;
    dle_routine = 0;
    limit_left2 = kept_limits[0]; limit_right2 = kept_limits[1]; limit_top2 = kept_limits[2]; limit_btm1 = kept_limits[3]; limit_btm2 = kept_limits[4]; limit_right1 = kept_limits[5];
}


// --- Oil Ocean's boss (the pre-beta's) ---

// (runs the boss's balls and flames, which the test steps itself: the sprite loop is not running here)
static int OOZ_RunBalls(int *flames) {
    int balls = 0;
    for (int j = 0x20; j < SST_COUNT; j++) {
        if (sst[j].code == sst_object_codes[0x55] && sst[j].routine == 8) {
            if (sst[j].routine_secondary == 4)
                (*flames)++;
            else if (sst[j].routine_secondary == 2)
                balls++;
            Obj_OOZBoss(&sst[j]);
        }
    }
    return balls;
}

// It dives out of the oil to Y $290, bobs, goes up a little and down again; with no hit it goes on to hover, wearing a tail of eight links, and spits balls of oil: the ones at the lowest height light a row of flames where
// they pass the middle
static void OOZ_TheBossDivesThenHoversAndSpitsBallsThatLightFlames(void) {
    const uint16_t kept_frame = frame_count;
    memset(&boss_move, 0, sizeof(boss_move));
    const dword_u kept_seed = random_seed;
    Reset(ZoneId_OOZ);
    level_id = LEVEL_ID(ZoneId_OOZ, 1);
    boss_flag = 8;
    Sst *boss = Spawn(0x55, 0, 0, 0);
    Obj_OOZBoss(boss);
    CHECK_EQ(boss->collision_property, 8);
    CHECK_EQ(boss->child_count, 0);
    CHECK_EQ(boss->collision_flags, 0x0F);
    player->x_pos.f.u = 0x2800;
    Obj_OOZBoss(boss); // (the dive starts: low in the oil, looking at the character)
    CHECK_EQ(boss->y_pos.f.u, 0x2D0);
    CHECK_EQ(boss->child_count, 1);
    CHECK(boss->render_flags & SST_RENDER_X_FLIP);
    int16_t min_y = 0x7FFF;
    bool dived = false;
    for (int i = 0; i < 700 && boss->routine == 2; i++) {
        scrpos_x.f.u = 0x2800;
        scrpos_y.f.u = 0x200;
        Obj_OOZBoss(boss);
        if (boss->y_pos.f.u < min_y)
            min_y = boss->y_pos.f.u;
        dived = true;
    }
    CHECK(dived);
    CHECK_EQ(boss->routine, 4);                // (not hit: it goes on to hover)
    CHECK(min_y <= 0x290 && min_y >= 0x28C - 6); // (it came up to about $290, and a little more)
    CHECK_EQ(boss_move.y.f.u, 0x2D0);          // (down in the oil again)
    int balls = 0, flames = 0;
    bool tailed = false;
    uint16_t cycles = 0;
    for (int i = 0; i < 12000 && flames == 0; i++) {
        scrpos_x.f.u = 0x2800;
        scrpos_y.f.u = 0x200;
        frame_count++;
        if (boss->routine == 2)
            cycles++;
        Obj_OOZBoss(boss);
        if (boss->routine == 4 && boss->child_count == 8 && boss->collision_flags == 0x8A)
            tailed = true;
        const int now = OOZ_RunBalls(&flames);
        if (now > balls)
            balls = now;
    }
    CHECK(tailed);
    CHECK(balls > 0);   // (it spits)
    CHECK(flames > 0);  // (and the low ones light flames)
    int row = 0;
    for (int i = 0; i < 40; i++) {
        OOZ_RunBalls(&flames);
        int now = 0;
        for (int j = 0x20; j < SST_COUNT; j++)
            now += sst[j].code == sst_object_codes[0x55] && sst[j].routine == 8 && sst[j].routine_secondary == 4;
        if (now > row)
            row = now;
    }
    CHECK(row >= 3);    // (a row of them: each lights the next outwards)
    frame_count = kept_frame;
    random_seed = kept_seed;
    ClearPLC();
    boss_flag = 0;
}

// A hit in the dive sends it on to swing round the arena (a circle of radius $68 about $28C0 or $29C0) before it hovers
static void OOZ_ABossThatWasHitSwingsRoundTheArena(void) {
    const uint16_t kept_frame = frame_count;
    memset(&boss_move, 0, sizeof(boss_move));
    Reset(ZoneId_OOZ);
    level_id = LEVEL_ID(ZoneId_OOZ, 1);
    boss_flag = 8;
    Sst *boss = Spawn(0x55, 0, 0, 0);
    player->x_pos.f.u = 0x2800;
    Obj_OOZBoss(boss);
    Obj_OOZBoss(boss);
    for (int i = 0; i < 40; i++) { // (the touch code counts a hit)
        scrpos_x.f.u = 0x2800;
        scrpos_y.f.u = 0x200;
        Obj_OOZBoss(boss);
    }
    boss->collision_flags = 0;
    boss->collision_property--;
    int16_t min_x = 0x7FFF, max_x = 0;
    bool orbited = false;
    for (int i = 0; i < 1500 && boss->routine != 4; i++) {
        scrpos_x.f.u = 0x2800;
        scrpos_y.f.u = 0x200;
        frame_count++;
        Obj_OOZBoss(boss);
        if (boss->routine == 6 && boss->child_count == 8) {
            orbited = true;
            if (boss->x_pos.f.u < min_x)
                min_x = boss->x_pos.f.u;
            if (boss->x_pos.f.u > max_x)
                max_x = boss->x_pos.f.u;
        }
    }
    CHECK(orbited);
    CHECK_EQ(boss->routine, 4);                 // (and then it hovers)
    CHECK_EQ(boss->collision_property, 7);
    CHECK(min_x < 0x28C0 && max_x > 0x28C0 && max_x - min_x >= 0x60); // (round the point $28C0: the character is to the left)
    frame_count = kept_frame;
    ClearPLC();
    boss_flag = 0;
}

// Beaten in the dive it blows up for $B3 frames, then sinks as the arena's right edge opens to $2D00, and is gone with the act over
static void OOZ_TheBossBlowsUpAndSinksWhenBeaten(void) {
    const uint16_t kept_frame = frame_count;
    memset(&boss_move, 0, sizeof(boss_move));
    const uint16_t kept_limits[2] = { limit_right2, limit_right1 };
    Reset(ZoneId_OOZ);
    level_id = LEVEL_ID(ZoneId_OOZ, 1);
    boss_flag = 8;
    boss_status = 0;
    Sst *boss = Spawn(0x55, 0, 0, 0);
    player->x_pos.f.u = 0x2800;
    Obj_OOZBoss(boss);
    Obj_OOZBoss(boss);
    for (int i = 0; i < 5; i++)
        Obj_OOZBoss(boss);
    boss->collision_property = 0; // (the last hit)
    boss->collision_flags = 0;
    limit_right2 = 0x28C0 + SCREEN_WIDEADD2;
    int frames = 0;
    for (; frames < 3000 && boss->code != NULL; frames++) {
        scrpos_x.f.u = 0x2800;
        scrpos_y.f.u = 0x200;
        frame_count++;
        Obj_OOZBoss(boss);
    }
    CHECK(boss->code == NULL);
    CHECK_EQ(boss_status, 1);
    CHECK_EQ(limit_right2, 0x2D00 + SCREEN_WIDEADD2);
    CHECK(frames > 0xB3);
    boss_status = 0;
    limit_right2 = kept_limits[0];
    limit_right1 = kept_limits[1];
    frame_count = kept_frame;
    ClearPLC();
    boss_flag = 0;
}

// DynResize_OOz (act 2) of the pre-beta: X $2668 fixes the left edge and the lower limit and raises the oil to $2D8; X $2880 shuts the arena between $2880 and $28C0, Boss_Flag 8; the top is held at $1D8 and, $5A
// frames on, the boss is made
static void OOZ_TheEventsShutTheArenaAndMakeTheBossLast(void) {
    const uint16_t kept_limits[6] = { limit_left2, limit_right2, limit_top2, limit_btm1, limit_btm2, limit_right1 };
    Reset(ZoneId_OOZ);
    dle_routine = 0;
    level_id = LEVEL_ID(ZoneId_OOZ, 1);
    Sst *oil = Spawn(0x07, 0, 0x2600, 0x700);
    scrpos_x.f.u = 0x2668 - SCREEN_WIDEADD2 - 1;
    OOZBoss_Events();
    CHECK_EQ(dle_routine, 0);
    scrpos_x.f.u = 0x2668 - SCREEN_WIDEADD2;
    OOZBoss_Events();
    CHECK_EQ(dle_routine, 2);
    CHECK_EQ(limit_btm1, 0x01E0 - SCREEN_TALLADD);
    CHECK_EQ(limit_left2, 0x2668 - SCREEN_WIDEADD2);
    CHECK_EQ(oil->y_pos.f.u, 0x2D8);
    CHECK_EQ(boss_flag, 0);
    scrpos_x.f.u = 0x2880 - SCREEN_WIDEADD2;
    OOZBoss_Events();
    CHECK_EQ(dle_routine, 4);
    CHECK_EQ(boss_flag, 8);
    CHECK_EQ(limit_left2, 0x2880 - SCREEN_WIDEADD2);
    CHECK_EQ(limit_right2, 0x28C0 + SCREEN_WIDEADD2);
    { int made = 0; for (int j = 1; j < 0x60; j++) made += sst[j].code == sst_object_codes[0x55]; CHECK_EQ(made, 0); }
    scrpos_y.f.u = 0x1D8 - SCREEN_TALLADD;
    for (int i = 0; i < 0x59; i++)
        OOZBoss_Events();
    CHECK_EQ(limit_top2, 0x1D8 - SCREEN_TALLADD);
    CHECK_EQ(dle_routine, 4);
    OOZBoss_Events();
    CHECK_EQ(dle_routine, 6);
    { int made = 0; for (int j = 1; j < 0x60; j++) made += sst[j].code == sst_object_codes[0x55]; CHECK_EQ(made, 1); }
    ClearPLC();
    boss_flag = 0;
    dle_routine = 0;
    limit_left2 = kept_limits[0]; limit_right2 = kept_limits[1]; limit_top2 = kept_limits[2]; limit_btm1 = kept_limits[3]; limit_btm2 = kept_limits[4]; limit_right1 = kept_limits[5];
}


// --- Metropolis's boss (the pre-beta's) ---

// (the sprite loop does not run here: the test steps the boss's objects in slot order)
static void MTZ_Step(void) {
    scrpos_x.f.u = 0x2A00;
    scrpos_y.f.u = 0x380;
    frame_count++;
    for (int j = 0x20; j < SST_COUNT; j++) {
        if (sst[j].code == sst_object_codes[0x54])
            Obj_MTZBoss(&sst[j]);
        else if (sst[j].code == sst_object_codes[0x53])
            Obj_MTZBossBall(&sst[j]);
    }
}

static int MTZ_Count(uint8_t id, int routine) {
    int n = 0;
    for (int j = 0x20; j < SST_COUNT; j++)
        n += sst[j].code == sst_object_codes[id] && (routine < 0 || sst[j].routine == routine);
    return n;
}

static Sst *MTZ_Ship(void) {
    for (int j = 0x20; j < SST_COUNT; j++)
        if (sst[j].code == sst_object_codes[0x54] && sst[j].routine == 2)
            return &sst[j];
    return NULL;
}

// A hit as the touch code counts it (the flash must be over)
static bool MTZ_Hit(Sst *ship) {
    if (ship->collision_flags == 0)
        return false;
    ship->collision_flags = 0;
    ship->collision_property--;
    return true;
}

// It starts at ($2B50,$380), makes the part that follows it and the seven balls, goes down to $4A0 and patrols between $2AD0 and $2BD0; the balls spread out; each hit lets one go, a small Robotnik walks out of it,
// and rolled into it pops
static void MTZ_TheShipPatrolsAndEachHitLetsABallGo(void) {
    const uint16_t kept_frame = frame_count;
    memset(&boss_move, 0, sizeof(boss_move));
    Reset(ZoneId_MTZ);
    level_id = LEVEL_ID(ZoneId_MTZ, 2);
    boss_flag = 7;
    Sst *ship = Spawn(0x54, 0, 0, 0);
    MTZ_Step();
    CHECK_EQ(ship->x_pos.f.u, 0x2B50);
    CHECK_EQ(ship->y_pos.f.u, 0x380);
    CHECK_EQ(ship->child_count, 2);
    CHECK_EQ(ship->collision_property, 8);
    CHECK_EQ(MTZ_Count(0x54, 6), 1);              // (the part that follows it)
    MTZ_Step();                                   // (the maker makes the balls)
    CHECK_EQ(MTZ_Count(0x53, 2), 7);
    player->x_pos.f.u = 0x2A00;
    int16_t min_x = 0x7FFF, max_x = 0;
    for (int i = 0; i < 1500; i++) {
        MTZ_Step();
        if (ship->own.u8[1] == 2) {
            if (ship->x_pos.f.u < min_x) min_x = ship->x_pos.f.u;
            if (ship->x_pos.f.u > max_x) max_x = ship->x_pos.f.u;
        }
        if (ship->own.u8[1] >= 6)
            break;
    }
    CHECK(min_x <= 0x2AD0 + 4 && max_x >= 0x2BD0 - 4);  // (to and fro)
    for (int i = 0; i < 600 && ship->own.u8[1] != 8; i++)
        MTZ_Step();
    CHECK_EQ(ship->own.u8[1], 8);                 // (the balls are out; they come in again)
    for (int i = 0; i < 400 && ship->own.u8[1] != 0; i++)
        MTZ_Step();
    CHECK_EQ(ship->own.u8[1], 0);
    // the hit
    for (int i = 0; i < 600 && ship->own.u8[1] != 2; i++)
        MTZ_Step();
    CHECK(MTZ_Hit(ship));
    MTZ_Step();
    MTZ_Step();
    CHECK_EQ(MTZ_Count(0x53, 4) + MTZ_Count(0x53, 6), 1); // (one ball is let go: it falls to the floor at once if it was low)
    CHECK_EQ(ship->own.u8[1], 0xA);
    Sst *walker = NULL;
    for (int i = 0; i < 400 && walker == NULL; i++) {
        MTZ_Step();
        for (int j = 0x20; j < SST_COUNT; j++)
            if (sst[j].code == sst_object_codes[0x53] && sst[j].routine == 6)
                walker = &sst[j];
    }
    CHECK(walker != NULL);                        // (it fell to the floor: a small Robotnik)
    if (walker != NULL) {
        CHECK_EQ(walker->y_pos.f.u, 0x4AC);
        for (int i = 0; i < 120; i++)
            MTZ_Step();
        CHECK_EQ(walker->collision_flags, 0xDA);  // (it hurts after its wait)
        walker->collision_property = 0xFE;        // (rolled into)
        for (int i = 0; i < 20 && walker->code != NULL; i++)
            MTZ_Step();
        CHECK(walker->code == NULL || walker->code != sst_object_codes[0x53]);
        CHECK_EQ(ship->own.u8[6], 0);         // (no balls out any more)
    }
    frame_count = kept_frame;
    ClearPLC();
    boss_flag = 0;
    memset(&boss_move, 0, sizeof(boss_move));
}

// After the seventh hit it stops guarding itself and dives, shooting; the eighth blows it up, and it leaves as the arena opens
static void MTZ_AfterSevenHitsTheShipDivesAndShootsAndTheEighthEndsIt(void) {
    const uint16_t kept_frame = frame_count;
    const uint16_t kept_right = limit_right2, kept_right1 = limit_right1;
    memset(&boss_move, 0, sizeof(boss_move));
    Reset(ZoneId_MTZ);
    level_id = LEVEL_ID(ZoneId_MTZ, 2);
    boss_flag = 7;
    boss_status = 0;
    Sst *ship = Spawn(0x54, 0, 0, 0);
    player->x_pos.f.u = 0x2A00;
    bool shot = false, dived = false;
    int hits = 0;
    for (int i = 0; i < 40000 && ship->code != NULL; i++) {
        MTZ_Step();
        if (ship->code == NULL)
            break;
        for (int j = 0x20; j < SST_COUNT; j++) // (the small Robotnik is rolled into at once)
            if (sst[j].code == sst_object_codes[0x53] && sst[j].routine == 6 && sst[j].collision_flags == 0xDA)
                sst[j].collision_property = 0xFE;
        if (ship->own.u8[1] == 0xE) {
            dived = true;
            if (MTZ_Count(0x54, 4) > 0)
                shot = true;
        }
        if (hits < 7 && ship->own.u8[1] == 2 && ship->collision_flags != 0 && ship->own.u8[1] == 2) {
            MTZ_Hit(ship);
            hits++;
        } else if (hits == 7 && dived && shot && ship->collision_flags != 0) {
            ship->collision_property = 0;      // (the last)
            ship->collision_flags = 0;
            hits++;
            limit_right2 = 0x2AB0 - 0x80 + SCREEN_WIDEADD2;
        }
    }
    CHECK_EQ(hits, 8);
    CHECK(dived);
    CHECK(shot);
    CHECK(ship->code == NULL);
    CHECK_EQ(boss_status, 1);
    boss_status = 0;
    limit_right2 = kept_right;
    limit_right1 = kept_right1;
    frame_count = kept_frame;
    ClearPLC();
    boss_flag = 0;
    memset(&boss_move, 0, sizeof(boss_move));
}

// DynResize_Mz3: X $2980 fixes the left edge and the lower limit; X $2A80 shuts the arena at $2AB0, Boss_Flag 7; the top is held at $400 and $5A frames on the boss is made
static void MTZ_TheEventsShutTheArenaAndMakeTheBossLast(void) {
    const uint16_t kept_limits[6] = { limit_left2, limit_right2, limit_top2, limit_btm1, limit_btm2, limit_right1 };
    Reset(ZoneId_MTZ);
    dle_routine = 0;
    level_id = LEVEL_ID(ZoneId_MTZ, 2);
    scrpos_x.f.u = 0x2980 - SCREEN_WIDEADD2 - 1;
    MTZBoss_Events();
    CHECK_EQ(dle_routine, 0);
    scrpos_x.f.u = 0x2980 - SCREEN_WIDEADD2;
    MTZBoss_Events();
    CHECK_EQ(dle_routine, 2);
    CHECK_EQ(limit_btm1, 0x0400 - SCREEN_TALLADD);
    scrpos_x.f.u = 0x2A80 - SCREEN_WIDEADD2;
    MTZBoss_Events();
    CHECK_EQ(dle_routine, 4);
    CHECK_EQ(boss_flag, 7);
    CHECK_EQ(limit_left2, 0x2AB0 - SCREEN_WIDEADD2);
    CHECK_EQ(limit_right2, 0x2AB0 + SCREEN_WIDEADD2);
    CHECK_EQ(MTZ_Count(0x54, -1), 0);
    scrpos_y.f.u = 0x400 - SCREEN_TALLADD;
    for (int i = 0; i < 0x59; i++)
        MTZBoss_Events();
    CHECK_EQ(limit_top2, 0x400 - SCREEN_TALLADD);
    CHECK_EQ(dle_routine, 4);
    MTZBoss_Events();
    CHECK_EQ(dle_routine, 6);
    CHECK_EQ(MTZ_Count(0x54, -1), 1);
    ClearPLC();
    boss_flag = 0;
    dle_routine = 0;
    limit_left2 = kept_limits[0]; limit_right2 = kept_limits[1]; limit_top2 = kept_limits[2]; limit_btm1 = kept_limits[3]; limit_btm2 = kept_limits[4]; limit_right1 = kept_limits[5];
}


static void CNZ_TheAutoSpinRollsWhoeverTouchesItIntoABall(void) { // (the alpha's box, which Hidden Palace and Hill Top still have: Casino Night has the pre-beta's line)
    Reset(ZoneId_HPZ);
    Sst *box = Spawn(0x84, 0, 300, 300);
    Obj_AutoSpin(box);
    CHECK_EQ(box->collision_flags, 0xC6);
    CHECK_EQ(box->mapping_frame, 0);
    player->status &= (uint8_t)~SST_PSTATUS_IN_BALL;
    player->ground_vel = 0;
    player->y_pos.f.u = 300;
    box->collision_property = 1; // (the touch response sets a bit for each character: Sonic's is the first)
    Obj_AutoSpin(box);
    CHECK(((player->status & SST_PSTATUS_IN_BALL) != 0));
    CHECK_EQ(player->anim, SonAnimId_Roll);
    CHECK_EQ(player->ground_vel, 0x200);
    CHECK_EQ(player->y_pos.f.u, 305);
    CHECK_EQ(box->collision_property, 0);
    box->collision_property = 1; // (already a ball: nothing)
    Obj_AutoSpin(box);
    CHECK_EQ(player->y_pos.f.u, 305);
}

static void CNZ_TheBumperKnowsWhichCharacterTouchedIt(void) {
    Reset(ZoneId_CNZ);
    Sst *bumper = Spawn(0x44, 0, 300, 300);
    Obj_CNZBumper(bumper);
    player->x_pos.f.u = 300;
    player->y_pos.f.u = 330;
    TAILS_OBJ->code = sst_object_codes[0x02];
    TAILS_OBJ->x_pos.f.u = 300;
    TAILS_OBJ->y_pos.f.u = 270;
    TAILS_OBJ->y_vel = 0;
    player->y_vel = 0;
    bumper->collision_property = 2; // Tails' bit only
    Obj_CNZBumper(bumper);
    CHECK_EQ(player->y_vel, 0);
    CHECK(TAILS_OBJ->y_vel < -0x500); // (thrown up: he is above it)
    TAILS_OBJ->code = sst_object_codes[0];
}

// --- the pre-beta's differences from the alpha (2026-11-06) ---

// the roots (7F) take hold of a character who is 40 pixels under them, if nothing else holds him (object control bit 7), he is not dead (routine 6; a hurt one, 4, can take hold) and not in debug mode
static bool RootsHold(uint8_t routine, uint8_t control, bool debug) {
    Reset(ZoneId_MCZ);
    Sst *obj = Spawn(0x7F, 2, 300, 400);
    player->x_pos.f.u = 300;
    player->y_pos.f.u = 400 + 0x28;
    player->routine = routine;
    OBJ_CONTROL(player) = control;
    debug_use = debug;
    Obj_DHZVinesSwitch(obj);
    debug_use = false;
    return (f_switch[2] & 1) != 0 && player->anim == 0x14;
}

static void DHZ_TheRootsHoldAHurtCharacterButNotADeadOneOrOneHeldOrInDebugMode(void) {
    CHECK(RootsHold(2, 0, false));
    CHECK(RootsHold(4, 0, false));
    CHECK(!RootsHold(6, 0, false));
    CHECK(!RootsHold(2, 0x80, false));
    CHECK(!RootsHold(2, 0, true));
}

static void DHZ_TheLongRootsAreALongerChainWithAHookInSkyFortressAndCountTheirOwnHeight(void) {
    Reset(ZoneId_SFZ);
    Sst *obj = Spawn(0x80, 0x10, 300, 400); // (it starts hanging down)
    Obj_DHZVines(obj);
    CHECK(obj->mappings == Mappings_SFZHookChain);
    CHECK_EQ(obj->y_pos.f.u, 400 + 0xA0);
    CHECK_EQ(obj->mapping_frame, 6);
    Reset(ZoneId_MCZ);
    obj = Spawn(0x80, 0x10, 300, 400);
    Obj_DHZVines(obj);
    CHECK(obj->mappings == Mappings_DHZVines);
    CHECK_EQ(obj->y_pos.f.u, 400 + 0xB0);
    CHECK_EQ(obj->height_pixels, 0x80); // (the pre-beta's Obj_Height_2: what keeps it on screen)
}

static void DHZ_TheChainsCountAs40PixelsWideAndHighWhereTheirFifthLinkIs(void) {
    Reset(ZoneId_MCZ);
    Sst *ball = Spawn(0x75, 0x16, 200, 400);
    Obj_SpikeballChain(ball);
    CHECK_EQ(sst[0x21].width_pixels, 0x40);
    CHECK_EQ(sst[0x21].height_pixels, 0x40);
    CHECK_EQ(sst[0x21].x_pos.f.u, sst[0x21].children[4].x);
    CHECK_EQ(sst[0x21].y_pos.f.u, sst[0x21].children[4].y);

    Reset(ZoneId_MCZ);
    Sst *bridge = Spawn(0x81, 4, 300, 400);
    Obj_DHZVerticalBridge(bridge);
    Sst *chain = &sst[0x21];
    CHECK_EQ(chain->width_pixels, 0x40);
    CHECK_EQ(chain->height_pixels, 0x40);
    CHECK_EQ(chain->x_pos.f.u, chain->children[4].x);
    CHECK_EQ(chain->y_pos.f.u, chain->children[4].y);
    f_switch[4] = 1;
    for (int i = 0; i < 8; i++)
        Obj_DHZVerticalBridge(bridge);
    CHECK_EQ(chain->x_pos.f.u, chain->children[4].x); // (and keeps it while the bridge swings)
    CHECK_EQ(chain->y_pos.f.u, chain->children[4].y);
}

static void DHZ_ABridgeFlippedInXAndNothingElseMovesLeftOfItsFootWhenItsSwitchGoesDown(void) {
    Reset(ZoneId_MCZ);
    Sst *plain = Spawn(0x81, 4, 300, 400);
    Obj_DHZVerticalBridge(plain);
    f_switch[4] = 1;
    Obj_DHZVerticalBridge(plain);
    CHECK_EQ(plain->x_pos.f.u, 300);

    Reset(ZoneId_MCZ);
    Sst *flipped = Spawn(0x81, 4, 300, 400);
    flipped->status = SST_STATUS_X_FLIP;
    Obj_DHZVerticalBridge(flipped);
    CHECK_EQ(flipped->x_pos.f.u, 300);
    f_switch[4] = 1;
    Obj_DHZVerticalBridge(flipped);
    CHECK_EQ(flipped->x_pos.f.u, 300 - 0x48);
}

static void DHZ_TheCrawltonsBodyIsAsBigAsTheChainsAndGoesWhereTheHeadGoes(void) {
    Reset(ZoneId_MCZ);
    Sst *head = Spawn(0x9E, 0x22, 200, 400);
    player->x_pos.f.u = 200 + 0x100;
    player->y_pos.f.u = 400;
    Obj_Crawlton(head);
    Sst *body = &sst[0x21];
    CHECK_EQ(body->width_pixels, 0x40);
    CHECK_EQ(body->height_pixels, 0x40);
    head->x_pos.f.u = 230;
    head->y_pos.f.u = 410;
    Obj_Crawlton(body);
    CHECK_EQ(body->x_pos.f.u, 230);
    CHECK_EQ(body->y_pos.f.u, 410);
}

// frames until the Flasher is in `routine`, from where it is (a frame it spends in it first does not count)
static int FlasherFramesTo(Sst *obj, uint8_t routine) {
    int frames = 0;
    while (obj->routine != routine && frames < 0x800) {
        Obj_Flasher(obj);
        frames++;
    }
    return frames;
}

static void DHZ_TheFlashersAnimationsEachStartFromTheirBeginning(void) {
    Reset(ZoneId_MCZ);
    Sst *obj = Spawn(0xA3, 0x2C, 300, 300);
    FlasherFramesTo(obj, 6);
    const int first_flash = FlasherFramesTo(obj, 8); // (its script ends: the glow comes on its fourth frame, from the start of its own script)
    CHECK_EQ(obj->mapping_frame, 3);
    CHECK_EQ(obj->anim_frame, 0);
    FlasherFramesTo(obj, 0xA); // (the glow ends: the fade starts from the beginning of its own, on the first frame)
    CHECK_EQ(obj->mapping_frame, 0);
    CHECK_EQ(obj->anim_frame, 0);
    FlasherFramesTo(obj, 4);
    CHECK_EQ(obj->mapping_frame, 0);
    FlasherFramesTo(obj, 6);
    const int second_flash = FlasherFramesTo(obj, 8);
    CHECK_EQ(first_flash, second_flash); // (the same length each time: it does not start in the middle of its script)
}

// Sonic's invincibility is his own: spikes hurt Tails all the same (Sand Shower keeps the one flag for both)
static bool TailsHurtBySpikes(int zone) {
    Reset(zone);
    Sst *spikes = Spawn(0x36, 0, 300, 400);
    TAILS_OBJ->code = SstTestFiller;
    TAILS_OBJ->routine = 2;
    TAILS_OBJ->x_pos.f.u = 300;
    TAILS_OBJ->y_pos.f.u = 380;
    invincibility = true;
    Spikes_HurtCharacter(spikes, TAILS_OBJ, SolidChar_Tails);
    invincibility = false;
    const bool hurt = TAILS_OBJ->routine == 4;
    TAILS_OBJ->code = NULL;
    return hurt;
}

static void DHZ_SpikesHurtTailsWhileSonicIsInvincibleExceptInSandShower(void) {
    CHECK(TailsHurtBySpikes(ZoneId_MCZ));
    CHECK(!TailsHurtBySpikes(ZoneId_SSZ));
}

// --- 3D (Oil Ocean's block that is broken for a boost) ---

static void OOZ_ABlockBreaksUnderARollingCharacterAndCarriesHimAlong(void) {
    Reset(ZoneId_OOZ);
    Sst *block = Spawn(0x3D, 0, 300, 400);
    Obj_OOZBreakBoost(block);
    StandOn(300, 400 - 0x10);
    player->anim = SonAnimId_Walk;
    Obj_OOZBreakBoost(block);
    CHECK_EQ(block->routine, 2); // (walking on it does nothing)
    CHECK_EQ(CountOf(0x3D, 4), 0);
    StandOn(300, 400 - 0x10);
    player->anim = SonAnimId_Roll;
    player->y_vel = 0x300;
    block->render_flags |= SST_RENDER_ON_SCREEN; // (it was drawn the frame before: a piece that is off screen is deleted)
    Obj_OOZBreakBoost(block);
    CHECK_EQ(block->routine, 4);
    CHECK_EQ(CountOf(0x3D, 4), 16);   // (the block is its first piece)
    CHECK_EQ(CountOf(0x3D, 6), 1);    // (the tracker)
    CHECK_EQ(player->routine, 2);
    CHECK_EQ(player->y_vel, 0x300);
    Sst *tracker = NULL;
    for (int i = 0x20; i < 0x60; i++)
        if (sst[i].code == sst_object_codes[0x3D] && sst[i].routine == 6)
            tracker = &sst[i];
    CHECK(tracker != NULL);
    player->x_pos.f.u = 300;
    player->y_pos.f.u = 400;
    Obj_OOZBreakBoost(tracker); // he is in the middle: it takes hold of him
    CHECK_EQ(player->x_vel, 0x800);
    CHECK_EQ(player->y_vel, 0);
    CHECK_EQ(OBJ_CONTROL(player), 0x81);
    int16_t x = player->x_pos.f.u;
    Obj_OOZBreakBoost(tracker);
    CHECK(player->x_pos.f.u > x);
}

static void OOZ_TheVerticalBlockSendsHimUp(void) {
    Reset(ZoneId_OOZ);
    Sst *block = Spawn(0x3D, 1, 300, 400);
    Obj_OOZBreakBoost(block);
    CHECK_EQ(block->mapping_frame, 2);
    StandOn(300, 400 - 0x10);
    player->anim = SonAnimId_Roll;
    block->render_flags |= SST_RENDER_ON_SCREEN;
    Obj_OOZBreakBoost(block);
    CHECK_EQ(block->mapping_frame, 3);
    Sst *tracker = NULL;
    for (int i = 0x20; i < 0x60; i++)
        if (sst[i].code == sst_object_codes[0x3D] && sst[i].routine == 6)
            tracker = &sst[i];
    CHECK(tracker != NULL);
    player->x_pos.f.u = 300 + 4;
    player->y_pos.f.u = 400 - 0x10;
    Obj_OOZBreakBoost(tracker);
    CHECK_EQ(player->x_pos.f.u, 300);
    CHECK_EQ(player->y_vel, -0x800);
}

// --- 3F (Oil Ocean's fans) ---

static void OOZ_AVerticalFanPushesACharacterSidewaysWhileItBlows(void) {
    Reset(ZoneId_OOZ);
    Sst *fan = Spawn(0x3F, 0x02, 300, 400); // (bit 1: it blows all the time)
    player->x_pos.f.u = 300 + 0x10;
    player->y_pos.f.u = 400 - 0x30;
    Obj_OOZFan(fan);
    CHECK(sst_object_codes[0x3F] == Obj_OOZFan);
    const int16_t x = player->x_pos.f.u;
    Obj_OOZFan(fan);
    CHECK(player->x_pos.f.u != x);
    player->x_pos.f.u = 300 + 0x200; // (far away)
    const int16_t far_x = player->x_pos.f.u;
    Obj_OOZFan(fan);
    CHECK_EQ(player->x_pos.f.u, far_x);
}

static void OOZ_AHorizontalFanLiftsAndTumblesACharacter(void) {
    Reset(ZoneId_OOZ);
    Sst *fan = Spawn(0x3F, 0x82, 300, 400);
    player->x_pos.f.u = 300;
    player->y_pos.f.u = 400 - 0x40;
    player->status &= (uint8_t)~SST_PSTATUS_IN_AIR;
    Obj_OOZFan(fan);
    Obj_OOZFan(fan);
    CHECK((player->status & SST_PSTATUS_IN_AIR) != 0);
    CHECK_EQ(player->y_vel, 0);
    CHECK(player->y_pos.f.u != 400 - 0x40);
    CHECK_EQ(((Scratch_Sonic *)player->own.u8)->flips_remaining, 0x7F);
}

static void OOZ_TheFanSpinsUpForAWhileThenBlows(void) {
    Reset(ZoneId_OOZ);
    Sst *fan = Spawn(0x3F, 0x00, 300, 400);
    player->x_pos.f.u = 300 + 0x10;
    player->y_pos.f.u = 400 - 0x30;
    Obj_OOZFan(fan); // the first frame: it starts spinning up
    const int16_t x = player->x_pos.f.u;
    for (int i = 0; i < 0x70; i++)
        Obj_OOZFan(fan);
    CHECK_EQ(player->x_pos.f.u, x); // (spinning up it does not blow)
    for (int i = 0; i < 0x20; i++)
        Obj_OOZFan(fan);
    CHECK(player->x_pos.f.u != x); // (and then it does)
}

// --- 4A (Oil Ocean's Octus) ---

static void OOZ_TheOctusWaitsRisesAndFiresAShotAndAPuff(void) {
    Reset(ZoneId_OOZ);
    Sst *octus = Spawn(0x4A, 0, 300, 400);
    octus->routine = 2; // (landed)
    player->x_pos.f.u = 300 + 0x200;
    player->y_pos.f.u = 400;
    Obj_Octus(octus);
    Obj_Octus(octus);
    CHECK_EQ(octus->routine_secondary, 0); // (Sonic is far: it waits)
    player->x_pos.f.u = 300 + 0x40;
    Obj_Octus(octus);
    CHECK_EQ(octus->routine_secondary, 2); // (he is near: it rises)
    CHECK_EQ(octus->anim, 1);
    ((int16_t *)octus->own.u8)[3] = 400; // (its home height, at $2E: where it landed)
    for (int i = 0; i < 40 && octus->routine_secondary == 2; i++)
        Obj_Octus(octus);
    CHECK_EQ(octus->routine_secondary, 4);
    Obj_Octus(octus); // it fires
    CHECK_EQ(CountOf(0x4A, 4), 1);    // the puff
    CHECK_EQ(CountOf(0x4A, 6), 1);    // the shot
    Sst *shot = NULL;
    for (int i = 0x20; i < 0x60; i++)
        if (sst[i].code == sst_object_codes[0x4A] && sst[i].routine == 6)
            shot = &sst[i];
    CHECK(shot != NULL);
    if (shot == NULL)
        return;
    CHECK_EQ(shot->x_vel, -0x580);
    CHECK_EQ(shot->collision_flags, 0); // (the pre-beta gives it none)
}

// --- 50 (Oil Ocean's Aquis) ---

static void OOZ_TheAquisPatrolsLeapsAtSonicAndSpitsAHarmlessShot(void) {
    Reset(ZoneId_OOZ);
    const int16_t saved_water = wtr_pos1; // (the level's water: other tests read it)
    wtr_pos1 = 400;
    Sst *aquis = Spawn(0x50, 0x1F, 300, 400);
    Obj_Aquis(aquis);
    CHECK_EQ(aquis->collision_flags, 0x0A);
    CHECK_EQ(CountOf(0x50, 4), 1); // the fin
    CHECK(aquis->x_vel != 0);
    // at the surface, facing right, with Sonic above it and in front
    ((uint8_t *)aquis->own.u8)[0x2C - 0x28] = 0xFF; // (at the surface)
    aquis->status |= SST_STATUS_X_FLIP;
    player->x_pos.f.u = 300 + 0x40;
    player->y_pos.f.u = 400 - 0x20;
    Obj_Aquis(aquis);
    CHECK_EQ(aquis->routine_secondary, 4);
    CHECK_EQ(aquis->anim, 1);
    for (int i = 0; i < 60 && CountOf(0x50, 6) == 0; i++)
        Obj_Aquis(aquis);
    CHECK_EQ(CountOf(0x50, 6), 1);
    Sst *shot = NULL;
    for (int i = 0x20; i < 0x60; i++)
        if (sst[i].code == sst_object_codes[0x50] && sst[i].routine == 6)
            shot = &sst[i];
    CHECK(shot != NULL);
    if (shot == NULL) {
        wtr_pos1 = saved_water;
        return;
    }
    CHECK_EQ(shot->x_vel, 0x300);
    CHECK_EQ(shot->collision_flags, 0xE5); // (a type nothing in the pre-beta handles: it hurts nobody)
    wtr_pos1 = saved_water;
}

// --- the monitors, through the object loader ---

static int LiveMonitors(void) {
    int n = 0;
    for (int i = 0x20; i < 0x80; i++)
        n += sst[i].code == sst_object_codes[0x26];
    return n;
}

static void Monitors_AreLoadedOnceHoweverTheCameraGoesBackAndForth(void) {
    Reset(ZoneId_EHZ);
    sst_load_everything_in_range = false;
    // one monitor at x $400 (bit 15 of the Y word: loaded whatever the camera's Y), a super rings one
    static const uint8_t layout[] = { 0x04, 0x00, 0x83, 0x00, 0x26, 0x04, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00 };
    static const SstObjectIndex index_ = {sst_object_codes, 0x100};
    SstLoadCamera camera = {0, 0x200, false, 0xFFF};
    SstLoad_Init(&index_, layout, false, &camera);
    CHECK_EQ(LiveMonitors(), 0);
    int max_alive = 0;
    // forward past its load edge, on, back over it, and forward again, running the monitor each frame
    static const int path[] = { 0x100, 0x200, 0x300, 0x380, 0x400, 0x380, 0x300, 0x200, 0x100, 0x180, 0x200, 0x300, 0x380, 0x400, 0x500 };
    for (size_t k = 0; k < sizeof(path) / sizeof(path[0]); k++) {
        camera.x = (int16_t)path[k];
        scrpos_x.f.u = camera.x;
        SstLoad_Update(&camera);
        for (int i = 0x20; i < 0x80; i++)
            if (sst[i].code == sst_object_codes[0x26])
                sst[i].code(&sst[i]);
        if (LiveMonitors() > max_alive)
            max_alive = LiveMonitors();
    }
    CHECK(max_alive >= 1);
    CHECK_EQ(max_alive, 1); // (never a second one on top of it)

    // ... and up and down: every 128-pixel row the camera enters makes the loader look at the entries again (Oil Ocean's shafts and cannons)
    camera.x = 0x300;
    scrpos_x.f.u = camera.x;
    for (int k = 0; k < 24; k++) {
        camera.y = (int16_t)(0x200 + ((k & 1) ? 0x100 : 0) + (k / 2) * 0x80);
        scrpos_y.f.u = camera.y;
        SstLoad_Update(&camera);
        for (int i = 0x20; i < 0x80; i++)
            if (sst[i].code == sst_object_codes[0x26])
                sst[i].code(&sst[i]);
        if (LiveMonitors() > max_alive)
            max_alive = LiveMonitors();
    }
    CHECK_EQ(max_alive, 1);
    sst_load_everything_in_range = true;
}

static int LiveCode(void (*code)(Sst *)) {
    int n = 0;
    for (int i = 0x20; i < 0x80; i++)
        n += sst[i].code == code;
    return n;
}

static void Monitors_BreakOnceHoweverLongSonicStaysInThem(void) {
    Reset(ZoneId_EHZ);
    Sst *mon = Spawn(0x26, 4, 400, 400);
    Obj_Monitor(mon);
    player->x_pos.f.u = 400;
    player->y_pos.f.u = 400 - 10;
    player->anim = SonAnimId_Roll;
    player->y_vel = 0x200;
    player->status |= SST_PSTATUS_IN_AIR | SST_PSTATUS_IN_BALL;
    player->y_radius = 14;
    mon->render_flags |= SST_RENDER_ON_SCREEN;
    int items = 0;
    for (int f = 0; f < 12; f++) {
        ReactToItem(player);
        if (f == 0)
            ReactToItem(player); // (Tails reacting in the same frame, before the monitor has been run: the same call for a second character)
        Obj_Monitor(mon);
        mon->render_flags |= SST_RENDER_ON_SCREEN;
        for (int i = 0x20; i < 0x80; i++)
            if (sst[i].code == Obj_MonitorItem && sst[i].routine == 0)
                items++;
        for (int i = 0x20; i < 0x80; i++)
            if (sst[i].code == Obj_MonitorItem)
                Obj_MonitorItem(&sst[i]);
    }
    CHECK_EQ(items, 1);
}

static void Monitors_LetGoOfWhoStandsOnThemWhenTheyBreak(void) {
    Reset(ZoneId_EHZ);
    Sst *mon = Spawn(0x26, 4, 400, 400);
    Obj_Monitor(mon);
    StandOn(400, 400 - 15);
    mon->status |= 0x08;
    mon->routine_secondary = 2;
    mon->routine = 4; // (the other character broke it)
    Obj_Monitor(mon);
    CHECK(!(player->status & SST_PSTATUS_OBJECT_STAND));
    CHECK((player->status & SST_PSTATUS_IN_AIR) != 0);
    CHECK_EQ(mon->status & 0x18, 0);
}

void RegisterDHZObjectTests(void) {
    RUN_TEST(DHZ_TheCradleSwingsOutOnOneSideOnlyAndTheMiddleHangsStill);
    RUN_TEST(DHZ_TheCollapsingPlatformBreaksIntoSixPiecesOnceStoodOn);
    RUN_TEST(DHZ_TheCollapsingPlatformOfOilOceanIsWiderAndHasSevenPieces);
    RUN_TEST(DHZ_TheCollapsingPlatformKeepsItsFacingAndItsPiecesTakeIt);
    RUN_TEST(DHZ_PiecesFallOneAfterAnother);
    RUN_TEST(DHZ_TheStomperRisesAPixelAFrameAndDropsEight);
    RUN_TEST(DHZ_TheSwitchIsDownWhileSomeoneStandsOnIt);
    RUN_TEST(DHZ_ASwitchWithBitSixSetsTheTopBit);
    RUN_TEST(DHZ_TheDrawbridgeLowersAtStartAndRaisesOnceItsSwitchIsDown);
    RUN_TEST(DHZ_ARaisedDrawbridgeLetsGoOfWhoeverStandsOnIt);
    RUN_TEST(DHZ_TheBoxesMakeTwoMoreAndMoveOnAPath);
    RUN_TEST(DHZ_FlippedBoxesTurnTheOtherWay);
    RUN_TEST(DHZ_TheSpikedBallIsAtTheEndOfItsChain);
    RUN_TEST(DHZ_TheFlipOfTheObjectGivesTheStartingAngle);
    RUN_TEST(DHZ_TheChainTurnsBySpeedTimesEightAFrame);
    RUN_TEST(DHZ_ASubtypeOfLowNibbleFIsASolidBlock);
    RUN_TEST(DHZ_ThePlatformWithSpikesSlidesAwayWhenSomeoneComesNear);
    RUN_TEST(DHZ_ACharacterInTheAirDoesNotSetItOff);
    RUN_TEST(DHZ_AFlippedPlatformWithSpikesSlidesRight);
    RUN_TEST(DHZ_TheCrawltonWaitsUntilSonicIsNearThenTurnsToHimAndCrawls);
    RUN_TEST(DHZ_TheCrawltonBodyFollowsTheHeadWhileItCrawls);
    RUN_TEST(DHZ_TheFlasherDriftsFlashesAndGlowsDangerously);
    RUN_TEST(DHZ_TheRootsHoldAHurtCharacterButNotADeadOneOrOneHeldOrInDebugMode);
    RUN_TEST(DHZ_TheLongRootsAreALongerChainWithAHookInSkyFortressAndCountTheirOwnHeight);
    RUN_TEST(DHZ_TheChainsCountAs40PixelsWideAndHighWhereTheirFifthLinkIs);
    RUN_TEST(DHZ_ABridgeFlippedInXAndNothingElseMovesLeftOfItsFootWhenItsSwitchGoesDown);
    RUN_TEST(DHZ_TheCrawltonsBodyIsAsBigAsTheChainsAndGoesWhereTheHeadGoes);
    RUN_TEST(DHZ_TheFlashersAnimationsEachStartFromTheirBeginning);
    RUN_TEST(DHZ_SpikesHurtTailsWhileSonicIsInvincibleExceptInSandShower);
    RUN_TEST(OOZ_ABlockBreaksUnderARollingCharacterAndCarriesHimAlong);
    RUN_TEST(OOZ_TheVerticalBlockSendsHimUp);
    RUN_TEST(OOZ_TheOctusWaitsRisesAndFiresAShotAndAPuff);
    RUN_TEST(OOZ_TheAquisPatrolsLeapsAtSonicAndSpitsAHarmlessShot);
    RUN_TEST(OOZ_AVerticalFanPushesACharacterSidewaysWhileItBlows);
    RUN_TEST(OOZ_AHorizontalFanLiftsAndTumblesACharacter);
    RUN_TEST(OOZ_TheFanSpinsUpForAWhileThenBlows);
    RUN_TEST(Monitors_AreLoadedOnceHoweverTheCameraGoesBackAndForth);
    RUN_TEST(Monitors_BreakOnceHoweverLongSonicStaysInThem);
    RUN_TEST(Monitors_LetGoOfWhoStandsOnThemWhenTheyBreak);
    RUN_TEST(DHZ_TheBossDigsDropsRocksAndChargesAlongTheFloor);
    RUN_TEST(DHZ_TheEventsShutTheArenaAndMakeTheBossLast);
    RUN_TEST(CNZ_TheRoundBumperThrowsWhoeverTouchesIt);
    RUN_TEST(CNZ_TheBossGoesToAndFroAndLetsTheBallFall);
    RUN_TEST(CNZ_TheEventsShutTheArenaAndMakeTheBossLast);
    RUN_TEST(OOZ_TheBossDivesThenHoversAndSpitsBallsThatLightFlames);
    RUN_TEST(OOZ_ABossThatWasHitSwingsRoundTheArena);
    RUN_TEST(OOZ_TheBossBlowsUpAndSinksWhenBeaten);
    RUN_TEST(OOZ_TheEventsShutTheArenaAndMakeTheBossLast);
    RUN_TEST(MTZ_TheShipPatrolsAndEachHitLetsABallGo);
    RUN_TEST(MTZ_AfterSevenHitsTheShipDivesAndShootsAndTheEighthEndsIt);
    RUN_TEST(MTZ_TheEventsShutTheArenaAndMakeTheBossLast);
    RUN_TEST(CNZ_TheAutoSpinRollsWhoeverTouchesItIntoABall);
    RUN_TEST(CNZ_TheBumperKnowsWhichCharacterTouchedIt);
}
