// Casino Night's boss for Sonic 2, as the pre-beta has it (object 51, Obj_0x51_CNz_Boss): see CNZBoss.h. Like Dust Hill's it uses the shared boss helpers (BossCommon): the ship's place and speed in a buffer, and the
// animation of its parts, each a pair of nibbles in a small buffer. Its parts are the object's child sprites: the right catcher, the ship, Robotnik and the left catcher (its own sprite is the flame under the ship).
#include "Object/CNZBoss.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"
#include "Object/BossCommon.h"
#include "Constants.h"

#include "EngineObject.h"
#include "EnginePalette.h"
#include "HTZQuake.h"
#include "Level.h"
#include "LevelData.h"
#include "LevelCollision.h"
#include "MathUtil.h"
#include "Sound.h"

#include <string.h>

#include "Macros.h"

#include "Resource/Animation/CNZBoss.h"
#include "Resource/Mappings/CNZBoss.h"

#define ArtTile_CNZBoss 0x3A7 // ($74E0: list $2D)

enum { Boss_Start = 0, Boss_Run = 2, Boss_Ball = 4 };
// What the boss is doing (Obj_Ani_Boss_Routine; the animations of its parts move it on too)
enum { Run_Move = 0, Run_Choose = 2, Run_Drop = 4, Run_Catch = 6, Run_Wreck = 8, Run_Fall = 0xA, Run_Leave = 0xC };
// Where it goes (Obj_Control_Var_0C): a round of the arena's corners
enum { Path_Right = 0, Path_Up = 2, Path_Left = 4, Path_Down = 6 };
enum { Part_RightCatcher = 0, Part_Ship = 1, Part_Robotnik = 2, Part_LeftCatcher = 3 };
enum { Ball_Make = 0, Ball_Follow = 2, Ball_Fall = 4, Ball_Roll = 6, Ball_Away = 8 };

typedef struct {
    Own_AlphaBoss boss; // flag: the path (0x38), side: it has been beaten (0x2C), count: which side it is catching on, and whether the catchers have opened (0x3E), flash: Obj_Inertia
    int16_t left_speed;     // 0x2E: how fast the left catcher falls when the wreck lets go of it
    int16_t right_speed;    // 0x30
    int32_t right_y;        // 0x34 (Control_Var_08)
    int32_t left_y;         // 0x3A (Control_Var_0E)
} Own_CNZBoss;

// The ball's own: it shares the object's scratch memory with the ship's (the pre-beta's own fields)
typedef struct {
    int16_t drop;       // 0x28: how far it has fallen from the catcher, a pixel a frame up to $2E
    int16_t x_base;     // 0x2A (Obj_Timer): its X, which its falling moves along with the pixels only
    uint8_t direction;  // 0x2C (var_00): bit 7, the way it rolls
    uint8_t parent;     // 0x34: the slot of the boss it fell from
    int16_t x_speed;    // 0x10
    int16_t y_speed;    // 0x12
    int16_t timer;      // 0x30 (var_04): the pre-beta sets it to 1, so the ball goes after two frames of rolling
    int16_t blink;      // 0x32 (Obj_Boss_Hit_2): the blinking
} Own_CNZBall;

static uint8_t catcher_state; // $F73F: 0 the catchers do not hurt, 1 they are shut, 2 open (the box under the ship hurts)
static uint8_t dle_counter;   // $F73A

void CNZBoss_Reset(void) {
    catcher_state = 0;
    dle_counter = 0;
}

