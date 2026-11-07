// Death Egg's Metal Sonic for Sonic 2, as the pre-beta has it (object AF, Obj_0xAF): see DEZMetalSonic.h. The object comes in kinds by its routine: 2 to 0xA are Metal Sonic (waiting for the camera, falling, standing,
// attacking, blowing up); 0xC to 0x10 the flame behind him; 0x12 to 0x16 the box on his shoulder; 0x18 to 0x20 Robotnik's window. His attacks follow the pre-beta's table of steps (Obj_Routine_2).
#include "Object/DEZMetalSonic.h"
#include "Object/Coconuts.h"
#include "Object/BossCommon.h"
#include "Object/BossArt.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"
#include "Constants.h"

#include "EnginePalette.h"
#include "Level.h"
#include "LevelCollision.h"
#include "Sound.h"

#include "Macros.h"

#include "Resource/Animation/DEZMetalSonic.h"
#include "Resource/Animation/DEZMetalSonicFlame.h"
#include "Resource/Animation/DEZRobotnikWindow.h"
#include "Resource/Mappings/DEZMetalSonic.h"
#include "Resource/Mappings/DEZRobotnikWindow.h"

enum {
    Routine_Init = 0, Routine_Wait = 2, Routine_Fall = 4, Routine_Stand = 6, Routine_Attack = 8, Routine_Dying = 0xA,
    Routine_FlameInit = 0xC, Routine_Flame = 0xE, Routine_FlameOff = 0x10,
    Routine_BoxInit = 0x12, Routine_BoxOff = 0x14, Routine_BoxOn = 0x16,
    Routine_WindowInit = 0x18, Routine_WindowWait = 0x1A, Routine_WindowOpen = 0x1C, Routine_WindowLook = 0x1E, Routine_WindowClose = 0x20
};
// the status bits of Metal Sonic: 1 he has landed (the window opens), 2 he is beaten
enum { Status_Landed = 2, Status_Beaten = 4 };

typedef struct {
    uint8_t timer;     // 0x2A
    uint8_t attacks;   // 0x2C (var_00): dashes left in this attack
    uint8_t alternate; // 0x2D (var_01): which way the next dash goes
    uint8_t fired;     // 0x2E (var_02): the spikes have been let out in this leap
    uint8_t sequence;  // 0x2F (var_03): where he is in the order of attacks
    uint8_t flash;     // 0x30 (var_04)
    int16_t death;     // 0x32 (var_06): how long he blows up
    uint8_t window;    // 0x3A: the slots of the parts
    uint8_t box;       // 0x3C
    uint8_t flame;     // 0x3E
} Own_MetalSonic;

typedef struct {
    uint8_t master; // 0x2C (var_00)
} Own_MetalSonicPart;

// The order of attacks (Offset_0x033C40): the step he starts at
static const uint8_t attack_start[16] = { 0x00, 0x06, 0x10, 0x06, 0x00, 0x06, 0x10, 0x06, 0x00, 0x06, 0x10, 0x06, 0x00, 0x06, 0x10, 0x1E };

// His spikes (Offset_0x034100): from his centre, how fast, which frame, flipped how
static const int8_t spikes[8][6] = {
    { 0x00, (int8_t)0xE8, 0x00, (int8_t)0xFD, 0x0F, 0 }, { (int8_t)0xF0, (int8_t)0xF0, (int8_t)0xFE, (int8_t)0xFE, 0x10, 0 },
    { (int8_t)0xE8, 0x00, (int8_t)0xFD, 0x00, 0x11, 0 }, { (int8_t)0xF0, 0x10, (int8_t)0xFE, 0x02, 0x12, 0 },
    { 0x00, 0x18, 0x00, 0x03, 0x13, 0 }, { 0x10, 0x10, 0x02, 0x02, 0x14, 0 },
    { 0x18, 0x00, 0x03, 0x00, 0x15, 0 }, { 0x10, (int8_t)0xF0, 0x02, (int8_t)0xFE, 0x16, 0 },
};

static uint8_t Slot(const Sst *obj) {
    return (uint8_t)(obj - sst);
}

// A part by its slot; a part that could not be made (no free slot) is a place where nothing is kept
static Sst *Part(uint8_t slot) {
    static Sst nowhere;
    return (slot != 0 && sst[slot].code == Obj_DEZMetalSonic) ? &sst[slot] : &nowhere;
}

