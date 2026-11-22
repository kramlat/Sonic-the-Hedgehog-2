// Beta 4's character of the special stage: see SpecialStageB4Character.h
#include "SpecialStageB4Character.h"
#include "MathUtil.h"

#include "Resource/SSData/B4CharacterArtScript.h"

#include <string.h>

static void Sine(uint8_t angle, int *sin, int *cos) {
    int16_t s, c;
    CalcSine(angle, &s, &c);
    *sin = s;
    *cos = c;
}

void SS4Player_Init(SS4Player *p, bool miles) { // loc_2FE34 and loc_30CF0
    memset(p, 0, sizeof(*p));
    p->miles = miles;
    p->routine = 2;
    p->x = 0;
    p->y = (int32_t)0x80 << 16;
    p->radius = miles ? 0x80 : 0x6E;
    p->flip_period = 4;
    p->angle = 0x40;
    p->priority = miles ? 2 : 3;
    p->screen_x = (int16_t)(0 + SS4_ORIGIN_X);
    p->screen_y = (int16_t)(0x80 + SS4_ORIGIN_Y);
}

static int16_t XWord(const SS4Player *p) {
    return (int16_t)(p->x >> 16);
}

static int16_t YWord(const SS4Player *p) {
    return (int16_t)(p->y >> 16);
}

static void SetXWord(SS4Player *p, int16_t v) { // (a word written at $2A leaves the low word)
    p->x = (int32_t)(((uint32_t)(uint16_t)v << 16) | ((uint32_t)p->x & 0xFFFF));
}

static void SetYWord(SS4Player *p, int16_t v) {
    p->y = (int32_t)(((uint32_t)(uint16_t)v << 16) | ((uint32_t)p->y & 0xFFFF));
}

// loc_30474: the pad turns it round the pipe (the left adds to the inertia, the right takes from it, to $600 each way); let go, it slows by an eighth a frame, and for $1E frames the slope does not pull
static void Steer(SS4Player *p, uint8_t held) {
    int16_t d2 = p->inertia;
    if (held & 0x04) {
        d2 = (int16_t)(d2 + 0x60);
        if (d2 > 0x600)
            d2 = 0x600;
    } else if (held & 0x08) {
        d2 = (int16_t)(d2 - 0x60);
        if (d2 < -0x600)
            d2 = -0x600;
    } else {
        if (!(p->status & SS4_STATUS_RELEASED))
            p->let_go_time = 0x1E;
        p->status |= SS4_STATUS_RELEASED;
        d2 = (int16_t)(d2 - (d2 >> 3));
        p->inertia = d2;
        if (p->let_go_time != 0)
            p->let_go_time--;
        return;
    }
    p->inertia = d2;
    p->status &= (uint8_t)~SS4_STATUS_RELEASED;
    p->let_go_time = 0;
}

// loc_304F4: the slope pulls it back to the bottom (by the cosine of the angle, $60), and at the top with hardly any speed it falls off the pipe
static void Slope(SS4Player *p) {
    if (p->let_go_time == 0) {
        int sin, cos;
        Sine(p->angle, &sin, &cos);
        p->inertia = (int16_t)(p->inertia + ((cos * 0x60) >> 8));
    }
    if (p->angle & 0x80) {
        const uint8_t d0 = (uint8_t)(p->angle + 4);
        if (d0 >= 0x88) {
            const int speed = p->inertia < 0 ? -p->inertia : p->inertia;
            if (speed < 0x100)
                p->routine = 8;
        }
    }
}

// loc_30334: the radius goes to $80 for the one in front and to $6E for the other, a step a frame; the nearer is drawn over the other (priority 2 against 3)
static void Radius(SS4Player *p, const SS4World *world) {
    int16_t r = p->radius;
    const bool grow = p->miles ? !world->front : world->front;
    if (grow) {
        if (r == 0x80)
            return;
        r++;
    } else {
        if (r == 0x6E)
            return;
        r--;
    }
    p->radius = r;
    p->priority = (uint16_t)r >= 0x77 ? 2 : 3;
}

// loc_30534: the angle goes on by the inertia's high byte, and the place is the angle's on the circle
static void Place(SS4Player *p) {
    const int16_t i = p->inertia;
    if (i < 0)
        p->angle = (uint8_t)(p->angle - (((uint16_t)-i) >> 8));
    else
        p->angle = (uint8_t)(p->angle + (((uint16_t)i) >> 8));
    int sin, cos;
    Sine(p->angle, &sin, &cos);
    SetXWord(p, (int16_t)((cos * p->radius) >> 8));
    SetYWord(p, (int16_t)((sin * p->radius) >> 8));
}

