// The pre-beta's $C5 (Obj_0xC5_SFz_Boss): see SFZBoss.h. The art is the one at tile $3F9 (the lists of the zone have other art there: the object is a leftover of the Metropolis' elevator art place, as the pre-beta has it)
#include "Object/SFZBoss.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"
#include "Constants.h"
#include "Level.h"
#include "Object/CharControl.h"
#include "Object/Tails.h"
#include "Solid.h"

#include "Resource/Animation/SFZBoss.h"
#include "Resource/Mappings/SFZBoss.h"

typedef struct {
    uint8_t subtype_;  // 0x28 (self->subtype)
    uint8_t pad0;     // 0x29
    int16_t timer;    // 0x2A: (the body's is overwritten with a part's address by the pre-beta's Load_Sub_Obj, which makes the wait that follows end at once)
    uint16_t parent;  // 0x2C (var_00): the object that made it
    int16_t v02;      // 0x2E: (the body's) the camera's lower limit it found
    int16_t v04;      // 0x30: (the bouncing part's) the box's top
    int16_t v06;      // 0x32: ... its bottom
    int16_t v08;      // 0x34: ... its left
    int16_t v0a;      // 0x36: ... its right
    uint16_t third;   // 0x38 (unused)
    uint16_t part3;   // 0x3A: the body's third part
    uint16_t v10;     // 0x3C (var_10): (the body's) its bouncing part; (the following part's) the part it follows
    uint16_t v12;     // 0x3E (var_12): (the body's) its following part
} Own_SFZBoss;

// The address the pre-beta stores for an object (its objects are $40 bytes each from $B000): a word, and a negative one as a number
static uint16_t Address(const Sst *obj) {
    return (uint16_t)(0xB000 + (obj - sst) * 0x40);
}

static Sst *FromAddress(uint16_t address) {
    return &sst[(address - 0xB000) / 0x40];
}

// Load_Sub_Obj: a new object after this one, of the same code, with the subtype given; the address of it goes into the field of this object (offset in `field`) and this object is its parent, at this one's place
static Sst *LoadSub(Sst *self, uint16_t *field, uint8_t subtype) {
    Sst *sub = Sst_AllocateAfter(self);
    if (sub == NULL)
        return NULL;
    *field = Address(sub);
    sub->code = Obj_SFZBoss;
    sub->subtype = subtype;
    ((Own_SFZBoss *)sub->own.u8)->parent = Address(self);
    sub->x_pos.f.u = self->x_pos.f.u;
    sub->y_pos.f.u = self->y_pos.f.u;
    return sub;
}

enum { Routine_Init = 0, Routine_Body = 2, Routine_Platform = 4, Routine_Follower = 6, Routine_Bouncer = 8, Routine_Mover = 0xA };

static void Body_Start(Sst *self, Own_SFZBoss *own) {
    self->routine_secondary += 2;
    own->v02 = (int16_t)limit_btm2;
    limit_btm2 = 0x442 - SCREEN_TALLADD;
    limit_btm1 = 0x442 - SCREEN_TALLADD;
}

static void Body_WaitForSonic(Sst *self, Own_SFZBoss *own) {
    const uint16_t d = (uint16_t)((uint16_t)self->x_pos.f.u - (uint16_t)player->x_pos.f.u + 0x20);
    if (d >= 0x40)
        return;
    self->routine_secondary += 2;
    own->timer = 0x20;
    Sst *a = LoadSub(self, (uint16_t *)&own->timer, 0x8C); // (the pre-beta stores its address in the timer)
    if (a != NULL) {
        a->x_pos.f.u -= 0x88;
        a->y_pos.f.u += 0x48;
    }
    Sst *b = LoadSub(self, (uint16_t *)&own->timer, 0x8C);
    if (b != NULL) {
        b->x_pos.f.u += 0x88;
        b->y_pos.f.u += 0x48;
    }
    LoadSub(self, &own->part3, 0x92);
}

static void Body_Wait(Sst *self, Own_SFZBoss *own) {
    if (--own->timer < 0)
        self->routine_secondary += 2;
}

static void Body_MakeParts(Sst *self, Own_SFZBoss *own) {
    self->routine_secondary += 2;
    self->anim = 1;
    own->timer = 0x90;
    Sst *bouncer = LoadSub(self, &own->v10, 0x90);
    Sst *follower = LoadSub(self, &own->v12, 0x8E);
    if (follower != NULL && bouncer != NULL)
        ((Own_SFZBoss *)follower->own.u8)->v10 = Address(bouncer);
}

// Offset_0x036412: the rest that every turn of the loop starts with
static void Body_Rest(Sst *self, Own_SFZBoss *own) {
    own->timer = 0x80;
    self->anim = 0;
}

static void SFZBoss_Body(Sst *self, Own_SFZBoss *own) {
    switch (self->routine_secondary) {
    case 0x00: Body_Start(self, own); break;
    case 0x02: Body_WaitForSonic(self, own); break;
    case 0x04: case 0x0A: case 0x10: case 0x16: Body_Wait(self, own); break;
    case 0x06: case 0x0C: case 0x12: case 0x18: Sst_Animate(self, Animation_SFZBoss); break;
    case 0x08: Body_MakeParts(self, own); break;
    case 0x0E:
        self->routine_secondary += 2;
        Body_Rest(self, own);
        break;
    case 0x14:
        self->routine_secondary += 2;
        own->timer = 0x80;
        self->anim = 1;
        self->status |= 0x04; // (the mover looks for it)
        break;
    case 0x1A:
        self->routine_secondary = 0x0E;
        Body_Rest(self, own);
        break;
    }
    Sst_Draw(self);
}

