// Oil Ocean's block that is broken for a boost (object 3D of the pre-beta): a block a character stands on rolled up breaks into sixteen pieces and he is sent off along it, at $800 across (the horizontal kind, subtype 0)
// or up (the vertical, any other subtype), carried by an invisible object of its own (the tracker, a copy of the block in routine 6) that moves him on his speed until another object takes hold of him (the cannon does).
#include "Standing.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"
#include "Object/OOZBreakBoost.h"
#include "Constants.h"

#include "Level.h"
#include "Object/CharControl.h"
#include "Object/CPZObjects.h"
#include "Object/Sonic.h"
#include "Object/Tails.h"
#include "Solid.h"
#include "Sound.h"

#include "Macros.h"

#include "Resource/Mappings/OOZBreakBoost.h"

#define FOR_EACH_CHARACTER(chr, who) \
    for (int who = SolidChar_Sonic; who <= SolidChar_Tails; who++) \
        for (Sst *chr = (who == SolidChar_Sonic) ? player : (TAILS_OBJ->code != NULL ? TAILS_OBJ : NULL); chr != NULL; chr = NULL)

// The block's own memory; the tracker (a copy of the first $2C bytes, so its own memory starts clear) keeps what each character is doing at the same places as the cannon does ($2C and $36), which is where the cannon
// looks when it takes a character from it
typedef struct {
    uint8_t subtype;      // 0x28
    uint8_t pad0[3];      // 0x29-0x2B
    uint8_t state_sonic;  // 0x2C: (the tracker's) 0 free, 2 carrying him
    uint8_t pad1[3];      // 0x2D-0x2F
    uint8_t anim[2];      // 0x30, 0x31: (the block's) the animation each character had when he came
    int16_t y_vel[2];     // 0x32, 0x34: and his vertical speed
    uint8_t state_tails;  // 0x36: (the tracker's)
} Own_BreakBoost;

enum { BreakBoost_Init = 0, BreakBoost_Block = 2, BreakBoost_Piece = 4, BreakBoost_Tracker = 6 };
enum { Carry_Free = 0, Carry_Flying = 2 };

// Break_Boost_Data: the speed of each of the sixteen pieces
static const int16_t break_boost_pieces[16][2] = {
    { -0x400, -0x400 }, { -0x200, -0x400 }, { 0x200, -0x400 }, { 0x400, -0x400 },
    { -0x3C0, -0x200 }, { -0x1C0, -0x200 }, { 0x1C0, -0x200 }, { 0x3C0, -0x200 },
    { -0x380, 0x200 }, { -0x180, 0x200 }, { 0x180, 0x200 }, { 0x380, 0x200 },
    { -0x340, 0x400 }, { -0x140, 0x400 }, { 0x140, 0x400 }, { 0x340, 0x400 },
};

// Offset_0x01F91A and the two after it: the character comes off the block (in the air, on nothing, in his normal routine); a rolling one keeps rolling, and the speed he had before he landed
static void BreakBoost_Release(Sst *chr, bool rolling, int16_t y_vel) {
    if (rolling) {
        chr->status |= SST_PSTATUS_IN_BALL;
        chr->y_radius = 0x0E;
        chr->x_radius = 7;
        chr->anim = SonAnimId_Roll;
        chr->y_vel = y_vel;
    }
    chr->status |= SST_PSTATUS_IN_AIR;
    chr->status &= (uint8_t)~SST_PSTATUS_OBJECT_STAND;
    chr->routine = 2;
}

// Offset_0x01F9EC (state 0) and Offset_0x01FA8E (state 2) of the tracker, for one character
static void BreakBoost_Track(Sst *self, uint8_t *state, Sst *chr) {
    if (*state == Carry_Flying) {
        chr->x_pos.v += (int32_t)chr->x_vel << 8;
        chr->y_pos.v += (int32_t)chr->y_vel << 8;
        return;
    }
    if ((uint16_t)(chr->x_pos.f.u - self->x_pos.f.u + 0x10) >= 0x20)
        return;
    int16_t dy = (int16_t)(chr->y_pos.f.u - self->y_pos.f.u);
    if (self->subtype != 0)
        dy += 0x10;
    if ((uint16_t)dy >= 0x10)
        return;
    *state += 2;
    OBJ_CONTROL(chr) = 0x81;
    chr->anim = SonAnimId_Roll;
    chr->ground_vel = 0x800;
    if (self->subtype != 0) { // the vertical block: straight up from its middle
        chr->x_pos.f.u = self->x_pos.f.u;
        chr->x_vel = 0;
        chr->y_vel = -0x800;
    } else {
        chr->y_pos.f.u = self->y_pos.f.u;
        chr->x_vel = 0x800;
        chr->y_vel = 0;
    }
    self->status &= (uint8_t)~0x20; // (bit 5 for either, as the cannon does)
    chr->status &= (uint8_t)~SST_PSTATUS_PUSHING;
    chr->status |= SST_PSTATUS_IN_AIR | SST_PSTATUS_OBJECT_STAND;
    ((Scratch_Sonic *)chr->own.u8)->standing_obj = Standing_IdOfSst(self);
    PlaySound(sfx_Roll);
}

