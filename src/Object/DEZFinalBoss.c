// The pre-beta's $C7, the giant Robotnik: see DEZFinalBoss.h
#include "Object/DEZFinalBoss.h"
#include "Object/DEZRunning.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"
#include "Constants.h"

#include "Resource/Animation/DEZFinalBoss1.h"
#include "Resource/Animation/DEZFinalBoss2.h"
#include "Resource/Animation/DEZFinalBossPuppet.h"
#include "Resource/Mappings/DEZFinalBossGiant.h"

// The body keeps the addresses of its ten parts in the words from $2A on (one for each of the subtypes $A0 to $B4); a part keeps its maker's in the word at $2C
typedef struct {
    uint8_t subtype_;   // 0x28
    uint8_t pad0;       // 0x29
    uint16_t w[11];     // 0x2A-0x3F
    uint8_t count;      // 0x40: (the body's) a count of frames (Obj_Ani_Time)
    uint8_t puppet_frame; // 0x41: (the body's) where it is in the puppet's sequence of tables (Obj_Ani_Frame)
    uint8_t puppet_time;  // 0x42: ... and how long the table has run (Obj_Ani_Time_2)
} Own_DEZFinalBoss;

#define PARENT(o) ((o)->w[1])
#define WORD(offset) (((offset) - 0x2A) / 2)

enum {
    Routine_Init = 0, Routine_Body = 2, Routine_Head = 4, Routine_PartA = 6, Routine_PartB = 8, Routine_PartC = 0xA, Routine_PartD = 0xC, Routine_PartE = 0xE,
    Routine_Eye = 0x10, Routine_Window = 0x12, Routine_FrontA = 0x14, Routine_FrontB = 0x16, Routine_FrontC = 0x18
};

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
    sub->code = Obj_DEZFinalBoss;
    sub->subtype = subtype;
    ((Own_DEZFinalBoss *)sub->own.u8)->w[1] = Address(self);
    sub->x_pos.f.u = self->x_pos.f.u;
    sub->y_pos.f.u = self->y_pos.f.u;
    return sub;
}

// Offset_0x037026: at a place beside its maker
static void Follow(Sst *self, Own_DEZFinalBoss *own, int16_t dx, int16_t dy) {
    const Sst *maker = FromAddress(PARENT(own));
    self->x_pos.f.u = (int16_t)(maker->x_pos.f.u + dx);
    self->y_pos.f.u = (int16_t)(maker->y_pos.f.u + dy);
}

// Offset_0x037040: its parts put at these places from the body's (the field of each, then how far across and down)
static void PlaceParts(Sst *self, Own_DEZFinalBoss *own) {
    static const struct { uint8_t field; int8_t dx, dy; } places[] = {
        { 0x2C, (int8_t)0xFC, 0x3C }, { 0x2E, 0x04, 0x08 }, { 0x30, (int8_t)0xEC, 0x08 }, { 0x32, 0x0C, (int8_t)0xF8 },
        { 0x34, 0x04, 0x24 }, { 0x3A, (int8_t)0xFC, 0x3C }, { 0x3C, (int8_t)0xEC, 0x08 }, { 0x3E, 0x04, 0x24 },
    };
    for (size_t i = 0; i < sizeof places / sizeof places[0]; i++) {
        Sst *part = FromAddress(own->w[WORD(places[i].field)]);
        part->x_pos.f.u = (int16_t)(self->x_pos.f.u + places[i].dx);
        part->y_pos.f.u = (int16_t)(self->y_pos.f.u + places[i].dy);
    }
}

static int16_t Be16(const uint8_t *p) {
    return (int16_t)((p[0] << 8) | p[1]);
}

// DEz_Boss_Animate_Date: the tables of the puppet, one after another (the sequence is the eleven of them twice); each moves some parts (a field of the body that holds the address of the part, or 0 for the body itself) by
// how far across and down, in 256ths of a pixel, every frame, for as long as it lasts. False once the sequence is done
static bool Puppet(Sst *self, Own_DEZFinalBoss *own) {
    static const uint8_t order[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 0xFF };
    const uint8_t index = order[own->puppet_frame];
    if (index & 0x80)
        return false;
    const uint8_t *script = Animation_DEZFinalBossPuppet + (uint16_t)Be16(Animation_DEZFinalBossPuppet + index * 2);
    const int parts = script[0] + 1;
    const uint8_t duration = script[1];
    script += 2;
    uint8_t time = (uint8_t)(own->puppet_time + 1);
    if (time >= duration) {
        own->puppet_frame++;
        time = 0;
    }
    own->puppet_time = time;
    for (int i = 0; i < parts; i++, script += 6) {
        const uint16_t field = (uint16_t)Be16(script);
        Sst *part = field == 0 ? self : FromAddress(own->w[WORD(field)]);
        part->x_pos.v += (int32_t)Be16(script + 2) * 256;
        part->y_pos.v += (int32_t)Be16(script + 4) * 256;
    }
    return true;
}

