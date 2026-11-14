#include "Object/EggPrison.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"

#include "Game.h"
#include "Level.h"
#include "MathUtil.h"
#include "Mappings.h"
#include "Nemesis.h"
#include "Object/Animal.h"
#include "Object/ObjectHit.h"
#include "Backend/VDP.h"
#include "Resource/Art/EggPrison.h"
#include "Resource/Mappings/EggPrison.h"

#include <string.h>

// The four parts, by their table at loc_3B042: how far above the object each is, its routine, width, priority and frame
static const struct { uint8_t y_off, routine, width, priority, frame; } part[4] = {
    { 0x00, 2, 0x20, 4, 0 }, // the capsule
    { 0x28, 4, 0x10, 5, 4 }, // the button
    { 0x18, 6, 0x08, 3, 5 }, // the lid
    { 0x00, 8, 0x20, 4, 0 }, // the one that lets the animals out (not drawn)
};
#define EGG_ART_TILE 0x680 // (Egg_Prison loads at $D000)

typedef struct {
    uint16_t link[3]; // the button, the lid and the counter (slots)
    int16_t base_y;   // 0x30: where the part sits when nothing stands on it
    uint16_t timer;   // 0x34 / 0x1E
    uint8_t step;     // 0x25
    uint8_t pressed;  // 0x32: the button has been stood on
    uint8_t frame_time;
} Own_EggPrison;

static Sst *Slot(uint16_t i) {
    return &sst[i];
}

static Sst *NewAnimal(void) {
    Sst *animal = Sst_Allocate();
    if (animal != NULL) {
        memset(animal, 0, sizeof(Sst));
        animal->code = sst_object_codes[ObjId_Animal];
    }
    return animal;
}

static void Egg_Setup(Sst *self, int i) {
    Own_EggPrison *own = (Own_EggPrison *)self->own.u8;
    self->mappings = Mappings_EggPrison;
    self->art_tile = TILE_MAP(0, 1, 0, 0, EGG_ART_TILE);
    self->render_flags = SST_RENDER_LEVEL;
    self->y_pos.f.u = (int16_t)(self->y_pos.f.u - part[i].y_off);
    own->base_y = self->y_pos.f.u;
    self->routine = part[i].routine;
    self->width_pixels = part[i].width;
    self->priority = (uint16_t)(part[i].priority * 0x80);
    self->mapping_frame = part[i].frame;
}

// Routine 0: the capsule makes the other three, and puts the art in
static void Egg_Init(Sst *self) {
    Own_EggPrison *own = (Own_EggPrison *)self->own.u8;
    VDP_SeekVRAM((size_t)EGG_ART_TILE * 32);
    NemDec(Art_EggPrison);
    const int16_t x = self->x_pos.f.u, y = self->y_pos.f.u;
    Egg_Setup(self, 0);
    for (int i = 1; i < 4; i++) {
        Sst *child = Sst_Allocate();
        if (child == NULL)
            return;
        child->code = Obj_EggPrison;
        child->x_pos.f.u = x;
        child->y_pos.f.u = y;
        Egg_Setup(child, i);
        own->link[i - 1] = (uint16_t)(child - sst);
    }
}

