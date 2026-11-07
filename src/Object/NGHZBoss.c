// Neo Green Hill's boss for Sonic 2, as the pre-beta has it (object 89, Obj_0x89_NGHz_Boss): see NGHZBoss.h. It uses the boss helpers of BossCommon (the ship's place and speed in a buffer, the animation buffer of its
// three parts). The object comes in three kinds by its routine: 0 waits for the characters to come into the arena and then becomes the ship (2), which makes two launchers (4, one for each wall); a launcher makes
// the marker of an arrow (4 with the second routine 8) and the pillar that follows it (second routine 6).
#include "Object/NGHZBoss.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"
#include "Object/BossCommon.h"
#include "Object/CharControl.h"
#include "Constants.h"

#include "EngineObject.h"
#include "EnginePalette.h"
#include "HTZQuake.h"
#include "Level.h"
#include "LevelCollision.h"
#include "MathUtil.h"
#include "Game.h"
#include "Object/Sonic.h"
#include "Object/Tails.h"
#include "Solid.h"
#include "Sound.h"

#include <string.h>

#include "Macros.h"

#include "Resource/Animation/NGHZBoss.h"
#include "Resource/Animation/NGHZBossPillars.h"
#include "Resource/Mappings/NGHZBoss.h"
#include "Resource/Mappings/NGHZBossLauncher.h"

#define FOR_EACH_CHARACTER(chr, who) \
    for (int who = SolidChar_Sonic; who <= SolidChar_Tails; who++) \
        for (Sst *chr = (who == SolidChar_Sonic) ? player : (TAILS_OBJ->code != NULL ? TAILS_OBJ : NULL); chr != NULL; chr = NULL)

#define ArtTile_NGHZBoss 0x3E0 // ($7C00: list $2B)

// The pre-beta's effects by their ids (the sound ids lay out one effect after the other from $A0: see SoundIds.h): the hammer $BD, a pillar arriving $DD, a launcher rising $E1
#define SFX(id) ((enum SoundID)(sfx_Jump + ((id) - 0xA0)))

enum { Boss_Wait = 0, Boss_Ship = 2, Boss_Parts = 4 };
// the ship's states (Obj_Ani_Boss_Routine)
enum { Ship_Descend = 0, Ship_Cross = 2, Ship_Hammer = 4, Ship_Turn = 6, Ship_Wreck = 8, Ship_Fall = 0xA, Ship_Leave = 0xC };
// the second routine of what the parts are: a launcher rises (0), waits for the ship (2) and sinks (4); a pillar is 6; the marker of an arrow is 8
enum { Launcher_Rise = 0, Launcher_Wait = 2, Launcher_Sink = 4, Pillar = 6, Marker = 8 };
enum { Part_Robotnik = 0, Part_Hammer = 1, Part_Boost = 2 };
enum { Pillar_Make = 0, Pillar_Fly = 2, Pillar_Shake = 4, Pillar_Fall = 6, Pillar_Gone = 8 };

typedef struct {
    Own_AlphaBoss boss; // flag: going left ($FF; var_0C), side: the hammer has let go ($FF; var_00), count: bit 0 an arrow is to be thrown (var_12), angle: the bob (Obj_Map_Id), flash: how long its flash lasts (Obj_Inertia)
    int16_t hammer_speed; // var_02: how fast the hammer falls when the wreck lets go of it
    int32_t hammer_y;     // var_0E
} Own_NGHZBoss;

// What a launcher, a marker and a pillar keep (the offsets are the pre-beta's)
typedef struct {
    uint8_t pad0[2];  // 0x28-0x29
    uint8_t pstate;   // 0x2A: a pillar's state (Obj_Boss_Routine_2: the code $F9 of its animation adds 2 to it)
    uint8_t ship;     // 0x2B: the slot of the ship (a launcher's, a pillar's)
    uint8_t pad1[4];  // 0x2C-0x2F
    int16_t var04;    // 0x30: a launcher's shake, a marker's time, a pillar's time on a platform
    uint8_t pad2[2];  // 0x32-0x33
    uint8_t marker;   // 0x34: (a pillar's) the slot of its marker
    uint8_t pad3[3];  // 0x35-0x37
    uint8_t active;   // 0x38: (a launcher's) it shakes after an arrow
} Own_Part;

static uint8_t hammer_down; // $F73F: the hammer is down (its box hurts)
static uint8_t dle_counter; // $F73A: how long the arena has been shut

