// Beta 4's $C7, the Death Egg's final boss (Obj_0xC7_DEz_Final_Boss, objects/obj_0xC7.asm): see DEZFinalBoss.h. One object id with several subtypes: the layout's body is subtype 2, and the subtype of a part is its
// routine. The body makes ten parts through Load_Sub_Obj (the address of each goes in a field of the body, from $2C to $3E, and the part's `$2C` is the body's address); an address is $B000 + the slot * $40 as in the original.
// The fields are the original's offsets in the object's own memory (B(o) a byte, W(o) a word at $o). The body's own bytes sit past the parts' words: $40 the pose's counter ($1F), $41 the pose it is at ($1B), $42 its timer
// ($1E), $43 its second step ($1D), $44 its first ($1C), $45 the count of its moves ($26), $46 the flash ($2A).
#include "Object/DEZFinalBoss.h"
#include "Object/DEZRunning.h"
#include "Object/ObjectHit.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"
#include "Constants.h"
#include "Macros.h"
#include "Game.h"
#include "Level.h"
#include "Screen.h"
#include "EnginePalette.h"
#include "Sound.h"
#include "HTZQuake.h"
#include "Object/BossCommon.h"
#include "Object/CharControl.h"
#include "Object/Tails.h"

#include "Resource/Animation/DEZFinalBossEye.h"
#include "Resource/Animation/DEZFinalBossFace.h"
#include "Resource/Animation/DEZFinalBossMarker.h"
#include "Resource/Animation/DEZFinalBossPosesA.h"
#include "Resource/Animation/DEZFinalBossPosesB.h"
#include "Resource/Animation/DEZFinalBossPosesC.h"
#include "Resource/Mappings/DEZFinalBoss.h"

#define B(o) (self->own.u8[(o) - 0x28])
#define W(o) (self->own.u16[((o) - 0x28) / 2])
#define BO(s, o) ((s)->own.u8[(o) - 0x28])
#define WO(s, o) ((s)->own.u16[((o) - 0x28) / 2])

#define ArtTile_DEZFinalBoss 0x330 // (the list $31's art at $6600)

#define PUPPET_TIMER B(0x40)
#define PUPPET_POSE B(0x41)
#define TIMER B(0x42)
#define STEP2 B(0x43)
#define STEP1 B(0x44)
#define MOVES B(0x45)
#define FLASH B(0x46)

// Which routine each subtype is (the parts' subtypes are their routines)
enum {
    R_Init = 0, R_Body = 2, R_Head = 4, R_PartA = 6, R_HandA = 8, R_PartB = 0xA, R_PartC = 0xC, R_Eye = 0xE, R_Face = 0x10, R_PartD = 0x12, R_HandB = 0x14, R_PartE = 0x16,
    R_Marker = 0x18, R_Reticle = 0x1A, R_Bomb = 0x1C, R_Debris = 0x1E, R_Leaving = 0x20
};
// The body's states ($25) and the fields it keeps its parts in
enum { S_Make = 0, S_Wait = 2, S_Rise = 4, S_Ready = 6, S_Choose = 8, S_Move = 0xA, S_Beaten = 0xC };
enum { F_Head = 0x2C, F_PartA = 0x2E, F_HandA = 0x30, F_PartB = 0x32, F_PartC = 0x34, F_Eye = 0x36, F_Face = 0x38, F_PartD = 0x3A, F_HandB = 0x3C, F_PartE = 0x3E };

static const uint8_t all_fields[10] = { F_Head, F_PartA, F_HandA, F_PartB, F_PartC, F_Eye, F_Face, F_PartD, F_HandB, F_PartE };

// How far the shake of the screen lasts (the original's $FFFFF660 counts it down and $FFFFEEBD is on while it does)
static int16_t shake_timer;

static uint16_t Address(const Sst *obj) {
    return (uint16_t)(0xB000 + (obj - sst) * 0x40);
}

static Sst *FromAddress(uint16_t address) {
    return &sst[(address - 0xB000) / 0x40];
}

// loc_32970, Load_Sub_Obj: a new object after this one, this id with the subtype, its address in the field `field` of this one's memory (none for 0: the originals that put it in the speed clear it at once); its `$2C` is
// this one's address, at this one's place
static Sst *LoadSub(Sst *self, int field, uint8_t subtype) {
    Sst *sub = Sst_AllocateAfter(self);
    if (sub == NULL)
        return NULL;
    if (field != 0)
        W(field) = Address(sub);
    sub->code = Obj_DEZFinalBoss;
    sub->subtype = subtype;
    WO(sub, 0x2C) = Address(self);
    sub->x_pos.f.u = self->x_pos.f.u;
    sub->y_pos.f.u = self->y_pos.f.u;
    return sub;
}

