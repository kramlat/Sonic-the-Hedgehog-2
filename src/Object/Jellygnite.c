// The Jellygnite (object $E0): the blue bell-shaped badnik of Hidden Palace (the Sonic 2 (2013) sheet: three bell frames with tentacles). It lives in the water: IN WATER it chases Sonic slowly, in bursts (it pushes toward him
// and then drifts to a stop, as a jellyfish pulses: a pulse every $40 frames when he is within $C0 across and $80 up or down); OUT OF WATER it does not move itself and falls (the user: "only moves in water",
// "otherwise follows gravity"). When Sonic comes within $60 across and $40 up or down (the Asteron's range) it starts a three seconds' count (the user: "Sonic should have 3 seconds"): it can be destroyed as
// any badnik during it (a spin destroys it: "if Sonic is fast enough"), and when the count ends it explodes like the Asteron (the starfish) but with no shrapnel: an explosion that hurts a character only at close quarters
// ($20 across and $28 up or down). STILL A STUB for the art: no sheet yet. When it comes: the sheet's green is replaced with BLUE when it is drawn (the user), and its colours are to come from the silver ramps of palette
// line 0 (the user). Subtype: unused so far.
#include "Object/Jellygnite.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"
#include "Constants.h"

#include "Level.h"
#include "MathUtil.h"
#include "VersusStats.h"
#include "Object/Sonic.h"
#include "Object/Tails.h"

typedef struct {
    uint8_t subtype;   // 0x28
    uint8_t pad[7];    // 0x29-0x2F
    uint16_t timer;    // 0x30: frames until it explodes (once the count has begun)
    uint8_t pulse;     // 0x32: where it is in its burst
} Own_Jellygnite;

enum { JellyRoutine_Init = 0, JellyRoutine_Wait = 2, JellyRoutine_Count = 4 };
#define JELLY_COUNT_FRAMES (3 * 60) // three seconds
#define JELLY_PULSE_FRAMES 0x40
#define JELLY_PUSH 0x180 // (a push: 1.5 pixels a frame, which decays by a sixteenth of itself each frame)

extern int16_t wtr_pos1;

// The explosion: it becomes an explosion (no animal: object $27) and hurts whoever is at close quarters, once, as it goes off
static void Jelly_Explode(Sst *self) {
    self->code = sst_object_codes[0x27];
    self->routine = 2;
    for (int who = 0; who < 2; who++) {
        Sst *chr = who == 0 ? player : (TAILS_OBJ->code != NULL ? TAILS_OBJ : NULL);
        if (chr == NULL || chr->routine >= 4 || VsStats_Has(chr, VS_INVINCIBLE))
            continue;
        const int16_t dx = (int16_t)(chr->x_pos.f.u - self->x_pos.f.u), dy = (int16_t)(chr->y_pos.f.u - self->y_pos.f.u);
        if ((uint16_t)(dx + 0x20) >= 0x40 || (uint16_t)(dy + 0x28) >= 0x50)
            continue;
        if (who == 0)
            HurtSonicFrom(chr, self->x_pos.f.u, false);
        else
            Tails_Hurt(chr, self);
    }
}

static bool Jelly_InWater(const Sst *self) {
    return Level_HasWater() && self->y_pos.f.u >= wtr_pos1;
}

static void Jelly_SetFlip(Sst *self, bool flip) {
    SST_RENDER_SET(self, SST_RENDER_X_FLIP, flip);
    SST_STATUS_SET(self, SST_STATUS_X_FLIP, flip);
}

void Obj_Jellygnite(Sst *self) {
    Own_Jellygnite *own = (Own_Jellygnite *)self->own.u8;
    switch (self->routine) {
    case JellyRoutine_Init:
        self->render_flags |= SST_RENDER_LEVEL;
        self->collision_flags = 0xA; // (a badnik's, as the BBat has: Sonic's spin destroys it, anything else hurts)
        self->priority = 4 * 0x80;
        self->width_pixels = 0x10;
        self->y_radius = 0x10;
        self->x_radius = 8;
        self->routine = JellyRoutine_Wait;
        break;
    case JellyRoutine_Wait:
    case JellyRoutine_Count: {
        const int16_t dx = (int16_t)(player->x_pos.f.u - self->x_pos.f.u), dy = (int16_t)(player->y_pos.f.u - self->y_pos.f.u);
        if (Jelly_InWater(self)) {
            // a burst: at the start of each pulse it pushes toward Sonic (when he is near enough to notice), and the push dies away
            if (own->pulse == 0 && (uint16_t)(dx + 0xC0) < 0x180 && (uint16_t)(dy + 0x80) < 0x100) {
                self->x_vel = (int16_t)(dx > 8 ? JELLY_PUSH : dx < -8 ? -JELLY_PUSH : 0);
                self->y_vel = (int16_t)(dy > 8 ? JELLY_PUSH / 2 : dy < -8 ? -JELLY_PUSH / 2 : 0);
                if (self->x_vel != 0)
                    Jelly_SetFlip(self, self->x_vel < 0);
            }
            own->pulse = (uint8_t)((own->pulse + 1) % JELLY_PULSE_FRAMES);
            self->x_vel -= (int16_t)(self->x_vel >> 4);
            self->y_vel -= (int16_t)(self->y_vel >> 4);
            Sst_MoveSprite2(self); // (no gravity in the water)
        } else {
            self->x_vel = 0;
            Sst_MoveSprite(self); // (out of the water it falls)
        }
        if (self->routine == JellyRoutine_Wait) {
            if ((uint16_t)(dx + 0x60) < 0xC0 && (uint16_t)(dy + 0x40) < 0x80) { // (Sonic is near: the count begins)
                self->routine = JellyRoutine_Count;
                own->timer = JELLY_COUNT_FRAMES;
            }
        } else if (--own->timer == 0) {
            Jelly_Explode(self); // (it explodes, hurting only at close quarters)
        }
        SstOnScreenTest(self);
        break;
    }
    }
}
