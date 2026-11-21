// Beta 4's $C5, Wing Fortress's boss (Obj_0xC5_SFz_Boss, objects/obj_0xC5.asm): see SFZBoss.h. One object id with several subtypes: its routine is the subtype less $90 (the layout's boss is subtype $92). The body makes the
// rest through Load_Sub_Obj (the address of each part goes in a field of the body, and the part's `$2C` is the body's address); an address is $B000 + the slot * $40, as in the original, so the fields are kept as it has them.
// The fields are the original's offsets in the object's own memory (B(o) a byte, W(o) a word at $o).
#include "Object/SFZBoss.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"
#include "Constants.h"
#include "EnginePalette.h"
#include "Level.h"
#include "Sound.h"
#include "Object/BossCommon.h"
#include "Object/CharControl.h"
#include "Object/Tails.h"
#include "Solid.h"

#include "Resource/Animation/SFZBoss.h"
#include "Resource/Mappings/SFZBoss.h"
#include "Resource/Mappings/SFZBossRobotnik.h"
extern const uint8_t Animation_DEZRobotnikRunning[]; // (DEZRunning.c has them)
extern const uint8_t Mappings_DEZRobotnikRunning[];

#define B(o) (self->own.u8[(o) - 0x28])
#define W(o) (self->own.u16[((o) - 0x28) / 2])
#define BO(s, o) ((s)->own.u8[(o) - 0x28])
#define WO(s, o) ((s)->own.u16[((o) - 0x28) / 2])

#define ArtTile_SFZBoss 0x379     // (the spike balls' and lasers' art of the boss's list, over the Clucker's)
#define ArtTile_SFZBossPlate 0x46D // (the platform's art, the plate's)

// Which routine each subtype is (its subtype less $90)
enum { R_Init = 0, R_Body = 2, R_Platform = 4, R_Hanger = 6, R_Bouncer = 8, R_Box = 0xA, R_Rising = 0xC, R_Beam = 0xE, R_Robotnik = 0x10, R_Plate = 0x12 };
// The subtypes the body makes (Load_Sub_Obj's tables loc_38B44 and on): the field of the body they are kept in, and the subtype
enum { Sub_Platform = 0x94, Sub_Hanger = 0x96, Sub_Bouncer = 0x98, Sub_Box = 0x9A, Sub_Rising = 0x9C, Sub_Beam = 0x9E, Sub_Robotnik = 0xA0, Sub_Plate = 0xA2 };
enum { F_Platform = 0x2A, F_Beam = 0x3E, F_Box = 0x3C, F_Rising = 0x3C, F_Hanger = 0x3A, F_Robotnik = 0x38, F_Plate = 0x3E, F_Bouncer = 0x3E };

static uint16_t Address(const Sst *obj) {
    return (uint16_t)(0xB000 + (obj - sst) * 0x40);
}

static Sst *FromAddress(uint16_t address) {
    return &sst[(address - 0xB000) / 0x40];
}

// loc_32970, Load_Sub_Obj: a new object after this one, this id with the subtype, its address in the field `field` of this one's memory; its `$2C` is this one's address, at this one's place
static Sst *LoadSub(Sst *self, int field, uint8_t subtype) {
    Sst *sub = Sst_AllocateAfter(self);
    if (sub == NULL)
        return NULL;
    W(field) = Address(sub);
    sub->code = Obj_SFZBoss;
    sub->subtype = subtype;
    WO(sub, 0x2C) = Address(self);
    sub->x_pos.f.u = self->x_pos.f.u;
    sub->y_pos.f.u = self->y_pos.f.u;
    return sub;
}

// loc_32866: the nearer of the two characters in X (Sonic when equal); how far this one is to the right of it (dx; negative: it is to the left)
static Sst *Nearest(const Sst *self, int16_t *dx) {
    Sst *chr = player;
    int16_t d = (int16_t)(self->x_pos.f.u - chr->x_pos.f.u);
    if (TAILS_OBJ->code != NULL) {
        const int16_t d2 = (int16_t)(self->x_pos.f.u - TAILS_OBJ->x_pos.f.u);
        const int16_t a = d < 0 ? -d : d, b = d2 < 0 ? -d2 : d2;
        if (!(a <= b)) {
            chr = TAILS_OBJ;
            d = d2;
        }
    }
    *dx = d;
    return chr;
}