static Sst *Part(Sst *self, int field) {
    return FromAddress(W(field));
}

// loc_32866: the nearer of the two characters in X (Sonic when equal); dx is how far this one is to the right of it, `below` whether it is above it (the original's d1 is 2 then)
static Sst *Nearest(const Sst *self, int16_t *dx, bool *above) {
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
    *above = (uint16_t)self->y_pos.f.u < (uint16_t)chr->y_pos.f.u;
    return chr;
}

// Whether the body faces the nearer character (loc_32866's d0, less 2 when the body is flipped, is 0)
static bool FacesCharacter(const Sst *self) {
    int16_t dx;
    bool above;
    Nearest(self, &dx, &above);
    int d0 = dx < 0 ? 2 : 0;
    if (self->render_flags & 1)
        d0 -= 2;
    return d0 == 0;
}

static bool Dec(uint8_t *timer) { // (subq.b #1 and bmi: the timer has gone past zero)
    *timer = (uint8_t)(*timer - 1);
    return (*timer & 0x80) != 0;
}

// loc_3A0D4: at a place beside its maker (the maker's flip turns the side)
static void Follow(Sst *self, int16_t dx, int16_t dy) {
    const Sst *maker = FromAddress(W(0x2C));
    if (maker->render_flags & 1)
        dx = (int16_t)-dx;
    self->x_pos.f.u = (int16_t)(maker->x_pos.f.u + dx);
    self->y_pos.f.u = (int16_t)(maker->y_pos.f.u + dy);
}

// loc_3A0FA: parts put at these places from the body's (a field, how far across and down)
static void PlaceParts(Sst *self, const int8_t *places, int count) {
    for (int i = 0; i < count; i++, places += 3) {
        Sst *part = Part(self, (uint8_t)places[0]);
        int16_t dx = places[1];
        if (self->render_flags & 1)
            dx = (int16_t)-dx;
        part->x_pos.f.u = (int16_t)(self->x_pos.f.u + dx);
        part->y_pos.f.u = (int16_t)(self->y_pos.f.u + places[2]);
    }
}

static void PlaceAll(Sst *self) { // (loc_3A132)
    static const int8_t places[7 * 3] = { 0x2E, -4, 0x3C, 0x30, -0x0C, 8, 0x32, 0x0C, -8, 0x34, 4, 0x24, 0x3A, -4, 0x3C, 0x3C, -0x0C, 8, 0x3E, 4, 0x24 };
    PlaceParts(self, places, 7);
}

// loc_39FBA: the ten parts take the body's side
static void FlipParts(Sst *self) {
    const uint8_t flip = self->render_flags & 1;
    for (int i = 0; i < 10; i++) {
        Sst *part = Part(self, all_fields[i]);
        part->render_flags = (uint8_t)((part->render_flags & 0xFE) | flip);
        part->status = (uint8_t)((part->status & 0xFE) | flip);
    }
}

// loc_39FFC: the poses of the body, one after another: a pose moves some parts (a field of the body that holds the address of the part, or 0 for the body itself) by how far across and down, in 16ths of a pixel, every
// frame, for as long as it lasts. A script is a list of poses; $40 and a sound id plays the sound as the pose begins, $80 and a pose moves the other way, and $C0 or $FF ends it. True once the script is done
static bool Puppet(Sst *self, const uint8_t *table, const uint8_t *script) {
    uint8_t d0 = script[PUPPET_POSE];
    bool reverse = false;
    for (;;) {
        const uint8_t flags = d0 & 0xC0;
        if (flags == 0)
            break;
        if (flags == 0xC0) {
            PUPPET_POSE = 0;
            return true;
        }
        if (flags == 0x80) {
            reverse = true;
            d0 &= 0x3F;
            break;
        }
        d0 &= 0x3F; // (a sound: once, as the pose begins)
        if (PUPPET_TIMER != 0)
            break;
        uint8_t i = (uint8_t)(PUPPET_POSE + 1);
        QueueSound2(sfx_Jump + (script[i] - 0xA0)); // (Flamedriver ids: $BD is sfx_ChainStomp)
        i++;
        PUPPET_POSE = i;
        d0 = script[i];
    }
    const uint8_t *pose = table + ((table[d0 * 2] << 8) | table[d0 * 2 + 1]);
    const int count = pose[0];
    const uint8_t duration = pose[1];
    pose += 2;
    uint8_t d2 = (uint8_t)(PUPPET_TIMER + 1);
    if (d2 >= duration) {
        PUPPET_POSE++;
        d2 = 0;
    }
    PUPPET_TIMER = d2;
    for (int i = 0; i <= count; i++, pose += 3) {
        Sst *part = pose[0] == 0 ? self : FromAddress(W(pose[0]));
        int16_t dx = (int16_t)((int8_t)pose[1] * 16);
        if (self->render_flags & 1)
            dx = (int16_t)-dx;
        if (reverse)
            dx = (int16_t)-dx;
        part->x_pos.v += (int32_t)dx * 256;
        int16_t dy = (int16_t)((int8_t)pose[2] * 16);
        if (reverse)
            dy = (int16_t)-dy;
        part->y_pos.v += (int32_t)dy * 256;
    }
    return false;
}

