// Oil Ocean's badniks for Sonic 2, as the pre-beta has them: the Octus ($4A), an octopus that waits on the ground until Sonic is within $80 pixels each way, rises, puts out a puff and a shot, and goes off sideways.
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"
#include "Object/OOZBadniks.h"
#include "Constants.h"

#include "Level.h"
#include "LevelCollision.h"
#include "Object/Sonic.h"

#include "Macros.h"

#include "Resource/Animation/Octus.h"
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