// loc_3056E: the place on the screen: across by $CC of 256, from the middle of the pipe
static void Screen(SS4Player *p) {
    p->screen_x = (int16_t)(((XWord(p) * 0xCC) >> 8) + SS4_ORIGIN_X);
    p->screen_y = (int16_t)(YWord(p) + SS4_ORIGIN_Y);
}

// loc_3003E, loc_30042: a jump button (B, C or A) throws it across the circle, from the angle's other side
static void Jump(SS4Player *p, uint8_t pressed) {
    if (!(pressed & 0x70))
        return;
    int sin, cos;
    Sine((uint8_t)(p->angle + 0x80), &sin, &cos);
    p->x_vel = (int16_t)(p->x_vel + ((0x780 * cos) >> 8));
    p->y_vel = (int16_t)(p->y_vel + ((0x780 * sin) >> 7));
    p->status |= SS4_STATUS_AIR;
    p->routine = 4;
    p->anim = 3;
    p->anim_time = 0;
    p->anim_frame = 0;
    p->hit = 0;
    p->sound_jump = true;
}

// loc_30128: the pad in the air
static void AirSteer(SS4Player *p, uint8_t held) {
    if (held & 0x04)
        p->x_vel = (int16_t)(p->x_vel - 0x40);
    else if (held & 0x08)
        p->x_vel = (int16_t)(p->x_vel + 0x40);
}

// loc_300FC: the place goes on by the speed, and the speed down by $A8
static void Integrate(SS4Player *p) {
    p->x += (int32_t)p->x_vel * 256;
    const int16_t y_vel = p->y_vel;
    p->y_vel = (int16_t)(p->y_vel + 0xA8);
    p->y += (int32_t)y_vel * 256;
}

// loc_3014A: the angle from where it is (a ratio of the two distances, 32 to the eighth of a turn)
static void AngleFromPlace(SS4Player *p) {
    int32_t d2 = YWord(p), d3 = XWord(p);
    uint8_t angle;
    if (d2 >= 0) {
        if (d3 >= 0) {
            if ((uint16_t)d3 < (uint16_t)d2) {
                angle = (uint8_t)(0x40 - (uint8_t)(((uint32_t)d3 << 5) / (uint16_t)d2));
            } else if (d3 == d2 && d3 == 0) {
                p->angle = 0x40;
                return;
            } else {
                angle = (uint8_t)(((uint32_t)d2 << 5) / (uint16_t)d3);
            }
        } else {
            d3 = -d3;
            if ((uint16_t)d3 < (uint16_t)d2)
                angle = (uint8_t)(((uint32_t)d3 << 5) / (uint16_t)d2 + 0x40);
            else
                angle = (uint8_t)(0x80 - (uint8_t)(((uint32_t)d2 << 5) / (uint16_t)d3));
        }
    } else {
        d2 = -d2;
        if (d3 < 0) {
            d3 = -d3;
            if ((uint16_t)d3 >= (uint16_t)d2)
                angle = (uint8_t)(((uint32_t)d2 << 5) / (uint16_t)d3 + 0x80);
            else
                angle = (uint8_t)(0xC0 - (uint8_t)(((uint32_t)d3 << 5) / (uint16_t)d2));
        } else {
            if ((uint16_t)d3 < (uint16_t)d2)
                angle = (uint8_t)(((uint32_t)d3 << 5) / (uint16_t)d2 + 0xC0);
            else
                angle = (uint8_t)(0x100 - (uint8_t)(((uint32_t)d2 << 5) / (uint16_t)d3));
        }
    }
    p->angle = angle;
}

// loc_302AA: coming down (below the middle) to the circle again, it lands
static void Land(SS4Player *p) {
    const int y = YWord(p);
    if (y <= 0)
        return;
    const int x = XWord(p);
    const uint32_t dist = (uint32_t)(y * y) + (uint32_t)(x * x);
    const uint32_t r = (uint32_t)((uint16_t)p->radius * (uint16_t)p->radius);
    if (dist < r)
        return;
    p->routine = 2;
    p->status &= (uint8_t)~SS4_STATUS_AIR;
    p->x_vel = p->y_vel = p->inertia = 0;
    p->let_go_time = 0;
    p->status |= SS4_STATUS_RELEASED;
    Place(p);
    Screen(p);
}

// loc_3038A: the animation by the angle: six directions of the eight, each with its animation and its flips
static void ChooseAnimation(SS4Player *p) {
    static const uint8_t anim[8] = { 1, 0, 1, 2, 1, 0, 1, 2 };
    static const uint8_t flips[8] = { 1, 0, 0, 0, 2, 2, 3, 1 };
    if (p->status & SS4_STATUS_AIR) {
        p->anim = 3;
        p->status &= 0xFC;
        return;
    }
    const uint8_t d1 = (uint8_t)((uint8_t)(p->angle - 0x10) >> 5);
    if (anim[d1] == p->anim && p->direction == d1)
        return;
    p->direction = d1;
    p->anim = anim[d1];
    p->status = (uint8_t)((p->status & 0xFC) | flips[d1]);
    if (d1 == 1 || d1 == 5) {
        p->flip_period = 4;
        p->flip_count = 0;
    }
}

