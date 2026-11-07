#ifndef _CNZBOSS_H
#define _CNZBOSS_H

#include "Sst.h"

// Casino Night's boss of the pre-beta (object 51, which also makes the ball it lets fall: the same object with another routine, 4): Robotnik's ship, with a catcher at each side, goes to and fro along the arena,
// opens its catchers (which hurt) and lets a ball fall from one (the ball falls to the floor, rolls to and fro for a moment and breaks into two that fall away). Hit twice it blows up, the catchers fall, it leaves, and the act ends.
#define ObjId_CNZBoss 0x51

void Obj_CNZBoss(Sst *self);

// DynResize_CNz of act 2 (nothing in act 1): the camera stops at the arena and the boss comes
void CNZBoss_Events(void);

// The box of its open or shut catchers under the ship, which hurts (the touch code of Boss_Flag 6); false when they are not out or there is no boss
bool CNZBoss_CatcherBox(const Sst *hit, int16_t *centre_x, int16_t *centre_y, int16_t *half_width, int16_t *half_height);

// Where a level starts: the catchers' state and the counter of the events are cleared
void CNZBoss_Reset(void);

#endif //_CNZBOSS_H