static void Body(Sst *self, Own_DEZFinalBoss *own) {
    switch (self->routine_secondary) {
    case 0: {
        self->routine_secondary += 2;
        self->mapping_frame = 3;
        self->priority = 5 * 0x80;
        for (int i = 0; i < 11; i++)
            LoadSub(self, &own->w[i], (uint8_t)(0xA0 + 2 * i));
        break;
    }
    case 2:
        if (!(self->status & 0x04))
            break;
        self->routine_secondary += 2;
        self->y_vel = -0x100;
        own->count = 0x79;
        FromAddress(own->w[WORD(0x38)])->routine_secondary = 4;
        break;
    case 4:
        if (--own->count == 0) {
            self->routine_secondary += 2;
            self->y_vel = 0;
            own->count = 0x1F;
            FromAddress(own->w[WORD(0x38)])->routine_secondary = 6;
        } else {
            Sst_MoveSprite2(self);
            PlaceParts(self, own);
        }
        break;
    case 6:
        if ((int8_t)--own->count < 0)
            self->routine_secondary += 2;
        break;
    case 8:
        Puppet(self, own);
        break;
    }
    Sst_Draw(self);
}

// A part that is only a picture at a place beside the body's at first, and then moved by the puppet
static void Part(Sst *self, uint8_t frame, int16_t dx, int16_t dy, int collision, int priority) {
    if (self->routine_secondary == 0) {
        self->routine_secondary += 2;
        self->mapping_frame = frame;
        self->x_pos.f.u = (int16_t)(self->x_pos.f.u + dx);
        self->y_pos.f.u = (int16_t)(self->y_pos.f.u + dy);
        if (collision >= 0)
            self->collision_flags = (uint8_t)collision;
        if (priority > 0)
            self->priority = (uint16_t)(priority * 0x80);
    }
    Sst_Draw(self);
}

static void Head(Sst *self, Own_DEZFinalBoss *own) {
    if (self->routine_secondary == 0) {
        self->routine_secondary += 2;
        self->mapping_frame = 4;
        self->collision_flags = 0;
    }
    Follow(self, own, 0x0C, -0x14);
    Sst_Draw(self);
}

static void Eye(Sst *self, Own_DEZFinalBoss *own) {
    switch (self->routine_secondary) {
    case 0:
        self->routine_secondary += 2;
        self->collision_flags = 0x88;
        break;
    case 2: {
        const Sst *running = DEZRunning_Robotnik();
        if (running != NULL && (running->status & 0x08)) {
            self->routine_secondary += 2;
            ((int16_t *)&own->w[0])[0] = 0x40;
        }
        break;
    }
    case 4:
        Sst_Animate(self, Animation_DEZFinalBoss1);
        Sst_Draw(self);
        break;
    case 6:
        if (--*(int16_t *)&own->w[0] < 0) {
            self->routine_secondary += 2;
            FromAddress(PARENT(own))->status |= 0x04;
        }
        Sst_Draw(self);
        break;
    case 8:
        Sst_Draw(self);
        break;
    }
    Follow(self, own, 0x00, -0x34);
}

static void Window(Sst *self, Own_DEZFinalBoss *own) {
    switch (self->routine_secondary) {
    case 0:
        self->routine_secondary += 2;
        self->mapping_frame = 0x0C;
        self->collision_flags = 0x8B;
        break;
    case 4:
        Sst_Animate(self, Animation_DEZFinalBoss2);
        Sst_Draw(self);
        break;
    case 6:
        self->anim = 1;
        Sst_Animate(self, Animation_DEZFinalBoss2);
        Sst_Draw(self);
        break;
    }
    Follow(self, own, 0x38, 0x18);
}

void Obj_DEZFinalBoss(Sst *self) {
    Own_DEZFinalBoss *own = (Own_DEZFinalBoss *)self->own.u8;
    if (self->routine == Routine_Init) {
        // Object_Settings: one entry for all of them (the giant's mappings, art at $330, render 4, priority 4, width $38, collision $96), then the routine is the subtype less $9C
        self->mappings = Mappings_DEZFinalBossGiant;
        self->art_tile = TILE_MAP(0, 0, 0, 0, 0x330);
        self->render_flags |= SST_RENDER_LEVEL;
        self->priority = 4 * 0x80;
        self->width_pixels = 0x38;
        self->collision_flags = 0x96;
        self->routine = (uint8_t)(self->subtype - 0x9C);
        return;
    }
    switch (self->routine) {
    case Routine_Body: Body(self, own); break;
    case Routine_Head: Head(self, own); break;
    case Routine_PartA: Part(self, 0x0B, -2, 0x3C, 0xA2, 0); break;
    case Routine_PartB: Part(self, 6, 4, 8, 0, 0); break;
    case Routine_PartC: Part(self, 7, -0x14, 8, 0x88, 0); break;
    case Routine_PartD: Part(self, 5, 0x0C, -8, 0, 0); break;
    case Routine_PartE: Part(self, 0x0A, 4, 0x24, 0x86, 0); break;
    case Routine_Eye: Eye(self, own); break;
    case Routine_Window: Window(self, own); break;
    case Routine_FrontA: Part(self, 0x0B, -2, 0x3C, -1, 5); break;
    case Routine_FrontB: Part(self, 7, -0x14, 8, -1, 5); break;
    case Routine_FrontC: Part(self, 0x0A, 4, 0x24, -1, 5); break;
    }
}
