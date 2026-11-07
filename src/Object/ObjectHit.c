#include "Object/ObjectHit.h"
#include "Constants.h"
#include "Object.h"
#include "Sound.h"
#include "SstLoad.h"

#include "Resource/Mappings/ObjectHit.h"
extern const uint8_t Mappings_Points[]; // (defined by the shared Points.c, which the build replaces with Sonic 2's own mapping)

const uint8_t *ObjectHit_Mappings(void) {
    return Mappings_ObjectHit;
}

// Object $27 (Obj_0x27_Object_Hit): makes the animal (object $28) first, then shows five frames: the first for 4 frames and the others for 8
void Obj_ObjectHit(Sst *self) {
    switch (self->routine) {
    case 0: {
        self->routine += 2;
        Sst *animal = Sst_Spawn(ObjId_Animal, self->x_pos.f.u, self->y_pos.f.u);
        if (animal != NULL)
            animal->own.u16[0xB] = self->own.u16[0xB];
    }
    // Fallthrough
    case 2:
        self->routine += 2;
        self->mappings = Mappings_ObjectHit;
        self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_ObjectHit);
        self->render_flags = SST_RENDER_LEVEL;
        self->priority = 1 * 0x80;
        self->collision_flags = 0;
        self->width_pixels = 0x0C;
        self->anim_frame_timer = 3;
        self->mapping_frame = 0;
        PlaySound(sfx_BreakItem);
    // Fallthrough
    case 4:
        if ((int8_t)--self->anim_frame_timer < 0) {
            self->anim_frame_timer = 7;
            if (++self->mapping_frame == 5) {
                Sst_Delete(self);
                return;
            }
        }
        Sst_Draw(self);
        break;
    }
}

// Object $29 (Obj_0x29_Enemy_Points): the score that rises where something was destroyed (its creator sets the frame); the art word is $04AC, palette line 0
void Obj_EnemyPoints(Sst *self) {
    switch (self->routine) {
    case 0:
        self->routine += 2;
        self->mappings = Mappings_Points;
        self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_Points);
        self->render_flags = SST_RENDER_LEVEL;
        self->priority = 1 * 0x80;
        self->width_pixels = 8;
        self->y_vel = -0x300;
    // Fallthrough
    case 2:
        if (self->y_vel >= 0) {
            Sst_Delete(self);
            return;
        }
        Sst_MoveSprite2(self);
        self->y_vel += 0x18;
        Sst_Draw(self);
        break;
    }
}
