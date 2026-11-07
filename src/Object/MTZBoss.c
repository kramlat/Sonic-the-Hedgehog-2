// Metropolis's boss for Sonic 2, as the pre-beta has it (objects 54 and 53, Obj_0x54 and Obj_0x53): see MTZBoss.h. It uses the shared boss helpers (BossCommon): the ship's place and speed in a buffer, and the
// animation of its parts, each a pair of nibbles in a small buffer; its hits are counted by its own code, not Boss_Hit (the flash is $40 frames). The ship is a multi-sprite object: the main sprite is the hull, and
// its children are Robotnik and the boost. Object 54 comes in three kinds by its routine: 2 is the ship, 4 a shot and 6 a part that follows the ship; object 53 in five: 0 makes the seven balls, 2 is a ball in orbit,
// 4 one that has been let go and falls, 6 the small Robotnik that walks out of it and 8 its end.
#include "Object/MTZBoss.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"
#include "Object/BossCommon.h"
#include "Object/BossArt.h"
#include "Object/Tails.h"
#include "Constants.h"

#include "EnginePalette.h"
#include "Level.h"
#include "MathUtil.h"
#include "Sound.h"

#include <string.h>

#include "Macros.h"

#include "Resource/Animation/MTZBoss.h"
#include "Resource/Mappings/MTZBoss.h"

enum { Boss_Start = 0, Boss_Ship = 2, Boss_Shot = 4, Boss_Part = 6 };
enum { Ball_Make = 0, Ball_Orbit = 2, Ball_Fall = 4, Ball_Walk = 6, Ball_Gone = 8 };
// the ship's states (Obj_Ani_Boss_Routine)
enum { Ship_Descend = 0, Ship_Patrol = 2, Ship_Settle = 4, Ship_Grow = 6, Ship_Shrink = 8, Ship_Hit = 0xA, Ship_Rise = 0xC, Ship_Dive = 0xE, Ship_Wreck = 0x10, Ship_Leave = 0x12 };
// the dive's steps (Obj_Control_Var_02)
enum { Dive_Out = 0, Dive_Down = 2, Dive_Up = 4 };
// what the ship's status byte (Obj_Player_Status) keeps
enum { Status_Right = 0x80, Status_Turned = 0x40 };

typedef struct {
    Own_AlphaBoss boss; // flag: a hit to tell the balls of (var_0C), side: how many balls are out (var_00), count: how many hits still let a ball go (var_12), angle: the bob (Obj_Map_Id), flash: Obj_Inertia
    uint8_t status;     // 0x2B (Obj_Player_Status)
    uint8_t shots;      // 0x2D (var_01): how many shots are left in this dive
    uint8_t dive;       // 0x2E (var_02): the dive's step
    uint8_t pause;      // 0x2F (var_03): frames to wait after a shot
    uint8_t radius;     // 0x33 (var_07): how far out the balls orbit across
    uint8_t depth;      // 0x39 (var_0D): ... and up and down
    uint8_t mode;       // 0x3A (var_0E): $FF the balls are drawn in (they go round flatter), $80 they are let out again
} Own_MTZBoss;

// A ball's own (the offsets are the pre-beta's)
typedef struct {
    uint8_t angle;      // 0x28 (Obj_Subtype): the angle round the ship, on by 4 a frame
    uint8_t angle_up;   // 0x29: the angle the ship's radius is seen at ($40: the middle of the swing)
    int16_t y_centre;   // 0x2A (Obj_Timer): the ship's Y less 4
    uint8_t walk;       // 0x2C (var_00): a small Robotnik's hop
    uint8_t parent;     // 0x34: the slot of the ship
    int16_t x_centre;   // 0x38 (var_0C): the ship's X
    int16_t ground;     // 0x2E (var_02): the Y the small Robotnik walks on
    int16_t z;          // 0x30 (var_04): how near the ball is (it is drawn bigger and in front)
    int8_t wait;        // 0x32 (Obj_Boss_Hit_2): frames before it hurts
    uint8_t angle_2;    // 0x3B (var_0F): the ball's up-and-down angle
    uint8_t angle_3;    // 0x3C (var_10): ... while the balls are drawn in
} Own_MTZBall;

