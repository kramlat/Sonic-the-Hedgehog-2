#ifndef _GM_LEVELSELECT_H
#define _GM_LEVELSELECT_H

#include <stdbool.h>
#include <stdint.h>

// The level select of the final game, in this port's layout (res/S2Menu/LevelSelectMap): the zones in the order of their slots (ZoneIds.h) in two columns, the special stage and the sound test ($00-$FF), on the shared
// Sonic and Miles background, with the zone's icon in the emblem. Up and Down move (with a repeat), Left and Right change side, or the sound number on the sound test (A adds $10, B and C play it); Start picks.
// A zone that is not built yet does nothing when picked. Start with B held picks the level in the split screen.
typedef enum { LevelSelect_Level, LevelSelect_Special, LevelSelect_Sega } LevelSelectKind;
typedef struct {
    LevelSelectKind kind;
    uint16_t level;  // LEVEL_ID of the zone and act (for LevelSelect_Level)
    bool split;      // B was held: the split screen
} LevelSelectPick;

LevelSelectPick LevelSelect_Run(void);

#endif //_GM_LEVELSELECT_H