static const uint8_t seq_reach[] = { 0, 1, 2, 3, 0xFF };                                                                               // loc_3A148 (poses A)
static const uint8_t seq_return[] = { 5, 6, 7, 8, 0xFF };                                                                              // loc_3A152 (poses A)
static const uint8_t seq_slam[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 0xC0 };                                                                  // loc_3A15C (poses A)
static const uint8_t seq_lift[] = { 0, 1, 2, 0xC0 };                                                                                   // loc_3A222 (poses B)
static const uint8_t seq_walk[] = { 0, 1, 2, 3, 0x40, 0xBD, 4, 5, 6, 7, 8, 0x40, 0xBD, 9, 0xA, 1, 2, 3, 0x40, 0xBD, 4, 5, 6, 7, 8, 0x40, 0xBD, 0xC0 }; // loc_3A25E (poses C)
static const uint8_t seq_rest[] = { 0x88, 0x87, 0x86, 0x85, 0xB, 0xC0 };                                                                // loc_3A27E (poses C)

// loc_39F88: two bombs thrown from the body's side
static void ThrowBombs(Sst *self) {
    static const int16_t speeds[2][2] = { { 0x60, -0x800 }, { 0xC0, -0xA00 } };
    for (int i = 0; i < 2; i++) {
        Sst *bomb = LoadSub(self, 0, R_Bomb);
        if (bomb == NULL)
            continue;
        bomb->x_vel = (self->render_flags & 1) ? (int16_t)-speeds[i][0] : speeds[i][0];
        bomb->y_vel = speeds[i][1];
    }
}

// loc_39F40: what hurts, the parts' collision once the fight has begun
static void SetPartCollisions(Sst *self) {
    static const uint8_t flags[10] = { 0, 0x8F, 0x9C, 0, 0x86, 0x2A, 0x8B, 0x8F, 0x9C, 0x8B };
    for (int i = 0; i < 10; i++)
        Part(self, all_fields[i])->collision_flags = flags[i];
}

// loc_39E12: a puff of the end of the fight, near the body
static void MakeExplosion(Sst *self) {
    Sst *exp = Sst_Allocate();
    if (exp == NULL)
        return;
    exp->code = sst_object_codes[ObjId_BossExplosionPiece];
    exp->x_pos.f.u = self->x_pos.f.u;
    exp->y_pos.f.u = self->y_pos.f.u;
    const uint32_t rand = RandomNumber();
    exp->x_pos.f.u = (int16_t)(exp->x_pos.f.u + (int16_t)(((rand & 0xFF) >> 2) - 0x30));
    exp->y_pos.f.u = (int16_t)(exp->y_pos.f.u + (int16_t)((((rand >> 8) & 0xFF) >> 2) - 0x30));
}

// loc_39EB2: the last hit: the body is beaten, and eight parts fly off
static void Beaten(Sst *self) {
    static const int16_t speeds[8][2] = { { 0x200, -0x400 }, { -0x100, -0x100 }, { 0x300, -0x300 }, { -0x100, -0x400 }, { 0x180, -0x200 }, { -0x200, -0x300 }, { 0, -0x400 }, { 0x100, -0x300 } };
    static const uint8_t flying[8] = { F_Head, F_PartA, F_HandA, F_PartB, F_PartC, F_PartD, F_HandB, F_PartE };
    AddPoints(100);
    TIMER = 0;
    self->routine_secondary = S_Beaten;
    STEP1 = 0;
    self->collision_flags = 0;
    self->x_vel = 0;
    self->y_vel = 0;
    for (int i = 0; i < 10; i++)
        Part(self, all_fields[i])->collision_flags = 0;
    for (int i = 0; i < 8; i++) {
        Sst *part = Part(self, flying[i]);
        part->routine = R_Debris;
        part->routine_secondary = 0;
        WO(part, 0x2A) = 0x80;
        part->x_vel = speeds[i][0];
        part->y_vel = speeds[i][1];
    }
    Sst_Delete(Part(self, F_Face));
}

