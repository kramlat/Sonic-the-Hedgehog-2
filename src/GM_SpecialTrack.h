#ifndef _GM_SPECIALTRACK_H
#define _GM_SPECIALTRACK_H

#include <stdbool.h>

// A screen that shows the pre-beta's special stage track with no characters (stage 3b of docs/Sonic2SpecialStage.md): the track runs by itself and the pad changes the speed. It is entered from the special stage's
// place while the switch SONIC2_SS_TRACK is set in the environment, until the stage itself is built (the pre-beta's replaces Sonic 1's in the last stage).
extern int ss4_stage;
extern bool ss4_emerald[7];
void GM_SpecialTrack(void);

#endif //_GM_SPECIALTRACK_H