// Where the arrows come from (their heights)
static const int16_t arrow_targets[4] = { 0x458, 0x478, 0x498, 0x4B8 };

// DynResize_NGHz of act 2: the camera comes to the arena (X $2800): the left edge and the lower limit are fixed and the boss's list loaded; at X $2A40 the arena is shut, the music fades and the boss is made; its
// camera's top is held once it has gone to Y $3F8, and $5A frames later the boss's music starts; when it is beaten, the results card
void NGHZBoss_Events(void) {
    switch (dle_routine) {
    case 0:
        if (scrpos_x.f.u < (0x2800 - SCREEN_WIDEADD2))
            break;
        limit_left2 = 0x2800 - SCREEN_WIDEADD2;
        limit_btm1 = 0x400 - SCREEN_TALLADD;
        dle_routine += 2;
        boss_flag = 4;
        AddPLC(PlcId_NGHZBoss);
        break;
    case 2:
        if (scrpos_x.f.u < (0x2A40 - SCREEN_WIDEADD2))
            break;
        limit_right1 = limit_right2 = 0x2A40 + SCREEN_WIDEADD2;
        limit_left2 = 0x2A40 - SCREEN_WIDEADD2;
        dle_routine += 2;
        QueueSound1(bgm_Fade);
        dle_counter = 0;
        {
            Sst *boss = Sst_Allocate();
            if (boss != NULL)
                boss->code = sst_object_codes[ObjId_NGHZBoss];
        }
        break;
    case 4:
        if (scrpos_y.f.u >= (0x3F8 - SCREEN_TALLADD))
            limit_top2 = 0x3F8 - SCREEN_TALLADD;
        if (++dle_counter >= 0x5A) {
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

// The hammer's box while it is down (Offset_0x0385C2): $50 in front of the ship, $4 down, $10 across and $14 high each way: it hurts
bool NGHZBoss_HammerBox(const Sst *hit, int16_t *centre_x, int16_t *centre_y, int16_t *half_width, int16_t *half_height) {
    if (hit->code != sst_object_codes[ObjId_NGHZBoss] || hit->routine != Boss_Ship || boss_flag != 4 || !hammer_down)
        return false;
    *centre_x = (int16_t)(hit->x_pos.f.u + (((hit->render_flags & SST_RENDER_X_FLIP) != 0) ? 0x50 : -0x50));
    *centre_y = (int16_t)(hit->y_pos.f.u + 4);
    *half_width = 0x10;
    *half_height = 0x14;
    return true;
}

static void SetPartAnim(int part, uint8_t anim) {
    boss_anim[(part + 1) * 2] = (uint8_t)((boss_anim[(part + 1) * 2] & 0xF0) | anim);
}

static void FillAnimationBuffer(void) { // Offset_0x02BB76
    static const uint8_t buffer[8] = { 0x04, 0x00, 0x00, 0x00, 0x02, 0x00, 0x01, 0x00 };
    memcpy(boss_anim, buffer, sizeof(buffer));
}

// Offset_0x02BD84: where the parts are, from the ship's place; once it has been beaten the hammer falls
static void Ship_Parts(Sst *self, Own_NGHZBoss *own) {
    const int16_t x = self->x_pos.f.u, y = self->y_pos.f.u;
    self->children[Part_Robotnik].x = x;
    self->children[Part_Robotnik].y = y;
    self->children[Part_Boost].x = x;
    self->children[Part_Boost].y = y;
    if (!own->boss.side) {
        self->children[Part_Hammer].x = x;
        self->children[Part_Hammer].y = y;
        own->hammer_y = (int32_t)((uint32_t)(uint16_t)y << 16) | (own->hammer_y & 0xFFFF);
        return;
    }
    if (boss_move.timer > 0x78)
        return;
    self->children[Part_Hammer].x--;
    own->hammer_y += (int32_t)own->hammer_speed << 8;
    own->hammer_speed += 0x38;
    self->children[Part_Hammer].y = (int16_t)(own->hammer_y >> 16);
    if (self->children[Part_Hammer].y >= 0x540)
        own->hammer_speed = 0;
}

// Offset_0x02BD0A: the bob, and the hit (a flash that lasts $40 frames, or the wreck when the hits are out)
static void Ship_Bob(Sst *self, Own_NGHZBoss *own) {
    int16_t sin, cos;
    CalcSine(own->boss.angle, &sin, &cos);
    self->y_pos.f.u = (int16_t)((sin >> 6) + boss_move.y.f.u);
    self->x_pos.f.u = boss_move.x.f.u;
    own->boss.angle = (uint8_t)(own->boss.angle + 2);
    if (own->boss.anim_routine >= Ship_Wreck)
        return;
    if (self->collision_property == 0) {
        AddPoints(100);
        boss_move.timer = 0xB3;
        own->boss.anim_routine = Ship_Wreck;
        return;
    }
    if (self->collision_flags != 0)
        return;
    if (own->boss.flash == 0) {
        own->boss.flash = 0x40;
        QueueSound2(sfx_HitBoss);
    }
    dry_palette[1][1] = (dry_palette[1][1] == 0) ? 0x0EEE : 0;
    if (--own->boss.flash == 0)
        self->collision_flags = 0xF;
}

// Offset_0x02BCD8: the bob and hit, and Robotnik's face: it laughs when a character is hurt and winces for the first frame of a flash
static void Ship_Common(Sst *self, Own_NGHZBoss *own) {
    Ship_Bob(self, own);
    if (player->routine == 4 || TAILS_OBJ->routine == 4)
        boss_anim[3] = 0x31;
    if (own->boss.flash == 0x3F)
        boss_anim[3] = 0xC0;
}

static void Ship_Show(Sst *self, Own_NGHZBoss *own) {
    Boss_AnimateSprite(self, &own->boss, Animation_NGHZBoss);
    Sst_Draw(self);
}

// One step of a state that moves, bobs and shows the ship
static void Ship_Step(Sst *self, Own_NGHZBoss *own) {
    Boss_Move();
    Ship_Common(self, own);
    Ship_Parts(self, own);
}

static void Ship_Run(Sst *self, Own_NGHZBoss *own) {
    Own_AlphaBoss *boss = &own->boss;

    switch (boss->anim_routine) {
    case Ship_Descend:
        Ship_Step(self, own);
        if (boss_move.y.f.u >= 0x430) {
            boss_move.y.f.u = 0x430;
            boss->anim_routine += 2;
            boss_move.ysp = 0;
            boss_move.xsp = (int16_t)0xFF38;
            boss->flag = 0xFF;
        }
        Ship_Show(self, own);
        break;

    case Ship_Cross: // along the arena, until its end
        Ship_Step(self, own);
        if (boss->flag == 0 ? (boss_move.x.f.u >= 0x2B10) : (boss_move.x.f.u <= 0x2AB0)) {
            boss->anim_routine += 2;
            boss_move.xsp = 0;
        }
        Ship_Show(self, own);
        break;

    case Ship_Hammer: // the hammer swings (its animation comes with the lowest point of the bob)
        Ship_Step(self, own);
        if (boss->angle == 0xC0) {
            boss_anim[4] = (uint8_t)((boss_anim[4] & 0xF0) | 3);
            boss->anim_routine += 2;
            boss->flag = (self->render_flags & SST_RENDER_X_FLIP) ? 0xFF : 0;
            boss_move.timer = 0x1E;
            QueueSound2(SFX(0xBD));
        }
        Ship_Show(self, own);
        break;

    case Ship_Turn: // the hammer is down for a moment, an arrow is thrown, and it turns
        if (boss_move.timer == 0x14) {
            boss->count |= 1;
            hammer_down = 1;
        }
        if (--boss_move.timer < 0) {
            hammer_down = 0;
            boss->anim_routine = Ship_Cross;
            self->render_flags ^= SST_RENDER_X_FLIP;
            if (self->render_flags & SST_RENDER_X_FLIP)
                boss_move.xsp = 0x00C8;
            else
                boss_move.xsp = (int16_t)0xFF38;
        }
        Ship_Step(self, own);
        Ship_Show(self, own);
        break;

    case Ship_Wreck:
        boss->side = 0xFF;
        if (--boss_move.timer >= 0) {
            Boss_Defeated(self);
            self->children[Part_Robotnik].frame = 5;
        } else {
            boss_anim[4] = 1;
            boss_anim[5] = 0;
            self->render_flags |= SST_RENDER_X_FLIP;
            boss_move.xsp = 0;
            boss_move.ysp = 0;
            boss->anim_routine += 2;
            boss_move.timer = -0x12;
        }
        self->y_pos.f.u = boss_move.y.f.u;
        self->x_pos.f.u = boss_move.x.f.u;
        Boss_AnimateSprite(self, boss, Animation_NGHZBoss);
        Ship_Parts(self, own);
        Sst_Draw(self);
        break;

    case Ship_Fall:
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
        Ship_Bob(self, own);
        self->y_pos.f.u = boss_move.y.f.u;
        self->x_pos.f.u = boss_move.x.f.u;
        Boss_AnimateSprite(self, boss, Animation_NGHZBoss);
        Ship_Parts(self, own);
        Sst_Draw(self);
        break;

    case Ship_Leave: // up and away to the right while the arena's right edge opens to $2D00; offscreen, the results card
        boss_move.xsp = 0x400;
        boss_move.ysp = (int16_t)0xFFC0;
        if (limit_right2 < 0x2D00 + SCREEN_WIDEADD2) {
            limit_right1 = limit_right2 += 2;
        } else if (!(self->render_flags & SST_RENDER_ON_SCREEN)) {
            if (!boss_status)
                boss_status = 1;
            Sst_Delete(self);
            return;
        }
        Boss_Move();
        Ship_Bob(self, own);
        self->y_pos.f.u = boss_move.y.f.u;
        self->x_pos.f.u = boss_move.x.f.u;
        Boss_AnimateSprite(self, boss, Animation_NGHZBoss);
        Ship_Parts(self, own);
        Sst_Draw(self);
        break;
    }
}

// Offset_0x02C0D8: a launcher is solid ($23 each way across, $44 up and $45 down; four down)
static void Launcher_Solid(Sst *self) {
    const int16_t saved_y = self->y_pos.f.u;
    self->y_pos.f.u += 4;
    const int16_t x = self->x_pos.f.u;
    FOR_EACH_CHARACTER(chr, who)
        Solid_Character(self, chr, who, 0x23, 0x44, 0x45, x, NULL);
    self->y_pos.f.u = saved_y;
}

// Offset_0x02C012: the marker of an arrow on the side the ship has hit, at one of four heights, and the pillar that follows
static void Launcher_Throw(Sst *self, const Sst *ship) {
    const Own_NGHZBoss *ship_scratch = (const Own_NGHZBoss *)ship->own.u8;
    const bool right = ship_scratch->boss.flag != 0;
    Sst *marker = Sst_Allocate();
    if (marker == NULL)
        return;
    marker->code = sst_object_codes[ObjId_NGHZBoss];
    marker->routine = Boss_Parts;
    marker->routine_secondary = Marker;
    marker->mappings = Mappings_NGHZBossLauncher;
    marker->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_NGHZBoss);
    marker->render_flags |= SST_RENDER_LEVEL;
    marker->mapping_frame = 2;
    marker->x_pos.f.u = 0x2A6A;
    if (right) {
        marker->x_pos.f.u = 0x2B56;
        marker->render_flags |= SST_RENDER_X_FLIP;
    }
    ((Own_Part *)marker->own.u8)->var04 = 0x28;
    marker->y_pos.f.u = arrow_targets[RandomNumber() & 3];
    Sst *pillar = Sst_Allocate();
    if (pillar == NULL)
        return;
    pillar->code = sst_object_codes[ObjId_NGHZBoss];
    pillar->routine = Boss_Parts;
    pillar->routine_secondary = Pillar;
    Own_Part *ps = (Own_Part *)pillar->own.u8;
    ps->marker = (uint8_t)(marker - sst);
    ps->active = right ? 0xFF : 0; // (the subtype)
    ps->ship = (uint8_t)(ship - sst);
    (void)self;
}

// Offset_0x02BFA0: a launcher shakes for a while after an arrow
static void Launcher_Shake(Sst *self, Own_Part *own, const Sst *ship) {
    if (!own->active)
        return;
    const bool right = ((const Own_NGHZBoss *)ship->own.u8)->boss.flag != 0;
    if (own->var04 <= 0)
        own->var04 = 0x1F;
    if (--own->var04 <= 0) {
        own->active = 0;
        own->var04 = 0;
        self->x_pos.f.u = right ? 0x2B70 : 0x2A50;
        self->y_pos.f.u = 0x488;
        return;
    }
    const int16_t offset = (frame_count & 1) ? -1 : 1;
    self->x_pos.f.u = (int16_t)((right ? 0x2B70 : 0x2A50) + offset);
    self->y_pos.f.u = (int16_t)(0x488 + offset);
}

static void Launcher_Run(Sst *self, Own_Part *own) {
    Sst *ship = &sst[own->ship];
    switch (self->routine_secondary) {
    case Launcher_Rise:
        Launcher_Solid(self);
        if (!(frame_count & 0x1F))
            QueueSound2(SFX(0xE1));
        self->y_pos.f.u--;
        if (self->y_pos.f.u <= 0x488) {
            self->routine_secondary += 2;
            dhz_shake = 0;
        }
        Sst_Draw(self);
        break;

    case Launcher_Wait: {
        Launcher_Solid(self);
        Own_NGHZBoss *ship_scratch = (Own_NGHZBoss *)ship->own.u8;
        if (ship_scratch->boss.count & 1) {
            const bool flipped = (self->render_flags & SST_RENDER_X_FLIP) != 0;
            if (ship_scratch->boss.flag != 0 ? flipped : !flipped) {
                ship_scratch->boss.count &= (uint8_t)~1;
                Launcher_Throw(self, ship);
                own->active = 0xFF;
            }
        }
        Launcher_Shake(self, own, ship);
        Sst_Draw(self);
        break;
    }

    case Launcher_Sink:
        dhz_shake = 1;
        self->y_pos.f.u++;
        if (self->y_pos.f.u >= 0x510) {
            dhz_shake = 0;
            Sst_Delete(self);
            return;
        }
        Sst_Draw(self);
        break;
    }
}

// The pillar (Offset_0x02C0FC): it flies across to the other wall, becomes a platform that shakes and falls
static void Pillar_Release(Sst *self) { // Offset_0x02C262: a character on the pillar is let go
    if (self->status & 0x08) {
        self->status &= (uint8_t)~0x08;
        player->status |= SST_PSTATUS_IN_AIR;
        player->status &= (uint8_t)~SST_PSTATUS_OBJECT_STAND;
    }
    if (self->status & 0x10) {
        self->status &= (uint8_t)~0x10;
        TAILS_OBJ->status |= SST_PSTATUS_IN_AIR;
        TAILS_OBJ->status &= (uint8_t)~SST_PSTATUS_OBJECT_STAND;
    }
}

static void Pillar_Run(Sst *self, Own_Part *own) {
    const Sst *ship = &sst[own->ship];
    if (((const Own_NGHZBoss *)ship->own.u8)->boss.anim_routine >= Ship_Wreck && own->pstate < Pillar_Fall)
        own->pstate = Pillar_Fall;

    switch (own->pstate) {
    case Pillar_Make: {
        const Sst *marker = &sst[own->marker];
        self->mappings = Mappings_NGHZBossLauncher;
        self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_NGHZBoss);
        self->render_flags |= SST_RENDER_LEVEL;
        self->y_radius = (int8_t)0x90;
        self->priority = 4 * 0x80;
        own->pstate += 2;
        self->x_pos.f.u = marker->x_pos.f.u;
        self->y_pos.f.u = marker->y_pos.f.u;
        self->y_vel = 4; // (how fast it falls later)
        self->mapping_frame = 4;
        self->y_pos.f.u += 9;
        if (own->active) {
            self->status |= SST_STATUS_X_FLIP;
            self->render_flags |= SST_RENDER_X_FLIP;
            self->x_vel = -3;
        } else {
            self->x_vel = 3;
        }
        self->collision_flags = 0xAC;
        break;
    }

    case Pillar_Fly: {
        if (self->status & 0x80)
            own->pstate = Pillar_Gone;
        int16_t x = (int16_t)(self->x_pos.f.u + self->x_vel);
        bool arrived = false;
        if (self->x_vel < 0) {
            if (x <= 0x2A77) {
                x = 0x2A77;
                arrived = true;
            }
        } else if (x >= 0x2B49) {
            x = 0x2B49;
            arrived = true;
        }
        self->x_pos.f.u = x;
        if (arrived) {
            own->pstate += 2;
            QueueSound2(SFX(0xDD));
        }
        Sst_Draw(self);
        break;
    }

    case Pillar_Shake: {
        self->collision_flags = 0;
        if (self->status & 0x80)
            own->pstate += 2;
        if (own->var04 == 0) {
            const int16_t x = self->x_pos.f.u;
            FOR_EACH_CHARACTER(chr, who)
                Solid_Platform(self, chr, who, 0x1B, 2, x);
            if (self->status & (0x08 | 0x10))
                own->var04 = 0x1F;
        }
        if (own->var04 != 0 && --own->var04 == 0)
            own->pstate = Pillar_Fall;
        Sst_Animate(self, Animation_NGHZBossPillars);
        Sst_Draw(self);
        break;
    }

    case Pillar_Fall: {
        Pillar_Release(self);
        const int16_t y = (int16_t)(self->y_pos.f.u + self->y_vel);
        if (y > 0x4F0) {
            Sst_Delete(self);
            return;
        }
        self->y_pos.f.u = y;
        Sst_Draw(self);
        break;
    }

    default:
        Sst_Delete(self);
        break;
    }
}

static void Marker_Run(Sst *self, Own_Part *own) {
    if (--own->var04 == 0) {
        Sst_Delete(self);
        return;
    }
    Sst_Draw(self);
}

static void Boss_Start(Sst *self, Own_NGHZBoss *own) {
    dhz_shake = 1;
    self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_NGHZBoss);
    self->mappings = Mappings_NGHZBoss;
    self->render_flags |= SST_RENDER_LEVEL | SST_RENDER_MULTI_DRAW;
    self->y_radius = 0x20;
    self->width_pixels = 0x90;
    self->priority = 2 * 0x80;
    self->routine = Boss_Ship;
    self->x_pos.f.u = 0x2AE0;
    self->y_pos.f.u = 0x388;
    boss_move.x.f.u = 0x2AE0;
    boss_move.y.f.u = 0x388;
    self->child_count = 3;
    self->collision_flags = 0x0F;
    self->collision_property = 8;
    self->mapping_frame = 8; // (the main sprite)
    own->hammer_speed = (int16_t)0xFC80;
    hammer_down = 0;
    for (int i = 0; i < 3; i++) {
        self->children[i].x = 0x2AE0;
        self->children[i].y = 0x488;
    }
    self->children[Part_Robotnik].frame = 0;
    self->children[Part_Hammer].frame = 9;
    self->children[Part_Boost].frame = 6;
    own->hammer_y = (int32_t)(0x488u << 16);
    boss_move.ysp = 0x100;
    boss_move.xsp = 0;
    boss_move.timer = 0;

    // The two launchers: the left one at ($2A50,$510), the right a copy of it turned round at $2B70
    for (int side = 0; side < 2; side++) {
        Sst *launcher = Sst_Allocate();
        if (launcher == NULL)
            break;
        launcher->code = sst_object_codes[ObjId_NGHZBoss];
        launcher->routine = Boss_Parts;
        launcher->routine_secondary = Launcher_Rise;
        launcher->mappings = Mappings_NGHZBossLauncher;
        launcher->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_NGHZBoss);
        launcher->render_flags |= SST_RENDER_LEVEL;
        launcher->width_pixels = 0x10;
        launcher->y_radius = 0x20;
        launcher->priority = 2 * 0x80;
        launcher->mapping_frame = 0;
        launcher->x_pos.f.u = side == 0 ? 0x2A50 : 0x2B70;
        launcher->y_pos.f.u = 0x510;
        if (side)
            launcher->render_flags |= SST_RENDER_X_FLIP;
        ((Own_Part *)launcher->own.u8)->ship = (uint8_t)(self - sst);
    }
}