// Animate_Sprite_2 (Offset_0x030CEC): the animation of the pre-beta's object table; it says when a script has come to its end ($FF back to the start, $FE the routine on, $FD the second routine on, $FC the end)
static bool Animate(Sst *self, const uint8_t *table) {
    const uint8_t anim = self->anim;
    if (anim != self->prev_anim) {
        self->prev_anim = anim;
        self->anim_frame = 0;
        self->anim_frame_timer = 0;
    }
    if (--self->anim_frame_timer >= 0)
        return false;
    const uint8_t *script = table + (uint16_t)((table[anim * 2] << 8) | table[anim * 2 + 1]);
    self->anim_frame_timer = (int8_t)script[0];
    const uint8_t cmd = script[1 + self->anim_frame];
    if (cmd < 0x80) {
        self->mapping_frame = cmd;
        self->anim_frame++;
        return false;
    }
    switch (cmd) {
    case 0xFF:
        self->anim_frame = 0;
        self->mapping_frame = script[1];
        self->anim_frame++;
        break;
    case 0xFE:
        self->routine += 2;
        self->anim_frame_timer = 0;
        self->anim_frame++;
        break;
    case 0xFD:
        self->routine_secondary += 2;
        break;
    case 0xFC:
        self->anim_frame_timer = 1;
        break;
    default:
        break;
    }
    return true;
}

static Sst *LoadSub(Sst *self, uint8_t *field, uint8_t routine, uint8_t subtype) { // Load_Sub_Obj, and the routine it is made to start at
    Sst *sub = Sst_AllocateAfter(self);
    if (sub == NULL)
        return NULL;
    *field = Slot(sub);
    sub->code = Obj_DEZMetalSonic;
    sub->subtype = subtype;
    ((Own_MetalSonicPart *)sub->own.u8)->master = Slot(self);
    sub->x_pos.f.u = self->x_pos.f.u;
    sub->y_pos.f.u = self->y_pos.f.u;
    sub->routine = routine;
    return sub;
}

// Object_Settings for what Metal Sonic's parts are made of: his look (subtype $48) or the window's ($94)
static void Look(Sst *self, bool window) {
    if (window) {
        self->mappings = Mappings_DEZRobotnikWindow;
        self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_DEZRobotnikWindow);
        self->priority = 6 * 0x80;
        self->width_pixels = 0x10;
        self->collision_flags = 0;
    } else {
        self->mappings = Mappings_DEZMetalSonic;
        self->art_tile = TILE_MAP(0, 1, 0, 0, ArtTile_DEZMetalSonic);
        self->priority = 4 * 0x80;
        self->width_pixels = 0x10;
        self->collision_flags = 0x1A;
    }
    self->render_flags |= SST_RENDER_LEVEL;
    self->routine += 2;
}

static void Place(Sst *part, const Sst *master, int16_t dx, int16_t dy) { // Offset_0x030C0A
    part->x_pos.f.u = (int16_t)(master->x_pos.f.u + dx);
    part->y_pos.f.u = (int16_t)(master->y_pos.f.u + dy);
}

// Offset_0x03408A and Offset_0x034092: his collision follows his frame: the frames of the attack hurt ($9A), the others can be hit ($1A)
static void Refresh(Sst *self) {
    if (self->collision_flags == 0)
        return;
    self->collision_flags = (self->mapping_frame >= 6 && self->mapping_frame <= 8) ? 0x9A : 0x1A;
}

// Offset_0x03401C: a hit flashes him (the usual sound and flicker); the last one blows him up
static void Hit(Sst *self, Own_MetalSonic *own) {
    if (self->collision_property == 0) {
        AddPoints(100);
        own->death = 0xEF;
        self->routine = Routine_Dying;
        self->collision_flags = 0;
        self->status |= Status_Beaten;
        if (Part(own->box) != Part(0))
            Sst_Delete(Part(own->box));
        if (Part(own->flame) != Part(0))
            Sst_Delete(Part(own->flame));
        return;
    }
    if (self->collision_flags != 0)
        return;
    if (own->flash == 0) {
        own->flash = 0x20;
        QueueSound2(sfx_HitBoss);
    }
    dry_palette[1][1] = (dry_palette[1][1] == 0) ? 0x0EEE : 0;
    if (--own->flash == 0) {
        dry_palette[1][1] = 0;
        self->collision_flags = (self->mapping_frame >= 6 && self->mapping_frame <= 8) ? 0x9A : 0x1A;
    }
}