static void SFZBoss_Platform(Sst *self) {
    if (self->routine_secondary == 0) {
        self->routine_secondary += 2;
        self->mapping_frame = 0x0C;
        Sst_Draw(self);
        return;
    }
    const int16_t x = self->x_pos.f.u;
    for (int who = SolidChar_Sonic; who <= SolidChar_Tails; who++) {
        Sst *chr = who == SolidChar_Sonic ? player : (TAILS_OBJ->code != NULL ? TAILS_OBJ : NULL);
        if (chr != NULL)
            Solid_Character(self, chr, who, 0x13, 0x40, 0x80, x, NULL);
    }
    const bool was = (self->status & 0x02) != 0; // (bchg #1: shown on every other frame)
    self->status ^= 0x02;
    if (!was)
        Sst_Draw(self);
}

static void SFZBoss_Follower(Sst *self, Own_SFZBoss *own) {
    if (self->routine_secondary == 0) {
        self->routine_secondary += 2;
        self->mapping_frame = 5;
        self->anim = 2;
    } else {
        const Sst *part = FromAddress(own->v10);
        self->x_pos.f.u = part->x_pos.f.u;
        self->y_pos.f.u = (int16_t)(part->y_pos.f.u - 0x10);
    }
    Sst_Animate(self, Animation_SFZBoss);
    Sst_Draw(self);
}

static void SFZBoss_Bouncer(Sst *self, Own_SFZBoss *own) {
    switch (self->routine_secondary) {
    case 0: {
        self->routine_secondary += 2;
        self->anim = 3;
        self->mapping_frame = 7;
        self->y_vel = 0x40;
        own->timer = 0x90;
        const Sst *body = FromAddress(own->parent);
        own->v04 = (int16_t)(body->y_pos.f.u + 0x0C);
        own->v06 = (int16_t)(own->v04 + 0x84);
        own->v08 = (int16_t)(body->x_pos.f.u - 0x70);
        own->v0a = (int16_t)(own->v08 + 0xE0);
        break;
    }
    case 2:
        if (--own->timer < 0) {
            self->routine_secondary += 2;
            own->timer = 0x40;
            self->y_vel = 0;
        } else {
            Sst_MoveSprite2(self);
        }
        break;
    case 4:
        if (--own->timer < 0) {
            self->routine_secondary += 2;
            self->x_vel = -0x100;
            self->y_vel = 0x100;
            self->priority = 3 * 0x80;
            const Sst *body = FromAddress(own->parent);
            FromAddress(((const Own_SFZBoss *)body->own.u8)->v12)->priority = 3 * 0x80;
        }
        break;
    case 6: {
        const uint16_t y = (uint16_t)self->y_pos.f.u;
        if (self->y_vel < 0) {
            if (y < (uint16_t)own->v04)
                self->y_vel = (int16_t)-self->y_vel;
        } else if (y >= (uint16_t)own->v06) {
            self->y_vel = (int16_t)-self->y_vel;
        }
        const uint16_t x = (uint16_t)self->x_pos.f.u;
        if (self->x_vel < 0) {
            if (x < (uint16_t)own->v08)
                self->x_vel = (int16_t)-self->x_vel;
        } else if (x >= (uint16_t)own->v0a) {
            self->x_vel = (int16_t)-self->x_vel;
        }
        Sst_MoveSprite2(self);
        break;
    }
    }
    Sst_Animate(self, Animation_SFZBoss);
    Sst_Draw(self);
}

static void SFZBoss_Mover(Sst *self, Own_SFZBoss *own) {
    switch (self->routine_secondary) {
    case 0:
        self->routine_secondary += 2;
        self->mapping_frame = 4;
        self->y_pos.f.u -= 4;
        break;
    case 2: {
        Sst *body = FromAddress(own->parent);
        if (body->status & 0x04) {
            body->status &= (uint8_t)~0x04;
            self->routine_secondary += 2;
            own->timer = 0x10;
            self->y_vel = 0x100;
        }
        break;
    }
    case 4:
        if (--own->timer < 0) {
            self->routine_secondary += 2;
            own->timer = 0x70;
            self->y_vel = 0;
        }
        break;
    case 6:
        if (--own->timer < 0) {
            self->routine_secondary += 2;
            own->timer = 0x10;
            self->y_vel = -0x100;
        }
        break;
    case 8:
        if (--own->timer < 0) {
            self->routine_secondary = 2;
            self->y_vel = 0;
        }
        break;
    }
    Sst_MoveSprite2(self);
    Sst_Draw(self);
}

void Obj_SFZBoss(Sst *self) {
    Own_SFZBoss *own = (Own_SFZBoss *)self->own.u8;
    if (self->routine == Routine_Init) {
        // Object_Settings: the entry of the subtype ($8A the body, $8C the platform, the others the small parts), then the routine is the subtype less $88
        self->mappings = Mappings_SFZBoss;
        self->art_tile = TILE_MAP(0, 0, 0, 0, 0x3F9);
        self->render_flags |= SST_RENDER_LEVEL;
        self->priority = (uint16_t)((self->subtype == 0x8A || self->subtype == 0x8C ? 4 : 5) * 0x80);
        self->width_pixels = self->subtype == 0x8A ? 0x20 : self->subtype == 0x8C ? 0x08 : 0x10;
        self->collision_flags = self->subtype == 0x8A ? 0x09 : 0x00;
        self->routine = (uint8_t)(self->subtype - 0x88);
        return;
    }
    switch (self->routine) {
    case Routine_Body: SFZBoss_Body(self, own); break;
    case Routine_Platform: SFZBoss_Platform(self); break;
    case Routine_Follower: SFZBoss_Follower(self, own); break;
    case Routine_Bouncer: SFZBoss_Bouncer(self, own); break;
    case Routine_Mover: SFZBoss_Mover(self, own); break;
    }
}
