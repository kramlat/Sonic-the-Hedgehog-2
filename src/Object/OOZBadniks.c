// Oil Ocean's badniks for Sonic 2, as the pre-beta has them: the Octus ($4A), an octopus that waits on the ground until Sonic is within $80 pixels each way, rises, puts out a puff and a shot, and goes off sideways;
// and the Aquis ($50), a seahorse that patrols under the surface of the oil (it follows the level's water), comes up and leaps when Sonic is above it, and spits a shot. The pre-beta's Aquis is not finished: its
// shot has a collision type that nothing handles, so it is harmless, and the pieces and the follower that it would make when hit are never made.
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"
#include "Object/OOZBadniks.h"
#include "Constants.h"

#include "Level.h"
#include "Object/Tails.h"
#include "LevelCollision.h"
#include "Object/Sonic.h"

#include "Macros.h"

#include "Resource/Animation/Aquis.h"
#include "Resource/Animation/Octus.h"
#include "Resource/Mappings/Aquis.h"
#include "Resource/Mappings/Octus.h"

typedef struct {
    uint8_t subtype;  // 0x28
    uint8_t pad0[3];  // 0x29-0x2B
    int16_t counter;  // 0x2C (Obj_Control_Var_00)
    int16_t home_y;   // 0x2E (the pre-beta keeps it in Obj_Timer): where it landed
} Own_Octus;

enum { OctusRoutine_Init = 0, OctusRoutine_Main = 2, OctusRoutine_Puff = 4, OctusRoutine_Shot = 6 };
enum { OctusState_Wait = 0, OctusState_Ready = 2, OctusState_Jump = 4, OctusState_Hang = 6, OctusState_Fall = 8 };
enum { OctusAnim_Idle = 0, OctusAnim_Ready = 3, OctusAnim_Leap = 4, OctusAnim_Shot = 2 };

// The shot (loc_28D10): an object of its own (the Octus' id) at the Octus' place, with its flips; it waits $F frames out of sight, and then flies at $200 the way the Octus faces
static void Octus_Shoot(Sst *parent) {
    Sst *shot = Sst_Allocate();
    if (shot == NULL)
        return;
    shot->code = parent->code;
    shot->routine = OctusRoutine_Shot;
    shot->mappings = parent->mappings;
    shot->art_tile = parent->art_tile;
    shot->priority = 4 * 0x80;
    shot->width_pixels = 0x10;
    shot->x_pos.f.u = parent->x_pos.f.u;
    shot->y_pos.f.u = parent->y_pos.f.u;
    ((Own_Octus *)shot->own.u8)->counter = 0x0F;
    shot->render_flags = parent->render_flags;
    shot->status = parent->status;
    shot->anim = OctusAnim_Shot;
    shot->collision_flags = 0x98;
    shot->x_vel = (shot->render_flags & SST_RENDER_X_FLIP) ? 0x200 : -0x200;
}

