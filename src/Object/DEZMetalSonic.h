#ifndef _DEZMETALSONIC_H
#define _DEZMETALSONIC_H

#include "Sst.h"

// Death Egg's boss of the pre-beta (object AF): Metal Sonic fights in front of Robotnik's window. It falls to the floor of the arena and then, over and over in a fixed order, dashes from side to side (a flame behind him),
// leaps, and bursts eight spikes out of himself when he is on his way down; eight hits and he blows up, and the arena opens. It is made of four objects: Metal Sonic himself, the flame behind him, a box on his
// shoulder that hurts while he attacks (never drawn), and Robotnik's window, which opens when he lands and shows Robotnik's mood.
#define ObjId_DEZMetalSonic 0xAF

void Obj_DEZMetalSonic(Sst *self);

// His mappings, for the spikes (the enemy weapon object draws them)
const uint8_t *MetalSonic_Mappings(void);

#endif //_DEZMETALSONIC_H
