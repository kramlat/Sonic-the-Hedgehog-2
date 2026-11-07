// Oil Ocean's boss for Sonic 2, as the pre-beta has it (object 55, Obj_0x55): see OOZBoss.h. It uses the shared boss helpers (BossCommon): the ship's place and speed in a buffer, and the animation of its parts, each a pair
// of nibbles in a small buffer. Its parts are the object's child sprites: the ship (one child) while it dives, and the eight links of its tail (the main sprite is Robotnik himself). The object comes in four kinds
// by its routine: 2 dives out of the oil and bobs (the only one that can be hurt), 4 hovers and spits balls, 6 swings round the arena, and 8 is a ball of oil, which lights a flame (the same routine, by its second one).
#include "Object/OOZBoss.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"
#include "Object/BossCommon.h"
#include "Constants.h"

#include "EngineObject.h"
#include "EnginePalette.h"
#include "Level.h"
#include "MathUtil.h"
#include "Sound.h"

#include "Macros.h"

#include "Resource/Animation/OOZBoss.h"
#include "Resource/Mappings/OOZBoss.h"

#include "Object/BossArt.h"

// the routines of the object
enum { Boss_Start = 0, Boss_Dive = 2, Boss_Oil = 4, Boss_Orbit = 6, Boss_Ball = 8 };
// the dive's states (Obj_Ani_Boss_Routine; the wreck is Boss_Hit's: it sets 8)
enum { Dive_Start = 0, Dive_Rise = 2, Dive_Hover = 4, Dive_Settle = 6, Dive_Wreck = 8 };
// the states of the hovering
enum { Oil_Start = 0, Oil_Rise = 2, Oil_Pick = 4, Oil_Move = 6, Oil_Return = 8 };
enum { Orbit_Start = 0, Orbit_Swing = 2 };
// the second routine of a ball: it flies (2); a flame (4); gone (6)
enum { Ball_Make = 0, Ball_Fly = 2, Ball_Flame = 4, Ball_Gone = 6 };
enum { Tail_Links = 8 };

typedef struct {
    Own_AlphaBoss boss; // flag: how many moves it has left (var_0C), side: how long its mouth stays open (var_00), count: the heights it has been to (var_12), angle: the bob and the swing (Obj_Map_Id), flash: Obj_Inertia
    uint8_t hit;        // 0x2A (Obj_Timer): bit 7 it has been hit in this dive, bit 6 it has been up and is coming down
} Own_OOZBoss;

// A ball of oil and a flame keep (the offsets are the pre-beta's)
typedef struct {
    int16_t wait;      // 0x32 (var_06): a flame lights the next after this many frames
    uint8_t parent;    // 0x34: the slot of the boss (a ball's)
    uint8_t pad;       // 0x35
    uint8_t left;      // 0x36 (var_0A): how many flames are still to be lit by this row
} Own_OOZBall;

static uint8_t dle_counter; // $F73A

void OOZBoss_Reset(void) {
    dle_counter = 0;
}

