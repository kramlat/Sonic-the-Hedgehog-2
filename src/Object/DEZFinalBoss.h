#ifndef _DEZFINALBOSS_H
#define _DEZFINALBOSS_H

#include "Sst.h"

// $C7, the giant Robotnik of the Death Egg's last battle (Obj_0xC7_DEz_Final_Boss). The layout places it with subtype $9E (the body, which makes ten parts); the subtype less $9C is the routine of each part. The parts
// are put into motion by tables of how far each moves a frame (a puppet), and some hurt (their collision type is $80 and up). Its sequence starts when Robotnik running ($C6) has hopped away.
void Obj_DEZFinalBoss(Sst *self);

#endif //_DEZFINALBOSS_H