static void Dash(Sst *self, Own_MetalSonic *own, int16_t speed) { // Offset_0x0340CE: each dash goes the other way
    if (own->alternate == 0)
        speed = (int16_t)-speed;
    own->alternate = (uint8_t)~own->alternate;
    self->x_vel = speed;
}

static void Brake(Sst *self) { // Offset_0x0340E0
    self->x_vel += (self->x_vel < 0) ? 0x20 : -0x20;
}

static void Burst(Sst *self) { // Offset_0x0340F0: eight spikes
    for (int i = 0; i < 8; i++) {
        Sst *spike = Sst_AllocateAfter(self);
        if (spike == NULL)
            return;
        spike->code = sst_object_codes[ObjId_EnemyWeapon];
        spike->subtype = 0x4A;
        ((Own_Weapon *)spike->own.u8)->weapon = Weapon_MetalSonicSpike;
        spike->x_pos.f.u = (int16_t)(self->x_pos.f.u + spikes[i][0]);
        spike->y_pos.f.u = (int16_t)(self->y_pos.f.u + spikes[i][1]);
        spike->x_vel = (int16_t)(spikes[i][2] * 0x100);
        spike->y_vel = (int16_t)(spikes[i][3] * 0x100);
        spike->mapping_frame = (uint8_t)spikes[i][4];
        spike->render_flags = (uint8_t)spikes[i][5];
    }
}

static void Stand(Sst *self) { // Offset_0x033D62: he stands, and the flame goes out and the shoulder hurts
    Own_MetalSonic *own = (Own_MetalSonic *)self->own.u8;
    self->routine = Routine_Stand;
    self->anim = 0;
    own->timer = 0x64;
    self->x_vel = 0;
    (*Part(own->flame)).routine = Routine_FlameOff;
    (*Part(own->box)).routine = Routine_BoxOn;
}

static void TurnAround(Sst *self) { // Offset_0x033DFA: he looks the other way
    self->routine_secondary += 2;
    self->anim = 5;
    self->render_flags ^= SST_RENDER_X_FLIP;
    self->x_vel = 0;
    self->y_vel = 0;
}

static void FlameOn(Own_MetalSonic *own) { // Offset_0x033CBE
    own->timer = 0x20;
    (*Part(own->flame)).routine = Routine_Flame;
    (*Part(own->flame)).anim = 1;
}