// loc_39E50: the hits. The body is hit itself (the touch counts its `$21` down and clears its collision), or by its eye, which has $FF hits of its own and clears its collision when touched (and then costs the body one). A hit
// makes it flash for $3C frames, and then it can be hit again. True when the last one has been taken: the body is beaten
static bool Body_Hits(Sst *self) {
    if (self->collision_property == 0) {
        Beaten(self);
        return true;
    }
    if (FLASH == 0) {
        if (self->collision_flags != 0) {
            if (Part(self, F_Eye)->collision_flags != 0)
                return false;
            self->collision_flags = 0;
            if (--self->collision_property == 0) {
                Beaten(self);
                return true;
            }
        }
        FLASH = 0x3C;
        QueueSound2(sfx_HitBoss);
    }
    dry_palette[1][1] = (dry_palette[1][1] == 0) ? 0x0EEE : 0;
    if (--FLASH == 0) {
        dry_palette[1][1] = 0;
        self->collision_flags = 0x16;
        Part(self, F_Eye)->collision_flags = 0x2A;
    }
    return false;
}

static void Wait(Sst *self) { // loc_39586
    if (Dec(&TIMER))
        STEP2 += 2;
}

static void Shake(int16_t frames) {
    dhz_shake = 1;
    shake_timer = frames;
}

// The body's first moves (STEP1 0, the walk), the second (2, the jump to the character and the fall: bombs after it) and the third (4, the hands' blows)
static void Move_Walk(Sst *self) {
    switch (STEP2) {
    case 0: case 4: Wait(self); break;
    case 2: // loc_39594
        if (Puppet(self, Animation_DEZFinalBossPosesC, seq_walk)) {
            STEP2 += 2;
            TIMER = 0x40;
        }
        break;
    case 6: // loc_395AE
        if (Puppet(self, Animation_DEZFinalBossPosesC, seq_rest)) {
            self->routine_secondary -= 2;
            TIMER = 0x40;
        }
        break;
    }
}

static void Move_Jump(Sst *self) {
    switch (STEP2) {
    case 0: case 6: Wait(self); break;
    case 2: // loc_395E6: it crouches, then rises
        if (Puppet(self, Animation_DEZFinalBossPosesB, seq_lift)) {
            STEP2 += 2;
            TIMER = 0x80;
            self->x_vel = 0;
            self->y_vel = -0x200;
        }
        break;
    case 4: // loc_3960A
        if (Dec(&TIMER)) {
            STEP2 += 2;
            self->y_vel = 0;
            LoadSub(self, 0, R_Marker); // (the mark that follows the character)
            self->x_vel = 0;
            W(0x28) = 0;
        } else {
            Sst_MoveSprite2(self);
            PlaceAll(self);
        }
        break;
    case 8: // loc_3963A: the mark says where (its X), and the body comes down there
        if (W(0x28) == 0)
            break;
        STEP2 += 2;
        self->x_pos.f.u = (int16_t)W(0x28);
        self->render_flags &= (uint8_t)~1;
        if (W(0x28) < 0x780)
            self->render_flags |= 1;
        FlipParts(self);
        self->y_vel = 0x800;
        TIMER = 0x20;
        break;
    case 0xA: // loc_3966E
        if (Dec(&TIMER)) {
            STEP2 += 2;
            self->y_vel = 0;
            Shake(0x40);
            Part(self, F_Face)->routine_secondary = 6;
            QueueSound2(sfx_Collapse);
        } else {
            Sst_MoveSprite2(self);
            PlaceAll(self);
        }
        break;
    case 0xC: // loc_396A6: the landing, then bombs unless it faces the character
        if (!Puppet(self, Animation_DEZFinalBossPosesA, seq_slam))
            break;
        PlaceAll(self);
        if (FacesCharacter(self)) {
            self->routine_secondary -= 2;
            break;
        }
        STEP2 += 2;
        TIMER = 0x60;
        ThrowBombs(self);
        break;
    case 0xE: // loc_396E4
        if (Dec(&TIMER))
            self->routine_secondary -= 2;
        break;
    }
}

