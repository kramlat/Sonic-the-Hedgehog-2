#ifndef _BOSSCOMMON_H
#define _BOSSCOMMON_H

#include "Sst.h"
#include "MathUtil.h"

// What the alpha's bosses of Metropolis and Hill Top share (its Boss_Move, Boss_AnimateSprite and Boss_Hit, and the explosions of a wreck, Boss_Defeated): a buffer for the boss's place as 16.16 numbers and its speed,
// a buffer for the animation of the main sprite and its child, and the boss's own state in its scratch memory.

typedef struct {
    uint8_t boss_routine; // the boss's own routine (the alpha's Obj_Boss_Routine): 0 starts it, 2 runs it
    uint8_t anim_routine; // 0x26: what it is doing (Boss_AnimateSprite also moves it on, at the end of an animation)
    uint8_t hits;         // 0x32: the hit counter that Boss_Hit looks at (Sonic's touch counts it down: a boss is a multi-sprite object)
    uint8_t flash;        // 0x14: how long its hit flash lasts
    uint8_t angle;        // 0x1A: the angle of its bob
    uint8_t flag;         // 0x38
    uint8_t side;         // 0x2C: (Hill Top's) which side of the arena it is on
    uint8_t count;        // 0x3E
} Own_AlphaBoss;

typedef struct {
    dword_s x, y;
    int16_t xsp, ysp;
    int16_t timer;
} BossMoveBuffer;

extern BossMoveBuffer boss_move;
extern uint8_t boss_anim[16];
extern uint8_t boss_flag; // the alpha's Boss_Flag: which boss is there (3: Hill Top's)

// How many hits a boss takes: the base ($8), and 4 more in the cooperative game (the split screen with no versus: two players), so it is not too easy. The Tornado zones' (Sky Chase, Wing Fortress) and the Death Egg's bosses keep their own counts
uint8_t Boss_Hits(uint8_t base);

void Boss_Move(void);
void Boss_AnimateSprite(Sst *self, Own_AlphaBoss *boss, const uint8_t *table);
void Boss_Hit(Sst *self, Own_AlphaBoss *boss);
void Boss_Defeated(Sst *self);
void Boss_DefeatedLoadFlickies(void); // Beta 4's Boss_Defeated_Load_Flickies: the zone's pair of animals and the explosions' art are loaded again (the boss's list had its tiles); the egg prison brings its own

#define ObjId_BossExplosionPiece 0x58

#endif //_BOSSCOMMON_H
