#ifndef _SPECIALRESULTS_H
#define _SPECIALRESULTS_H

#include <stdbool.h>

// Beta 4's special stage results screen (object $6F, objects/obj_0x6F.asm; stage 7 of docs/Sonic2SpecialStageB4.md): the card slides in (a longer one if an emerald was won, with the emeralds won blinking), the rings of both
// characters and the emerald's 1000 are counted into the score a frame at a time, and after the last the screen waits two seconds and leaves. Runs to its end and returns; the caller goes on to the level
void SpecialResults_Run(int sonic_rings, int miles_rings, bool got_emerald);

#endif //_SPECIALRESULTS_H
