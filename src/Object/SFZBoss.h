#ifndef _SFZBOSS_H
#define _SFZBOSS_H

#include "Sst.h"

// $C5, the Sky Fortress boss of the pre-beta (Obj_0xC5_SFz_Boss). Nothing in the pre-beta's build creates it (no layout places it and no event makes it; the Sky Fortress events make no boss), so it is not seen unless the
// editor places it. Its subtype less $88 is its routine: $8A the body that makes the rest, $8C a flashing platform, $8E a part that follows another, $90 a part that bounces inside a box, $92 a part that moves up and down.
void Obj_SFZBoss(Sst *self);

#endif //_SFZBOSS_H
