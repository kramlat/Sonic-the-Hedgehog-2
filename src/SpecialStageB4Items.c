// Beta 4's rings and bombs of the special stage: see SpecialStageB4Items.h
#include "SpecialStageB4Items.h"
#include "SpecialStageB4Tables.h"
#include "MathUtil.h"

#include <string.h>

#define SFX_RING 0xB5
#define SFX_BOMB 0xCB

static void Sine(uint8_t angle, int *sin, int *cos) {
    int16_t s, c;
    CalcSine(angle, &s, &c);
    *sin = s;
    *cos = c;
}

static void Sound(SS4World2 *w, uint16_t id) {
    if (w->sounds < 8)
        w->sound[w->sounds++] = id;
}

void SS4Items_Reset(SS4Items *s, int stage) {
    memset(s, 0, sizeof(*s));
    size_t size;
    s->stream = SS4_StageObjects(stage, &size);
    s->stage = stage;
    s->step_done = -1;
    for (int i = 0; i < SS4_ITEM_MAX; i++)
        s->item[i].link = -1;
}

static int FirstFree(SS4Items *s, int after) {
    for (int i = after; i < SS4_ITEM_MAX; i++)
        if (s->item[i].kind == SS4Kind_None)
            return i;
    return -1;
}

static void Delete(SS4Items *s, int i) {
    memset(&s->item[i], 0, sizeof(SS4Item));
    s->item[i].link = -1;
}

// loc_3170C: the shadow, in a slot after the item, on the side of the pipe by the item's lane
static void MakeShadow(SS4Items *s, int i) {
    const int j = FirstFree(s, i + 1);
    if (j < 0)
        return;
    SS4Item *sh = &s->item[j];
    SS4Item *it = &s->item[i];
    memset(sh, 0, sizeof(*sh));
    sh->kind = SS4Kind_Shadow;
    sh->routine = 4;
    sh->art_tile = 0x623C;
    sh->priority = 5;
    sh->link = i;
    const int8_t lane = (int8_t)it->lane;
    if (lane <= 0x10) {
        sh->flips = 1;
        sh->side = 2;
    } else if (lane <= 0x30) {
        sh->flips = 1;
        sh->side = 1;
    } else if (lane <= 0x50) {
        sh->side = 0;
    } else if (lane <= 0x70) {
        sh->side = 1;
    } else {
        sh->side = 2;
    }
    it->link = j;
}

void SS4Items_MakeStep(SS4Items *s, int step, int kind_length) {
    if (step == s->step_done)
        return;
    s->step_done = step;
    const int d3 = (kind_length - 1) * 4;
    for (;;) {
        const int i = FirstFree(s, 0);
        if (i < 0)
            return;
        const uint8_t d0 = *s->stream++;
        if (d0 & 0x80) { // the end of the step's objects (loc_61C8)
            if (d0 == 0xFF)
                return;
            if (d0 == 0xFE)
                s->check = 0x5A;
            else if (d0 == 0xFD)
                s->check = 0x59;
            else
                s->flash = true;
            return;
        }
        SS4Item *it = &s->item[i];
        memset(it, 0, sizeof(*it));
        it->link = -1;
        if (d0 & 0x40) {
            it->kind = SS4Kind_Bomb;
            it->distance = (int32_t)((((d0 & 0x3F) * 4) + d3) << 16);
        } else {
            s->total_rings++;
            it->kind = SS4Kind_Ring;
            it->distance = (int32_t)(((d0 * 4) + d3) << 16);
        }
        it->lane = *s->stream++;
        it->routine = 0;
    }
}

// loc_3159A: the item comes nearer: its distance goes down, and its anim by the distance; at nothing it is gone. True when it has gone
static bool Move(SS4Items *s, int i, const SS4World2 *w) {
    static const uint8_t anim_by_distance[32] = { 9, 9, 9, 8, 8, 7, 7, 6, 6, 5, 5, 4, 4, 3, 3, 3, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0 };
    SS4Item *it = &s->item[i];
    bool gone = it->gone;
    if (!gone) {
        it->distance -= w->frame_four ? 0xCCCC : 0xCCCD;
        gone = it->distance <= 0;
    }
    if (gone) {
        if (it->link >= 0)
            s->item[it->link].end = true;
        Delete(s, i);
        return true;
    }
    if (it->anim != 0x0A) {
        int d0 = (int)(it->distance >> 16);
        if (d0 > 0x1D)
            d0 = 0x1E;
        it->anim = anim_by_distance[d0];
    }
    return false;
}

