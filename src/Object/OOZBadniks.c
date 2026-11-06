// Oil Ocean's badniks for Sonic 2, as the pre-beta has them: the Octus ($4A), an octopus that waits on the ground until Sonic is within $80 pixels each way, rises, puts out a puff and a shot, and goes off sideways;
// and the Aquis ($50), a seahorse that patrols under the surface of the oil (it follows the level's water), comes up and leaps when Sonic is above it, and spits a shot. The pre-beta's Aquis is not finished: its
// shot has a collision type that nothing handles, so it is harmless, and the pieces and the follower that it would make when hit are never made.
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"
#include "Object/OOZBadniks.h"
#include "Constants.h"

#include "Level.h"
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
enum { OctusState_Wait = 0, OctusState_Rise = 2, OctusState_Fire = 4, OctusState_Leave = 6 };
enum { OctusAnim_Idle = 0, OctusAnim_Rise = 1, OctusAnim_Shot = 2 };

// The puff and the shot: objects of their own (the Octus' id), made at the Octus' place with its flips
static Sst *Octus_Child(Sst *parent, uint8_t routine, uint8_t priority) {
    Sst *child = Sst_Allocate();
    if (child == NULL)
        return NULL;
    child->code = parent->code;
    child->routine = routine;
    child->mappings = parent->mappings;
    child->art_tile = parent->art_tile;
    child->priority = priority * 0x80;
    child->width_pixels = 0x10;
    child->x_pos.f.u = parent->x_pos.f.u;
    child->y_pos.f.u = parent->y_pos.f.u;
    child->render_flags = parent->render_flags;
    child->status = parent->status;
    return child;
}

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
        self->y_radius = 0x10;
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
                self->anim = OctusAnim_Rise;
            }
            break;
        }
        case OctusState_Rise:
            self->y_pos.v -= 0x18000;
            if ((int16_t)(own->home_y - self->y_pos.f.u) > 0x20) {
                self->routine_secondary += 2;
                own->counter = 0;
            }
            break;
        case OctusState_Fire:
            if (--own->counter == 0) {
                self->routine_secondary += 2;
            } else if (own->counter < 0) {
                own->counter = 0x1E;
                Sst *puff = Octus_Child(self, OctusRoutine_Puff, 3);
                if (puff != NULL) {
                    puff->mapping_frame = 4;
                    ((Own_Octus *)puff->own.u8)->counter = 0x1E;
                }
                Sst *shot = Octus_Child(self, OctusRoutine_Shot, 4);
                if (shot != NULL) { // (the pre-beta gives it no collision)
                    ((Own_Octus *)shot->own.u8)->counter = 0x0F;
                    shot->anim = OctusAnim_Shot;
                    shot->x_vel = (self->render_flags & SST_RENDER_X_FLIP) ? 0x580 : -0x580;
                }
            }
            break;
        case OctusState_Leave:
            self->x_pos.f.u += (self->render_flags & SST_RENDER_X_FLIP) ? 6 : -6;
            break;
        }
        Sst_Animate(self, Animation_Octus);
        SstOnScreenTest(self);
        return;

    case OctusRoutine_Puff:
        if (--own->counter == 0) {
            Sst_Delete(self);
            return;
        }
        Sst_Draw(self);
        return;

    case OctusRoutine_Shot:
        if (--own->counter >= 0)
            return;
        Sst_MoveSprite(self);
        Sst_Animate(self, Animation_Octus);
        SstOnScreenTest(self);
        return;
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// The Aquis ($50)
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype;     // 0x28
    uint8_t pad0;        // 0x29
    int16_t home_y;      // 0x2A (Obj_Timer): where it sinks to
    uint8_t at_surface;  // 0x2C (Var_00): 0 it is going up to the surface, $FF it is at the surface
    uint8_t leapt;       // 0x2D (Var_01): it has spat its shot in this leap
    int16_t wait;        // 0x2E (Var_02): frames at the surface (or at the bottom)
    int16_t wait_reset;  // 0x30 (Var_04)
    int16_t turn;        // 0x32 (Var_06): frames until it turns round
    int16_t turn_reset;  // 0x34 (Var_08)
    uint8_t link;        // 0x36 (Var_0A): the slot of the fin (and of the body, for the fin)
    uint8_t pad1;        // 0x37
    uint8_t pad2[2];     // 0x38-0x39
    int16_t water;       // 0x3A (Var_0E): the level of the water as it was last frame
} Own_Aquis;

