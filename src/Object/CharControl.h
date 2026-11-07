#ifndef _CHARCONTROL_H
#define _CHARCONTROL_H

#include "Object.h"
#include "Level.h"

// The prototype's per-character "object control" byte ($2A of Sonic's and of Tails' object): bit 0 stops his own movement, bit 7 switches his collisions with objects off. An object that carries one
// of them (Chemical Plant's tubes) sets it for that character only. It lives at offset $2E of the character's scratch memory here ($2A is the tumble counter in Nick Arcade's Sonic). Sonic 1's
// global lock_multi (Sonic only) still counts for Sonic.
#define OBJ_CONTROL(chr) ((chr)->own.u8[0x2E - 0x28])

// The prototype's Obj_Player_Flip_Flag ($29 of the character): set by the Metropolis cylinder, it makes his tumble count up even when he runs the other way; cleared when he lands. At offset $2F of the scratch memory here.
#define CHAR_FLIP_FLAG(chr) ((chr)->own.u8[0x2F - 0x28])

static inline uint8_t CharObjControl(const Sst *chr) {
    return (uint8_t)(OBJ_CONTROL(chr) | (chr == player ? lock_multi : 0));
}

#endif //_CHARCONTROL_H
