// The star post of Sonic 2 Beta 4 (object $79, objects/obj_0x79.asm): the checkpoint. A player that passes it (the first player in the one player play, both on their own in the versus play) saves where he is (the
// level's state for the first: Obj_Checkpoint_StoreInfo, the position and the post's number for the second), a star ball circles the post for 32 frames and, in the one player play with 50 rings or more, four bonus stars
// leave it and circle: touching one takes the player to the special stage. The post a player has passed already flickers.
#include "Object/StarPost.h"
#include "Object/Checkpoint.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"
#include "Object/Tails.h"
#include "SplitScreen.h"
#include "Constants.h"
#include "Game.h"
#include "Level.h"
#include "MathUtil.h"
#include "Mappings.h"
#include "Sound.h"
#include "Resource/Mappings/StarPost.h"
#include "Resource/Mappings/StarPostStars.h"
extern uint8_t true_two_player; // (Game.h: the versus play)

uint8_t last_star_p2;
int16_t star_p2_x, star_p2_y;
bool star_bonus_stage;

typedef struct {
    int16_t base_x, base_y; // 0x30, 0x32: the centre of what circles
    uint16_t angle_sum;     // 0x34: the bonus star's angle
    uint16_t timer;         // 0x36
    uint16_t parent;        // 0x3E: the post (a slot)
    uint8_t flicker;        // 0x1B
    uint8_t anim_time;
    uint8_t anim_step;
} Own_StarPost;

#define ART_TILE TILE_MAP(0, 0, 0, 0, ArtTile_StarPost)

// The stars (loc_1B354): four, a quarter of a turn apart
static void SpawnBonusStars(Sst *post) {
    int angle = 0;
    for (int i = 0; i < 4; i++, angle += 0x40) {
        Sst *star = Sst_Allocate();
        if (star == NULL)
            return;
        Own_StarPost *own = (Own_StarPost *)star->own.u8;
        star->code = Obj_StarPost;
        star->mappings = Mappings_StarPostStars;
        star->art_tile = ART_TILE;
        star->render_flags = SST_RENDER_LEVEL;
        star->routine = 8;
        star->x_pos.f.u = post->x_pos.f.u;
        star->y_pos.f.u = (int16_t)(post->y_pos.f.u - 0x30);
        own->base_x = star->x_pos.f.u;
        own->base_y = star->y_pos.f.u;
        star->priority = post->priority;
        star->width_pixels = 8;
        star->mapping_frame = 1;
        star->x_vel = -0x400;
        star->y_vel = 0;
        own->angle_sum = (uint16_t)angle;
        star->collision_flags = 0xD8;
        star->collision_property = 0;
    }
}

// Whether the player has passed a post this far on (his own number), and if he is at it, saves
static void CheckPlayer(Sst *self, Sst *who, bool second) {
    const uint8_t passed = second ? last_star_p2 : last_lamp;
    if ((passed & 0x7F) >= (self->subtype & 0x7F)) {
        if (self->anim == 0)
            self->anim = 2;
        return;
    }
    const int16_t dx = (int16_t)(who->x_pos.f.u - self->x_pos.f.u + 8);
    if ((uint16_t)dx >= 0x10)
        return;
    const int16_t dy = (int16_t)(who->y_pos.f.u - self->y_pos.f.u + 0x40);
    if ((uint16_t)dy >= 0x68)
        return;
    PlaySound(sfx_Lamppost);
    Sst *ball = Sst_Allocate();
    if (ball != NULL) {
        Own_StarPost *bown = (Own_StarPost *)ball->own.u8;
        ball->code = Obj_StarPost;
        ball->routine = 6;
        bown->base_x = self->x_pos.f.u;
        bown->base_y = (int16_t)(self->y_pos.f.u - 0x14);
        ball->mappings = self->mappings;
        ball->art_tile = self->art_tile;
        ball->render_flags = SST_RENDER_LEVEL;
        ball->width_pixels = 8;
        ball->priority = 4 * 0x80;
        ball->mapping_frame = 2;
        bown->timer = 0x20;
        bown->parent = (uint16_t)(self - sst);
        if (!true_two_player && rings >= 0x32)
            SpawnBonusStars(self);
    }
    self->anim = 1;
    if (!second) {
        Own_Checkpoint cp = { 0 };
        Obj_Checkpoint_StoreInfo(self, &cp);
    } else {
        last_star_p2 = self->subtype;
        star_p2_x = self->x_pos.f.u;
        star_p2_y = self->y_pos.f.u;
    }
    (*SstLoad_State(self)) |= 1;
}