// The wait: the boss starts when both characters are in the arena (Sonic between $2A60 and $2B60 and Tails too, or held by an object ($81))
static bool CharactersInArena(void) {
    if ((uint16_t)(player->x_pos.f.u - 0x2A60) > 0x100)
        return false;
    if (TAILS_OBJ->code == NULL || OBJ_CONTROL(TAILS_OBJ) == 0x81)
        return true;
    return (uint16_t)(TAILS_OBJ->x_pos.f.u - 0x2A60) <= 0x100;
}

void Obj_NGHZBoss(Sst *self) {
    switch (self->routine) {
    case Boss_Wait:
        if (CharactersInArena())
            Boss_Start(self, (Own_NGHZBoss *)self->own.u8);
        FillAnimationBuffer();
        break;

    case Boss_Ship:
        Ship_Run(self, (Own_NGHZBoss *)self->own.u8);
        break;

    case Boss_Parts: {
        Own_Part *own = (Own_Part *)self->own.u8;
        switch (self->routine_secondary) {
        case Launcher_Rise: case Launcher_Wait: case Launcher_Sink:
            if (((Own_NGHZBoss *)sst[own->ship].own.u8)->boss.anim_routine >= Ship_Wreck)
                self->routine_secondary = Launcher_Sink;
            Launcher_Run(self, own);
            break;
        case Pillar:
            Pillar_Run(self, own);
            break;
        case Marker:
            Marker_Run(self, own);
            break;
        }
        break;
    }
    }
}

void NGHZBoss_Reset(void) {
    hammer_down = 0;
    dle_counter = 0;
}