// Routine 2: the capsule: solid; when the button has been stood on the lid is thrown off and, after a moment, it opens
static void Egg_Capsule(Sst *self) {
    Own_EggPrison *own = (Own_EggPrison *)self->own.u8;
    switch (own->step) {
    case 0: {
        const Own_EggPrison *button = (const Own_EggPrison *)Slot(own->link[0])->own.u8;
        if (button->pressed) {
            Sst *lid = Slot(own->link[1]);
            Sst *puff = Sst_Allocate(); // (an explosion where the lid was: Object $27, the one without an animal)
            if (puff != NULL) {
                puff->code = Obj_ObjectHit;
                puff->routine += 2;
                puff->x_pos.f.u = lid->x_pos.f.u;
                puff->y_pos.f.u = lid->y_pos.f.u;
            }
            lid->y_vel = -0x400;
            lid->x_vel = 0x800;
            ((Own_EggPrison *)lid->own.u8)->step += 2;
            own->timer = 0x1D;
            own->step += 1;
        }
        break;
    }
    case 1:
        if ((int16_t)--own->timer < 0) { // opens: eight animals in a row
            self->anim = 1;
            boss_status = 2; // (the prison is open: its animals hop out, they are not a badnik's, which give points)
            int16_t delay = 0x9A, x_off = -0x1C;
            for (int i = 0; i < 8; i++) {
                Sst *animal = NewAnimal();
                if (animal == NULL)
                    break;
                animal->x_pos.f.u = (int16_t)(self->x_pos.f.u + x_off);
                animal->y_pos.f.u = self->y_pos.f.u;
                ((Own_Animals *)animal->own.u8)->timer = (uint16_t)delay;
                x_off += 7;
                delay -= 8;
            }
            Sst *counter = Slot(own->link[2]);
            ((Own_EggPrison *)counter->own.u8)->timer = 0xB4;
            ((Own_EggPrison *)counter->own.u8)->step = 1;
            own->step = 2;
        }
        break;
    default:
        break;
    }
    SstSolidObject(self, 0x2B, 0x18, 0x18, self->x_pos.f.u, NULL, NULL);
    // Egg_Prison_Animate_Data: anim 1 is delay 3, frames 0 to 3 and then the last one again
    if (self->anim == 1 && self->mapping_frame < 3 && ++own->frame_time > 3) {
        own->frame_time = 0;
        self->mapping_frame++;
    }
    Sst_Draw(self);
    SstDeleteIfNotInRange2(self, self->x_pos.f.u);
}

// Routine 4: the button: solid, goes down 8 pixels when stood on (the clock stops), and says so
static void Egg_Button(Sst *self) {
    Own_EggPrison *own = (Own_EggPrison *)self->own.u8;
    SstSolidObject(self, 0x1B, 8, 8, self->x_pos.f.u, NULL, NULL);
    self->y_pos.f.u = own->base_y;
    if (self->status & SST_STATUS_PLAYER_STAND) {
        self->y_pos.f.u = (int16_t)(self->y_pos.f.u + 8);
        time_count = false;
        own->pressed = 1;
    }
    Sst_Draw(self);
    SstDeleteIfNotInRange2(self, self->x_pos.f.u);
}

// Routine 6: the lid: sits on the capsule until it is thrown, then falls until it is off the screen
static void Egg_Lid(Sst *self) {
    Own_EggPrison *own = (Own_EggPrison *)self->own.u8;
    if (own->step) {
        if (!(self->render_flags & SST_RENDER_ON_SCREEN)) {
            Sst_Delete(self);
            return;
        }
        Sst_MoveSprite(self);
    }
    Sst_Draw(self);
    SstDeleteIfNotInRange2(self, self->x_pos.f.u);
}

// Routine 8: lets more animals out for three seconds, then waits for them all to be gone and ends the act
static void Egg_Counter(Sst *self) {
    Own_EggPrison *own = (Own_EggPrison *)self->own.u8;
    if (self->routine == 8) {
        if (!own->step)
            return;
        if ((frame_count & 7) == 0) {
            Sst *animal = NewAnimal();
            if (animal != NULL) {
                const uint32_t rand = RandomNumber();
                int16_t off = (int16_t)((rand & 0x1F) - 6);
                if ((int16_t)(rand >> 16) < 0)
                    off = (int16_t)-off;
                animal->x_pos.f.u = (int16_t)(self->x_pos.f.u + off);
                animal->y_pos.f.u = self->y_pos.f.u;
                ((Own_Animals *)animal->own.u8)->timer = 0xC;
            }
        }
        if (--own->timer == 0) {
            self->routine = 0xA;
            own->timer = 0xB4;
        }
        return;
    }
    for (int i = 0; i < SST_COUNT; i++) // (routine $A: when no animal is left)
        if (sst[i].code != NULL && sst[i].code == sst_object_codes[ObjId_Animal])
            return;
    GotThroughAct();
    Sst_Delete(self);
}

void Obj_EggPrison(Sst *self) {
    switch (self->routine) {
    case 0:
        Egg_Init(self);
        break;
    case 2:
        Egg_Capsule(self);
        break;
    case 4:
        Egg_Button(self);
        break;
    case 6:
        Egg_Lid(self);
        break;
    case 8:
    case 0xA:
        Egg_Counter(self);
        break;
    }
}
