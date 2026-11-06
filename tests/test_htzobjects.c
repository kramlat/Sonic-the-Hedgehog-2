#include "test.h"

#include <string.h>

#include "Level.h"
#include "Object.h"
#include "SstLoad.h"
#include "Object/CPZObjects.h"
#include "Object/HTZObjects.h"
#include "Object/Platform.h"
#include "Object/Sonic.h"
#include "HTZQuake.h"

// Hill Top's lift (16), the valve barrier and rock (2D, 32, the objects Chemical Plant shares) and the platform (18) with its size field and solid kind.

// (the slots below $20 are taken, so that the objects a test makes get the slots the old tests named)
static void SstTestFiller(Sst *self) { (void)self; }

static void Reset(void) {
    Sst_Reset();
    sst_load_everything_in_range = true;
    for (int i = 4; i < 0x20; i++)
        sst[i].code = SstTestFiller;
    level_id = LEVEL_ID(ZoneId_HTZ, 0);
    scrpos_x.v = scrpos_y.v = 0;
    limit_btm2 = 0x720;
    player->routine = 2;
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

// The lift waits to be stood on, slides (to the right, or the left if flipped) for its subtype times eight frames, then stops, leaves a pole and falls
static void HTZObjects_ALiftSlidesForItsSubtypeAndThenFalls(void) {
    Reset();
    Sst *obj = Spawn(0x16, 0x14, 100, 200);
    Obj_HTZLift(obj);
    for (int i = 0; i < 10; i++)
        Obj_HTZLift(obj);
    CHECK_EQ(obj->x_pos.f.u, 100); // (it waits until it is stood on)
    obj->status |= 1 << 3;
    Obj_HTZLift(obj);
    CHECK_EQ(obj->x_vel, 0x200);
    CHECK_EQ(obj->y_vel, 0x100);
    for (int i = 0; i < 0x14 * 8; i++)
        Obj_HTZLift(obj);
    CHECK_EQ(obj->mapping_frame, 2); // (stopped, with its pole gone)
    CHECK_EQ(obj->x_vel, 0);
    int poles = 0;
    for (int i = 0x21; i < 0x60; i++)
        poles += sst[i].code == sst_object_codes[0x1C] && sst[i].subtype == 6;
    CHECK_EQ(poles, 1);
    const int16_t y = obj->y_pos.f.u;
    for (int i = 0; i < 10; i++)
        Obj_HTZLift(obj);
    CHECK(obj->y_pos.f.u > y); // falling
}

static void HTZObjects_AFlippedLiftSlidesLeft(void) {
    Reset();
    Sst *obj = Spawn(0x16, 0x14, 100, 200);
    obj->status |= SST_STATUS_X_FLIP;
    Obj_HTZLift(obj);
    obj->status |= 1 << 3;
    Obj_HTZLift(obj);
    CHECK_EQ(obj->x_vel, -0x200);
}

static void HTZObjects_TheValveBarrierHasItsOwnArtAndWidth(void) {
    Reset();
    Sst *obj = Spawn(0x2D, 0, 400, 300);
    Obj_CPZBarrier(obj);
    CHECK_EQ(obj->art_tile & 0x7FF, 0x426);
    CHECK_EQ(obj->width_pixels, 8);
    level_id = LEVEL_ID(ZoneId_CPZ, 0);
    Sst *cpz = Spawn(0x2D, 0, 400, 300);
    Obj_CPZBarrier(cpz);
    CHECK_EQ(cpz->art_tile & 0x7FF, 0x394);
    CHECK_EQ(cpz->width_pixels, 8); // (the pre-beta's: the alpha's was $C)
}

static void HTZObjects_TheRockIsWiderAndBreaksInSix(void) {
    Reset();
    Sst *obj = Spawn(0x32, 0, 300, 300);
    Obj_TubeCover(obj);
    CHECK_EQ(obj->art_tile & 0x7FF, 0x3B2);
    CHECK_EQ(obj->width_pixels, 0x18);
    level_id = LEVEL_ID(ZoneId_CPZ, 1);
    Sst *cover = Spawn(0x32, 0, 300, 300);
    Obj_TubeCover(cover);
    CHECK_EQ(cover->width_pixels, 0x10);
}

static void HTZObjects_ThePlatformSizeIsThreeBitsAndBitSevenIsTheSolidKind(void) {
    Reset();
    // sizes: widths $20, $20, $20, $40, $30 by (subtype >> 4) & 7
    Sst *a = Spawn(0x18, 0x30, 300, 300);
    Obj_BasicPlatform(a);
    CHECK_EQ(a->width_pixels, 0x40);
    Sst *b = Spawn(0x18, 0x40, 300, 300);
    Obj_BasicPlatform(b);
    CHECK_EQ(b->width_pixels, 0x30);
    Sst *solid = Spawn(0x18, 0x9B, 300, 300); // bit 7: the solid block, size (9B >> 4) & 7 = 1
    Obj_BasicPlatform(solid);
    CHECK_EQ(solid->width_pixels, 0x20);
    CHECK_EQ(solid->y_radius, 0x30); // (Neo Green Hill's is $28)
    level_id = LEVEL_ID(ZoneId_ARZ, 0);
    Sst *arz = Spawn(0x18, 0x9B, 300, 300);
    Obj_BasicPlatform(arz);
    CHECK_EQ(arz->y_radius, 0x28);
}

// The seesaw keeps how fast the faster of the two came down only while nobody is on it (the pre-beta stores it there alone): the ball is thrown by it after he has landed
static void HTZObjects_TheSeesawKeepsTheLandingSpeedWhileSomeoneIsOnIt(void) {
    Reset();
    Sst *seesaw = Spawn(0x14, 1, 300, 300); // (subtype 1: no ball)
    player->y_vel = 0xA00;
    Obj_HTZSeesaw(seesaw);
    const int16_t *speed = (const int16_t *)(seesaw->own.u8 + 0x10); // (0x38)
    CHECK_EQ(*speed, 0xA00);
    seesaw->status |= 8; // (Sonic has landed: his speed is gone)
    player->y_vel = 0;
    Obj_HTZSeesaw(seesaw);
    CHECK_EQ(*speed, 0xA00);
}

// The breakable floor is $20 high to stand on whatever its subtype says (the pre-beta stores $20 over the half height its table gives)
static void HTZObjects_TheBreakableFloorIsAlways20High(void) {
    Reset();
    Sst *floor = Spawn(0x2F, 0x08, 300, 300); // (the last of its five heights: the table's $08)
    Obj_HTZBreakFloor(floor);
    CHECK_EQ(floor->y_radius, 0x20);
    CHECK_EQ(floor->mapping_frame, 8);
}

// The quake blocks of kind 6 are the lower route's (kept with the camera at $380 or below it), kind 8 the upper route's
static void HTZObjects_TheQuakeBlocksBelongToTheirRouteByTheCamera(void) {
    Reset();
    scrpos_y.f.u = 0x400;
    Sst *lower = Spawn(0x30, 6, 300, 0x4F0);
    Obj_HTZQuakeBlock(lower);
    CHECK(lower->code != NULL);
    Sst *upper = Spawn(0x30, 8, 600, 0x40);
    Obj_HTZQuakeBlock(upper);
    CHECK(upper->code == NULL);
    scrpos_y.f.u = 0x100;
    Sst *lower2 = Spawn(0x30, 6, 900, 0x4F0);
    Obj_HTZQuakeBlock(lower2);
    CHECK(lower2->code == NULL);
    Sst *upper2 = Spawn(0x30, 8, 1200, 0x40);
    Obj_HTZQuakeBlock(upper2);
    CHECK(upper2->code != NULL);
}

void RegisterHTZObjectTests(void) {
    RUN_TEST(HTZObjects_TheSeesawKeepsTheLandingSpeedWhileSomeoneIsOnIt);
    RUN_TEST(HTZObjects_TheBreakableFloorIsAlways20High);
    RUN_TEST(HTZObjects_TheQuakeBlocksBelongToTheirRouteByTheCamera);
    RUN_TEST(HTZObjects_ALiftSlidesForItsSubtypeAndThenFalls);
    RUN_TEST(HTZObjects_AFlippedLiftSlidesLeft);
    RUN_TEST(HTZObjects_TheValveBarrierHasItsOwnArtAndWidth);
    RUN_TEST(HTZObjects_TheRockIsWiderAndBreaksInSix);
    RUN_TEST(HTZObjects_ThePlatformSizeIsThreeBitsAndBitSevenIsTheSolidKind);
}