// DynResize_CNz of act 2: X $27C0 fixes the left edge and the lower limit and closes the arena's first wall (the layout's chunk $F9 at ($54,12)); X $2890 closes the second, fixes the edges ($2860 to $28E0), fades
// the music, sets Boss_Flag 6 and loads the boss's list; once the camera is at Y $4E0 its top is held, and $5A frames on the boss is made and its music starts; when it is beaten, the results card
void CNZBoss_Events(void) {
    switch (dle_routine) {
    case 0:
        if (scrpos_x.f.u < (0x27C0 - SCREEN_WIDEADD2))
            break;
        limit_left2 = scrpos_x.f.u;
        limit_btm1 = 0x062E - SCREEN_TALLADD;
        LevelLayout_SetChunk(LEVEL_LAYOUT_PLANE_FG, 0x54, 0x0C, 0xF9);
        dle_routine += 2;
        break;
    case 2:
        if (scrpos_x.f.u < (0x2890 - SCREEN_WIDEADD2))
            break;
        LevelLayout_SetChunk(LEVEL_LAYOUT_PLANE_FG, 0x50, 0x0C, 0xF9);
        limit_left2 = 0x2860 - SCREEN_WIDEADD2;
        limit_right2 = 0x28E0 + SCREEN_WIDEADD2;
        dle_routine += 2;
        QueueSound1(bgm_Fade);
        dle_counter = 0;
        boss_flag = 6;
        AddPLC(PlcId_CNZBoss);
        break;
    case 4:
        if (scrpos_y.f.u >= (0x4E0 - SCREEN_TALLADD))
            limit_top2 = 0x4E0 - SCREEN_TALLADD;
        if (++dle_counter >= 0x5A) {
            Sst *boss = Sst_Allocate();
            if (boss != NULL)
                boss->code = sst_object_codes[ObjId_CNZBoss];
            dle_routine += 2;
            QueueSound1(bgm_Boss);
        }
        break;
    case 6:
        if (boss_status)
            GotThroughAct();
        break;
    }
}

// The catchers' box (the touch code of Boss_Flag 6, Offset_0x038684): under the ship, $10 across and 4 high each way while they are shut, $20 across and 4 high, a little to the right, while they are open: it hurts
bool CNZBoss_CatcherBox(const Sst *hit, int16_t *centre_x, int16_t *centre_y, int16_t *half_width, int16_t *half_height) {
    if (hit->code != sst_object_codes[ObjId_CNZBoss] || hit->routine != Boss_Run || boss_flag != 6 || catcher_state == 0)
        return false;
    *centre_x = hit->x_pos.f.u;
    *centre_y = (int16_t)(hit->y_pos.f.u + 0x28);
    *half_width = 0x10;
    *half_height = 4;
    if (catcher_state != 1) {
        *half_width = 0x20;
        *centre_y = (int16_t)(*centre_y - 8);
        *centre_x = (int16_t)(*centre_x + 4);
    }
    return true;
}

static void SetPartAnim(int part, uint8_t anim) {
    boss_anim[(part + 1) * 2] = (uint8_t)((boss_anim[(part + 1) * 2] & 0xF0) | anim);
}

// CNz_Boss_Hit (Offset_0x028DF2): the bob (the sine of its angle, a byte that goes on by 2 a frame, about the ship's place) and then the hit
static void Boss_Bob(Sst *self, Own_CNZBoss *own) {
    int16_t sin, cos;
    CalcSine(own->boss.angle, &sin, &cos);
    self->y_pos.f.u = (int16_t)((sin >> 6) + boss_move.y.f.u);
    self->x_pos.f.u = boss_move.x.f.u;
    own->boss.angle = (uint8_t)(own->boss.angle + 2);
    Boss_Hit(self, &own->boss);
}

// Offset_0x02D23A: where the parts are, from the ship's place; once it has been beaten the catchers fall (the left first)
static void Boss_Parts(Sst *self, Own_CNZBoss *own) {
    const int16_t x = self->x_pos.f.u, y = self->y_pos.f.u;
    self->children[Part_Ship].x = x;
    self->children[Part_Ship].y = y;
    self->children[Part_Robotnik].x = x;
    self->children[Part_Robotnik].y = y;
    if (!own->boss.side) {
        self->children[Part_LeftCatcher].x = x;
        self->children[Part_LeftCatcher].y = y;
        self->children[Part_RightCatcher].x = x;
        self->children[Part_RightCatcher].y = y;
        own->left_y = (int32_t)((uint32_t)(uint16_t)y << 16) | (own->left_y & 0xFFFF);
        own->right_y = (int32_t)((uint32_t)(uint16_t)y << 16) | (own->right_y & 0xFFFF);
        return;
    }
    if (boss_move.timer > 0x78)
        return;
    self->children[Part_LeftCatcher].x--;
    own->left_y += (int32_t)own->left_speed << 8;
    own->left_speed += 0x38;
    self->children[Part_LeftCatcher].y = (int16_t)(own->left_y >> 16);
    if (self->children[Part_LeftCatcher].y >= 0x6F0)
        own->left_speed = 0;
    if (boss_move.timer > 0x3C)
        return;
    self->children[Part_RightCatcher].x++;
    own->right_y += (int32_t)own->right_speed << 8;
    own->right_speed += 0x38;
    self->children[Part_RightCatcher].y = (int16_t)(own->right_y >> 16);
    if (self->children[Part_RightCatcher].y >= 0x6F0)
        own->right_speed = 0;
}

