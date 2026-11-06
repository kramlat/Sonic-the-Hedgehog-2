// Oil Ocean's fans (object 3F of the pre-beta): a fan that spins, and for a while at full speed blows a character who stands in front of it along (subtype bit 7 clear: a vertical fan that pushes sideways, bit 0 which way;
// bit 7 set: a horizontal fan that lifts, with a flip in the air). Every 120 frames it spins up (and then idles for 180); with subtype bit 1 it blows all the time.
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"
#include "Object/OOZFans.h"
#include "Constants.h"

#include "Level.h"
#include "Object/Sonic.h"
#include "Object/Tails.h"
#include "Oscillatory Routines.h"
#include "Solid.h"

#include "Macros.h"

#include "Resource/Mappings/OOZFanHorizontal.h"
#include "Resource/Mappings/OOZFanVertical.h"

typedef struct {
    uint8_t subtype;   // 0x28
    uint8_t pad0[7];   // 0x29-0x2F
    int16_t timer;     // 0x30 (Obj_Control_Var_04): frames until it changes
    uint8_t spinning;  // 0x32 (Var_06): 0 it blows, 1 it spins up
    uint8_t pad1;      // 0x33
    int16_t speed;     // 0x34 (Var_08): how fast it has spun up
} Own_Fan;

enum { FanRoutine_Init = 0, FanRoutine_Vertical = 2, FanRoutine_Horizontal = 4 };

// Offset_0x025400: the character in front of a vertical fan (within $78 either way sideways and from $60 above the fan's middle to $10 under it) is moved along, more the nearer the fan's middle
static void Fan_PushSideways(Sst *self, Sst *chr) {
    int16_t d0 = (int16_t)(chr->x_pos.f.u - self->x_pos.f.u);
    if (!(self->status & SST_STATUS_X_FLIP))
        d0 = (int16_t)-d0;
    d0 = (int16_t)(d0 + 0x50);
    if ((uint16_t)d0 >= 0xF0)
        return;
    const uint16_t top = (uint16_t)(chr->y_pos.f.u + 0x60), fan = (uint16_t)self->y_pos.f.u;
    if (top < fan || (uint16_t)(top - fan) >= 0x70)
        return;
    const bool borrow = (uint16_t)d0 < 0x50;
    d0 = (int16_t)(d0 - 0x50);
    if (borrow) {
        d0 = (int16_t)~d0;
        d0 = (int16_t)(d0 + d0);
    }
    d0 = (int16_t)(d0 + 0x60);
    if (self->status & SST_STATUS_X_FLIP)
        d0 = (int16_t)-d0;
    d0 = (int16_t)(d0 >> 4);
    if (self->subtype & 1)
        d0 = (int16_t)-d0;
    chr->x_pos.f.u = (int16_t)(chr->x_pos.f.u + d0);
}

// Offset_0x0254EE: the character over a horizontal fan (within $40 either way sideways and up to $90 above it, the oscillation moving the reach) is lifted, and tumbles
static void Fan_PushUp(Sst *self, Sst *chr) {
    const int16_t d0 = (int16_t)(chr->x_pos.f.u - self->x_pos.f.u + 0x40);
    if ((uint16_t)d0 >= 0x80)
        return;
    const uint16_t osc = (uint8_t)(oscillatory.state[5][0] >> 8);
    const uint16_t top = (uint16_t)(osc + chr->y_pos.f.u + 0x60), fan = (uint16_t)self->y_pos.f.u;
    if (top < fan)
        return;
    uint16_t d1 = (uint16_t)(top - fan);
    if (d1 >= 0x90)
        return;
    const bool borrow = d1 < 0x60;
    d1 = (uint16_t)(d1 - 0x60);
    if (!borrow) {
        d1 = (uint16_t)~d1;
        d1 = (uint16_t)(d1 + d1);
    }
    d1 = (uint16_t)(d1 + 0x60);
    d1 = (uint16_t)-(int16_t)d1;
    chr->y_pos.f.u = (int16_t)(chr->y_pos.f.u + ((int16_t)d1 >> 4));
    chr->status |= SST_PSTATUS_IN_AIR;
    chr->y_vel = 0;
    chr->ground_vel = 1;
    Scratch_Sonic *sscratch = (Scratch_Sonic *)chr->own.u8;
    if (sscratch->flip_angle == 0) {
        sscratch->flip_angle = 1;
        chr->anim = SonAnimId_Walk;
        sscratch->flips_remaining = 0x7F;
        sscratch->flip_speed = 8;
    }
}

void Obj_OOZFan(Sst *self) {
    Own_Fan *own = (Own_Fan *)self->own.u8;

    if (self->routine == FanRoutine_Init) {
        self->routine = FanRoutine_Vertical;
        self->mappings = Mappings_OOZFanVertical;
        self->art_tile = TILE_MAP(0, 3, 0, 0, 0x403);
        self->render_flags |= SST_RENDER_LEVEL;
        self->width_pixels = 0x10;
        self->priority = 4 * 0x80;
        if (self->subtype & 0x80) { // the horizontal fan, and its first frame at once
            self->routine = FanRoutine_Horizontal;
            self->mappings = Mappings_OOZFanHorizontal;
        }
    }

    if (!(self->subtype & 2)) { // it spins up for 120 frames and blows for 180, round and round
        if (--own->timer < 0) {
            own->speed = 0;
            own->timer = 0x78;
            const uint8_t before = own->spinning;
            own->spinning ^= 1;
            if (before)
                own->timer = 0xB4;
        }
    }

    bool animate = false;
    if (own->spinning) { // spinning up: slower and slower between frames of its picture, up to a speed
        if (--self->anim_frame_timer < 0 && (uint16_t)own->speed < 0x400) {
            own->speed = (int16_t)(own->speed + 0x2A);
            self->anim_frame_timer = (int8_t)(uint8_t)((uint16_t)own->speed >> 8); // (move.b Var_08: the word's first byte)
            animate = true;
        }
    } else { // it blows, and goes round at full speed
        for (int who = SolidChar_Sonic; who <= SolidChar_Tails; who++) {
            Sst *chr = who == SolidChar_Sonic ? player : (TAILS_OBJ->code != NULL ? TAILS_OBJ : NULL);
            if (chr == NULL)
                continue;
            if (self->routine == FanRoutine_Horizontal)
                Fan_PushUp(self, chr);
            else
                Fan_PushSideways(self, chr);
        }
        if (--self->anim_frame_timer < 0) {
            self->anim_frame_timer = 0;
            animate = true;
        }
    }
    if (animate) {
        if (++self->anim_frame >= 6)
            self->anim_frame = 0;
        self->mapping_frame = (uint8_t)(((self->subtype & 1) ? 5 : 0) + self->anim_frame);
    }
    SstOnScreenTest(self);
}