void Obj_OOZBreakBoost(Sst *self) {
    Own_BreakBoost *own = (Own_BreakBoost *)self->own.u8;

    switch (self->routine) {
    case BreakBoost_Init:
        self->routine += 2;
        self->mappings = Mappings_OOZBreakBoost;
        self->art_tile = TILE_MAP(0, 3, 0, 0, 0x332);
        if (self->subtype != 0) {
            self->art_tile = TILE_MAP(0, 3, 0, 0, 0x3FF);
            self->mapping_frame = 2;
        }
        self->render_flags = SST_RENDER_LEVEL; // (move.b #4: the layout's flips are cleared)
        self->width_pixels = 0x10;
        self->status |= 0x80;
        self->priority = 4 * 0x80;
        // Fallthrough
    case BreakBoost_Block: {
        // what each character was doing before the block meets him: it is what he keeps when he breaks it
        own->anim[0] = player->anim;
        own->anim[1] = TAILS_OBJ->anim;
        own->y_vel[0] = player->y_vel;
        own->y_vel[1] = TAILS_OBJ->y_vel;
        FOR_EACH_CHARACTER(chr, who)
            Solid_Character(self, chr, who, 0x1B, 0x10, 0x11, self->x_pos.f.u, NULL);
        const uint8_t stand = self->status & 0x18;
        const bool sonic_rolls = own->anim[0] == SonAnimId_Roll, tails_rolls = own->anim[1] == SonAnimId_Roll;
        if (stand == 0x18) { // both are on it: if either rolls, it breaks, and both come off
            if (!sonic_rolls && !tails_rolls) {
                SstOnScreenTest(self);
                return;
            }
            BreakBoost_Release(player, sonic_rolls, own->y_vel[0]);
            BreakBoost_Release(TAILS_OBJ, tails_rolls, own->y_vel[1]);
        } else if (stand & 0x08) {
            if (!sonic_rolls) {
                SstOnScreenTest(self);
                return;
            }
            BreakBoost_Release(player, true, own->y_vel[0]);
        } else if (stand & 0x10) {
            if (!tails_rolls) {
                SstOnScreenTest(self);
                return;
            }
            BreakBoost_Release(TAILS_OBJ, true, own->y_vel[1]);
        } else {
            SstOnScreenTest(self);
            return;
        }
        self->status &= 0xE7;
        Sst *tracker = Sst_AllocateAfter(self);
        if (tracker != NULL) {
            tracker->code = self->code;
            tracker->x_pos.f.u = self->x_pos.f.u;
            tracker->y_pos.f.u = self->y_pos.f.u;
            tracker->subtype = self->subtype;
            tracker->status = self->status;
            tracker->render_flags = self->render_flags;
            tracker->routine = BreakBoost_Tracker;
            tracker->respawn = self->respawn;
        }
        self->mapping_frame++;
        ObjectBreakToPieces(self, break_boost_pieces, 16);
        // it goes on as its first piece, this very frame
        self->routine = BreakBoost_Piece;
        Sst_MoveSprite2(self);
        self->y_vel += 0x18;
        if (!(self->render_flags & SST_RENDER_ON_SCREEN))
            Sst_Delete(self);
        else
            Sst_Draw(self);
        return;
    }
    case BreakBoost_Piece:
        Sst_MoveSprite2(self);
        self->y_vel += 0x18;
        if (!(self->render_flags & SST_RENDER_ON_SCREEN))
            Sst_Delete(self);
        else
            Sst_Draw(self);
        return;

    case BreakBoost_Tracker:
        BreakBoost_Track(self, &own->state_sonic, player);
        if (TAILS_OBJ->code != NULL)
            BreakBoost_Track(self, &own->state_tails, TAILS_OBJ);
        if (own->state_sonic + own->state_tails == 0)
            SstDeleteIfNotInRange(self);
        return;
    }
}