static void Move_Punch(Sst *self) {
    switch (STEP2) {
    case 0: case 4: case 8: Wait(self); break;
    case 2: // loc_3970C: the arms go out; if it faces the character the hands are sent, if not it throws bombs
        self->status |= 0x40;
        if (!Puppet(self, Animation_DEZFinalBossPosesA, seq_reach))
            break;
        if (FacesCharacter(self)) {
            STEP2 += 2;
            TIMER = 0x40;
            self->status |= 0x10;
        } else {
            STEP2 = 8;
            TIMER = 0x20;
            ThrowBombs(self);
        }
        break;
    case 6: // loc_39754
        if (Dec(&TIMER)) {
            STEP2 += 2;
            self->status |= 0x20;
            TIMER = 0x40;
        }
        break;
    case 0xA: // loc_3976E: the arms come back
        if (Puppet(self, Animation_DEZFinalBossPosesA, seq_return)) {
            self->routine_secondary -= 2;
            self->status &= (uint8_t)~0x40;
        }
        break;
    }
}

// loc_39788: beaten: it falls with explosions and bounces once, the explosions go on, then Sonic walks on to the end of the level
static void Body_Beaten(Sst *self) {
    switch (STEP1) {
    case 0: // loc_3979C
        Boss_Defeated(self);
        Sst_MoveSprite(self);
        if ((uint16_t)self->y_pos.f.u < 0x15C)
            return;
        self->y_pos.f.u = 0x15C;
        if (self->y_vel >= 0) {
            const uint16_t d0 = (uint16_t)self->y_vel >> 2;
            if (d0 >= 0x100) {
                self->y_vel = (int16_t)-d0;
                return;
            }
        }
        STEP1 += 2;
        TIMER = 0x40;
        break;
    case 2: // loc_397D8
        if (Dec(&TIMER)) {
            STEP1 += 2;
            limit_right2 = 0x1000;
        } else {
            Boss_Defeated(self);
        }
        break;
    case 4: // loc_397EE: Sonic is walked to the right until the camera has come to $840
        lock_ctrl = true;
        jpad1_hold2 = JPAD_RIGHT;
        jpad1_press2 = JPAD_RIGHT;
        if (scrpos_x.f.u < 0x840)
            break;
        self->routine = R_Leaving;
        self->routine_secondary = 0;
        WO(self, 0x2A) = 0x20;
        Shake(0x1000);
        Sst_Delete(Part(self, F_Eye));
        break;
    }
}