// Offset_0x02D162: the path (the pre-beta's corners are one line: the track is at Y $63A all round, so it only goes to and fro between $28A0 and $29E0, and the catching starts where it passes $2940 going left)
static void Boss_Path(Sst *self, Own_CNZBoss *own) {
    switch (own->boss.flag) {
    case Path_Right:
        if (boss_move.x.f.u < 0x29E0)
            return;
        boss_move.x.f.u = 0x29E0;
        boss_move.xsp = 0;
        boss_move.ysp = (int16_t)0xFE80;
        own->boss.flag = Path_Up;
        break;
    case Path_Up:
        if (boss_move.y.f.u > 0x63A)
            return;
        SST_RENDER_SET(self, SST_RENDER_X_FLIP, false);
        boss_move.y.f.u = 0x63A;
        boss_move.ysp = 0;
        boss_move.xsp = (int16_t)0xFE80;
        own->boss.flag = Path_Left;
        own->boss.count &= 0x7F;
        break;
    case Path_Left:
        if (boss_move.x.f.u > 0x2940)
            return;
        if (!(own->boss.count & 0x80)) {
            own->boss.count |= 0x80;
            own->boss.anim_routine += 2;
        }
        if (boss_move.x.f.u > 0x28A0)
            return;
        boss_move.x.f.u = 0x28A0;
        boss_move.xsp = 0;
        boss_move.ysp = 0x180;
        own->boss.flag = Path_Down;
        break;
    case Path_Down:
        if (boss_move.y.f.u < 0x63A)
            return;
        SST_RENDER_SET(self, SST_RENDER_X_FLIP, true);
        boss_move.y.f.u = 0x63A;
        boss_move.ysp = 0;
        boss_move.xsp = 0x180;
        own->boss.flag = Path_Right;
        break;
    }
}

// Offset_0x02CF30: what a frame of the boss's moving is (the several waits and catches below go on with it)
static void Boss_Moving(Sst *self, Own_CNZBoss *own) {
    Boss_Move();
    Boss_Bob(self, own);
    Boss_Path(self, own);
    Boss_Parts(self, own);
    Boss_AnimateSprite(self, &own->boss, Animation_CNZBoss);
    Sst_Draw(self);
}

static void Boss_OpenCatchers(void) { // Offset_0x02CF9E
    catcher_state = 2;
    boss_anim[3] = 0x20;
    boss_anim[9] = 0x20;
    boss_anim[1] = 0x40;
    SetPartAnim(Part_Robotnik, 5);
}

static void Boss_ShutCatchers(void) { // Offset_0x02CFCA
    catcher_state = 1;
    boss_anim[3] = 0;
    boss_anim[9] = 0;
    boss_anim[1] = 0;
}

static void Boss_DropBall(Sst *self) { // Offset_0x02D04E
    Sst *ball = Sst_Allocate();
    if (ball == NULL)
        return;
    ball->code = sst_object_codes[ObjId_CNZBoss];
    ball->routine = Boss_Ball;
    ((Own_CNZBall *)ball->own.u8)->parent = (uint8_t)(self - sst);
}