// loc_376D0: whoever stands on this object is let go
static void Release(Sst *self) {
    if ((self->status & 0x18) == 0)
        return;
    if (self->status & 0x08) {
        self->status &= (uint8_t)~0x08;
        player->status &= (uint8_t)~SST_PSTATUS_OBJECT_STAND;
        player->status |= SST_PSTATUS_IN_AIR;
    }
    if (self->status & 0x10) {
        self->status &= (uint8_t)~0x10;
        if (TAILS_OBJ->code != NULL) {
            TAILS_OBJ->status &= (uint8_t)~SST_PSTATUS_OBJECT_STAND;
            TAILS_OBJ->status |= SST_PSTATUS_IN_AIR;
        }
    }
}

// loc_3A8F2 / loc_3A8F8: the solid box for both characters
static void SolidBox(Sst *self, int16_t x_rad, int16_t y_air, int16_t y_walk, int16_t x) {
    for (int who = SolidChar_Sonic; who <= SolidChar_Tails; who++) {
        Sst *chr = who == SolidChar_Sonic ? player : (TAILS_OBJ->code != NULL ? TAILS_OBJ : NULL);
        if (chr != NULL)
            Solid_Character(self, chr, who, x_rad, y_air, y_walk, x, NULL);
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// The body ($92)

// loc_38AD0: the hits: the body flashes when it is hit (only while it can be: status bit 6), and when they are out ($21) it is beaten
static void Body_Hits(Sst *self) {
    if (self->collision_property == 0) {
        AddPoints(100);
        self->collision_flags = 0;
        WO(self, 0x32) = 0xEF; // (the original: the word at $30)
        self->routine_secondary = 0x1C;
        self->status |= 0x20;
        self->status &= (uint8_t)~0x40;
        return;
    }
    if (self->collision_flags != 0)
        return;
    if (B(0x30) == 0) {
        if (!(self->status & 0x40))
            return;
        B(0x30) = 0x20;
        QueueSound2(sfx_HitBoss);
    }
    dry_palette[1][1] = (dry_palette[1][1] == 0) ? 0x0EEE : 0;
    if (--B(0x30) == 0 && (self->status & 0x10))
        self->collision_flags = 6;
}

static void Body_Facing(Sst *self, int16_t speed) { // (toward the nearer character: loc_32866's d0 is 2 when it is to the left of him)
    int16_t dx;
    Nearest(self, &dx);
    self->x_vel = dx < 0 ? speed : (int16_t)-speed;
}

// Returns false when the object has gone (no hits and no drawing after it)
static bool Body_Run(Sst *self) {
    switch (self->routine_secondary) {
    case 0x00: // loc_38382
        self->routine_secondary += 2;
        self->collision_flags = 0;
        self->collision_property = 8;
        limit_btm2 = 0x442 - SCREEN_TALLADD;
        limit_btm1 = 0x442 - SCREEN_TALLADD;
        W(0x34) = (uint16_t)(self->x_pos.f.u - 0x60);
        W(0x36) = (uint16_t)(self->x_pos.f.u + 0x60);
        Sst_Draw(self);
        break;
    case 0x02: { // loc_383B6: it waits for a character under it (within $20 each way)
        int16_t dx;
        Nearest(self, &dx);
        if ((uint16_t)(dx + 0x20) >= 0x40) {
            Sst_Draw(self);
            break;
        }
        self->routine_secondary += 2; // loc_383C8
        self->y_vel = 0x40;
        Sst *p = LoadSub(self, F_Platform, Sub_Platform);
        if (p != NULL) {
            p->x_pos.f.u -= 0x88;
            p->y_pos.f.u += 0x60;
        }
        p = LoadSub(self, F_Platform, Sub_Platform);
        if (p != NULL) {
            p->x_pos.f.u += 0x88;
            p->y_pos.f.u += 0x60;
        }
        LoadSub(self, F_Rising, Sub_Rising);
        LoadSub(self, F_Hanger, Sub_Hanger);
        LoadSub(self, F_Robotnik, Sub_Robotnik);
        W(0x2A) = 0x60;
        Sst_Draw(self);
        break;
    }
    case 0x04: // loc_38426
        if (--W(0x2A) != 0) {
            Sst_MoveSprite2(self);
            Sst_Draw(self);
            break;
        }
        self->routine_secondary += 2;
        self->y_vel = 0;
        Sst_Draw(self);
        break;
    case 0x06: // loc_38440: it goes for the nearer character along its track
        self->routine_secondary += 2;
        Body_Facing(self, 0x100);
        self->status |= 0x04;
        W(0x2A) = 0x70;
        Sst_Draw(self);
        break;
    case 0x08: // loc_38466
        if ((int16_t)--W(0x2A) < 0) {
            self->routine_secondary += 2;
            self->anim = 0;
            Sst_Draw(self);
            break;
        }
        if (self->x_vel >= 0) {
            if ((uint16_t)self->x_pos.f.u >= W(0x36))
                self->x_vel = (int16_t)-self->x_vel;
        } else if ((uint16_t)self->x_pos.f.u < W(0x34)) {
            self->x_vel = (int16_t)-self->x_vel;
        }
        Sst_MoveSprite2(self);
        Sst_Draw(self);
        break;
    case 0x0A: // loc_3849E: its animation (the end of the script goes on)
    case 0x18:
        Sst_Animate(self, Animation_SFZBoss);
        Sst_Draw(self);
        break;
    case 0x0C: { // loc_384AC: the part that rises is told to
        self->routine_secondary += 2;
        W(0x2A) = 0x0E;
        Sst *rising = FromAddress(W(0x3C));
        rising->routine_secondary = 4;
        Sst_Draw(self);
        break;
    }
    case 0x0E: // loc_384C4
        if (--W(0x2A) != 0) {
            FromAddress(W(0x3C))->y_pos.f.u++;
            Sst_Draw(self);
            break;
        }
        self->routine_secondary += 2; // loc_384D6: it can be hit
        W(0x2A) = 0x40;
        self->status |= 0x10 | 0x40;
        self->collision_flags = 6;
        Sst_Draw(self);
        break;
    case 0x10: // loc_384F6
        if ((int16_t)--W(0x2A) < 0) {
            self->routine_secondary += 2;
            LoadSub(self, F_Beam, Sub_Beam);
        }
        Sst_Draw(self);
        break;
    case 0x12: { // loc_38512: when the beam is down (its status bit 2) it goes the other way
        const Sst *beam = FromAddress(W(F_Beam));
        if (beam->status & 0x04) {
            self->routine_secondary += 2;
            W(0x2A) = 0x80;
            Body_Facing(self, 0x80);
        }
        Sst_Draw(self);
        break;
    }
    case 0x14: // loc_38542
        if ((int16_t)--W(0x2A) < 0) { // loc_3856E: the beam is taken in
            self->routine_secondary += 2;
            W(0x2A) = 0x0E;
            self->status &= (uint8_t)~(0x08 | 0x10 | 0x40);
            self->collision_flags = 0;
            Sst_Delete(FromAddress(W(F_Beam)));
            Sst_Draw(self);
            break;
        }
        if (self->x_vel >= 0) {
            if ((uint16_t)self->x_pos.f.u >= W(0x36))
                self->x_vel = 0;
        } else if ((uint16_t)self->x_pos.f.u < W(0x34)) {
            self->x_vel = 0;
        }
        Sst_MoveSprite2(self);
        Sst_Draw(self);
        break;
    case 0x16: // loc_3859A
        if (--W(0x2A) != 0) {
            FromAddress(W(0x3C))->y_pos.f.u--;
            Sst_Draw(self);
            break;
        }
        self->routine_secondary += 2;
        self->anim = 1;
        Sst_Draw(self);
        break;
    case 0x1A: // loc_385BA: back to the rounds
        self->routine_secondary = 6;
        Body_Run(self); // (the state above, which goes on to the next)
        return true;
    case 0x1C: // loc_385C8: beaten: it blows up, and then the camera is free again
        self->collision_flags = 0;
        self->collision_property = 0xFF;
        self->status &= (uint8_t)~0x40;
        if ((int16_t)--WO(self, 0x32) < 0) {
            limit_btm2 = 0x720 - SCREEN_TALLADD;
            limit_btm1 = 0x720 - SCREEN_TALLADD;
            Sst_Delete(self);
            return false;
        }
        Boss_Defeated(self);
        Sst_Draw(self);
        break;
    }
    return true;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// The parts

static void Platform_Run(Sst *self) { // $94, loc_385F8: a solid platform that flashes away when the body is beaten
    const Sst *body = FromAddress(WO(self, 0x2C));
    switch (self->routine_secondary) {
    case 0:
        self->routine_secondary += 2;
        self->mapping_frame = 0x0C;
        Sst_Draw(self);
        break;
    case 2:
        if (!(body->status & 0x20)) {
            const uint8_t old = B(0x2F) & 1; // (bchg #0,$2F: it is shown every other frame)
            B(0x2F) ^= 1;
            if (!old)
                Sst_Draw(self);
        } else {
            self->routine_secondary += 2;
            B(0x30) = 4;
            Sst_Draw(self);
        }
        break;
    case 4: {
        uint8_t timer = (uint8_t)(self->anim_frame_timer - 1); // (subq.b #1,$1E)
        self->anim_frame_timer = (int8_t)timer;
        if ((int8_t)timer >= 0)
            return;
        uint8_t d0 = (uint8_t)(timer + 2);
        uint8_t d1 = self->anim_frame;
        if ((int8_t)d0 < 0) {
            self->anim_frame_timer = (int8_t)d1;
            if ((int8_t)--B(0x30) < 0) {
                B(0x30) = 0x10;
                d1++;
                if (d1 >= 5) {
                    Sst_Delete(self);
                    return;
                }
                self->anim_frame = d1;
                self->anim_frame_timer = (int8_t)d1;
            }
        }
        B(0x2F) &= (uint8_t)~1;
        Sst_Draw(self);
        break;
    }
    }
    if (self->code != NULL)
        SolidBox(self, 0x13, 0x40, 0x80, self->x_pos.f.u);
}

static void Hanger_Run(Sst *self) { // $96, loc_3869E: it lets down the bouncers one at a time
    Sst *body = FromAddress(WO(self, 0x2C));
    switch (self->routine_secondary) {
    case 0:
        self->routine_secondary += 2;
        self->mapping_frame = 5;
        self->y_pos.f.u += 8;
        break;
    case 2:
        if (body->status & 0x04) {
            self->routine_secondary += 2;
            W(0x2A) = 0x40;
            self->y_vel = 0x40;
        }
        break;
    case 4:
        if (--W(0x2A) != 0) {
            Sst_MoveSprite2(self);
        } else {
            self->routine_secondary += 2;
            self->y_vel = 0;
            W(0x2A) = 0x10;
        }
        break;
    case 6: {
        if (body->status & 0x20) { // loc_38756: the body is beaten
            self->routine_secondary += 2;
            self->status |= 0x20;
            break;
        }
        if (--W(0x2A) != 0)
            break;
        W(0x2A) = 0x80;
        uint8_t d0 = (uint8_t)(B(0x2E) + 1);
        if (d0 >= 3)
            d0 = 0;
        B(0x2E) = d0;
        if (B(0x30 + d0) != 0)
            break;
        B(0x30 + d0) = 0xFF;
        Sst *bouncer = LoadSub(self, F_Bouncer, Sub_Bouncer);
        if (bouncer != NULL)
            BO(bouncer, 0x2E) = B(0x2E);
        break;
    }
    case 8:
        if (body->code != Obj_SFZBoss) {
            Sst_Delete(self);
            return;
        }
        Boss_Defeated(self);
        break;
    }
    Sst_Draw(self);
}

// loc_38832: a bouncer is gone when its body is beaten, or when it is under the box that is let down (the body's box's range): it explodes
static bool Bouncer_Check(Sst *self) {
    Sst *hanger = FromAddress(WO(self, 0x2C));
    bool go = (hanger->status & 0x20) != 0;
    if (!go) {
        Sst *body = FromAddress(WO(hanger, 0x2C));
        if (!(body->status & 0x08))
            return false;
        const int16_t x = self->x_pos.f.u, left = (int16_t)(body->x_pos.f.u - 0x20);
        if ((uint16_t)x <= (uint16_t)left)
            return false;
        if ((uint16_t)x > (uint16_t)(left + 0x40))
            return false;
        Release(self);
        BO(hanger, 0x30 + B(0x2E)) = 0;
    }
    // loc_3887A: it becomes an explosion
    self->code = sst_object_codes[ObjId_BossExplosionPiece];
    self->routine = 0;
    Sst_Delete(FromAddress(W(0x3C)));
    return true;
}

static void Bouncer_Move(Sst *self, int16_t old_x) { // loc_38818
    Sst_MoveSprite2(self);
    SolidBox(self, 0x10, 8, 8, old_x);
}

static void Bouncer_Run(Sst *self) { // $98, loc_38778
    switch (self->routine_secondary) {
    case 0:
        self->routine_secondary += 2;
        self->anim = 3;
        self->mapping_frame = 7;
        self->y_vel = 0x100;
        W(0x2A) = 0x60;
        LoadSub(self, F_Box, Sub_Box);
        break;
    case 2:
        if (Bouncer_Check(self))
            return;
        if (--W(0x2A) == 0) {
            self->routine_secondary += 2;
            W(0x2A) = 0x60;
            self->x_vel = (int16_t)0xFF00;
            W(0x34) = (uint16_t)self->y_pos.f.u;
        }
        Bouncer_Move(self, self->x_pos.f.u);
        break;
    case 4: {
        if (Bouncer_Check(self))
            return;
        if (--W(0x2A) == 0) {
            W(0x2A) = 0xC0;
            self->x_vel = (int16_t)-self->x_vel;
        }
        const int16_t d = ((uint16_t)self->y_pos.f.u < W(0x34)) ? 4 : -4;
        self->y_vel = (int16_t)(self->y_vel + d);
        Bouncer_Move(self, self->x_pos.f.u);
        break;
    }
    }
    Sst_Animate(self, Animation_SFZBoss);
    if (self->code != NULL)
        Sst_Draw(self);
}

static void Box_Run(Sst *self) { // $9A, loc_3888E: the hurting box that goes with a bouncer (nothing is drawn)
    if (self->routine_secondary == 0) {
        self->routine_secondary += 2;
        self->collision_flags = 0x9B;
        return;
    }
    const Sst *bouncer = FromAddress(WO(self, 0x2C));
    if (bouncer->status & 0x20) {
        Sst_Delete(self);
        return;
    }
    self->x_pos.f.u = bouncer->x_pos.f.u;
    self->y_pos.f.u = (int16_t)(bouncer->y_pos.f.u + 0x0C);
}

static void Rising_Run(Sst *self) { // $9C, loc_388CE: a part that follows the body (the body lets it down and up by its Y)
    const Sst *body = FromAddress(WO(self, 0x2C));
    if (body->status & 0x20) {
        Sst_Delete(self);
        return;
    }
    switch (self->routine_secondary) {
    case 0:
        self->routine_secondary += 2;
        self->mapping_frame = 4;
        break;
    case 2:
        self->x_pos.f.u = body->x_pos.f.u;
        self->y_pos.f.u = body->y_pos.f.u;
        break;
    case 4:
        self->x_pos.f.u = body->x_pos.f.u;
        break;
    }
    Sst_Draw(self);
}

static void Beam_Run(Sst *self) { // $9E, loc_38920: the beam: it grows down in steps, then the body goes under it; it flickers
    const Sst *body = FromAddress(WO(self, 0x2C));
    if (body->status & 0x20) {
        Sst_Delete(self);
        return;
    }
    static const uint8_t frames[6] = { 0x0E, 0x0F, 0x10, 0x11, 0x12, 0 };
    static const uint8_t cols[6] = { 0x86, 0xAB, 0xAC, 0xAD, 0xAE, 0 };
    switch (self->routine_secondary) {
    case 0:
        self->routine_secondary += 2;
        self->mapping_frame = 0x0D;
        self->priority = 4 * 0x80;
        self->collision_flags = 0;
        self->y_pos.f.u += 0x10;
        self->anim_frame = 0x0C;
        self->y_pos.f.u -= 3;
        break;
    case 2: {
        B(0x2F) |= 1;
        int8_t timer = (int8_t)(self->anim_frame_timer - 1);
        self->anim_frame_timer = timer;
        if (timer >= 0)
            break;
        if ((int8_t)(timer + 2) >= 0) {
            B(0x2F) &= (uint8_t)~1;
            break;
        }
        uint8_t d0 = (uint8_t)(self->anim_frame - 1);
        if (d0 == 0) {
            self->routine_secondary += 2;
            W(0x2A) = 0x40;
            break;
        }
        self->anim_frame = d0;
        self->anim_frame_timer = (int8_t)d0;
        B(0x2F) &= (uint8_t)~1;
        break;
    }
    case 4:
        if ((int16_t)--W(0x2A) < 0) {
            self->routine_secondary += 2;
            self->y_pos.f.u += 0x10;
        }
        break;
    case 6: {
        const uint8_t d0 = (uint8_t)(B(0x2E) + 1);
        if (d0 < 5) {
            self->y_pos.f.u += 0x10;
            B(0x2E) = d0;
            self->mapping_frame = frames[d0];
            self->collision_flags = cols[d0];
            break;
        }
        self->routine_secondary += 2;
        W(0x2A) = 0x80;
        self->status |= 0x04;
        FromAddress(WO(self, 0x2C))->status |= 0x08;
        break;
    }
    case 8:
        self->x_pos.f.u = body->x_pos.f.u;
        break;
    }
    const uint8_t old = B(0x2F) & 1; // (bchg #0,$2F: shown every other frame)
    B(0x2F) ^= 1;
    if (!old)
        Sst_Draw(self);
}

static void Robotnik_Run(Sst *self) { // $A0, loc_38A22: Robotnik, where the body's end leaves him
    const Sst *body = FromAddress(WO(self, 0x2C));
    switch (self->routine_secondary) {
    case 0:
        self->routine_secondary += 2;
        self->mapping_frame = 0;
        self->anim = 1;
        self->x_pos.f.u = 0x2C60;
        self->y_pos.f.u = 0x4E6;
        LoadSub(self, F_Plate, Sub_Plate);
        break;
    case 2:
        if (!(body->status & 0x20)) {
            Sst_Animate(self, Animation_DEZRobotnikRunning);
        } else {
            self->routine_secondary += 2;
            W(0x2A) = 0xC0;
        }
        break;
    case 4:
        if ((int16_t)--W(0x2A) < 0) {
            Sst_Delete(FromAddress(W(F_Plate)));
            Sst_Delete(self);
            return;
        }
        self->y_pos.f.u++;
        break;
    }
    Sst_Draw(self);
}

static void Plate_Run(Sst *self) { // $A2, loc_38AA2: the plate that follows Robotnik, a little below him
    const Sst *robotnik = FromAddress(WO(self, 0x2C));
    self->y_pos.f.u = (int16_t)(robotnik->y_pos.f.u + 0x26);
    Sst_Draw(self);
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
void Obj_SFZBoss(Sst *self) {
    if (self->routine == R_Init) {
        // EnemySettings: the subtype's entry (mappings, art, priority, width, collision), then the routine is the subtype less $90
        self->render_flags |= SST_RENDER_LEVEL;
        self->collision_flags = 0;
        switch (self->subtype) {
        case 0x92: // the body
            self->mappings = Mappings_SFZBoss;
            self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_SFZBoss);
            self->priority = 4 * 0x80;
            self->width_pixels = 0x20;
            break;
        case 0x94: // the platform
            self->mappings = Mappings_SFZBoss;
            self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_SFZBoss);
            self->priority = 1 * 0x80;
            self->width_pixels = 0x08;
            break;
        case 0xA0: // Robotnik
            self->mappings = Mappings_DEZRobotnikRunning;
            self->art_tile = 0;
            self->priority = 5 * 0x80;
            self->width_pixels = 0x20;
            break;
        case 0xA2: // the plate
            self->mappings = Mappings_SFZBossRobotnik;
            self->art_tile = TILE_MAP(1, 1, 0, 0, ArtTile_SFZBossPlate);
            self->priority = 5 * 0x80;
            self->width_pixels = 0x20;
            break;
        default: // the small parts
            self->mappings = Mappings_SFZBoss;
            self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_SFZBoss);
            self->priority = 5 * 0x80;
            self->width_pixels = 0x10;
            break;
        }
        self->routine = (uint8_t)(self->subtype - 0x90);
        return;
    }
    switch (self->routine) {
    case R_Body:
        if (Body_Run(self)) // (the hits go with the state, the beaten state ends it with no more)
            Body_Hits(self);
        break;
    case R_Platform: Platform_Run(self); break;
    case R_Hanger: Hanger_Run(self); break;
    case R_Bouncer: Bouncer_Run(self); break;
    case R_Box: Box_Run(self); break;
    case R_Rising: Rising_Run(self); break;
    case R_Beam: Beam_Run(self); break;
    case R_Robotnik: Robotnik_Run(self); break;
    case R_Plate: Plate_Run(self); break;
    }
}