// loc_31610: where it is on the screen: the segment's table of places for its distance, and for the lane the angle gives where across and down it is; hidden for some lanes
static void Place(SS4Items *s, int i, const SS4World2 *w) {
    SS4Item *it = &s->item[i];
    const int n = (int)(it->distance >> 16) & 0xFFFF;
    it->visible = false;
    if (n == 0 || n > SS4_PlaceCount(w->segment))
        return;
    const uint8_t *e = SS4_Place(w->segment, n - 1);
    int d2, d3, d4 = e[2], d5 = e[3];
    if (!w->flipped) {
        if (e[5] != 0 && it->lane >= e[4] && it->lane < e[5])
            return;
        d2 = e[0];
    } else {
        if (e[5] != 0) {
            const uint8_t d6 = (uint8_t)(0x80 - e[4]), d7 = (uint8_t)(0x80 - e[5]);
            if (!(it->lane < d7) && it->lane < d6)
                return;
        }
        d2 = 0x100 - e[0];
    }
    d3 = e[1];
    if (e[1] >= 0x48 && e[1] >= 0x80)
        d3 = (int8_t)e[1];
    int sin, cos;
    Sine(it->lane, &sin, &cos);
    it->x = (int16_t)(((cos * d4) >> 8) + d2);
    it->y = (int16_t)(((sin * d5) >> 8) + d3);
    if (it->link >= 0) { // its shadow, on a bigger ellipse
        int a = d4 + (d4 >> 2), b = d5 + (d5 >> 2);
        SS4Item *sh = &s->item[it->link];
        sh->x = (int16_t)(((cos * a) >> 8) + d2);
        sh->y = (int16_t)(((sin * b) >> 8) + d3);
    }
    it->visible = true;
}

// loc_31556: a character at about the item's lane (within `window` of the angle), on the pipe and not tumbling
static bool Touches(const SS4Player *p, uint8_t lane, int window) {
    if (p->routine != 2 || p->hurt != 0)
        return false;
    const uint8_t a = p->angle;
    const uint8_t hi = (uint8_t)(lane + window), lo = (uint8_t)(lane - window);
    const bool carry = (int)lane + window > 0xFF, borrow = lane < window;
    if (carry || borrow)
        return a < hi || a >= lo;
    if (a >= hi)
        return false;
    return a >= lo;
}

// loc_31514: the item at its nearest ($1C is 8) and not yet taken: which of the characters has it (the one with the smaller circle is asked second)
static int Taker(const SS4Item *it, SS4World2 *w, int window) {
    if (it->anim != 8 || it->collision == 0)
        return -1;
    const bool sonic_first = w->player[0]->radius < w->player[1]->radius;
    const int order[2] = { sonic_first ? 0 : 1, sonic_first ? 1 : 0 };
    // (the original asks the one with the smaller circle first, then the other, and the answer is the last one asked: a miss for the first does not stop it)
    for (int k = 0; k < 2; k++)
        if (Touches(w->player[order[k]], it->lane, window))
            return order[k];
    return -1;
}

static void Take(SS4Items *s, int i, SS4World2 *w, int who) {
    SS4Item *it = &s->item[i];
    it->collision = 0;
    it->routine = 6;
    if (it->kind == SS4Kind_Ring) {
        w->rings[who]++;
        uint8_t *d = w->digits[who];
        d[2]++; // the ones, the tens and the hundreds
        if (d[2] >= 10) {
            d[1]++;
            d[2] = 0;
            if (d[1] >= 10) {
                d[0]++;
                d[1] = 0;
            }
        }
        Sound(w, SFX_RING);
    } else {
        w->player[who]->hit = 1;
        it->anim_frame = 0;
        it->anim_time = 0;
        Sound(w, SFX_BOMB);
    }
    if (it->link >= 0) {
        s->item[it->link].end = true;
        it->link = -1;
    }
}

// The animation of the item: a script of frames of a delay, as the rings' and bombs' tables have it (the first byte of a script, the delay; $FF at its end goes back to the first frame)
static void Animate(SS4Item *it, const uint8_t *table) {
    if (--it->anim_time >= 0)
        return;
    const uint8_t *script = table + ((table[it->anim * 2] << 8) | table[it->anim * 2 + 1]);
    it->anim_time = (int8_t)script[0];
    uint8_t d0 = script[1 + it->anim_frame];
    if (d0 & 0x80) {
        it->anim_frame = 0;
        d0 = script[1];
    }
    it->frame = d0 & 0x7F;
    it->anim_frame++;
}

#include "Resource/Animation/SSB4Rings.h"
#include "Resource/Animation/SSB4Bombs.h"