static void Boss_Dig(Sst *self, Own_CNZBoss *own) {
    Own_AlphaBoss *boss = &own->boss;

    switch (boss->anim_routine) {
    case Run_Move:
        Boss_Moving(self, own);
        break;

    case Run_Choose: // on one side it opens its catchers, on the other it lets the ball fall
        if (!(boss->count & 1)) {
            boss->count++;
            self->mapping_frame = 0x0C; // (the main sprite shows while the catchers wait)
            boss->anim_routine += 2;
            boss_move.timer = 0xC8;
        } else {
            boss->count--;
            self->mapping_frame = 0;
            boss->anim_routine += 4;
            Boss_DropBall(self);
            boss_move.timer = 0xDC;
        }
        Boss_Moving(self, own);
        break;

    case Run_Drop:
        boss_move.timer--;
        if (boss_move.timer < 0) {
            if (boss_move.timer == (int16_t)0xFFB5) {
                Boss_OpenCatchers();
            } else if (boss_move.timer == (int16_t)0xFF6A) {
                boss->anim_routine = Run_Move;
                Boss_ShutCatchers();
            }
        } else if (boss_move.timer == 0) {
            Boss_ShutCatchers();
        } else if (boss_move.timer == 0x4B) {
            Boss_OpenCatchers();
        }
        Boss_Moving(self, own);
        break;

    case Run_Catch:
        catcher_state = 0;
        boss_move.timer--;
        if (boss_move.timer == 0) {
            boss_anim[3] = 0x20;
            boss_anim[9] = 0x20;
            SetPartAnim(Part_Robotnik, 5);
        } else if (boss_move.timer <= -0x14) {
            boss_anim[3] = 0;
            boss_anim[9] = 0;
            boss->anim_routine = Run_Move;
        }
        Boss_Moving(self, own);
        break;

    case Run_Wreck: // blowing up where it is (and the arena's wall opens at the end)
        boss->side = 0xFF;
        dhz_shake = 0;
        if (--boss_move.timer >= 0) {
            Boss_Defeated(self);
        } else {
            self->children[Part_Robotnik].frame = 0x0B;
            self->mapping_frame = 0;
            SST_RENDER_SET(self, SST_RENDER_X_FLIP, true);
            boss_move.xsp = 0;
            boss_move.ysp = 0;
            boss->anim_routine += 2;
            LevelLayout_SetChunk(LEVEL_LAYOUT_PLANE_FG, 0x54, 0x0C, 0xDD);
            boss_move.timer = -0x12;
        }
        self->y_pos.f.u = boss_move.y.f.u;
        self->x_pos.f.u = boss_move.x.f.u;
        Boss_Parts(self, own);
        Sst_Draw(self);
        break;

    case Run_Fall: // the ship drops a little and hovers, and then goes
        boss_move.timer++;
        if (boss_move.timer == 0) {
            boss_move.ysp = 0;
        } else if (boss_move.timer > 0) {
            if (boss_move.timer < 0x18)
                boss_move.ysp -= 8;
            else if (boss_move.timer == 0x18)
                boss_move.ysp = 0;
            else if (boss_move.timer >= 0x20)
                boss->anim_routine += 2;
        } else {
            boss_move.ysp += 0x18;
        }
        Boss_Move();
        Boss_Bob(self, own);
        self->y_pos.f.u = boss_move.y.f.u;
        self->x_pos.f.u = boss_move.x.f.u;
        Boss_Parts(self, own);
        Sst_Draw(self);
        break;

    case Run_Leave: // up and away to the right, the arena opening as it goes; when it is off the screen the act is over
        boss_move.xsp = 0x400;
        boss_move.ysp = (int16_t)0xFFC0;
        if (limit_right2 != 0x2980 + SCREEN_WIDEADD2) {
            limit_right1 = limit_right2 += 2;
        } else if (!(self->render_flags & SST_RENDER_ON_SCREEN)) {
            if (!boss_status)
                boss_status = 1;
            Sst_Delete(self);
            return;
        }
        Boss_Move();
        Boss_Bob(self, own);
        self->y_pos.f.u = boss_move.y.f.u;
        self->x_pos.f.u = boss_move.x.f.u;
        Boss_Parts(self, own);
        Sst_Draw(self);
        break;
    }
}