// The steps of an attack (the table at Offset_0x033C82: the same step several times)
static void Step(Sst *self, Own_MetalSonic *own) {
    switch (self->routine_secondary / 2) {
    case 0: // Offset_0x033CAE: four dashes
        self->routine_secondary += 2;
        self->mapping_frame = 3;
        own->attacks = 4;
        FlameOn(own);
        break;
    case 1: // Offset_0x033CD6: he waits and sets off
        if ((int8_t)--own->timer < 0) {
            self->routine_secondary += 2;
            own->timer = 0x40;
            self->anim = 1;
            Dash(self, own, 0x800);
            (*Part(own->flame)).anim = 2;
        }
        break;
    case 2: // Offset_0x033D02: he dashes, brakes and turns; then the next dash, or he stands
        if ((int8_t)--own->timer < 0) {
            if (--own->attacks == 0) {
                Stand(self);
            } else {
                self->routine_secondary = 2;
                self->x_vel = 0;
                FlameOn(own);
            }
            break;
        }
        if (own->timer == 0x20) {
            self->anim = 2;
            (*Part(own->flame)).routine = Routine_FlameOff;
        }
        Brake(self);
        Animate(self, Animation_DEZMetalSonic);
        if (self->anim == 2 && self->anim_frame == 2 && self->anim_frame_timer == 3)
            self->render_flags ^= SST_RENDER_X_FLIP;
        break;
    case 3: case 8: case 15: // Offset_0x033D90
        self->routine_secondary += 2;
        self->mapping_frame = 3;
        self->anim = 3;
        break;
    case 4: case 9: case 16: // Offset_0x033DA2
        if (Animate(self, Animation_DEZMetalSonic)) {
            self->routine_secondary += 2;
            own->timer = 0x20;
            self->anim = 4;
        }
        break;
    case 5: // Offset_0x033DC2: he waits, then dashes
        if ((int8_t)--own->timer < 0) {
            self->routine_secondary += 2;
            own->timer = 0x40;
            Dash(self, own, 0x800);
        } else {
            Animate(self, Animation_DEZMetalSonic);
        }
        break;
    case 6: // Offset_0x033DE6: ... and turns round
        if ((int8_t)--own->timer < 0) {
            TurnAround(self);
        } else {
            Brake(self);
            Animate(self, Animation_DEZMetalSonic);
        }
        break;
    case 7: case 14: case 21: // Offset_0x033E14: the turn is done
        if (Animate(self, Animation_DEZMetalSonic))
            Stand(self);
        break;
    case 10: case 17: // Offset_0x033E28: a wait, then a slower dash
        if ((int8_t)--own->timer < 0) {
            self->routine_secondary += 2;
            own->timer = 0x40;
            Dash(self, own, 0x400);
        } else {
            Animate(self, Animation_DEZMetalSonic);
        }
        break;
    case 11: case 18: // Offset_0x033E4C: ... and a leap
        if (--own->timer == 0x3C) {
            self->routine_secondary += 2;
            self->y_vel = (int16_t)0xFA00;
        }
        Animate(self, Animation_DEZMetalSonic);
        break;
    case 12: { // Offset_0x033E72: in the air, down to the floor
        if ((int8_t)--own->timer < 0) {
            TurnAround(self);
            break;
        }
        const int16_t d = SstFloorDist(self, self->x_pos.f.u);
        if (d < 0) {
            self->routine_secondary += 2;
            self->y_pos.f.u += d;
            self->y_vel = 0;
        }
        self->y_vel += 0x38;
        Animate(self, Animation_DEZMetalSonic);
        break;
    }
    case 13: case 20: { // Offset_0x033EA6: on the floor, sliding
        if ((int8_t)--own->timer < 0) {
            TurnAround(self);
            break;
        }
        self->y_pos.f.u += SstFloorDist(self, self->x_pos.f.u);
        Animate(self, Animation_DEZMetalSonic);
        break;
    }
    case 19: { // Offset_0x033EC2: a leap that lets the spikes out at the top
        if ((int8_t)--own->timer < 0) {
            TurnAround(self);
            break;
        }
        if (own->fired == 0 && self->y_vel >= 0) {
            own->fired = 0xFF;
            Burst(self);
        }
        const int16_t d = SstFloorDist(self, self->x_pos.f.u);
        if (d < 0) {
            self->routine_secondary += 2;
            self->y_pos.f.u += d;
            self->y_vel = 0;
        }
        self->y_vel += 0x38;
        Animate(self, Animation_DEZMetalSonic);
        break;
    }
    }
}

static void Box_Place(Sst *self, const Own_MetalSonic *own) { // Offset_0x0340B8: the shoulder box is $C to the side he looks away from and $C up
    int16_t dx = 0x0C;
    if (self->render_flags & SST_RENDER_X_FLIP)
        dx = -dx;
    Sst *box = Part(own->box);
    Place(box, self, dx, -0x0C);
    if (self->render_flags & SST_RENDER_ON_SCREEN)
        box->render_flags |= SST_RENDER_ON_SCREEN; // (it is never drawn, and this port's touch code looks at objects that are on the screen)
}