static void Body(Sst *self) {
    if (dhz_shake && --shake_timer < 0)
        dhz_shake = 0;
    switch (self->routine_secondary) {
    case S_Make: { // loc_39412
        static const struct { uint8_t field, subtype; } parts[10] = {
            { F_Head, R_Head }, { F_HandA, R_HandA }, { F_PartA, R_PartA }, { F_PartB, R_PartB }, { F_PartC, R_PartC },
            { F_Eye, R_Eye }, { F_Face, R_Face }, { F_PartD, R_PartD }, { F_HandB, R_HandB }, { F_PartE, R_PartE },
        };
        self->routine_secondary += 2;
        self->mapping_frame = 3;
        self->priority = 5 * 0x80;
        for (int i = 0; i < 10; i++)
            LoadSub(self, parts[i].field, parts[i].subtype);
        PlaceAll(self);
        break;
    }
    case S_Wait: // loc_39490: the eye says when
        if (!(self->status & 0x04))
            break;
        self->routine_secondary += 2;
        self->y_vel = -0x100;
        TIMER = 0x79;
        Part(self, F_Face)->routine_secondary = 4;
        break;
    case S_Rise: // loc_394B6
        if (--TIMER == 0) {
            self->routine_secondary += 2;
            self->y_vel = 0;
            TIMER = 0x1F;
            self->collision_flags = 0x16;
            self->collision_property = 8;
            SetPartCollisions(self);
            Part(self, F_Face)->routine_secondary = 6;
        } else {
            Sst_MoveSprite2(self);
            PlaceAll(self);
        }
        break;
    case S_Ready: // loc_394F4
        if (Body_Hits(self))
            break;
        if (Dec(&TIMER))
            self->routine_secondary += 2;
        break;
    case S_Choose: { // loc_39506: the next move, of a round of the walk, the jump, the blows and the jump
        static const uint8_t moves[4] = { 2, 0, 2, 4 };
        if (Body_Hits(self))
            break;
        self->routine_secondary += 2;
        TIMER = 0x20;
        MOVES++;
        STEP1 = moves[MOVES & 3];
        STEP2 = 0;
        if (STEP1 == 2) {
            Sst *face = Part(self, F_Face);
            face->routine_secondary = 4;
            face->anim = 2;
        }
        break;
    }
    case S_Move: // loc_3954A
        if (Body_Hits(self))
            break;
        switch (STEP1) {
        case 0: Move_Walk(self); break;
        case 2: Move_Jump(self); break;
        case 4: Move_Punch(self); break;
        }
        break;
    case S_Beaten:
        Body_Beaten(self);
        break;
    }
    if (self->code != NULL && self->routine == R_Body)
        Sst_Draw(self);
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// The parts

// A part that is a picture at a place of the body's: its frame, and the priority (5) when it is in front
static void Picture(Sst *self, uint8_t frame, bool front) {
    if (self->routine_secondary == 0) {
        self->routine_secondary += 2;
        self->mapping_frame = frame;
        if (front)
            self->priority = 5 * 0x80;
    }
    Sst_Draw(self);
}

// The hands ($30 and $3C): they wait for the body's say (its status bit 4 or 5), fall a little, dash toward the character (how fast up and down by how far it is) and come back
static void Hand(Sst *self, uint8_t bit) {
    Sst *body = FromAddress(W(0x2C));
    switch (self->routine_secondary) {
    case 0:
        self->routine_secondary += 2;
        self->mapping_frame = 6;
        if (bit == 0x20)
            self->priority = 5 * 0x80;
        break;
    case 2: // loc_39952 / loc_39BA6
        if (!(body->status & bit))
            break;
        body->status &= (uint8_t)~bit;
        self->routine_secondary += 2;
        WO(self, 0x2A) = 0x10;
        WO(self, 0x2E) = (uint16_t)self->y_pos.f.u;
        break;
    case 4: // loc_39972
        if ((int16_t)--WO(self, 0x2A) < 0) {
            static const int16_t speeds[4] = { 0x200, 0x100, 0x80, 0 };
            self->routine_secondary += 2;
            WO(self, 0x2A) = 0x20;
            int16_t dx;
            bool above;
            Nearest(self, &dx, &above);
            uint16_t d2 = (uint16_t)(dx < 0 ? -dx : dx);
            if (d2 >= 0x100)
                d2 = 0xFF;
            int16_t speed = speeds[(d2 & 0xC0) >> 6];
            if (!above)
                speed = (int16_t)-speed;
            self->y_vel = speed;
            self->x_vel = (self->render_flags & 1) ? 0x800 : -0x800;
            QueueSound2(sfx_Teleport);
        } else {
            self->y_vel = (int16_t)(self->y_vel + 0x20);
            Sst_MoveSprite2(self);
        }
        break;
    case 6: // loc_399D8
        if ((int16_t)--WO(self, 0x2A) < 0) {
            self->routine_secondary += 2;
            self->x_vel = (int16_t)-self->x_vel;
            WO(self, 0x2A) = 0x20;
            self->y_vel = (int16_t)((int16_t)(WO(self, 0x2E) - self->y_pos.f.u) * 8);
        } else {
            Sst_MoveSprite2(self);
        }
        break;
    case 8: // loc_39A00
        if ((int16_t)--WO(self, 0x2A) < 0) {
            self->routine_secondary = 2;
            self->x_vel = 0;
            self->y_vel = 0;
        } else {
            Sst_MoveSprite2(self);
        }
        break;
    }
    if (bit == 0x10 && (self->status & 0x40)) // (the first hand is not shown while its own bit 6 is set)
        return;
    Sst_Draw(self);
}

static void Eye(Sst *self) {
    switch (self->routine_secondary) {
    case 0:
        self->routine_secondary += 2;
        self->mapping_frame = 0x15;
        break;
    case 2: { // loc_39AA8: Robotnik running ($C6) has hopped away
        const Sst *running = DEZRunning_Robotnik();
        if (running != NULL && (running->status & 0x08)) {
            self->routine_secondary += 2;
            WO(self, 0x2A) = 0x40;
        }
        break;
    }
    case 4: // loc_39AC2
        Sst_Animate(self, Animation_DEZFinalBossEye);
        break;
    case 6: // loc_39AD0
        if ((int16_t)--WO(self, 0x2A) < 0) {
            self->routine_secondary += 2;
            FromAddress(W(0x2C))->status |= 0x04;
        }
        break;
    case 8: // loc_39AEC: it takes $FF hits of its own
        self->collision_property = 0xFF;
        break;
    }
    Follow(self, 0, -0x34);
    Sst_Draw(self);
}

static void Face(Sst *self) { // loc_39AF6
    switch (self->routine_secondary) {
    case 0:
        self->routine_secondary += 2;
        self->mapping_frame = 0x0C;
        Follow(self, 0x38, 0x18);
        return;
    case 2: case 8:
        self->anim = 3;
        break;
    case 6:
        self->anim = 1;
        break;
    }
    Sst_Animate(self, Animation_DEZFinalBossFace);
    Sst_Draw(self);
    Follow(self, 0x38, 0x18);
}

// The mark: it follows the character (a few frames behind by the speeds it keeps) for $A0 frames, then stays $40 more with a blinking one on it, and tells the body where it was
static void Marker(Sst *self) {
    const Sst *sonic = player;
    switch (self->routine_secondary) {
    case 0: // loc_39C0A
        self->routine_secondary += 2;
        self->mapping_frame = 0x10;
        self->art_tile |= 0x8000;
        self->priority = 1 * 0x80;
        WO(self, 0x2A) = 0xA0;
        self->x_pos.f.u = sonic->x_pos.f.u;
        self->y_pos.f.u = sonic->y_pos.f.u;
        WO(self, 0x30) = (uint16_t)sonic->x_vel;
        WO(self, 0x32) = (uint16_t)sonic->y_vel;
        Sst_Draw(self);
        break;
    case 2: // loc_39C46
        if ((int16_t)--WO(self, 0x2A) < 0) {
            self->routine_secondary += 2;
            WO(self, 0x2A) = 0x40;
            self->x_pos.f.u = sonic->x_pos.f.u;
            self->y_pos.f.u = sonic->y_pos.f.u;
            Sst *reticle = LoadSub(self, 0, R_Reticle);
            (void)reticle;
            self->x_vel = 0;
            Sst_Draw(self);
            break;
        }
        self->y_vel = (int16_t)WO(self, 0x3E);
        self->x_vel = (int16_t)WO(self, 0x3C);
        WO(self, 0x3C) = WO(self, 0x38);
        WO(self, 0x3E) = WO(self, 0x3A);
        WO(self, 0x38) = WO(self, 0x34);
        WO(self, 0x3A) = WO(self, 0x36);
        WO(self, 0x34) = WO(self, 0x30);
        WO(self, 0x36) = WO(self, 0x32);
        if (sonic->x_vel == 0)
            self->x_pos.f.u = sonic->x_pos.f.u;
        WO(self, 0x30) = (uint16_t)sonic->x_vel;
        if (sonic->y_vel == 0)
            self->y_pos.f.u = sonic->y_pos.f.u;
        WO(self, 0x32) = (uint16_t)sonic->y_vel;
        Sst_MoveSprite2(self);
        Sst_Animate(self, Animation_DEZFinalBossMarker);
        Sst_Draw(self);
        break;
    case 4: // loc_39CC0
        if ((int16_t)--WO(self, 0x2A) < 0) {
            WO(FromAddress(W(0x2C)), 0x28) = (uint16_t)self->x_pos.f.u;
            Sst_Delete(self);
            return;
        }
        Sst_Animate(self, Animation_DEZFinalBossMarker);
        Sst_Draw(self);
        break;
    }
}

static void Reticle(Sst *self) { // loc_39CE2: the blinking mark on the spot, until the mark is gone
    if (self->routine_secondary == 0) {
        self->routine_secondary += 2;
        self->mapping_frame = 0x14;
        self->priority = 1 * 0x80;
        self->art_tile |= 0x8000;
        WO(self, 0x2A) = 4;
        Sst_Draw(self);
        return;
    }
    if (FromAddress(W(0x2C))->code == NULL) {
        Sst_Delete(self);
        return;
    }
    if (--WO(self, 0x2A) == 0) {
        WO(self, 0x2A) = 4;
        self->art_tile ^= 0x2000;
    }
    Sst_Draw(self);
}

static void Bomb(Sst *self) { // loc_39D34: thrown up from the body's side, it falls to the floor and blows up
    switch (self->routine_secondary) {
    case 0:
        self->routine_secondary += 2;
        self->mapping_frame = 0x0E;
        self->collision_flags = 0x89;
        self->priority = 5 * 0x80;
        self->width_pixels = 0x0C;
        Follow(self, 0x38, -0x14);
        break;
    case 2: // loc_39D76
        Sst_MoveSprite(self);
        if ((uint16_t)self->y_pos.f.u >= 0x170) {
            self->routine_secondary += 2;
            self->y_pos.f.u = 0x170;
            WO(self, 0x2A) = 0x40;
        }
        break;
    case 4: // loc_39D9C
        if ((int16_t)--WO(self, 0x2A) >= 0)
            break;
        self->routine_secondary += 2;
        self->mappings = ObjectHit_Mappings();
        self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_ObjectHit);
        self->priority = 1 * 0x80;
        self->anim_frame_timer = 7;
        self->mapping_frame = 0;
        QueueSound2(sfx_Bomb);
        break;
    case 6: // loc_39DD8
        if ((int8_t)--self->anim_frame_timer >= 0)
            break;
        self->anim_frame_timer = 7;
        self->mapping_frame++;
        if (self->mapping_frame >= 5) {
            self->collision_flags = 0;
            if (self->mapping_frame == 7) {
                Sst_Delete(self);
                return;
            }
        }
        break;
    }
    Sst_Draw(self);
}

