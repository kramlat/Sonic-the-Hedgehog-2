// The pre-beta's special stage character: see SpecialStageCharacter.h
#include "SpecialStageCharacter.h"

#include "MathUtil.h"

#include <string.h>

// What each of the stage's speeds (8 to $C) gives (Offset_0x02F26A: how fast the pad turns the character, Offset_0x02F2C0: the fastest it goes round) and what the pipe's slope adds (Offset_0x02F36E: from the bottom,
// $40 steps, each more than the one before)
static const int16_t steer_table[5] = { 0x0100, 0x0180, 0x0280, 0x0480, 0x0A80 };
static const int16_t max_table[5] = { 0x2000, 0x2800, 0x3000, 0x3800, 0x4400 };
static const uint16_t slope_table[65] = {
    0x0000, 0x0008, 0x0010, 0x0018, 0x0020, 0x0028, 0x0030, 0x0038, 0x0040, 0x0048, 0x0050, 0x0058, 0x0060, 0x0068, 0x0070, 0x0078,
    0x0080, 0x0090, 0x00A0, 0x00B0, 0x00C0, 0x00D0, 0x00E0, 0x00F0, 0x0100, 0x0120, 0x0140, 0x0160, 0x0180, 0x01A0, 0x01C0, 0x01E0,
    0x0200, 0x0240, 0x0280, 0x02C0, 0x0300, 0x0340, 0x0380, 0x03C0, 0x0400, 0x0480, 0x0500, 0x0580, 0x0600, 0x0680, 0x0700, 0x0780,
    0x0800, 0x0900, 0x0A00, 0x0B00, 0x0C00, 0x0D00, 0x0E00, 0x0F00, 0x1000, 0x1200, 0x1400, 0x1600, 0x1800, 0x1A00, 0x1C00, 0x1E00,
    0x2000,
};

// How a character in the air has its angle from how far across it is (Offset_0x02EFEC): $40 at the middle and a step for each 2 pixels, to $80
static uint8_t AirAngle(int16_t centre_x, int16_t x) {
    int d = (int16_t)(centre_x - x);
    const bool negative = d < 0;
    if (negative)
        d = -d;
    d >>= 1;
    uint8_t angle = d <= 0x40 ? (uint8_t)(0x40 + d) : 0x00; // (the table ends in a 0 after $80)
    if (negative)
        angle = (uint8_t)(0x80 - angle);
    return angle;
}

void SSPlayer_Init(SSPlayer *p) { // Offset_0x02ED58
    memset(p, 0, sizeof(*p));
    p->routine = 2;
    p->x = (int32_t)0x007F << 16;
    p->y = (int32_t)0x00A8 << 16;
    p->flip_period = 4; // (the pre-beta has the word $0400 at $30: its high byte is the period, its low byte (var_05) the count)
    p->flip_count = 0;
    p->angle = 0x40;
    p->centre_x = 0x007F;
    p->centre_y = (int16_t)(0x00A8 - 0x0064);
    p->radius_x = 0x0050;
    p->radius_y = 0x0064;
    p->landing_offset = 0x3C;
    p->landing_sum = 0xC8;
}

void SSPlayer_InitMiles(SSPlayer *p) { // Offset_0x02FC30
    SSPlayer_Init(p);
    p->y = (int32_t)0x00B8 << 16;
    p->centre_y = (int16_t)(0x00B8 - 0x0080);
    p->radius_x = 0x0066;
    p->radius_y = 0x0080;
    p->landing_offset = 0x4D;
    p->landing_sum = 0x100;
}

// Offset_0x02F274: the pad turns the character round the pipe, and it slows when the pad is let go
static void Steer(SSPlayer *p, const SSTrackState *track, uint8_t held) {
    if (p->cannot_steer)
        return;
    int index = (int)track->speed - 8;
    index = index < 0 ? 0 : index > 4 ? 4 : index;
    const int16_t step = steer_table[index];
    if (held & 0x04)
        p->inertia = (int16_t)(p->inertia + step);
    if (held & 0x08)
        p->inertia = (int16_t)(p->inertia - step);
    if (!(held & 0x0C)) {
        if (p->inertia > 0)
            p->inertia = (int16_t)(p->inertia - 8);
        else if (p->inertia < 0)
            p->inertia = (int16_t)(p->inertia + 8);
    }
}

