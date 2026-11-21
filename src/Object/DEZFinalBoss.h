#ifndef _DEZFINALBOSS_H
#define _DEZFINALBOSS_H

#include "Sst.h"

// $C7, the Death Egg's final boss, Beta 4's (Obj_0xC7_DEz_Final_Boss): the giant Robotnik. The layout places it with subtype 2 (the body, which makes ten parts); the subtype of a part is its routine. After Robotnik running
// ($C6) has hopped away, the eye tells the body, which rises and fights: a round of walking, a jump to the character's place (a mark follows him first) with a slam, hands sent out and bombs thrown, with eight hits to take (on
// the body or by the eye). Beaten, it blows up, Sonic is walked to the right, and the screen goes white to the end (Beta 4's goes to the Sega screen).
void Obj_DEZFinalBoss(Sst *self);

#endif //_DEZFINALBOSS_H