static void Debris(Sst *self) { // loc_39E02: a part that flew off the beaten body
    if ((int16_t)--WO(self, 0x2A) < 0) {
        Sst_Delete(self);
        return;
    }
    Sst_MoveSprite(self);
    Sst_Draw(self);
}

// loc_39822: the body once it is beaten and the camera has gone on: unseen, it goes along with Sonic, makes explosions near him, rumbles, and once he has come to $EC0 the screen goes white and the game goes on
// to the Sega screen (Beta 4's end)
static void Leaving(Sst *self) {
    if ((frame_count & 0x1F) == 0) {
        QueueSound2(sfx_Rumbling2);
        WO(self, 0x2A)--;
    }
    self->x_pos.f.u = (int16_t)(player->x_pos.f.u - (int16_t)WO(self, 0x2A));
    self->y_pos.f.u = player->y_pos.f.u;
    MakeExplosion(self);
    if (dhz_shake && --shake_timer < 0)
        dhz_shake = 0;
    switch (self->routine_secondary) {
    case 0: // loc_39862
        if ((uint16_t)player->x_pos.f.u < 0xEC0)
            return;
        self->routine_secondary += 2;
        TIMER = 0x16;
        break;
    case 2: // loc_39882
        if (--TIMER == 0) {
            for (int line = 0; line < 4; line++)
                for (int i = 0; i < 16; i++)
                    dry_palette[line][i] = 0x0EEE;
            gamemode = GameMode_Sega;
            Sst_Delete(self);
            return;
        }
        for (int line = 0; line < 4; line++) { // (every colour one step to white: Pal_AddColor2)
            for (int i = 0; i < 16; i++) {
                uint16_t v = dry_palette[line][i];
                if (v == 0x0EEE)
                    continue;
                if ((v & 0x00E) != 0x00E)
                    v += 0x002;
                else if ((v & 0x0E0) != 0x0E0)
                    v += 0x020;
                else if ((v & 0xE00) != 0xE00)
                    v += 0x200;
                dry_palette[line][i] = v;
            }
        }
        break;
    }
}