// Star_Post_Animate_Data: 0 the post with its ball, 1 without, 2 the two flickering (every fourth frame)
static void Animate(Sst *self) {
    Own_StarPost *own = (Own_StarPost *)self->own.u8;
    switch (self->anim) {
    case 0:
        self->mapping_frame = 0;
        break;
    case 1:
        self->mapping_frame = 1;
        break;
    case 2:
        if (--own->anim_time == 0xFF) {
            own->anim_time = 3;
            own->anim_step ^= 1;
        }
        self->mapping_frame = own->anim_step ? 4 : 0;
        break;
    }
}

static void Ball(Sst *self) {
    Own_StarPost *own = (Own_StarPost *)self->own.u8;
    if ((int16_t)--own->timer < 0) {
        Sst *post = &sst[own->parent];
        if (post->code == Obj_StarPost) { // (it flickers from now on)
            post->anim = 2;
            post->mapping_frame = 0;
        }
        Sst_Delete(self);
        return;
    }
    const uint8_t angle = self->angle;
    self->angle = (uint8_t)(self->angle - 0x10);
    int16_t sine, cosine;
    CalcSine((uint8_t)(angle - 0x40), &sine, &cosine);
    self->x_pos.f.u = (int16_t)(own->base_x + (int16_t)(((int32_t)cosine * 0xC00) >> 16));
    self->y_pos.f.u = (int16_t)(own->base_y + (int16_t)(((int32_t)sine * 0xC00) >> 16));
    Sst_Draw(self);
    SstDeleteIfNotInRange2(self, self->x_pos.f.u);
}

// loc_1B3CC: a bonus star: one that is touched by the first player sends him to the special stage; it circles and grows, then shrinks away
static void BonusStar(Sst *self) {
    Own_StarPost *own = (Own_StarPost *)self->own.u8;
    if (self->collision_property) {
        if (self->collision_property & 1) {
            star_bonus_stage = true;
            gamemode = GameMode_Special;
        }
        self->collision_property = 0;
    }
    own->angle_sum = (uint16_t)(own->angle_sum + 0xA);
    int16_t sine, cosine;
    CalcSine((uint8_t)own->angle_sum, &sine, &cosine);
    int16_t d0 = (int16_t)(sine >> 5), d1 = (int16_t)(cosine >> 4);
    int16_t d3 = d1;
    int16_t d2 = (int16_t)((own->angle_sum & 0x7C0) >> 5);
    int16_t d4 = 0;
    if (d2 > 0x10)
        d1 = (int16_t)-d1;
    for (int i = 0; i < 3; i++) { // (the loop at loc_1B42A: it keeps shifting the same d2)
        d2 &= 0xF;
        if (d2 > 8) {
            d2 = (int16_t)-d2;
            d2 &= 7;
        }
        d2 = (int16_t)(d2 >> 1);
        if (d2 != 0)
            d4 = (int16_t)(d4 + d1);
        d1 = (int16_t)(d1 << 1);
    }
    d4 = (int16_t)(d4 >> 3);
    d0 = (int16_t)(d0 + d4);
    own->timer++;
    int16_t scale = (int16_t)own->timer;
    bool scaled = true;
    if (scale >= 0x40) {
        scaled = false;
        if (scale > 0x1C0) {
            scale = (int16_t)(-scale + 0x200);
            if (scale < 0) {
                Sst_Delete(self);
                return;
            }
            scaled = true;
        }
    }
    if (scaled) {
        d0 = (int16_t)(((int32_t)d0 * scale) >> 5);
        d3 = (int16_t)(((int32_t)d3 * scale) >> 5);
    }
    self->x_pos.f.u = (int16_t)(own->base_x + d3);
    self->y_pos.f.u = (int16_t)(own->base_y + d0);
    own->flicker++;
    int f = (own->flicker & 6) >> 1;
    if (f == 3)
        f = 1;
    self->mapping_frame = (uint8_t)f;
    Sst_Draw(self);
    SstDeleteIfNotInRange2(self, self->x_pos.f.u);
}

void Obj_StarPost(Sst *self) {
    switch (self->routine) {
    case 0: {
        self->routine = 2;
        self->mappings = Mappings_StarPost;
        self->art_tile = ART_TILE;
        self->render_flags = SST_RENDER_LEVEL;
        self->width_pixels = 8;
        self->priority = 5 * 0x80;
        const bool hit = ((*SstLoad_State(self)) & 1) || ((last_lamp & 0x7F) >= (self->subtype & 0x7F));
        if (hit) {
            (*SstLoad_State(self)) |= 1;
            self->anim = 2;
        }
        // (on into the check)
    }
        __attribute__((fallthrough));
    case 2:
        if (!debug_use && !(lock_multi & 0x80)) {
            CheckPlayer(self, player, false);
            if (true_two_player)
                CheckPlayer(self, TAILS_OBJ, true);
        }
        Animate(self);
        Sst_Draw(self);
        SstDeleteIfNotInRange2(self, self->x_pos.f.u);
        break;
    case 6:
        Ball(self);
        break;
    case 8:
        BonusStar(self);
        break;
    }
}