// loc_303EA: the script's frames, each as long as half a segment of the track; the running animation turns the character round now and then
static void Animate(SS4Player *p, const SS4World *world, const uint8_t *table) {
    if (p->anim != p->prev_anim) {
        p->anim_frame = 0;
        p->prev_anim = p->anim;
        p->anim_time = 0;
    }
    if (--p->anim_time >= 0)
        return;
    const uint8_t *script = table + ((table[p->anim * 2] << 8) | table[p->anim * 2 + 1]);
    p->anim_time = (int8_t)(world->period_less >> 1);
    if (p->anim == 0) {
        if (--p->flip_count <= 0) {
            p->status ^= 1;
            p->flips ^= 1;
            p->flip_count = p->flip_period;
        }
    }
    uint8_t d0 = script[1 + p->anim_frame];
    if (d0 & 0x80) {
        p->anim_frame = 0;
        d0 = script[1];
    }
    p->frame = d0 & 0x7F;
    p->flips = (uint8_t)((p->flips & 0xFC) | (p->status & 3));
    p->anim_frame++;
}

// loc_302F2: a bomb has touched it: it tumbles, and the rings are lost
static void CheckHit(SS4Player *p, SS4World *world, uint16_t rings) {
    if (p->hit == 0)
        return;
    p->hit = 0;
    p->inertia &= 0x00FF; // (clr.b $14: the high byte)
    world->front = !p->miles;
    if (rings != 0)
        p->sound_ring_loss = true;
    p->hurt = 2;
    p->hurt_time = 0;
}

// loc_2FF3E: tumbling for $20 frames: the angle turns the frame, and once the first step is done the rings spill out
static void Tumble(SS4Player *p, uint16_t rings) {
    static const uint8_t frames[8] = { 4, 0, 4, 0xC, 4, 0, 4, 0xC };
    static const uint8_t flips[8] = { 1, 0, 0, 0, 2, 2, 3, 1 };
    p->hurt_time = (uint8_t)(p->hurt_time + 8);
    if (p->hurt_time == 0)
        p->hurt = 0;
    const uint8_t d0 = (uint8_t)((uint8_t)(p->hurt_time + p->angle - 0x10) >> 5);
    p->flips &= 0xFC;
    p->frame = frames[d0];
    p->flips |= flips[d0];
    if ((uint8_t)(p->hurt_time - 8) == 0) {
        p->hit = 0;
        if (rings != 0)
            p->spill = true;
    }
}

void SS4Player_Step(SS4Player *p, SS4World *world, uint8_t held, uint8_t pressed, uint16_t rings, const uint8_t *script) {
    switch (p->routine) {
    case 2: // loc_2FEEA
        if (p->hurt != 0) {
            Tumble(p, rings);
            Radius(p, world);
            Place(p);
            Screen(p);
            return;
        }
        Steer(p, held);
        Slope(p);
        Radius(p, world);
        Place(p);
        Screen(p);
        Jump(p, pressed);
        ChooseAnimation(p);
        Animate(p, world, script);
        CheckHit(p, world, rings);
        return;
    case 4: // loc_3009C
    case 8: // loc_300CA
        AirSteer(p, held);
        Integrate(p);
        AngleFromPlace(p);
        Land(p);
        Radius(p, world);
        Screen(p);
        if (p->routine == 8)
            ChooseAnimation(p);
        Animate(p, world, script);
        return;
    }
}

// loc_30228: the angle of the pipe's surface under a place across (by its ratio to the radius, 128 steps)
static const uint8_t surface[130] = {
    0x40, 0x40, 0x40, 0x40, 0x41, 0x41, 0x41, 0x42, 0x42, 0x42, 0x43, 0x43, 0x43, 0x44, 0x44, 0x44, 0x45, 0x45, 0x45, 0x46, 0x46, 0x46, 0x47, 0x47, 0x47, 0x48, 0x48, 0x48, 0x48, 0x49, 0x49, 0x49,
    0x4A, 0x4A, 0x4A, 0x4B, 0x4B, 0x4B, 0x4C, 0x4C, 0x4C, 0x4D, 0x4D, 0x4D, 0x4E, 0x4E, 0x4E, 0x4F, 0x4F, 0x50, 0x50, 0x50, 0x51, 0x51, 0x51, 0x52, 0x52, 0x52, 0x53, 0x53, 0x53, 0x54, 0x54, 0x54,
    0x55, 0x55, 0x56, 0x56, 0x56, 0x57, 0x57, 0x57, 0x58, 0x58, 0x59, 0x59, 0x59, 0x5A, 0x5A, 0x5B, 0x5B, 0x5B, 0x5C, 0x5C, 0x5D, 0x5D, 0x5E, 0x5E, 0x5E, 0x5F, 0x5F, 0x60, 0x60, 0x61, 0x61, 0x62,
    0x62, 0x63, 0x63, 0x64, 0x64, 0x65, 0x65, 0x66, 0x66, 0x67, 0x67, 0x68, 0x68, 0x69, 0x6A, 0x6A, 0x6B, 0x6C, 0x6C, 0x6D, 0x6E, 0x6E, 0x6F, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x77, 0x78, 0x7A,
    0x80, 0x00,
};