// DynResize_OOz of act 2: X $2668 fixes the left edge and the lower limit, and the oil is raised to Y $2D8 (the oil is a level object); X $2880 shuts the arena ($2880 to $28C0), fades the music, sets Boss_Flag 8 and
// loads the boss's list; once the camera is at Y $1D8 its top is held, and $5A frames on the boss is made and its music starts; when it is beaten, the results card
void OOZBoss_Events(void) {
    switch (dle_routine) {
    case 0:
        if (scrpos_x.f.u < (0x2668 - SCREEN_WIDEADD2))
            break;
        limit_left2 = scrpos_x.f.u;
        for (int i = 0; i < SST_COUNT; i++) {
            if (sst[i].code == sst_object_codes[0x07])
                sst[i].y_pos.f.u = 0x2D8;
        }
        limit_btm1 = 0x01E0 - SCREEN_TALLADD;
        dle_routine += 2;
        break;
    case 2:
        if (scrpos_x.f.u < (0x2880 - SCREEN_WIDEADD2))
            break;
        limit_left2 = 0x2880 - SCREEN_WIDEADD2;
        limit_right2 = 0x28C0 + SCREEN_WIDEADD2;
        dle_routine += 2;
        QueueSound1(bgm_Fade);
        dle_counter = 0;
        boss_flag = 8;
        AddPLC(PlcId_OOZBoss);
        break;
    case 4:
        if (scrpos_y.f.u >= (0x1D8 - SCREEN_TALLADD))
            limit_top2 = 0x1D8 - SCREEN_TALLADD;
        if (++dle_counter >= 0x5A) {
            Sst *boss = Sst_Allocate();
            if (boss != NULL)
                boss->code = sst_object_codes[ObjId_OOZBoss];
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

// Offset_0x02E5D4: the hit; the frame it starts to flash (Obj_Inertia counted down to $1F) the ship shows the hurt face, and the dive is noted as one that was hit
static void Dive_Hit(Sst *self, Own_OOZBoss *own) {
    Boss_Hit(self, &own->boss);
    if (own->boss.flash == 0x1F) {
        boss_anim[0] = (uint8_t)((boss_anim[0] & 0xF0) | 3);
        own->hit |= 0x80;
    }
}

// Offset_0x02E5BE: what every state of the dive ends with
static void Dive_Finish(Sst *self, Own_OOZBoss *own) {
    Dive_Hit(self, own);
    Boss_AnimateSprite(self, &own->boss, Animation_OOZBoss);
    self->children[0].x = self->x_pos.f.u;
    self->children[0].y = self->y_pos.f.u;
    Sst_Draw(self);
}

// Offset_0x02E54A: the bob (the cosine of its angle, a byte that goes on by 4 a frame, about the ship's place)
static void Dive_Bob(Sst *self, Own_OOZBoss *own) {
    int16_t sin, cos;
    CalcSine(own->boss.angle, &sin, &cos);
    self->y_pos.f.u = (int16_t)((cos >> 7) + boss_move.y.f.u);
    own->boss.angle = (uint8_t)(own->boss.angle + 4);
}

// The side it looks from: the character is on the other side of $293A (the dive turns to him, the hovering from him)
static void Face(Sst *self, bool right) {
    SST_RENDER_SET(self, SST_RENDER_X_FLIP, right);
}

static void Dive_Run(Sst *self, Own_OOZBoss *own) {
    Own_AlphaBoss *boss = &own->boss;

    switch (boss->anim_routine) {
    case Dive_Start: // Offset_0x02E45E: it begins at the arena's middle, low in the oil, looking at the character
        boss_move.x.f.u = 0x2940;
        Face(self, player->x_pos.f.u < 0x293A);
        self->y_pos.f.u = 0x2D0;
        boss_move.y.f.u = 0x2D0;
        self->mapping_frame = 8;
        self->child_count = 1;
        boss->anim_routine += 2;
        boss_move.ysp = (int16_t)0xFF80;
        self->collision_flags = 0x0F;
        self->children[0].x = self->x_pos.f.u;
        self->children[0].y = self->y_pos.f.u;
        boss->angle = 0;
        own->hit = 0;
        self->children[0].frame = 8;
        boss_anim[0] = 0x05;
        boss_anim[1] = 0x00;
        boss_anim[2] = 0x01;
        boss_anim[3] = 0x00;
        return; // (it draws nothing this frame)

    case Dive_Rise: // Offset_0x02E4D8: it comes up (and bobs) to Y $290
        Boss_Move();
        self->x_pos.f.u = boss_move.x.f.u;
        Dive_Bob(self, own);
        if (boss_move.y.f.u < 0x290) {
            boss_move.y.f.u = 0x290;
            boss->anim_routine += 2;
            boss_move.timer = 0xA8;
            if (!(own->hit & 0x80)) {
                boss_anim[0] = 0x10;
                boss_anim[1] = 0x00;
            }
        }
        break;

    case Dive_Hover: // Offset_0x02E51A: it bobs for $A9 frames (a hit ends the wait), laughing, and then sets off up
        if (!(own->hit & 0x80)) {
            Dive_Bob(self, own);
            if (--boss_move.timer >= 0)
                break;
            boss_anim[0] = 0x05;
            boss_anim[1] = 0x00;
        }
        boss->anim_routine += 2;
        boss_move.ysp = (int16_t)0xFFC0;
        break;

    case Dive_Settle: // Offset_0x02E564: up a little more, then down to the oil again; a dive that was hit goes on to swing round the arena, the others to spit
        Boss_Move();
        self->y_pos.f.u = boss_move.y.f.u;
        self->x_pos.f.u = boss_move.x.f.u;
        if (!(own->hit & 0x40)) {
            if (boss_move.y.f.u < 0x28C) {
                boss_move.y.f.u = 0x28C;
                boss_move.ysp = 0x80;
                own->hit |= 0x40;
            }
        } else if (boss_move.y.f.u >= 0x2D0) {
            boss_move.y.f.u = 0x2D0;
            boss->anim_routine = 0;
            self->routine += 2;
            if (own->hit & 0x80)
                self->routine += 2;
        }
        break;

    case Dive_Wreck: // Offset_0x02E606: blowing up where it is (the face turns to the wreck's $1E frames before the end), and then it sinks while the arena opens to $2D00; offscreen of the oil, the act is over
        if (--boss_move.timer >= 0) {
            if (boss_move.timer < 0x1E)
                self->mapping_frame = 0x0B;
            Boss_Defeated(self);
            Sst_Draw(self);
            return;
        }
        if (limit_right2 < 0x2CFE + SCREEN_WIDEADD2) {
            limit_right2 += 2;
        } else {
            limit_right2 = 0x2D00 + SCREEN_WIDEADD2;
            if (self->y_pos.f.u >= 0x2D0) {
                boss_status = 1;
                Sst_Delete(self);
                return;
            }
        }
        limit_right1 = limit_right2;
        self->y_pos.f.u++;
        self->children[0].x = self->x_pos.f.u;
        self->children[0].y = self->y_pos.f.u;
        Sst_Draw(self);
        return;
    }
    Dive_Finish(self, own);
}

// Offset_0x02E82A: it turns to the character, unless he is within 8 pixels of it
static void Oil_Look(Sst *self) {
    const int16_t d = (int16_t)(player->x_pos.f.u - self->x_pos.f.u);
    if (d < 0) {
        if (d + 8 <= 0)
            Face(self, false);
    } else if (d - 8 >= 0) {
        Face(self, true);
    }
}

// The tail's link `k` hangs below the one before, 15 pixels each, the whole of it swaying on a cosine (Offset_0x02E890): the angle, the place of the link
static void Oil_Link(uint8_t angle, int16_t base_x, int16_t base_y, int16_t *x, int16_t *y) {
    int16_t sin, cos;
    CalcSine(angle, &sin, &cos);
    *x = (int16_t)((cos >> 4) + base_x);
    *y = (int16_t)((sin >> 6) + base_y);
}

// Offset_0x02E850
static void Oil_Tail(Sst *self, Own_OOZBoss *own) {
    const int16_t base_x = boss_move.x.f.u;
    int16_t base_y = boss_move.y.f.u;
    uint8_t angle = own->boss.angle;
    Oil_Link(angle, base_x, base_y, &self->x_pos.f.u, &self->y_pos.f.u);
    own->boss.angle = (uint8_t)(own->boss.angle + 2);
    for (int k = 0; k < Tail_Links; k++) {
        base_y = (int16_t)(base_y + 0x0F);
        angle = (uint8_t)(angle - 0x10);
        Oil_Link(angle, base_x, base_y, &self->children[k].x, &self->children[k].y);
    }
}

// Offset_0x02E804
static void Oil_Finish(Sst *self, Own_OOZBoss *own) {
    Oil_Look(self);
    Oil_Tail(self, own);
    Sst_Draw(self);
}

// Offset_0x02E810: the way to the height it has picked
static void Oil_Head(void) {
    boss_move.ysp = ((int16_t)boss_move.timer - (int16_t)boss_move.y.f.u >= 0) ? 0x80 : (int16_t)0xFF80;
}

static void Oil_SpitBall(Sst *self) { // Offset_0x02E78C
    Sst *ball = Sst_Allocate();
    if (ball == NULL)
        return;
    ball->code = sst_object_codes[ObjId_OOZBoss];
    ball->routine = Boss_Ball;
    ((Own_OOZBall *)ball->own.u8)->parent = (uint8_t)(self - sst);
    QueueSound2(sfx_LaserBurst);
    Own_OOZBoss *own = (Own_OOZBoss *)self->own.u8;
    own->boss.anim_routine = Oil_Pick;
    boss_move.timer = 0x28;
    boss_move.ysp = (int16_t)0xFF80;
}

static void Oil_Run(Sst *self, Own_OOZBoss *own) {
    Own_AlphaBoss *boss = &own->boss;
    // the heights it hovers at (Offset_0x02E766)
    static const int16_t heights[4] = { 0x238, 0x230, 0x240, 0x25F };

    switch (boss->anim_routine) {
    case Oil_Start: // Offset_0x02E66C: it comes up from low in the oil (facing the character) with its tail
        dry_palette[1][1] = 0;
        boss_move.x.f.u = 0x2940;
        Face(self, player->x_pos.f.u >= 0x293A);
        boss_move.y.f.u = 0x2B0;
        self->y_pos.f.u = 0x2B0;
        boss->anim_routine = Oil_Rise;
        self->collision_flags = 0x8A;
        self->mapping_frame = 5;
        for (int k = 0; k < Tail_Links; k++) {
            self->children[k].frame = 7;
            self->children[k].y = (int16_t)(0x2B0 + 0x0F * (k + 1));
        }
        self->child_count = Tail_Links;
        boss_move.ysp = (int16_t)0xFF80;
        boss->count = 0;
        return;

    case Oil_Rise: // Offset_0x02E6E0
        Boss_Move();
        if (boss_move.y.f.u < 0x240) {
            boss_move.y.f.u = 0x240;
            boss_move.ysp = 0;
            boss->anim_routine += 2;
            boss_move.timer = 0x80;
            boss->flag = 3;
        }
        break;

    case Oil_Pick: // Offset_0x02E710: it waits, and picks a height it has not been to (three of the four), or sinks
        if (--boss->side == 0)
            self->mapping_frame = 5;
        if (--boss_move.timer != 0)
            break;
        if ((int8_t)--boss->flag >= 0) {
            uint8_t pick = (uint8_t)RandomNumber();
            do {
                pick = (uint8_t)((pick + 1) & 3);
            } while (boss->count & (1 << pick));
            boss->count |= (uint8_t)(1 << pick);
            boss_move.timer = heights[pick];
            boss->anim_routine += 2;
            Oil_Head();
        } else {
            boss_move.ysp = 0x80;
            boss->anim_routine = Oil_Return;
        }
        break;

    case Oil_Move: { // Offset_0x02E76E: to that height, where it spits a ball
        Boss_Move();
        const bool down = boss_move.ysp >= 0;
        const bool arrived = down ? (uint16_t)boss_move.timer < (uint16_t)boss_move.y.f.u : (uint16_t)boss_move.timer >= (uint16_t)boss_move.y.f.u;
        if (!arrived)
            break;
        boss_move.ysp = 0;
        boss->side = 8;
        self->mapping_frame = 6;
        Oil_SpitBall(self);
        break;
    }

    case Oil_Return: // Offset_0x02E7D2: down again, and the dive starts over
        if (--boss->side == 0)
            self->mapping_frame = 5;
        Boss_Move();
        if (boss_move.y.f.u >= 0x2B0) {
            boss_move.y.f.u = 0x2B0;
            boss_move.ysp = 0;
            boss->anim_routine = 0;
            self->routine = Boss_Dive;
            return;
        }
        break;
    }
    Oil_Finish(self, own);
}

// Offset_0x02E970: the place on the circle of radius $68 about the middle, for an angle; the swing goes the other way when it looks the other way
static void Orbit_Point(const Sst *self, uint8_t angle, int16_t *x, int16_t *y) {
    int16_t sin, cos;
    CalcSine(angle, &sin, &cos);
    int16_t dx = (int16_t)((cos * 0x68) >> 8);
    if (!(self->render_flags & SST_RENDER_X_FLIP))
        dx = (int16_t)-dx;
    *x = (int16_t)(dx + boss_move.x.f.u);
    *y = (int16_t)(((sin * 0x68) >> 8) + boss_move.y.f.u);
}

// Offset_0x02E996: the face by where it is on the circle
static uint8_t Orbit_Face(uint8_t angle) {
    if (angle < 0x52)
        return 0x15;
    if (angle < 0x6B)
        return 3;
    if (angle < 0x92)
        return 2;
    return 4;
}

static void Orbit_Run(Sst *self, Own_OOZBoss *own) {
    Own_AlphaBoss *boss = &own->boss;

    switch (boss->anim_routine) {
    case Orbit_Start: // Offset_0x02E8B2: it starts at the side the character is not on
        dry_palette[1][1] = 0;
        boss_move.x.f.u = 0x28C0;
        Face(self, false);
        if (player->x_pos.f.u >= 0x293A) {
            boss_move.x.f.u = 0x29C0;
            Face(self, true);
        }
        boss_move.y.f.u = 0x2A0;
        self->mapping_frame = 2;
        self->collision_flags = 0x8A;
        boss->anim_routine += 2;
        boss->angle = 0;
        for (int k = 0; k < Tail_Links; k++)
            self->children[k].frame = 7;
        self->child_count = Tail_Links;
        return;

    case Orbit_Swing: { // Offset_0x02E916: round the circle once, its tail behind it ($FE steps of an angle)
        uint8_t angle = (uint8_t)(boss->angle + 0x40);
        Orbit_Point(self, angle, &self->x_pos.f.u, &self->y_pos.f.u);
        boss->angle++;
        for (int k = 0; k < Tail_Links; k++) {
            angle = (uint8_t)(angle - 6);
            Orbit_Point(self, angle, &self->children[k].x, &self->children[k].y);
        }
        if (boss->angle >= 0xFE) {
            boss->anim_routine = 0;
            self->routine = Boss_Oil;
            return;
        }
        self->mapping_frame = Orbit_Face(boss->angle);
        Sst_Draw(self);
        break;
    }
    }
}

// What a ball of oil (routine 8) does: it flies along the arena at the height of the boss's mouth, and where it passes the middle (low enough) it lights a row of flames; a flame burns a while
// (lighting the next flame outwards, then the next, and so on) and goes out
static void Ball_LightFlames(Sst *self, int16_t x) { // Offset_0x02EA8A
    Sst *flame = Sst_Allocate();
    if (flame == NULL)
        return;
    flame->code = sst_object_codes[ObjId_OOZBoss];
    flame->routine = Boss_Ball;
    flame->routine_secondary = Ball_Flame;
    flame->collision_flags = 0x8B;
    flame->anim = 2;
    flame->mapping_frame = 0x0D;
    flame->y_vel = 0;
    flame->mappings = Mappings_OOZBoss;
    flame->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_OOZBoss);
    flame->render_flags |= SST_RENDER_LEVEL;
    flame->priority = 2 * 0x80;
    Own_OOZBall *own = (Own_OOZBall *)flame->own.u8;
    own->wait = 5;
    own->left = 7;
    flame->x_vel = self->x_vel;
    flame->x_pos.f.u = x;
    flame->y_pos.f.u = 0x250;
    QueueSound2(sfx_Scatter);
}

static void Ball_Fly_Check(Sst *self) { // Offset_0x02EA48
    if ((uint16_t)self->y_pos.f.u < 0x250)
        return;
    const uint16_t x = (uint16_t)self->x_pos.f.u;
    if (self->x_vel >= 0) {
        if (x < 0x2980 && x >= 0x297C)
            Ball_LightFlames(self, 0x2988);
    } else if (x >= 0x2900 && x < 0x2904) {
        Ball_LightFlames(self, 0x28F8);
    }
}

static void Ball_Run(Sst *self, Own_OOZBall *ball) {
    switch (self->routine_secondary) {
    case Ball_Make: { // Offset_0x02E9D0: at the boss's mouth, going the way it looks
        const Sst *boss = &sst[ball->parent];
        self->routine_secondary += 2;
        self->mappings = Mappings_OOZBoss;
        self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_OOZBoss);
        self->render_flags |= SST_RENDER_LEVEL;
        self->priority = 4 * 0x80;
        self->x_pos.f.u = boss->x_pos.f.u;
        self->y_pos.f.u = boss->y_pos.f.u;
        self->mapping_frame = 0x0C;
        int16_t offset = -0x20;
        self->x_vel = (int16_t)0xFC00;
        if (boss->render_flags & SST_RENDER_X_FLIP) {
            offset = -offset;
            self->x_vel = (int16_t)-self->x_vel;
        }
        self->x_pos.f.u += offset;
        self->collision_flags = 0xAB;
        break;
    }

    case Ball_Fly: // Offset_0x02EA28
        Ball_Fly_Check(self);
        Sst_MoveSprite2(self);
        if ((uint16_t)self->x_pos.f.u < 0x2870 || (uint16_t)self->x_pos.f.u >= 0x2A10) {
            Sst_Delete(self);
            return;
        }
        Sst_Draw(self);
        break;

    case Ball_Flame: // Offset_0x02EAFC
        if (--ball->wait < 0) {
            ball->wait = 0xC7;
            if ((int8_t)--ball->left >= 0) {
                Sst *next = Sst_AllocateAfter(self);
                if (next != NULL) {
                    *next = *self;
                    ((Own_OOZBall *)next->own.u8)->wait = 5;
                    next->anim = 2;
                    next->prev_anim = 0;
                    next->x_pos.f.u += (next->x_vel < 0) ? -0x10 : 0x10;
                    QueueSound2(sfx_Scatter);
                }
            }
        }
        Sst_Animate(self, Animation_OOZBoss);
        SstOnScreenTest(self);
        break;

    case Ball_Gone: // Offset_0x02EB58
        Sst_Delete(self);
        break;
    }
}

void Obj_OOZBoss(Sst *self) {
    Own_OOZBoss *own = (Own_OOZBoss *)self->own.u8;

    switch (self->routine) {
    case Boss_Start: // Offset_0x02E408
        self->mappings = Mappings_OOZBoss;
        self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_OOZBoss);
        self->render_flags |= SST_RENDER_LEVEL | SST_RENDER_MULTI_DRAW;
        self->priority = 3 * 0x80;
        self->width_pixels = 0x60;
        self->height_pixels = 0x40;
        self->child_count = 0;
        self->routine = Boss_Dive;
        self->collision_flags = 0x0F;
        self->collision_property = 8;
        break;

    case Boss_Dive:
        Dive_Run(self, own);
        break;

    case Boss_Oil:
        Oil_Run(self, own);
        break;

    case Boss_Orbit:
        Orbit_Run(self, own);
        break;

    case Boss_Ball:
        Ball_Run(self, (Own_OOZBall *)self->own.u8);
        break;
    }
}
