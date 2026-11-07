#ifndef _MTZBOSS_H
#define _MTZBOSS_H

#include "Sst.h"

// Metropolis's boss of the pre-beta (objects 54 and 53): Robotnik's ship (54) goes to and fro above the arena with seven balls (53, which the ship makes) orbiting it. Each hit lets one ball go: it falls, and a small
// Robotnik walks out of it (hurting, unless it is rolled into). After seven hits it stops guarding itself with the balls and shoots (a second kind of 54) while it dives; the eighth hit blows it up. A third kind of 54
// follows the ship (a part of its picture).
#define ObjId_MTZBoss 0x54
#define ObjId_MTZBossBall 0x53

void Obj_MTZBoss(Sst *self);
void Obj_MTZBossBall(Sst *self);

// DynResize_Mz3 (act 3 of Metropolis; nothing in the others): the camera stops at the arena and the boss comes
void MTZBoss_Events(void);

// Where a level starts: the counter of the events is cleared
void MTZBoss_Reset(void);

#endif //_MTZBOSS_H
