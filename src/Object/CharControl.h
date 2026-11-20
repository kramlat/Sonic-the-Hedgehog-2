#ifndef _CHARCONTROL_H
#define _CHARCONTROL_H

#include "Object.h"
#include "Level.h"
#include "LevelCollision.h"

// The prototype's per-character "object control" byte ($2A of Sonic's and of Tails' object): bit 0 stops his own movement, bit 7 switches his collisions with objects off. An object that carries one
// of them (Chemical Plant's tubes) sets it for that character only. It lives at offset $2E of the character's scratch memory here ($2A is the tumble counter in Nick Arcade's Sonic). Sonic 1's
// global lock_multi (Sonic only) still counts for Sonic.
#define OBJ_CONTROL(chr) ((chr)->own.u8[0x2E - 0x28])

// The prototype's Obj_Player_Flip_Flag ($29 of the character): set by the Metropolis cylinder, it makes his tumble count up even when he runs the other way; cleared when he lands. At offset $2F of the scratch memory here.
#define CHAR_FLIP_FLAG(chr) ((chr)->own.u8[0x2F - 0x28])

static inline uint8_t CharObjControl(const Sst *chr) {
    return (uint8_t)(OBJ_CONTROL(chr) | (chr == player ? lock_multi : 0));
}

// Beta 4's squash check (loc_1ABA4, run at the end of the normal and the rolling modes when he goes slower than $800): the left/right/bottom-solid tile at his centre (4 above his Y) is not a floor he stands on but a
// wall he is inside of when its distance is negative: he is crushed (Kill_Sonic, Kill_Miles). The probe clears the front angle buffer and writes the angle it finds there, as it does, and the character copies it after.
static inline bool CharCrushed(Sst *chr) {
    int16_t speed = chr->ground_vel;
    if (speed < 0)
        speed = (int16_t)-speed;
    if ((uint16_t)speed >= 0x800)
        return false;
    angle_buffer0 = 0;
    return FindFloor(chr, chr->x_pos.f.u, (int16_t)(chr->y_pos.f.u - 4), META_SOLID_LRB, 0, 0x10, &angle_buffer0) < 0;
}

#endif //_CHARCONTROL_H