// Offset_0x02F2CA: the pipe's slope pulls the character back to the bottom ($40), faster the higher it is; leaving the pipe's range it either goes round to the other side (slowly) or can no longer be steered
static void Slope(SSPlayer *p, const SSTrackState *track) {
    int index = (int)track->speed - 8;
    index = index < 0 ? 0 : index > 4 ? 4 : index;
    const int16_t max = max_table[index];
    int d0 = p->angle;
    if ((int8_t)p->angle <= 0) { // Offset_0x02F312
        if (p->inertia >= 0x2000 || p->inertia <= (int16_t)0xE000) {
            p->cannot_steer = 0xFF;
            return;
        }
        if (p->inertia != 0) {
            if (p->inertia > 0) {
                p->angle = 0x7F;
                d0 = 0x7F;
            } else {
                p->angle = 0x01;
                d0 = 0x01;
            }
        }
    } else {
        p->cannot_steer = 0;
    }
    // Offset_0x02F2E4
    if (d0 == 0x40)
        return;
    if (d0 > 0x40) {
        uint16_t pull = slope_table[(d0 - 0x40) % 65];
        if (p->inertia < 0)
            pull >>= 1;
        p->inertia = (int16_t)(p->inertia - (int16_t)pull);
        if (p->inertia >= 0) { // Offset_0x02F362
            if (max < p->inertia)
                p->inertia = max;
            return;
        }
    } else { // Offset_0x02F348
        uint16_t pull = slope_table[(0x40 - d0) % 65];
        if (p->inertia >= 0)
            pull >>= 1;
        p->inertia = (int16_t)(p->inertia + (int16_t)pull);
        if (p->inertia >= 0) {
            if (max < p->inertia)
                p->inertia = max;
            return;
        }
    }
    // Offset_0x02F304: it goes round the other way: not faster than the fastest
    const uint16_t floor = (uint16_t)-max;
    if (floor >= (uint16_t)p->inertia)
        p->inertia = (int16_t)floor;
}

// Offset_0x02F3F0: round the pipe: the angle goes on by the inertia's top 4 bits, the place is on the ellipse
static void Place(SSPlayer *p) {
    int d2 = p->inertia;
    if (d2 < 0) {
        d2 = (uint16_t)(-d2) >> 12;
        p->angle = (uint8_t)(p->angle - d2);
    } else {
        d2 = (uint16_t)d2 >> 12;
        p->angle = (uint8_t)(p->angle + d2);
    }
    int16_t sin, cos;
    CalcSine(p->angle, &sin, &cos);
    const int16_t x = (int16_t)(((cos * p->radius_x) >> 8) + p->centre_x);
    const int16_t y = (int16_t)(((sin * p->radius_y) >> 8) + p->centre_y);
    p->x = (int32_t)((uint32_t)(uint16_t)x << 16) | (p->x & 0xFFFF);
    p->y = (int32_t)((uint32_t)(uint16_t)y << 16) | (p->y & 0xFFFF);
}

// Offset_0x02EF4A (Miles_In_SS_Sub): a button of A, B and C jumps: along the line from the angle's opposite, $680 strong
static void Jump(SSPlayer *p, uint8_t pressed) {
    if (!(pressed & 0x70))
        return;
    int16_t sin, cos;
    CalcSine((uint8_t)(p->angle + 0x80), &sin, &cos);
    p->x_speed = (int16_t)(p->x_speed + ((cos * 0x680) >> 8));
    p->y_speed = (int16_t)(p->y_speed + ((sin * 0x680) >> 8));
    p->status |= SS_STATUS_AIR;
    p->routine = 4;
    p->anim = 3;
    p->anim_time = 0;
    p->anim_frame = 0;
}

uint16_t SS_Sqrt16(uint16_t value, int iterations) { // Offset_0x02F0A4
    uint16_t guess = 1, previous = 0;
    int d3 = iterations;
    for (;;) {
        d3--;
        if (d3 < 0 || guess == 0) // (the iterations are out: the guess counts if the one before it was just over)
            return (uint16_t)(previous - 1) == guess ? guess : 0;
        previous = guess;
        guess = (uint16_t)(((uint16_t)(value / previous) + previous) >> 1);
        if (guess == previous)
            return guess;
    }
}

// Offset_0x02F02E: in the air the character is back on the pipe when it has gone as far as the ellipse whose two foci are the pipe's middle $3C above and below, with $C8 for the two distances together
static void Land(SSPlayer *p) {
    const int16_t x = (int16_t)(p->x >> 16), y = (int16_t)(p->y >> 16);
    const int16_t d5 = p->landing_offset, d3 = p->landing_sum;
    int16_t d0 = p->centre_y;
    if ((int16_t)(d0 - 8) > y)
        return;
    int16_t d2 = d0;
    d0 = (int16_t)(d0 - d5);
    int32_t d1 = (int32_t)(int16_t)(p->centre_x - x);
    d1 = d1 * d1;
    d0 = (int16_t)(d0 - y);
    int32_t sq = (int32_t)d0 * d0;
    d0 = (int16_t)(sq + d1);
    const uint16_t first = SS_Sqrt16((uint16_t)d0, 15);
    d2 = (int16_t)(d2 + d5);
    d2 = (int16_t)(d2 - y);
    int32_t sq2 = (int32_t)d2 * d2;
    d2 = (int16_t)(sq2 + d1);
    const uint16_t second = SS_Sqrt16((uint16_t)d2, 10);
    const uint16_t total = (uint16_t)(first + second);
    if (total < (uint16_t)d3)
        return;
    p->routine = 2;
    p->status &= (uint8_t)~SS_STATUS_AIR;
    p->x_speed = 0;
    p->y_speed = 0;
    p->inertia = 0;
}