// (the pieces and the follower: Var_00 is a word, with a flash timer after it)
typedef struct {
    uint8_t subtype;   // 0x28
    uint8_t pad0[3];   // 0x29-0x2B
    int16_t counter;   // 0x2C
    uint8_t flash;     // 0x2E (Obj_Timer)
} Own_AquisTrail;

enum { AquisRoutine_Init = 0, AquisRoutine_Main = 2, AquisRoutine_Fin = 4, AquisRoutine_Shot = 6, AquisRoutine_Piece = 8, AquisRoutine_Follow = 0xA };
enum { AquisState_Patrol = 0, AquisState_Move = 2, AquisState_Leap = 4 };
enum { AquisAnim_Swim = 0, AquisAnim_Leap = 1, AquisAnim_Shot = 2, AquisAnim_Fin = 3, AquisAnim_Piece = 4, AquisAnim_Follow = 5 };

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

// Offset_0x028210: it rides the water's level
static void Aquis_FollowWater(Sst *self, Own_Aquis *own) {
    self->y_pos.f.u = (int16_t)(self->y_pos.f.u - (int16_t)(own->water - wtr_pos1));
    own->water = wtr_pos1;
}

// Offset_0x02848A: it turns round after a while
static void Aquis_Turn(Sst *self, Own_Aquis *own) {
    if (--own->turn >= 0)
        return;
    own->turn = own->turn_reset;
    self->x_vel = (int16_t)-self->x_vel;
    self->status ^= SST_STATUS_X_FLIP;
    self->prev_anim = 1;
}

// Offset_0x0282C8: at the surface it leaps at a Sonic who is above it, in front of it, between $28 and $58 away
static void Aquis_LookForSonic(Sst *self, Own_Aquis *own) {
    if (!own->at_surface)
        return;
    const int16_t dy = (int16_t)(player->y_pos.f.u - self->y_pos.f.u);
    if (dy >= 0 || dy < -0x40)
        return;
    const int16_t dx = (int16_t)(player->x_pos.f.u - self->x_pos.f.u);
    if (dx > 0x58 || dx < -0x58)
        return;
    if (dx >= 0) {
        if (dx < 0x28 || !(self->status & SST_STATUS_X_FLIP))
            return;
    } else {
        if (dx > -0x28 || (self->status & SST_STATUS_X_FLIP))
            return;
    }
    if (0x20 > own->turn)
        return;
    self->routine_secondary = AquisState_Leap;
    self->anim = AquisAnim_Leap;
    self->y_vel = -0x400;
}

// Offset_0x028330: it goes up to the surface, or sinks again, after a wait
static void Aquis_StartMoving(Sst *self, Own_Aquis *own) {
    if (--own->wait != 0)
        return;
    own->wait = own->wait_reset;
    self->routine_secondary += 2;
    self->y_vel = own->at_surface ? 0x40 : -0x40;
}

// Offset_0x028352: ... until it is at the surface or at the bottom
static void Aquis_Arrive(Sst *self, Own_Aquis *own) {
    if (!own->at_surface) {
        if (self->y_pos.f.u > wtr_pos1)
            return;
        self->routine_secondary -= 2;
        own->at_surface = 0xFF;
        self->y_vel = 0;
    } else {
        if (self->y_pos.f.u < own->home_y)
            return;
        self->routine_secondary -= 2;
        own->at_surface = 0;
        self->y_vel = 0;
    }
}

// Offset_0x028224: at the top of its leap it spits a shot, one for each leap
static void Aquis_Spit(Sst *self, Own_Aquis *own) {
    if (own->leapt || self->y_vel < 0)
        return;
    own->leapt = 0xFF;
    Sst *shot = Aquis_Child(self, AquisRoutine_Shot, 3);
    if (shot == NULL)
        return;
    shot->render_flags |= SST_RENDER_LEVEL;
    shot->collision_flags = 0xE5;
    shot->anim = AquisAnim_Shot;
    int16_t dx = 0x10, speed = -0x300;
    if (self->status & SST_STATUS_X_FLIP) {
        dx = -dx;
        speed = -speed;
    }
    shot->y_pos.f.u = (int16_t)(self->y_pos.f.u - 0x0C);
    shot->x_pos.f.u = (int16_t)(self->x_pos.f.u - dx);
    shot->x_vel = speed;
}

// Offset_0x0282A0: back in the oil, it swims again
static void Aquis_Land(Sst *self, Own_Aquis *own) {
    if (self->y_pos.f.u < wtr_pos1)
        return;
    self->routine_secondary = AquisState_Move;
    self->anim = AquisAnim_Swim;
    own->wait = own->wait_reset;
    self->y_vel = 0x40;
    own->leapt = 0;
}