// What a shot and the part that follows the ship keep
typedef struct {
    uint8_t parent;
} Own_MTZPart;

static uint8_t dle_counter; // $F73A

void MTZBoss_Reset(void) {
    dle_counter = 0;
}

// DynResize_Mz3: X $2980 fixes the left edge and the lower limit; X $2A80 shuts the arena ($2AB0, both edges), the music fades, Boss_Flag 7 and list $2E; the top is held at $400 and $5A frames on the boss is made
// and its music starts; when it is beaten, the results card
void MTZBoss_Events(void) {
    switch (dle_routine) {
    case 0:
        if (scrpos_x.f.u < (0x2980 - SCREEN_WIDEADD2))
            break;
        limit_left2 = scrpos_x.f.u;
        limit_btm1 = 0x0400 - SCREEN_TALLADD;
        dle_routine += 2;
        break;
    case 2:
        if (scrpos_x.f.u < (0x2A80 - SCREEN_WIDEADD2))
            break;
        limit_left2 = 0x2AB0 - SCREEN_WIDEADD2;
        limit_right2 = 0x2AB0 + SCREEN_WIDEADD2;
        dle_routine += 2;
        QueueSound1(bgm_Fade);
        dle_counter = 0;
        boss_flag = 7;
        AddPLC(PlcId_MTZBoss);
        break;
    case 4:
        if (scrpos_y.f.u >= (0x400 - SCREEN_TALLADD))
            limit_top2 = 0x400 - SCREEN_TALLADD;
        if (++dle_counter >= 0x5A) {
            Sst *boss = Sst_Allocate();
            if (boss != NULL)
                boss->code = sst_object_codes[ObjId_MTZBoss];
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

static bool Hurt_Anywhere(void) { // (Offset_0x02DC86: a character is being hurt: the slots $B024 and $B064 hold routine 4)
    return player->routine == 4 || (TAILS_OBJ->code != NULL && TAILS_OBJ->routine == 4);
}

// Offset_0x02DC28: where the children are (Robotnik, the boost)
static void Ship_Children(Sst *self) {
    for (int i = 0; i < 2; i++) {
        self->children[i].x = self->x_pos.f.u;
        self->children[i].y = self->y_pos.f.u;
    }
}

// Offset_0x02DD82: the hit (own to this boss: $40 frames of flash), and Offset_0x02DC42: what the first frame of a flash does
static void Ship_Damage(Sst *self, Own_MTZBoss *own) {
    Own_AlphaBoss *boss = &own->boss;
    if (boss->anim_routine < Ship_Wreck) {
        if (self->collision_property == 0) {
            AddPoints(100);
            boss_move.timer = 0xB3;
            boss->anim_routine = Ship_Wreck;
            goto after;
        }
        if (self->collision_flags == 0) {
            if (boss->flash == 0) {
                boss->flash = 0x40;
                QueueSound2(sfx_HitBoss);
            }
            dry_palette[1][1] = (dry_palette[1][1] == 0) ? 0x0EEE : 0;
            if (--boss->flash == 0)
                self->collision_flags = 0x0F;
        }
    }
after:
    if (boss->flash == 0x3F) { // (the first frame of the flash: the balls are told, Robotnik shows his hurt face)
        boss->flag = 0xFF;
        boss_anim[2] = (uint8_t)((boss_anim[2] & 0xF0) | 5);
        if (boss->count != 0) {
            boss->anim_routine = Ship_Hit;
            boss_move.ysp = (int16_t)0xFE80;
            boss->count--;
        }
        boss_move.xsp = 0;
    } else if (Hurt_Anywhere()) { // (Robotnik laughs at a character who is hurt)
        if ((boss_anim[2] & 0x0F) != 4)
            boss_anim[2] = (uint8_t)((boss_anim[2] & 0xF0) | 4);
    }
}

// What every state ends with (Offset_0x02D97A)
static void Ship_Finish(Sst *self, Own_MTZBoss *own) {
    Ship_Damage(self, own);
    Boss_AnimateSprite(self, &own->boss, Animation_MTZBoss);
    Ship_Children(self);
    Sst_Draw(self);
}

// Offset_0x02D8F0 and Offset_0x02D970: the ship goes to its place with a bob (the sine of an angle that goes on by 4 a frame)
static void Ship_Hover(Sst *self, Own_MTZBoss *own) {
    int16_t sin, cos;
    self->x_pos.f.u = boss_move.x.f.u;
    CalcSine(own->boss.angle, &sin, &cos);
    self->y_pos.f.u = (int16_t)((sin >> 6) + boss_move.y.f.u);
    own->boss.angle = (uint8_t)(own->boss.angle + 4);
    Ship_Finish(self, own);
}

// Offset_0x02DBF4: a shot at a time while it dives
static void Ship_Shoot(Sst *self, Own_MTZBoss *own) {
    if (--boss_move.timer != 0 || own->shots == 0)
        return;
    own->shots--;
    Sst *shot = Sst_Allocate();
    if (shot == NULL)
        return;
    shot->code = sst_object_codes[ObjId_MTZBoss];
    shot->routine = Boss_Shot;
    ((Own_MTZPart *)shot->own.u8)->parent = (uint8_t)(self - sst);
    boss_move.timer = 0x1E;
    own->pause = 0x10;
}

static void Ship_Turn(Sst *self, bool right) { // (the flip bit: facing right)
    SST_RENDER_SET(self, SST_RENDER_X_FLIP, right);
}

// Offset_0x02DAE2: the dive. It goes out to the side and down (Dive_Out), across and down to $4A0 (Dive_Down), shooting, and up again (Dive_Up), a shot every $1E frames
static void Ship_DiveRun(Sst *self, Own_MTZBoss *own) {
    if (own->pause) {
        own->pause--;
        Ship_Finish(self, own);
        return;
    }
    switch (own->dive) {
    case Dive_Out:
        Boss_Move();
        if ((uint16_t)boss_move.y.f.u < 0x420)
            boss_move.ysp = 0;
        if (!(own->status & Status_Right)) {
            if ((uint16_t)boss_move.x.f.u < 0x2AF0) {
                own->dive += 2;
                boss_move.ysp = 0x180;
                own->shots = 4;
                boss_move.timer = 0x1E;
                Ship_Turn(self, true);
            }
        } else if ((uint16_t)boss_move.x.f.u >= 0x2BB0) {
            own->dive += 2;
            boss_move.ysp = 0x180;
            own->shots = 4;
            boss_move.timer = 0x1E;
            Ship_Turn(self, false);
        }
        break;

    case Dive_Down:
        Boss_Move();
        if ((uint16_t)boss_move.y.f.u >= 0x4A0) {
            boss_move.ysp = (int16_t)0xFE80;
            own->dive += 2;
            own->status ^= Status_Right;
        } else if (!(own->status & Status_Right)) {
            if ((uint16_t)boss_move.x.f.u < 0x2AD0)
                boss_move.xsp = 0;
        } else if ((uint16_t)boss_move.x.f.u >= 0x2BD0) {
            boss_move.xsp = 0;
        }
        Ship_Shoot(self, own);
        break;

    case Dive_Up:
        Boss_Move();
        if ((uint16_t)boss_move.y.f.u < 0x470) {
            boss_move.xsp = 0x100;
            if (!(own->status & Status_Right))
                boss_move.xsp = (int16_t)0xFF00;
        }
        if ((uint16_t)boss_move.y.f.u < 0x420) {
            boss_move.ysp = 0;
            own->dive = Dive_Out;
        }
        Ship_Shoot(self, own);
        break;
    }
    Ship_Hover(self, own);
}

static void Ship_Run(Sst *self, Own_MTZBoss *own) {
    Own_AlphaBoss *boss = &own->boss;

    switch (boss->anim_routine) {
    case Ship_Descend: // Offset_0x02D890: down to $4A0, where it turns to the side Sonic is not on
        Boss_Move();
        self->y_pos.f.u = boss_move.y.f.u;
        if ((uint16_t)boss_move.y.f.u >= 0x4A0) {
            boss->anim_routine += 2;
            boss_move.ysp = 0;
            boss_move.xsp = (int16_t)0xFF00;
            own->status &= (uint8_t)~Status_Right;
            Ship_Turn(self, false);
            if ((uint16_t)player->x_pos.f.u >= (uint16_t)boss_move.x.f.u) {
                boss_move.xsp = 0x100;
                own->status |= Status_Right;
                Ship_Turn(self, true);
            }
        }
        Ship_Finish(self, own);
        return;

    case Ship_Patrol: // Offset_0x02D90A: to and fro between $2AD0 and $2BD0; the second time it turns it goes up
        Boss_Move();
        if (!(own->status & Status_Right)) {
            if ((uint16_t)boss_move.x.f.u < 0x2AD0) {
                own->status ^= Status_Right;
                boss_move.xsp = 0x100;
                Ship_Turn(self, true);
                if (own->status & Status_Turned) {
                    boss->anim_routine += 2;
                    boss_move.ysp = (int16_t)0xFF00;
                }
                own->status |= Status_Turned;
            }
        } else if ((uint16_t)boss_move.x.f.u >= 0x2BD0) {
            own->status ^= Status_Right;
            boss_move.xsp = (int16_t)0xFF00;
            Ship_Turn(self, false);
            if (own->status & Status_Turned) {
                boss->anim_routine += 2;
                boss_move.ysp = (int16_t)0xFF00;
            }
            own->status |= Status_Turned;
        }
        break;

    case Ship_Settle: // Offset_0x02D990: up to $470 and across to the middle ($2B50), where it stops
        Boss_Move();
        if ((uint16_t)boss_move.y.f.u < 0x470)
            boss_move.ysp = 0;
        if (!(own->status & Status_Right)) {
            if ((uint16_t)boss_move.x.f.u < 0x2B50)
                boss_move.xsp = 0;
        } else if ((uint16_t)boss_move.x.f.u >= 0x2B50) {
            boss_move.xsp = 0;
        }
        if (boss_move.xsp == 0 && boss_move.ysp == 0)
            boss->anim_routine += 2;
        break;

    case Ship_Grow: // Offset_0x02D9D8: the balls spread out, and wait
        if (own->radius < 0x68) {
            own->radius++;
            own->depth++;
        } else if (--own->depth == 0) {
            boss->anim_routine += 2;
        }
        break;

    case Ship_Shrink: // Offset_0x02D9F8: ... and come in again, and it patrols once more
        if (own->radius >= 0x27) {
            own->radius--;
        } else {
            own->depth++;
            if (own->depth >= 0x27) {
                boss_move.ysp = 0x100;
                boss->anim_routine = Ship_Descend;
                own->status &= (uint8_t)~Status_Turned;
            }
        }
        break;

    case Ship_Hit: // Offset_0x02DA28: a hit: up to $420 while the balls (the ones let go) are drawn in, and it waits until they are all gone
        if (own->depth != 0)
            own->depth--;
        else
            own->mode = 0xFF;
        if (own->radius >= 0x27)
            own->radius--;
        Boss_Move();
        if ((uint16_t)boss_move.y.f.u < 0x420)
            boss_move.ysp = 0;
        if (boss->side == 0) {
            if (own->mode != 0)
                own->mode = 0x80;
            boss->anim_routine += 2;
        }
        break;

    case Ship_Rise: // Offset_0x02DA72: the balls out again, then it patrols once more; after the seventh hit it dives instead
        if (boss->count != 0) {
            if (own->mode != 0)
                break;
            if (own->depth < 0x27) {
                own->depth++;
                break;
            }
            boss_move.ysp = 0x100;
            boss->anim_routine = Ship_Descend;
            own->status &= (uint8_t)~Status_Turned;
        } else {
            boss_move.ysp = (int16_t)0xFE80;
            boss_move.xsp = (int16_t)0xFF00;
            Ship_Turn(self, false);
            if (own->status & Status_Right) {
                boss_move.xsp = 0x100;
                Ship_Turn(self, true);
            }
            boss->anim_routine = Ship_Dive;
            own->dive = Dive_Out;
            own->status &= (uint8_t)~Status_Turned;
            own->pause = 0;
        }
        break;

    case Ship_Dive:
        Ship_DiveRun(self, own);
        return;

    case Ship_Wreck: // Offset_0x02DCB6: blowing up where it is, and then it turns and falls still
        if (--boss_move.timer >= 0) {
            Boss_Defeated(self);
            boss_anim[2] = 7;
        } else {
            Ship_Turn(self, true);
            boss_move.xsp = 0;
            boss_move.ysp = 0;
            boss->anim_routine += 2;
            boss_move.timer = (int16_t)0xFFEE;
            boss_anim[2] = 3;
        }
        self->y_pos.f.u = boss_move.y.f.u;
        self->x_pos.f.u = boss_move.x.f.u;
        Boss_AnimateSprite(self, boss, Animation_MTZBoss);
        Ship_Children(self);
        Sst_Draw(self);
        return;

    case Ship_Leave: // Offset_0x02DD0C: away to the right as the arena opens to $2AB0 (and its edge off the screen, the act is over)
        boss_move.xsp = 0x400;
        boss_move.ysp = (int16_t)0xFFC0;
        if (limit_right2 < 0x2AB0 + SCREEN_WIDEADD2) {
            limit_right1 = limit_right2 += 2;
        } else if (!(self->render_flags & SST_RENDER_ON_SCREEN)) {
            if (!boss_status)
                boss_status = 1;
            Sst_Delete(self);
            return;
        }
        Boss_Move();
        self->x_pos.f.u = boss_move.x.f.u;
        self->y_pos.f.u = boss_move.y.f.u;
        own->boss.angle = (uint8_t)(own->boss.angle + 2);
        Boss_AnimateSprite(self, boss, Animation_MTZBoss);
        Ship_Children(self);
        Sst_Draw(self);
        return;
    }
    Ship_Hover(self, own);
}

// The ship's shot (Mz_Boss_Sub_02): from its nose, $400 a frame the way it looks, until it is out of the arena
static void Shot_Run(Sst *self) {
    Own_MTZPart *shot = (Own_MTZPart *)self->own.u8;
    if (self->routine_secondary == 0) {
        const Sst *boss = &sst[shot->parent];
        self->mappings = Mappings_MTZBoss;
        self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_MTZBoss);
        self->render_flags |= SST_RENDER_LEVEL;
        self->priority = 5 * 0x80;
        self->mapping_frame = 0x12;
        self->routine_secondary += 2;
        self->width_pixels = 0x50;
        self->x_pos.f.u = (int16_t)(boss->x_pos.f.u - 4);
        self->y_pos.f.u = (int16_t)(boss->y_pos.f.u + 7);
        int16_t speed = (int16_t)0xFC00;
        if (boss->render_flags & SST_RENDER_X_FLIP) {
            speed = (int16_t)-speed;
            self->x_pos.f.u += 8;
        }
        self->x_vel = speed;
        self->collision_flags = 0x99;
        QueueSound2(sfx_LaserBurst);
    }
    Sst_MoveSprite2(self);
    if ((uint16_t)self->x_pos.f.u < 0x2AB0 || (uint16_t)self->x_pos.f.u >= 0x2BF0) {
        Sst_Delete(self);
        return;
    }
    Sst_Draw(self);
}

// The part that follows the ship (Mz_Boss_Sub_03): gone when the ship is
static void Part_Run(Sst *self) {
    const Sst *boss = &sst[((Own_MTZPart *)self->own.u8)->parent];
    if (boss->code != sst_object_codes[ObjId_MTZBoss]) {
        Sst_Delete(self);
        return;
    }
    self->x_pos.f.u = boss->x_pos.f.u;
    self->y_pos.f.u = boss->y_pos.f.u;
    SST_RENDER_SET(self, SST_RENDER_X_FLIP, boss->render_flags & SST_RENDER_X_FLIP);
    Sst_Draw(self);
}

static void Ship_Start(Sst *self) { // Offset_0x02D752
    Own_MTZBoss *own = (Own_MTZBoss *)self->own.u8;
    self->mappings = Mappings_MTZBoss;
    self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_MTZBoss);
    self->render_flags |= SST_RENDER_LEVEL | SST_RENDER_MULTI_DRAW;
    self->priority = 3 * 0x80;
    self->x_pos.f.u = 0x2B50;
    self->y_pos.f.u = 0x380;
    self->mapping_frame = 2;
    self->routine = Boss_Ship;
    self->child_count = 2;
    self->width_pixels = 0x60;
    self->height_pixels = 0x20;
    self->collision_flags = 0x0F;
    self->collision_property = 8;
    own->boss.count = 7;
    boss_move.x.f.u = self->x_pos.f.u;
    boss_move.y.f.u = self->y_pos.f.u;
    boss_move.xsp = 0;
    boss_move.ysp = 0x100;
    own->status = 0;
    own->boss.side = 0;
    own->boss.angle = 0x40;
    own->radius = 0x27;
    own->depth = 0x27;
    Ship_Children(self);
    self->children[0].frame = 0x0C;
    self->children[1].frame = 0;
    Sst *part = Sst_Allocate();
    if (part != NULL) {
        part->code = sst_object_codes[ObjId_MTZBoss];
        part->routine = Boss_Part;
        part->mapping_frame = 0x13;
        part->mappings = Mappings_MTZBoss;
        part->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_MTZBoss);
        part->render_flags |= SST_RENDER_LEVEL;
        part->priority = 6 * 0x80;
        part->x_pos.f.u = self->x_pos.f.u;
        part->y_pos.f.u = self->y_pos.f.u;
        ((Own_MTZPart *)part->own.u8)->parent = (uint8_t)(self - sst);
        part->width_pixels = 0x20;
        Sst *maker = Sst_Allocate();
        if (maker != NULL) {
            maker->code = sst_object_codes[ObjId_MTZBossBall];
            ((Own_MTZBall *)maker->own.u8)->parent = (uint8_t)(self - sst);
        }
    }
    static const uint8_t buffer[6] = { 0x10, 0x00, 0x03, 0x00, 0x01, 0x00 };
    memcpy(boss_anim, buffer, sizeof(buffer));
}