// The ball moves by its speeds (Offset_0x02D48E): its X and Y are taken as whole pixels each frame, as the pre-beta does (a negative speed moves at least a pixel), and gravity is added
static void Ball_Fall_Move(Sst *self, Own_CNZBall *ball) {
    uint32_t x = (uint32_t)(uint16_t)ball->x_base << 16;
    uint32_t y = (uint32_t)(uint16_t)self->y_pos.f.u << 16;
    x += (uint32_t)((int32_t)ball->x_speed << 8);
    y += (uint32_t)((int32_t)ball->y_speed << 8);
    ball->y_speed += 0x38;
    ball->x_base = (int16_t)(x >> 16);
    self->x_pos.f.u = ball->x_base;
    self->y_pos.f.u = (int16_t)(y >> 16);
}

// Offset_0x02D45C: the same for the rolling ball (its X is its own, not the base's), without gravity
static void Ball_Roll_Move(Sst *self, Own_CNZBall *ball) {
    uint32_t x = (uint32_t)(uint16_t)self->x_pos.f.u << 16;
    uint32_t y = (uint32_t)(uint16_t)self->y_pos.f.u << 16;
    x += (uint32_t)((int32_t)ball->x_speed << 8);
    y += (uint32_t)((int32_t)ball->y_speed << 8);
    self->x_pos.f.u = (int16_t)(x >> 16);
    self->y_pos.f.u = (int16_t)(y >> 16);
}

// Offset_0x02D4CA: its time and its blinking; when the time is out, or the boss has been beaten, the ball breaks into two that fall away
static void Ball_Time(Sst *self, Own_CNZBall *ball) {
    const Sst *boss = &sst[ball->parent];
    const Own_CNZBoss *boss_own = (const Own_CNZBoss *)boss->own.u8;
    if (boss_own->boss.anim_routine < Run_Fall && --ball->timer >= 0) {
        if ((uint16_t)ball->timer >= 0x1A4)
            return;
        if (--ball->blink >= 0)
            return;
        ball->blink = (int16_t)(ball->timer / 0x3C);
        self->art_tile ^= 0x2000;
        return;
    }
    self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_CNZBoss);
    self->anim = 7;
    ball->y_speed = (int16_t)0xFD00;
    ball->x_speed = (int16_t)0xFF00;
    ball->x_base = self->x_pos.f.u;
    self->routine = Boss_Ball;
    self->routine_secondary = Ball_Away;
    self->collision_flags = 0x98;
    Sst *other = Sst_AllocateAfter(self);
    if (other != NULL) {
        *other = *self;
        ((Own_CNZBall *)other->own.u8)->x_speed = (int16_t)-ball->x_speed;
    }
}