// Offset_0x028384: the fin is a little behind and above its body, with its facing
static void Aquis_PlaceFin(Sst *self, Own_Aquis *own) {
    Sst *fin = &sst[own->link];
    if (own->link == 0)
        return;
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

// Offset_0x0284A8 (a shot that has been hit, which the touch code of the pre-beta never lets happen): it breaks into three pieces and a follower, and is gone
static void Aquis_Break(Sst *self) {
    for (int i = 2; i >= 0; i--) {
        Sst *piece = Aquis_Child(self, AquisRoutine_Piece, 3);
        if (piece == NULL)
            break;
        piece->x_pos.f.u = self->x_pos.f.u;
        piece->y_pos.f.u = self->y_pos.f.u;
        piece->render_flags |= SST_RENDER_LEVEL;
        piece->y_vel = -0x100;
        piece->anim = AquisAnim_Piece;
        piece->collision_property = 0x78;
        if (i == 1) {
            piece->x_vel = 0x40;
        } else if (i < 1) {
            piece->x_vel = -0x100;
            piece->y_vel = (int16_t)(piece->y_vel - 0x40);
        } else {
            piece->x_vel = 0xC0;
            piece->y_vel = (int16_t)(piece->y_vel - 0xC0);
        }
    }
    Aquis_Child(self, AquisRoutine_Follow, 4);
    Sst_Delete(self);
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
        own->wait = own->wait_reset = (int16_t)((self->subtype & 0xF0) << 4);
        own->turn = own->turn_reset = (int16_t)(((self->subtype & 0x0F) << 4) - 1);
        own->home_y = self->y_pos.f.u;
        own->water = wtr_pos1;
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
        case AquisState_Patrol:
            Aquis_FollowWater(self, own);
            Sst_MoveSprite2(self);
            Aquis_Turn(self, own);
            Aquis_StartMoving(self, own);
            Aquis_LookForSonic(self, own);
            break;
        case AquisState_Move:
            Aquis_FollowWater(self, own);
            Sst_MoveSprite2(self);
            Aquis_Turn(self, own);
            Aquis_Arrive(self, own);
            break;
        case AquisState_Leap:
            Sst_MoveSprite(self);
            Aquis_Turn(self, own);
            Aquis_Spit(self, own);
            Aquis_Land(self, own);
            break;
        }
        Aquis_PlaceFin(self, own);
        SstOnScreenTest(self);
        return;

    case AquisRoutine_Fin: {
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
        if (self->collision_property)
            Aquis_Break(self);
        else {
            Sst_MoveSprite2(self);
            Sst_Animate(self, Animation_Aquis);
            SstOnScreenTest(self);
        }
        return;

    case AquisRoutine_Piece: {
        Sst_MoveSprite(self);
        if (SstFloorDist(self, self->x_pos.f.u) < 0) {
            self->y_pos.f.u += SstFloorDist(self, self->x_pos.f.u);
            self->y_vel = (int16_t)-(self->y_vel >> 1);
        }
        if (--self->collision_property == 0) {
            Sst_Delete(self);
            return;
        }
        Sst_Animate(self, Animation_Aquis);
        SstOnScreenTest(self);
        return;
    }

    case AquisRoutine_Follow: {
        Own_AquisTrail *trail = (Own_AquisTrail *)self->own.u8;
        if (self->routine_secondary == 0) { // it waits for Sonic to be in his normal routine, and then it is on him
            if (player->routine != 2)
                return;
            self->x_pos.f.u = player->x_pos.f.u;
            self->y_pos.f.u = player->y_pos.f.u;
            self->render_flags |= SST_RENDER_LEVEL;
            self->priority = 1 * 0x80;
            self->anim = AquisAnim_Follow;
            self->routine_secondary = 0xFF;
            trail->counter = 0x12C;
            trail->flash = 3;
        }
        if (--trail->counter == 0) {
            Sst_Delete(self);
            return;
        }
        self->x_pos.f.u = player->x_pos.f.u;
        self->y_pos.f.u = (int16_t)(player->y_pos.f.u + 0x0C);
        if (--trail->flash != 0) {
            Sst_Animate(self, Animation_Aquis);
            Sst_Draw(self);
            return;
        }
        trail->flash = 3;
        self->status ^= SST_STATUS_X_FLIP;
        self->render_flags ^= SST_RENDER_X_FLIP;
        return;
    }
    }
}