static void Metal_Run(Sst *self, Own_MetalSonic *own) {
    switch (self->routine) {
    case Routine_Init: { // Offset_0x033B3A
        Look(self, false);
        self->y_radius = 0x1B;
        self->x_radius = 0x10;
        self->collision_flags = 0;
        self->collision_property = 8;
        Sst *flame = LoadSub(self, &own->flame, Routine_FlameInit, 0x48);
        Sst *box = LoadSub(self, &own->box, Routine_BoxInit, 0x48);
        Sst *window = LoadSub(self, &own->window, Routine_WindowInit, 0x94);
        (void)flame; (void)box; (void)window;
        break;
    }

    case Routine_Wait: // Offset_0x033B88: until the camera is at $224, where it stops
        if (scrpos_x.f.u >= 0x224 - SCREEN_WIDEADD2) {
            self->routine += 2;
            self->y_vel = 0x80;
            limit_left2 = 0x224 - SCREEN_WIDEADD2;
            limit_right2 = 0x224 + SCREEN_WIDEADD2;
        }
        Sst_Draw(self);
        break;

    case Routine_Fall: { // Offset_0x033BB0: he drops at half a pixel a frame to the floor, and the window opens
        const int16_t d = SstFloorDist(self, self->x_pos.f.u);
        if (d < 0) {
            self->y_pos.f.u += d;
            self->y_vel = 0;
            self->collision_flags = 0x1A;
            self->status |= Status_Landed;
            Stand(self);
            break;
        }
        Sst_MoveSprite2(self);
        Place(Part(own->flame), self, 0, 0);
        Box_Place(self, own);
        Sst_Draw(self);
        break;
    }

    case Routine_Stand: // Offset_0x033BEC
        Hit(self, own);
        Refresh(self);
        if (--own->timer == 0) {
            self->routine += 2;
            self->routine_secondary = attack_start[own->sequence & 0x0F];
            own->sequence++;
            own->fired = 0;
            (*Part(own->box)).routine = Routine_BoxOff;
            Sst_Draw(self);
            break;
        }
        self->y_pos.f.u += SstFloorDist(self, self->x_pos.f.u);
        Animate(self, Animation_DEZMetalSonic);
        Box_Place(self, own);
        Sst_Draw(self);
        break;

    case Routine_Attack:
        Hit(self, own);
        Refresh(self);
        Step(self, own);
        Place(Part(own->flame), self, 0, 0);
        Box_Place(self, own);
        Sst_MoveSprite2(self);
        Sst_Draw(self);
        break;

    case Routine_Dying: // Offset_0x033F0A: blowing up, and then the arena opens
        self->collision_flags = 0;
        if (--own->death >= 0) {
            Boss_Defeated(self);
            Sst_Draw(self);
            break;
        }
        limit_right2 = 0x1000 + SCREEN_WIDEADD2;
        dle_routine += 2;
        Sst_Delete(self);
        break;
    }
}

// The flame behind him, the shoulder box and the window
static void Part_Run(Sst *self) {
    const Own_MetalSonicPart *part = (const Own_MetalSonicPart *)self->own.u8;
    Sst *master = &sst[part->master];

    switch (self->routine) {
    case Routine_FlameInit: // Offset_0x033F2A
        Look(self, false);
        self->width_pixels = 8;
        self->collision_flags = 0;
        break;
    case Routine_Flame: // Offset_0x033F3C: it turns as he does
        self->render_flags = (uint8_t)((self->render_flags & 0xFC) | (master->render_flags & 3));
        Animate(self, Animation_DEZMetalSonicFlame);
        Sst_Draw(self);
        break;
    case Routine_FlameOff:
    case Routine_BoxOff:
        if (self->routine == Routine_BoxOff)
            self->collision_flags = 0;
        break;
    case Routine_BoxInit: // Offset_0x033F54
        Look(self, false);
        self->width_pixels = 8;
        self->mapping_frame = 0x0B;
        self->priority = 3 * 0x80;
        break;
    case Routine_BoxOn: // Offset_0x033F74
        self->collision_flags = 0x98;
        break;
    case Routine_WindowInit: // Offset_0x033F7C: Robotnik's window, in the wall at ($2C0,$139)
        Look(self, true);
        self->mapping_frame = 4;
        self->x_pos.f.u = 0x2C0;
        self->y_pos.f.u = 0x139;
        break;
    case Routine_WindowWait: // Offset_0x033F94: it opens when he lands
        if (master->status & Status_Landed) {
            master->status &= (uint8_t)~Status_Landed;
            self->routine += 2;
        }
        Sst_Draw(self);
        break;
    case Routine_WindowOpen: // Offset_0x033FAC
        Sst_Animate(self, Animation_DEZRobotnikWindow);
        Sst_Draw(self);
        break;
    case Routine_WindowLook: // Offset_0x033FBA: Robotnik looks; he laughs when Sonic is hurt, and is angry when Metal Sonic is
        if (master->status & Status_Beaten) {
            self->routine += 2;
            self->anim = 1;
            Sst_Draw(self);
            break;
        }
        self->anim = 2;
        if (player->routine == 4)
            self->anim = 3;
        else if (master->collision_flags == 0)
            self->anim = 4;
        Sst_Animate(self, Animation_DEZRobotnikWindow);
        Sst_Draw(self);
        break;
    case Routine_WindowClose: // Offset_0x03400A
        Sst_Animate(self, Animation_DEZRobotnikWindow);
        SstOnScreenTest(self);
        break;
    }
}

void Obj_DEZMetalSonic(Sst *self) {
    if (self->routine <= Routine_Dying)
        Metal_Run(self, (Own_MetalSonic *)self->own.u8);
    else
        Part_Run(self);
}

const uint8_t *MetalSonic_Mappings(void) {
    return Mappings_DEZMetalSonic;
}
