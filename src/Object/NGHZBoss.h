#ifndef _NGHZBOSS_H
#define _NGHZBOSS_H

#include "Sst.h"

// Neo Green Hill's boss of the pre-beta (object 89, which makes its own parts: its routine 2 is the ship, 4 the launchers in the walls, the markers of the arrows and the pillars the launchers throw): Robotnik's ship goes
// to and fro over the arena with a hammer, and each time it turns the walls throw an arrow-pillar across.
#define ObjId_NGHZBoss 0x89

void Obj_NGHZBoss(Sst *self);

// DynResize_NGHz of act 2 (nothing in act 1): the camera stops at the arena and the boss comes
void NGHZBoss_Events(void);

// The box of the hammer while it is down (a hurt: the touch code of Boss_Flag 4); false when it is up or there is no boss
bool NGHZBoss_HammerBox(const Sst *hit, int16_t *centre_x, int16_t *centre_y, int16_t *half_width, int16_t *half_height);

// Where a level starts: the boss's flags are cleared
void NGHZBoss_Reset(void);

#endif //_NGHZBOSS_H