void Obj_MTZBoss(Sst *self) {
    switch (self->routine) {
    case Boss_Start:
        Ship_Start(self);
        break;
    case Boss_Ship:
        Ship_Run(self, (Own_MTZBoss *)self->own.u8);
        break;
    case Boss_Shot:
        Shot_Run(self);
        break;
    case Boss_Part:
        Part_Run(self);
        break;
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// The balls
// ---------------------------------------------------------------------------------------------------------------------------------------

// Offset_0x02E0C2: it turns to Sonic
static void Ball_Face(Sst *self) {
    SST_RENDER_SET(self, SST_RENDER_X_FLIP, (int16_t)(player->x_pos.f.u - self->x_pos.f.u) >= 0);
}

// Offset_0x02DEE6: the place on the orbit: seen from the side it is a circle across by the radius, its angle on by 4 a frame, and up and down by the second radius (a ball swings up and down, the ship's balls together,
// until they are drawn in: they go flat)
static void Ball_Move(Sst *self, Own_MTZBall *ball, Own_MTZBoss *ship) {
    int16_t s_up, c_up, s_a, c_a, s_b, c_b;
    CalcSine(ball->angle_up, &s_up, &c_up);
    const int32_t across = (int32_t)s_up * ship->radius;
    const int16_t d5 = (int16_t)across, d4 = (int16_t)across;
    int16_t d2 = ship->depth;
    if (ship->mode != 0)
        d2 = 0x10;
    const int16_t up = (int16_t)((int32_t)s_up * d2);
    CalcSine(ball->angle, &s_a, &c_a);
    self->x_pos.f.u = (int16_t)((((int32_t)s_a * d5) >> 16) + ball->x_centre);
    ball->z = (int16_t)(((int32_t)c_a * d4) >> 16);
    CalcSine(ship->mode != 0 ? ball->angle_3 : ball->angle_2, &s_b, &c_b);
    self->y_pos.f.u = (int16_t)((((int32_t)s_b * up) >> 16) + ball->y_centre);
    ball->angle = (uint8_t)(ball->angle + 4);
    if (ship->mode == 0) {
        ball->angle_2 = (uint8_t)(ball->angle_2 + 8);
        return;
    }
    if (ship->mode == 0xFF) {
        if (ball->angle_3 < 0x40)
            ball->angle_3 += 2;
        return;
    }
    if (ship->mode == 0x80) {
        ball->angle_3 = (uint8_t)(ball->angle_3 - 2);
        if ((int8_t)ball->angle_3 >= 0)
            return;
        ball->angle_3 = 0;
    }
    ship->mode = 0;
}

// Offset_0x02DF90: how near it is: the bigger, the more in front
static void Ball_Depth(Sst *self, const Own_MTZBall *ball) {
    if (ball->z >= 0) {
        if (ball->z >= 0x0C) {
            self->mapping_frame = 3;
            self->priority = 1 * 0x80;
        } else {
            self->mapping_frame = 4;
            self->priority = 2 * 0x80;
        }
    } else if (ball->z >= -0x0C) {
        self->mapping_frame = 4;
        self->priority = 6 * 0x80;
    } else {
        self->mapping_frame = 5;
        self->priority = 7 * 0x80;
    }
}

static void Ball_Start(Sst *self, Own_MTZBall *maker) { // Offset_0x02DDF6: seven balls, this the first
    static const uint8_t angles[7] = { 0x24, 0x6C, 0xB4, 0xFC, 0x48, 0x90, 0xD8 };
    Sst *ball = self;
    for (int i = 0; i < 7; i++) {
        if (i > 0) {
            ball = Sst_Allocate();
            if (ball == NULL)
                return;
        }
        Own_MTZBall *own = (Own_MTZBall *)ball->own.u8;
        ball->width_pixels = 0x20;
        own->parent = maker->parent;
        ball->code = sst_object_codes[ObjId_MTZBossBall];
        ball->mappings = Mappings_MTZBoss;
        ball->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_MTZBoss);
        ball->render_flags |= SST_RENDER_LEVEL;
        ball->priority = 3 * 0x80;
        ball->routine += 2;
        ball->mapping_frame = 5;
        own->angle = angles[i];
        own->angle_2 = angles[i];
        own->angle_up = 0x40;
        ball->collision_flags = 0x87;
        ball->collision_property = 2;
        own->angle_3 = 0;
    }
}

// Offset_0x02E038: a ball that has been rolled into (twice, and then once more) pops
static void Ball_PopCheck(Sst *self) {
    if ((int8_t)self->collision_property <= -2) {
        self->mapping_frame = 0x14;
        self->anim = 6;
        self->routine += 2;
    }
}

static void Ball_Animate(Sst *self) { // Offset_0x02E026
    Ball_PopCheck(self);
    Sst_Animate(self, Animation_MTZBoss);
    Sst_Draw(self);
}

// The wait before a let-go ball hurts ($DA), counted in Obj_Boss_Hit_2
static void Ball_Arm(Sst *self, Own_MTZBall *ball) {
    if (ball->wait >= 0) {
        if (--ball->wait < 0)
            self->collision_flags = 0xDA;
    }
}

void Obj_MTZBossBall(Sst *self) {
    Own_MTZBall *ball = (Own_MTZBall *)self->own.u8;

    switch (self->routine) {
    case Ball_Make:
        Ball_Start(self, ball);
        break;

    case Ball_Orbit: {
        Sst *ship = &sst[ball->parent];
        Own_MTZBoss *ship_own = (Own_MTZBoss *)ship->own.u8;
        ball->y_centre = (int16_t)(ship->y_pos.f.u - 4);
        ball->x_centre = ship->x_pos.f.u;
        if (ship_own->boss.flag != 0) { // a hit: this ball is let go
            ship_own->boss.flag = 0;
            ship_own->boss.side++;
            self->routine += 2;
            ball->wait = 0x3C;
            self->anim = 2;
            self->y_vel = (int16_t)0xFC00;
            int16_t speed = (int16_t)0xFF80;
            if ((int16_t)(player->x_pos.f.u - self->x_pos.f.u) < 0)
                speed = (int16_t)-speed;
            self->x_vel = speed;
        } else if (self->collision_property != 2) { // (a ball has been rolled into: the ship cannot be hurt)
            ship->collision_flags = 0;
        }
        Ball_Move(self, ball, ship_own);
        Ball_Depth(self, ball);
        Sst_Draw(self);
        break;
    }

    case Ball_Fall:
        Ball_Arm(self, ball);
        Sst_MoveSprite(self);
        self->y_vel -= 0x20;
        if (self->y_vel >= 0x180)
            self->y_vel = 0x180;
        if ((uint16_t)self->y_pos.f.u >= 0x4AC) {
            self->y_pos.f.u = 0x4AC;
            ball->ground = 0x4AC;
            ball->walk = 1;
            self->routine += 2;
            Ball_Face(self);
        }
        Ball_Animate(self);
        break;

    case Ball_Walk:
        Ball_Arm(self, ball);
        if (Hurt_Anywhere()) { // (Offset_0x02E0DC: when a character is hurt the small Robotnik is gone too)
            self->mapping_frame = 0x14;
            self->anim = 6;
        }
        if ((int8_t)self->collision_property <= -2) {
            self->mapping_frame = 0x14;
            self->anim = 6;
        }
        if (self->mapping_frame != 0x0B) {
            Ball_Animate(self);
            break;
        }
        {
            int16_t sin, cos;
            CalcSine(ball->walk, &sin, &cos);
            const int16_t y = (int16_t)(ball->ground + (-sin >> 2));
            if ((uint16_t)y < 0x4AC) {
                self->y_pos.f.u = y;
                ball->walk++;
                if (ball->walk & 1)
                    self->x_pos.f.u += (self->render_flags & SST_RENDER_X_FLIP) ? 1 : -1;
            } else {
                self->y_pos.f.u = 0x4AC;
                Ball_Face(self);
                ball->walk = 1;
            }
            Sst_Draw(self);
        }
        break;

    case Ball_Gone: {
        QueueSound2(sfx_Bomb);
        Own_MTZBoss *ship_own = (Own_MTZBoss *)sst[ball->parent].own.u8;
        ship_own->boss.side--;
        Sst_Delete(self);
        break;
    }
    }
}