// Beta 4's Octus (loc_28BB4; the pre-beta's rose slowly and made a puff and a shot, and flew off sideways): it lands, waits until Sonic is within $80 either way, readies for half a second, leaps up at $200
// and falls back (gravity $10 a frame) with a shot as it turns, hangs a second, falls and lands where it was; and again
void Obj_Octus(Sst *self) {
    Own_Octus *own = (Own_Octus *)self->own.u8;

    switch (self->routine) {
    case OctusRoutine_Init:
        self->mappings = Mappings_Octus;
        self->art_tile = TILE_MAP(0, 1, 0, 0, 0x538); // ($A700)
        self->render_flags |= SST_RENDER_LEVEL;
        self->collision_flags = 0x0A;
        self->priority = 4 * 0x80;
        self->width_pixels = 0x10;
        self->y_radius = 0x0B;
        self->x_radius = 8;
        Sst_MoveSprite(self);
        {
            const int16_t dist = SstFloorDist(self, self->x_pos.f.u);
            if (dist < 0) { // it has landed: it faces Sonic
                self->y_pos.f.u += dist;
                self->y_vel = 0;
                self->routine += 2;
                if ((int16_t)(self->x_pos.f.u - player->x_pos.f.u) < 0)
                    self->status ^= SST_STATUS_X_FLIP;
            }
        }
        own->home_y = self->y_pos.f.u;
        return;

    case OctusRoutine_Main:
        switch (self->routine_secondary) {
        case OctusState_Wait: {
            const int16_t dx = (int16_t)(self->x_pos.f.u - player->x_pos.f.u);
            if (dx <= 0x80 && dx >= -0x80) {
                self->routine_secondary += 2;
                self->anim = OctusAnim_Ready;
                own->counter = 0x20;
            }
            break;
        }
        case OctusState_Ready:
            if (--own->counter < 0) {
                self->routine_secondary += 2;
                self->anim = OctusAnim_Leap;
                self->y_vel = -0x200;
                Sst_MoveSprite2(self);
            }
            break;
        case OctusState_Jump:
            self->y_vel += 0x10;
            if (self->y_vel >= 0) { // the top: it shoots
                self->routine_secondary += 2;
                own->counter = 0x3C;
                Octus_Shoot(self);
            } else {
                Sst_MoveSprite2(self);
            }
            break;
        case OctusState_Hang:
            if (--own->counter < 0)
                self->routine_secondary += 2;
            break;
        default: // OctusState_Fall
            self->y_vel += 0x10;
            if ((uint16_t)self->y_pos.f.u >= (uint16_t)own->home_y) { // it is down again
                self->routine_secondary = OctusState_Wait;
                self->anim = OctusAnim_Idle;
                self->y_vel = 0;
                self->mapping_frame = 1;
            } else {
                Sst_MoveSprite2(self);
            }
            break;
        }
        Sst_Animate(self, Animation_Octus);
        SstOnScreenTest(self);
        return;

    case OctusRoutine_Puff: // (no longer made: the end of its short life)
        if (--own->counter == 0) {
            Sst_Delete(self);
            return;
        }
        Sst_Draw(self);
        return;

    case OctusRoutine_Shot:
        if (--own->counter >= 0)
            return;
        Sst_MoveSprite2(self);
        Sst_Animate(self, Animation_Octus);
        SstOnScreenTest(self);
        return;
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// The Aquis ($50), Beta 4's (loc_28FCC; the pre-beta's patrolled under the oil's surface and leapt): a seahorse that hovers, and when it is on the screen goes after the nearer character with the pull of a Whisp (a
// speed of up to $100 each way) for a while, stops, spits a shot down at him if he is below it, and starts again. A fin (an object of its own) rides on its back. The animations are 0 hover, 1 not used, 2 the
// shot, 3 the fin
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype;     // 0x28
    uint8_t pad0[4];     // 0x29-0x2C
    uint8_t shot_made;   // 0x2D: the shot of this stop has been made
    uint8_t pad1[8];     // 0x2E-0x35
    uint8_t link;        // 0x36: the slot of the fin (and the fin's of the body)
    uint8_t pad2[5];     // 0x37-0x3B
    int8_t timer;        // 0x3C
} Own_Aquis;

enum { AquisRoutine_Init = 0, AquisRoutine_Main = 2, AquisRoutine_Fin = 4, AquisRoutine_Shot = 6 };
enum { AquisState_Wait = 0, AquisState_Chase = 2, AquisState_Stop = 4 };
enum { AquisAnim_Hover = 0, AquisAnim_Shot = 2, AquisAnim_Fin = 3 };

static Sst *Aquis_Child(Sst *parent, uint8_t routine, uint8_t priority) {
    Sst *child = Sst_Allocate();
    if (child == NULL)
        return NULL;
    child->code = parent->code;
    child->routine = routine;
    child->mappings = parent->mappings;
    child->art_tile = TILE_MAP(0, 1, 0, 0, 0x500); // ($A000)
    child->render_flags = SST_RENDER_LEVEL;
    child->priority = priority * 0x80;
    return child;
}

// loc_32866: the character nearer in x (the first on a tie)
static Sst *Aquis_Nearest(const Sst *self, int *side, int *below) {
    Sst *chr = player;
    int16_t dx = (int16_t)(self->x_pos.f.u - player->x_pos.f.u);
    if (TAILS_OBJ->code != NULL) {
        const int16_t dx2 = (int16_t)(self->x_pos.f.u - TAILS_OBJ->x_pos.f.u);
        if ((dx < 0 ? -dx : dx) > (dx2 < 0 ? -dx2 : dx2)) {
            chr = TAILS_OBJ;
            dx = dx2;
        }
    }
    *side = dx < 0 ? 2 : 0;
    *below = (uint16_t)self->y_pos.f.u < (uint16_t)chr->y_pos.f.u ? 2 : 0;
    return chr;
}

// loc_29212: the fin is a little behind and above its body, with its facing
static void Aquis_PlaceFin(Sst *self, Own_Aquis *own) {
    if (own->link == 0)
        return;
    Sst *fin = &sst[own->link];
    int16_t dx = 0x0A;
    fin->x_pos.f.u = self->x_pos.f.u;
    fin->y_pos.f.u = self->y_pos.f.u;
    fin->status = self->status;
    fin->respawn = self->respawn;
    fin->render_flags = self->render_flags;
    if (fin->status & SST_STATUS_X_FLIP)
        dx = -dx;
    fin->x_pos.f.u = (int16_t)(fin->x_pos.f.u + dx);
    fin->y_pos.f.u = (int16_t)(fin->y_pos.f.u - 6);
}

// loc_29120: at a stop, once, it spits a shot down at a character below it
static void Aquis_Spit(Sst *self, Own_Aquis *own) {
    if (own->shot_made)
        return;
    own->shot_made = 0xFF;
    int side, below;
    Aquis_Nearest(self, &side, &below);
    if (below == 0)
        return;
    Sst *shot = Aquis_Child(self, AquisRoutine_Shot, 3);
    if (shot == NULL)
        return;
    shot->x_pos.f.u = self->x_pos.f.u;
    shot->y_pos.f.u = self->y_pos.f.u;
    shot->collision_flags = 0x98;
    shot->anim = AquisAnim_Shot;
    int16_t dx = 0x10, speed = -0x300;
    if (self->status & SST_STATUS_X_FLIP) {
        dx = -dx;
        speed = -speed;
    }
    shot->y_pos.f.u = (int16_t)(shot->y_pos.f.u - 0x0A);
    shot->x_pos.f.u = (int16_t)(shot->x_pos.f.u - dx);
    shot->x_vel = speed;
    shot->y_vel = 0x200;
}

void Obj_Aquis(Sst *self) {
    Own_Aquis *own = (Own_Aquis *)self->own.u8;

    switch (self->routine) {
    case AquisRoutine_Init: {
        self->routine += 2;
        self->mappings = Mappings_Aquis;
        self->art_tile = TILE_MAP(0, 1, 0, 0, 0x500);
        self->render_flags |= SST_RENDER_LEVEL;
        self->collision_flags = 0x0A;
        self->priority = 4 * 0x80;
        self->width_pixels = 0x10;
        self->x_vel = -0x100;
        Sst *fin = Aquis_Child(self, AquisRoutine_Fin, 3);
        if (fin != NULL) {
            fin->x_pos.f.u = (int16_t)(self->x_pos.f.u + 0x0A);
            fin->y_pos.f.u = (int16_t)(self->y_pos.f.u - 6);
            fin->status = self->status;
            fin->anim = AquisAnim_Fin;
            own->link = (uint8_t)(fin - sst);
            ((Own_Aquis *)fin->own.u8)->link = (uint8_t)(self - sst);
            self->status |= 0x40;
        }
    }
        // Fallthrough
    case AquisRoutine_Main:
        Sst_Animate(self, Animation_Aquis);
        switch (self->routine_secondary) {
        case AquisState_Wait: // until it is on the screen
            if (self->render_flags & SST_RENDER_ON_SCREEN)
                self->routine_secondary += 2;
            break;
        case AquisState_Chase:
            if (--own->timer < 0) { // (the first time at once: the timer starts at zero) it stops
                self->routine_secondary += 2;
                own->timer = 0x20;
                self->x_vel = 0;
                self->y_vel = 0;
            } else {
                int side, below;
                Aquis_Nearest(self, &side, &below);
                SST_STATUS_SET(self, SST_STATUS_X_FLIP, side != 0);
                self->x_vel = (int16_t)(self->x_vel + (side ? 0x10 : -0x10));
                self->y_vel = (int16_t)(self->y_vel + (below ? 0x10 : -0x10));
                if (self->x_vel > 0x100)
                    self->x_vel = 0x100;
                else if (self->x_vel < -0x100)
                    self->x_vel = -0x100;
                if (self->y_vel > 0x100)
                    self->y_vel = 0x100;
                else if (self->y_vel < -0x100)
                    self->y_vel = -0x100;
                Sst_MoveSprite2(self);
            }
            break;
        default: // AquisState_Stop
            if (--own->timer < 0) { // it goes on again: up a little, and the shot is made anew
                self->routine_secondary -= 2;
                self->y_vel = -0x100;
                own->timer = (int8_t)0x80;
                own->shot_made = 0;
            }
            Aquis_Spit(self, own);
            break;
        }
        Aquis_PlaceFin(self, own);
        SstOnScreenTest(self);
        return;

    case AquisRoutine_Fin: { // it goes when its body does
        Sst *body = &sst[own->link];
        if (own->link == 0 || body->code != self->code || (body->status & 0x80)) {
            Sst_Delete(self);
            return;
        }
        Sst_Animate(self, Animation_Aquis);
        Sst_Draw(self);
        return;
    }

    case AquisRoutine_Shot:
        Sst_MoveSprite2(self);
        Sst_Animate(self, Animation_Aquis);
        SstOnScreenTest(self);
        return;
    }
}