// A ring or a bomb: loc_31464 and loc_3137A (and what is left once it is taken: loc_31484, loc_313DE)
static void RunItem(SS4Items *s, int i, SS4World2 *w) {
    SS4Item *it = &s->item[i];
    const bool bomb = it->kind == SS4Kind_Bomb;
    if (it->routine == 0) { // loc_3142A, loc_3133A: the set-up, and then the first frame of its life
        it->routine = 2;
        it->collision = bomb ? 2 : 1;
        it->priority = 3;
        it->art_tile = bomb ? 0x238A : 0x6322;
        it->x = 0x7F;
        it->y = 0x58;
        if (!(it->lane & 0x80))
            MakeShadow(s, i);
    }
    if (it->routine == 6) { // taken: the sparkle or the puff
        it->anim = 0x0A;
        it->art_tile = bomb ? 0x42B5 : 0x477F;
        if ((int)(it->distance >> 16) < 4)
            it->priority = 1;
    }
    if (Move(s, i, w))
        return;
    Place(s, i, w);
    if (it->routine == 6) {
        Animate(it, bomb ? Animation_SSB4Bombs : Animation_SSB4Rings);
        return;
    }
    if (!bomb) {
        const int who = Taker(it, w, 0x0A);
        if (who >= 0)
            Take(s, i, w, who);
        Animate(it, Animation_SSB4Rings);
        return;
    }
    Animate(it, Animation_SSB4Bombs);
    if (it->visible) {
        const int who = Taker(it, w, 8);
        if (who >= 0)
            Take(s, i, w, who);
    }
}

// loc_317AA: the shadow of an item, as big as the item's anim says
static void RunShadow(SS4Items *s, int i) {
    SS4Item *sh = &s->item[i];
    if (sh->end) {
        Delete(s, i);
        return;
    }
    const SS4Item *owner = &s->item[sh->link];
    if (!owner->visible) {
        sh->visible = false;
        return;
    }
    int d0 = 9 - owner->anim + 1;
    if (d0 == 10)
        d0 = 9;
    sh->art_tile = sh->side == 0 ? 0x623C : sh->side == 1 ? 0x6262 : 0x629C;
    sh->frame = (uint8_t)(d0 * 3 + sh->side);
    sh->visible = true;
}

// loc_31954: a ring that a character lost: it flies, falls (gravity $38 and a pull of $80 more) and is gone off the screen; going down it is behind
static void RunSpilled(SS4Items *s, int i) {
    SS4Item *it = &s->item[i];
    it->px += (int32_t)it->x_vel * 256;
    it->py += (int32_t)it->y_vel * 256;
    it->y_vel = (int16_t)(it->y_vel + 0x38);
    it->y_vel = (int16_t)(it->y_vel + 0x80);
    if (it->y_vel >= 0) {
        it->priority = 0;
        it->anim = 9;
    }
    it->x = (int16_t)(it->px >> 16);
    it->y = (int16_t)(it->py >> 16);
    if (it->x < 0 || (uint16_t)it->x >= 0x100 || it->y > 0xE0) {
        Delete(s, i);
        return;
    }
    Animate(it, Animation_SSB4Rings);
    it->visible = true;
}

int SS4Items_Spill(SS4Items *s, SS4World2 *w, int who) {
    static const uint8_t spread_a[10] = { 0x38, 0x48, 0x2A, 0x56, 0x1C, 0x64, 0x0E, 0x72, 0x00, 0x80 };
    static const uint8_t spread_b[10] = { 0x40, 0x30, 0x50, 0x20, 0x60, 0x10, 0x70, 0x00, 0x80, 0x00 };
    uint8_t *d = w->digits[who];
    const uint8_t *spread = spread_a;
    int n;
    if (d[1] != 0) {
        d[1]--;
        n = 10;
    } else if (d[0] != 0) {
        d[0]--;
        d[1] = 9;
        n = 10;
    } else {
        n = d[2];
        d[2] = 0;
        if (n & 1)
            spread = spread_b;
    }
    w->rings[who] = (uint16_t)(w->rings[who] - n);
    const SS4Player *p = w->player[who];
    int made = 0;
    for (int k = 0; k < n; k++) {
        const int i = FirstFree(s, 0);
        if (i < 0)
            break;
        SS4Item *it = &s->item[i];
        memset(it, 0, sizeof(*it));
        it->kind = SS4Kind_Spilled;
        it->routine = 2;
        it->art_tile = 0x6322;
        it->priority = 5;
        it->anim = 8;
        it->link = -1;
        it->x = p->screen_x;
        it->y = p->screen_y;
        it->px = (int32_t)p->screen_x << 16;
        it->py = (int32_t)p->screen_y << 16;
        int sin, cos;
        Sine((uint8_t)(p->angle + 0x40 + spread[k]), &sin, &cos);
        it->x_vel = (int16_t)((cos * 0x400) >> 8);
        it->y_vel = (int16_t)((sin * 0x1000) >> 8);
        made++;
    }
    return made;
}

void SS4Items_Run(SS4Items *s, SS4World2 *world) {
    for (int i = 0; i < SS4_ITEM_MAX; i++) {
        switch (s->item[i].kind) {
        case SS4Kind_Ring:
        case SS4Kind_Bomb: RunItem(s, i, world); break;
        case SS4Kind_Shadow: RunShadow(s, i); break;
        case SS4Kind_Spilled: RunSpilled(s, i); break;
        default: break;
        }
    }
}
