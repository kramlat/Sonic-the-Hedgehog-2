#ifndef _SFZBOSS_H
#define _SFZBOSS_H

#include "Sst.h"

// $C5, Wing Fortress's boss (Beta 4's Obj_0xC5_SFz_Boss): one id with several subtypes (its routine is the subtype less $90). The layout places the body (subtype $92) once; it waits for a character to come under it, lets down
// two solid platforms and the parts that make its machine (a hanging piece that lets down bouncing boxes, a rising part, the beam, Robotnik and his plate), and is beaten by eight hits while it can be hit. The pre-beta's $C5 was
// another, unfinished object that nothing made.
void Obj_SFZBoss(Sst *self);

#endif //_SFZBOSS_H