// What the ball (routine 4) does: it hangs under the catcher, falls to the floor, rolls to and fro for a moment and breaks away
static void Ball_Run(Sst *self, Own_CNZBall *ball) {
    switch (self->routine_secondary) {
    case Ball_Make: {
        const Sst *boss = &sst[ball->parent];
        self->mappings = Mappings_CNZBoss;
        self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_CNZBoss);
        self->render_flags |= SST_RENDER_LEVEL;
        self->priority = 7 * 0x80;
        self->routine_secondary += 2;
        self->x_pos.f.u = boss->x_pos.f.u;
        ball->x_base = boss->x_pos.f.u;
        self->y_pos.f.u = (int16_t)(boss->y_pos.f.u + 0x30);
        self->y_radius = 8;
        self->x_radius = 8;
        self->width_pixels = 8;
        self->mapping_frame = 0x12;
        self->collision_flags = 0x98;
        break;
    }
    case Ball_Follow: { // it is held under the catcher, and goes down a little further each frame
        const Sst *boss = &sst[ball->parent];
        self->x_pos.f.u = boss->x_pos.f.u;
        ball->x_base = boss->x_pos.f.u;
        self->y_pos.f.u = (int16_t)(boss->y_pos.f.u + ball->drop);
        ball->drop++;
        if (ball->drop >= 0x2E)
            ball->drop = 0x2E;
        if (boss_move.timer == 0) {
            self->routine_secondary += 2;
            ball->x_speed = boss_move.xsp;
            ball->timer = 1;
        }
        Sst_Draw(self);
        break;
    }
    case Ball_Fall:
        Ball_Fall_Move(self, ball);
        ball->x_speed -= 8;
        {
            const int16_t d = SstFloorDist(self, self->x_pos.f.u);
            if (d < 0) {
                self->y_pos.f.u += d;
                ball->y_speed = 0;
                self->routine_secondary += 2;
                ball->direction = 0;
                ball->x_speed = (int16_t)0xFD00;
                ball->y_speed = 0x0800;
            }
        }
        Sst_Draw(self);
        break;
    case Ball_Roll: {
        Ball_Time(self, ball);
        const bool right = (ball->direction & 0x80) != 0;
        if (!right ? (uint16_t)self->x_pos.f.u < 0x2884 : (uint16_t)self->x_pos.f.u >= 0x29FC) {
            ball->direction ^= 0x80;
            ball->x_speed = (int16_t)-ball->x_speed;
            Sst_Draw(self);
            break;
        }
        Ball_Roll_Move(self, ball);
        const int16_t d = SstFloorDist(self, self->x_pos.f.u);
        ball->x_speed = (int16_t)(ball->x_speed + (int16_t)((int32_t)((int8_t)angle_buffer0) * 8 / 5)); // (the floor's angle, as D3: it speeds it up or slows it on a slope)
        if (!right ? ball->x_speed >= 0 : ball->x_speed < 0)
            ball->direction ^= 0x80;
        if (d < 0)
            self->y_pos.f.u += d;
        Sst_Draw(self);
        break;
    }
    case Ball_Away:
        Ball_Fall_Move(self, ball);
        Sst_Animate(self, Animation_CNZBoss);
        if ((uint16_t)self->y_pos.f.u >= 0x705) {
            Sst_Delete(self);
            return;
        }
        Sst_Draw(self);
        break;
    }
}

void Obj_CNZBoss(Sst *self) {
    Own_CNZBoss *own = (Own_CNZBoss *)self->own.u8;

    switch (self->routine) {
    case Boss_Start:
        self->mappings = Mappings_CNZBoss;
        self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_CNZBoss);
        self->render_flags |= SST_RENDER_LEVEL | SST_RENDER_MULTI_DRAW;
        self->priority = 3 * 0x80;
        self->width_pixels = 0x60;
        self->y_radius = 0x20;
        self->x_pos.f.u = 0x29D0;
        self->y_pos.f.u = 0x590;
        self->mapping_frame = 0;
        own->boss.flash = 0x80; // (Obj_Inertia: the first flash lasts $80 frames, with no sound)
        self->routine = Boss_Run;
        self->child_count = 4;
        self->collision_flags = 0x0F;
        self->collision_property = 2;
        boss_move.x.f.u = self->x_pos.f.u;
        boss_move.y.f.u = self->y_pos.f.u;
        boss_move.xsp = 0;
        boss_move.timer = 0;
        for (int i = 0; i < 4; i++) {
            self->children[i].x = self->x_pos.f.u;
            self->children[i].y = self->y_pos.f.u;
        }
        self->children[Part_RightCatcher].frame = 5;
        self->children[Part_Ship].frame = 1;
        self->children[Part_Robotnik].frame = 6;
        self->children[Part_LeftCatcher].frame = 2;
        own->boss.flag = Path_Up;
        boss_move.ysp = (int16_t)0xE800;
        own->left_y = own->right_y = (int32_t)((uint32_t)(uint16_t)self->y_pos.f.u << 16);
        {
            static const uint8_t buffer[10] = { 0x04, 0x00, 0x01, 0x00, 0x10, 0x00, 0x03, 0x00, 0x02, 0x00 };
            memcpy(boss_anim, buffer, sizeof(buffer));
        }
        break;

    case Boss_Run:
        Boss_Dig(self, own);
        break;

    case Boss_Ball:
        Ball_Run(self, (Own_CNZBall *)self->own.u8);
        break;
    }
}