// Offset_0x02F194: the animation by the angle (which side of the pipe it is on, and how it looks across or flipped)
static void ChooseAnimation(SSPlayer *p) {
    static const uint8_t by_angle[8][2] = { { 1, 1 }, { 0, 0 }, { 1, 0 }, { 2, 0 }, { 1, 2 }, { 0, 2 }, { 1, 3 }, { 2, 1 } };
    if (p->status & SS_STATUS_AIR) {
        p->anim = 3;
        return;
    }
    const uint8_t d0 = (uint8_t)(((uint8_t)(p->angle - 0x10)) >> 5);
    if (by_angle[d0][0] == p->anim)
        return;
    p->anim = by_angle[d0][0];
    p->status = (uint8_t)((p->status & 0xFC) | by_angle[d0][1]);
    if (d0 == 1 || d0 == 5) {
        p->flip_period = 4;
        p->flip_count = 0;
    }
}

// Offset_0x02EE4C: hit, the character spins round to where it was (the pipe's angle goes on 8 at a time) and gets up
static void Tumble(SSPlayer *p) {
    static const uint8_t frames[8][2] = { { 4, 1 }, { 0, 0 }, { 4, 0 }, { 0, 2 }, { 4, 2 }, { 0, 3 }, { 4, 3 }, { 0, 1 } };
    uint8_t d0 = (uint8_t)(p->tumble + 8);
    p->tumble = d0;
    if (d0 == p->angle)
        p->hit = 0;
    d0 = (uint8_t)((d0 - 0x10) >> 5);
    p->frame = frames[d0][0];
    p->flips = frames[d0][1] & 3;
}

void SSPlayer_Step(SSPlayer *p, const SSTrackState *track, uint8_t held, uint8_t pressed) {
    if (p->routine == 2) {
        if (p->hit) {
            Tumble(p);
            return;
        }
        Steer(p, track, held);
        Slope(p, track);
        Place(p);
        Jump(p, pressed);
        ChooseAnimation(p);
        return;
    }
    // Offset_0x02EF9C: in the air
    p->x += (int32_t)p->x_speed * 256;
    p->y += (int32_t)p->y_speed * 256;
    p->y_speed = (int16_t)(p->y_speed + 0x38);
    Land(p);
    p->angle = AirAngle(p->centre_x, (int16_t)(p->x >> 16));
    ChooseAnimation(p);
}

void SSPlayer_Animate(SSPlayer *p, const SSTrackState *track, const uint8_t *script_table) {
    const uint8_t anim = p->anim;
    if (anim != p->prev_anim) {
        p->anim_frame = 0;
        p->prev_anim = anim;
        p->anim_time = 0;
    }
    if ((int8_t)--p->anim_time >= 0)
        return;
    const uint8_t *script = script_table + (uint16_t)((script_table[anim * 2] << 8) | script_table[anim * 2 + 1]);
    p->anim_time = track->period_less; // (the frames last as long as the track's segments)
    if (p->anim == 0) { // (running along the bottom: it turns round every flip_count steps)
        if ((int8_t)--p->flip_count <= 0) {
            p->status ^= 1;
            p->flips ^= 1;
            p->flip_count = (int8_t)p->flip_period;
        }
    }
    uint8_t d0 = script[1 + p->anim_frame];
    if (d0 & 0x80) {
        p->anim_frame = 0;
        d0 = script[1];
    }
    p->frame = d0 & 0x7F;
    p->flips = p->status & 3;
    p->anim_frame++;
}

void SSPlayer_Shadow(const SSPlayer *p, SSShadow *shadow) { // Offset_0x02F436
    static const struct { uint16_t art; int16_t dx, dy; uint8_t frame, flips; } look[8] = {
        { 0x629F, 0x14, 0x14, 1, 1 }, { 0x6279, 0, 0x18, 0, 0 }, { 0x629F, -0x14, 0x14, 1, 0 }, { 0x62D9, -0x14, 0, 2, 0 },
        { 0x629F, -0x14, -0x14, 7, 0 }, { 0x6279, 0, -0x18, 9, 0 }, { 0x629F, 0x14, -0x14, 7, 1 }, { 0x62D9, 0x14, 0, 2, 1 },
    };
    int16_t x = (int16_t)(p->x >> 16), y = (int16_t)(p->y >> 16);
    if (p->status & SS_STATUS_AIR) { // (in the air the shadow stays on the pipe: where the angle puts it)
        int16_t sin, cos;
        CalcSine(p->angle, &sin, &cos);
        x = (int16_t)(((cos * p->radius_x) >> 8) + p->centre_x);
        y = (int16_t)(((sin * p->radius_y) >> 8) + p->centre_y);
    }
    const int d1 = (uint8_t)(p->angle - 0x10) >> 5;
    shadow->art_tile = look[d1].art;
    shadow->frame = look[d1].frame;
    shadow->flips = look[d1].flips;
    x = (int16_t)(x + look[d1].dx);
    y = (int16_t)(y + look[d1].dy);
    if ((int8_t)p->angle < 0) { // (below the pipe's middle line: mirrored about it)
        if (d1 == 3 || d1 == 7)
            shadow->frame += 3;
        y = (int16_t)(y + 2 * (p->centre_y - y));
    }
    shadow->x = x;
    shadow->y = y;
}
