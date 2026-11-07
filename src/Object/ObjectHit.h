#ifndef _OBJECTHIT_H
#define _OBJECTHIT_H

#include "Sst.h"

// The pre-beta's object hit ($27: the puff where a badnik or a monitor was destroyed, with an animal) and the points that rise from it ($29). Sonic 1's explosion and points (shared code) differ in the art (Sonic 2's
// is Explosn.nem at tile $5A4), the first frame's time (4 frames, not 8) and the points' palette (line 0, not 1).
void Obj_ObjectHit(Sst *self);
void Obj_EnemyPoints(Sst *self);
const uint8_t *ObjectHit_Mappings(void); // (the smoke of the Sky Fortress Tornado uses the puff's frames)

#define ArtTile_ObjectHit 0x5A4 // (the pre-beta's list $02 loads it at $B480)

#endif //_OBJECTHIT_H
