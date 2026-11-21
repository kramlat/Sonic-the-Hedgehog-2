#ifndef _INVINCIBILITYSTARS_H
#define _INVINCIBILITYSTARS_H

#include "Sst.h"

// The invincibility stars of the Sonic 2 pre-beta (its object $35): one object that draws eight stars, six of them going round Sonic by his angle and two trailing behind him
void Obj_InvincibilityStars(Sst *self);

// The shield (anim 0) or the stars (anim 1) for a character (`owner`: Sonic or Tails), made if he has none: they follow him and go when his own flag does (VersusStats.h)
void ShieldInvincibility_MakeFor(uint8_t anim, const Sst *owner);

#endif //_INVINCIBILITYSTARS_H
