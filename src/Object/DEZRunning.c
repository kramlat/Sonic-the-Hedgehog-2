// The pre-beta's $C6, Robotnik running: see DEZRunning.h
#include "Object/DEZRunning.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"
#include "Constants.h"
#include "Object/CharControl.h"
#include "Object/Tails.h"
#include "Solid.h"

#include "Resource/Animation/DEZRobotnikRunning.h"
#include "Resource/Animation/DEZRunDoor.h"
#include "Resource/Mappings/DEZRobotnikRunning.h"
#include "Resource/Mappings/DEZRunDoor.h"

typedef struct {
    uint8_t subtype_;  // 0x28
    uint8_t pad0;      // 0x29
    int16_t timer;     // 0x2A
    uint16_t parent;   // 0x2C (var_00)
    uint16_t v02;      // 0x2E
    uint16_t pad1[6];  // 0x30-0x3B
    uint16_t thrown;   // 0x3C: the last thing he threw
    uint16_t door;     // 0x3E: his door
} Own_DEZRunning;

static uint16_t robotnik_address; // ($F660)

Sst *DEZRunning_Robotnik(void) {
    return robotnik_address == 0 ? NULL : &sst[(robotnik_address - 0xB000) / 0x40];
}

enum { Routine_Init = 0, Routine_Robotnik = 2, Routine_Door = 4, Routine_Thrown = 6 };

static uint16_t Address(const Sst *obj) {
    return (uint16_t)(0xB000 + (obj - sst) * 0x40);
}

static Sst *FromAddress(uint16_t address) {
    return &sst[(address - 0xB000) / 0x40];
}

static Sst *LoadSub(Sst *self, uint16_t *field, uint8_t subtype) {
    Sst *sub = Sst_AllocateAfter(self);
    if (sub == NULL)
        return NULL;
    *field = Address(sub);
    sub->code = Obj_DEZRunning;
    sub->subtype = subtype;
    ((Own_DEZRunning *)sub->own.u8)->parent = Address(self);
    sub->x_pos.f.u = self->x_pos.f.u;
    sub->y_pos.f.u = self->y_pos.f.u;
    return sub;
}

// Offset_0x0369CE: what he throws, from his place a little above, to the left, falling
static void Throw(Sst *self, Own_DEZRunning *own) {
    Sst *thing = LoadSub(self, &own->thrown, 0x9A);
    if (thing == NULL)
        return;
    thing->mapping_frame = 5;
    thing->x_vel = -0x100;
    thing->y_pos.f.u -= 0x18;
    ((Own_DEZRunning *)thing->own.u8)->timer = 8;
}

static void Robotnik(Sst *self, Own_DEZRunning *own) {
    switch (self->routine_secondary) {
    case 0: {
        self->routine_secondary += 2;
        Sst *door = LoadSub(self, &own->door, 0x98);
        if (door != NULL) {
            door->x_pos.f.u = 0x3F8;
            door->y_pos.f.u = 0x160;
        }
        robotnik_address = Address(self); // ($F660: the final boss looks at him)
        break;
    }
    case 2: {
        const uint16_t d = (uint16_t)((uint16_t)self->x_pos.f.u - (uint16_t)player->x_pos.f.u + 0x5C);
        if (d >= 0xB8)
            break;
        self->routine_secondary += 2;
        own->timer = 0x18;
        self->mapping_frame = 1;
        break;
    }
    case 4:
        if (--own->timer < 0) {
            self->routine_secondary += 2;
            self->status |= 0x04; // (the door looks for it)
            self->x_vel = 0x200;
            own->timer = 0x10;
        }
        break;
    case 6:
        if ((uint16_t)self->x_pos.f.u >= 0x790) {
            self->mapping_frame = 2;
            self->x_vel = 0;
            if (self->render_flags & SST_RENDER_ON_SCREEN) {
                self->routine_secondary += 2;
                self->x_vel = 0x80;
                self->y_vel = -0x200;
                self->mapping_frame = 2;
                own->timer = 0x40;
                self->status |= 0x08;
            }
            break;
        }
        {
            const uint16_t d = (uint16_t)((uint16_t)self->x_pos.f.u - (uint16_t)player->x_pos.f.u + 0x50);
            if (d < 0xA0)
                self->x_pos.f.u = (int16_t)(player->x_pos.f.u + 0x50);
        }
        if (--own->timer >= 0) {
            // (nothing this frame)
        } else {
            own->timer = 0x20;
            Throw(self, own);
        }
        Sst_MoveSprite2(self);
        Sst_Animate(self, Animation_DEZRobotnikRunning);
        break;
    case 8:
        if (--own->timer < 0) {
            Sst_Delete(self);
            return;
        }
        self->y_vel += 0x10;
        Sst_MoveSprite2(self);
        break;
    }
    Sst_Draw(self);
}

static void DoorSolid(Sst *self) {
    const int16_t x = self->x_pos.f.u;
    for (int who = SolidChar_Sonic; who <= SolidChar_Tails; who++) {
        Sst *chr = who == SolidChar_Sonic ? player : (TAILS_OBJ->code != NULL ? TAILS_OBJ : NULL);
        if (chr != NULL)
            Solid_Character(self, chr, who, 0x13, 0x20, 0x20, x, NULL);
    }
}

static void Door(Sst *self, Own_DEZRunning *own) {
    switch (self->routine_secondary) {
    case 0: {
        const Sst *body = FromAddress(own->parent);
        if (body->status & 0x04)
            self->routine_secondary += 2;
        else
            DoorSolid(self);
        break;
    }
    case 2:
        DoorSolid(self);
        Sst_Animate(self, Animation_DEZRunDoor);
        break;
    case 4:
        player->status &= (uint8_t)~SST_PSTATUS_PUSHING;
        Sst_Delete(self);
        return;
    }
    Sst_Draw(self);
}

static void Thrown(Sst *self, Own_DEZRunning *own) {
    if (--own->timer < 0) {
        Sst_Delete(self);
        return;
    }
    self->y_vel += 0x10;
    Sst_MoveSprite2(self);
    SstOnScreenTest(self);
}

void Obj_DEZRunning(Sst *self) {
    Own_DEZRunning *own = (Own_DEZRunning *)self->own.u8;
    if (self->routine == Routine_Init) {
        // Object_Settings: the entry of the subtype ($96 Robotnik, $98 the door, $9A what he throws), then the routine is the subtype less $94
        self->render_flags |= SST_RENDER_LEVEL;
        self->collision_flags = 0;
        switch (self->subtype) {
        case 0x98:
            self->mappings = Mappings_DEZRunDoor;
            self->art_tile = TILE_MAP(0, 1, 0, 0, 0x328);
            self->priority = 1 * 0x80;
            self->width_pixels = 0x08;
            break;
        case 0x9A:
            self->mappings = Mappings_DEZRobotnikRunning;
            self->art_tile = TILE_MAP(0, 0, 0, 0, 0);
            self->priority = 5 * 0x80;
            self->width_pixels = 0x04;
            break;
        default:
            self->mappings = Mappings_DEZRobotnikRunning;
            self->art_tile = TILE_MAP(0, 0, 0, 0, 0);
            self->priority = 5 * 0x80;
            self->width_pixels = 0x18;
            break;
        }
        self->routine = (uint8_t)(self->subtype - 0x94);
        return;
    }
    switch (self->routine) {
    case Routine_Robotnik: Robotnik(self, own); break;
    case Routine_Door: Door(self, own); break;
    case Routine_Thrown: Thrown(self, own); break;
    }
}
