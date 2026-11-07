#ifndef _OOZBOSS_H
#define _OOZBOSS_H

#include "Sst.h"

// Oil Ocean's boss of the pre-beta (object 55, which also makes the balls of oil it spits and the flames they light: the same object with another routine, 8): Robotnik's ship comes up out of the oil
// and bobs about, and a hit sends it round the arena in a circle of its own tail; then, wearing a tail of eight links, it hovers to and fro and spits a ball of oil at a time, which lights a row of flames where it
// passes the middle. Hit eight times it blows up, sinks and the act ends.
#define ObjId_OOZBoss 0x55

void Obj_OOZBoss(Sst *self);

// DynResize_OOz of act 2 (nothing in act 1): the left edge is fixed and the oil is raised, the arena is shut, and the boss comes once the camera is at Y $1D8
void OOZBoss_Events(void);

// Where a level starts: the counter of the events is cleared
void OOZBoss_Reset(void);

#endif //_OOZBOSS_H