void Obj_DEZFinalBoss(Sst *self) {
    if (self->routine == R_Init) {
        // EnemySettings_02: the giant's mappings, art at $330, render 4, priority 4, width $38, no collision, then the routine is the subtype (the layout's body is 2)
        self->mappings = Mappings_DEZFinalBoss;
        self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_DEZFinalBoss);
        self->render_flags |= SST_RENDER_LEVEL;
        self->priority = 4 * 0x80;
        self->width_pixels = 0x38;
        self->collision_flags = 0;
        self->routine = self->subtype;
        return;
    }
    switch (self->routine) {
    case R_Body: Body(self); break;
    case R_Head:
        if (self->routine_secondary == 0) {
            self->routine_secondary += 2;
            self->mapping_frame = 4;
        }
        Follow(self, 0x0C, -0x14);
        Sst_Draw(self);
        break;
    case R_PartA: Picture(self, 0x0B, false); break;
    case R_HandA: Hand(self, 0x10); break;
    case R_PartB: Picture(self, 5, false); break;
    case R_PartC: Picture(self, 0x0A, false); break;
    case R_Eye: Eye(self); break;
    case R_Face: Face(self); break;
    case R_PartD: Picture(self, 0x0B, true); break;
    case R_HandB: Hand(self, 0x20); break;
    case R_PartE: Picture(self, 0x0A, true); break;
    case R_Marker: Marker(self); break;
    case R_Reticle: Reticle(self); break;
    case R_Bomb: Bomb(self); break;
    case R_Debris: Debris(self); break;
    case R_Leaving: Leaving(self); break;
    }
}
