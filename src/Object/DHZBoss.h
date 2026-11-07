#ifndef _DHZBOSS_H
#define _DHZBOSS_H

#include "Sst.h"

// Dust Hill's boss of the pre-beta (object 57, which also makes the rocks that fall from the ceiling as it digs): Robotnik's ship, with a drill at each end, comes up out of the floor of the arena, drops down again
// at the other end and charges along the floor at Sonic, with the ceiling shaking rocks loose all the while (a drill out hurts, and the boss gives up its charge when a character is hurt). Defeated, it blows up and the
// drills fall off, and the ship leaves across the arena (the act ends when it has gone). The rocks are the same object with another routine (4).
#define ObjId_DHZBoss 0x57

void Obj_DHZBoss(Sst *self);

// DynResize_DHz of act 2: the arena at the end (the events of the alpha: nothing in act 1)
void DHZBoss_Events(void);

// The boxes of its drills (the touch code of Boss_Flag 5): box `index` (0, then 1 while there is one) in the position the touch test uses, and false when there are no more; the first call also clears its flag of a character hurt.
// The boxes hurt. `DHZBoss_AfterTouch` is told the touching character's time of safety (the flag is set where it has just been hurt, which ends the boss's charge)
bool DHZBoss_Box(Sst *hit, int index, int16_t *centre_x, int16_t *centre_y, int16_t *half_width, int16_t *half_height);
void DHZBoss_AfterTouch(Sst *hit, uint16_t invulnerable_time);

// Where a level starts: the drill's state and the counter of the events are cleared
void DHZBoss_Reset(void);

#endif //_DHZBOSS_H