void SS4Player_Shadow(const SS4Player *p, SS4Shadow *shadow) { // loc_3058E, loc_305B6, loc_30604
    uint8_t angle;
    int x, y;
    if (p->routine == 2) {
        x = p->screen_x;
        y = p->screen_y;
        angle = p->angle;
    } else {
        // loc_301F8: in the air, under it on the pipe's surface
        int ax = XWord(p);
        const bool negative = ax < 0;
        if (negative)
            ax = -ax;
        uint32_t index = ((uint32_t)ax << 7) / (uint16_t)p->radius;
        if (index > 129)
            index = 129;
        uint8_t d0 = surface[index];
        if (!negative)
            d0 = (uint8_t)(0x80 - d0);
        if (YWord(p) < 0)
            d0 = (uint8_t)-d0;
        angle = d0;
        int sin, cos;
        Sine(angle, &sin, &cos);
        x = (int)((((int32_t)cos * p->radius * 0xCC) >> 16)) + SS4_ORIGIN_X;
        y = ((sin * p->radius) >> 8) + SS4_ORIGIN_Y;
    }
    static const struct { uint16_t art; int16_t dx, dy; uint8_t frame, flip; } parts[8] = {
        { 0x6262, 0x14, 0x14, 1, 1 }, { 0x623C, 0, 0x18, 0, 0 }, { 0x6262, -0x14, 0x14, 1, 0 }, { 0x629C, -0x14, 0, 2, 0 },
        { 0x6262, -0x14, -0x14, 7, 0 }, { 0x623C, 0, -0x18, 9, 0 }, { 0x6262, 0x14, -0x14, 7, 1 }, { 0x629C, 0x14, 0, 2, 1 },
    };
    const uint8_t d1 = (uint8_t)((uint8_t)(angle - 0x10) >> 5);
    shadow->art_tile = parts[d1].art;
    x += parts[d1].dx;
    y += parts[d1].dy;
    shadow->frame = parts[d1].frame;
    shadow->flips = parts[d1].flip;
    if (angle & 0x80) {
        if (d1 == 3 || d1 == 7)
            shadow->frame = (uint8_t)(shadow->frame + 3);
        y = y + 2 * (SS4_ORIGIN_Y - y);
    }
    if (p->miles && p->anim == 1 && (p->status & 3) < 2) { // loc_306A4
        x += (p->status & 3) == 0 ? 4 : -4;
        y -= 4;
    }
    shadow->x = (int16_t)x;
    shadow->y = (int16_t)y;
}

int SS4_FramePieces(int who, int frame, SS4ArtPiece *out, int max) { // loc_2FFD6, loc_30E74 and loc_30F36
    static const uint32_t sonic[4] = { 0x0000, 0x0B00, 0x2480, 0x2E20 };
    static const uint32_t miles[4] = { 0x3060, 0x3800, 0x4C80, 0x53C0 };
    static const uint32_t tails[3] = { 0x55C0, 0x5C60, 0x63C0 };
    uint32_t base;
    int entry_index;
    if (who == 2) {
        base = tails[frame < 7 ? 0 : frame < 0x0E ? 1 : 2];
        entry_index = frame + 0x24;
    } else {
        const uint32_t *set = who == 1 ? miles : sonic;
        base = set[frame < 4 ? 0 : frame < 0x0C ? 1 : frame < 0x10 ? 2 : 3];
        entry_index = frame + (who == 1 ? 0x12 : 0);
    }
    const uint8_t *table = SSData_B4CharacterArtScript;
    const uint8_t *entry = table + ((table[entry_index * 2] << 8) | table[entry_index * 2 + 1]);
    const int count = (entry[0] << 8) | entry[1];
    entry += 2;
    int n = 0;
    for (int i = 0; i < count && n < max; i++, entry += 2) {
        const uint16_t word = (uint16_t)((entry[0] << 8) | entry[1]);
        out[n].bytes = (uint32_t)(((word >> 8) & 0xF0) + 0x10) * 2;
        out[n].source = base + (uint32_t)(word & 0xFFF) * 2;
        n++;
    }
    return n;
}
