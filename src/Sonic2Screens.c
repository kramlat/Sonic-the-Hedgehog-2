#include "Game.h"

#include "GM_Continue.h"
#ifdef SCP_COUNTDOWN
#include "GM_Countdown.h"
#endif
#include "GM_Ending.h"
#include "GM_Level.h"
#include "GM_Sega.h"
#ifdef SCP_SPLASH
#include "GM_SSRG.h"
#endif
#include "GM_Special.h"
#include "GM_SpecialTrack.h"
#include <stdlib.h>
#include "GM_Title.h"

#include "Backend/VDP.h"
#include "Camera.h"
#include "HTZQuake.h"
#include "Sprites.h"

// Sonic 2's screens, by game mode (the engine's Screen.h). They start as Sonic 1's -- the SEGA logo, the title, the level, the special stage, the continue screen, the ending
// and the credits -- and each is replaced by Sonic 2's own as it is written (the title first: Sonic 2's emblem over an Emerald Hill background).
// A level is where the shadow/highlight mode may be on (a level started with C held, see Game_LevelObjects): it ends with the level, and the split screen too
static void Screen_Level(void) {
    GM_Level();
    HTZQuake_Reset(); // (the sprites follow the camera again)
    VDP_SetShadowHighlight(false);
    VDP_SetSplitScreen(VDP_SPLIT_NONE);
    VDP_SetSplitWater(NULL, NULL, 0, 0);
    camera_split = false;
    sprite_split_screen = SPRITE_SPLIT_NONE;
}

// The special stage: the pre-beta's (docs/Sonic2SpecialStage.md). SONIC2_SS_SONIC1 in the environment gives Sonic 1's again (kept while the new one is played through); SONIC2_SS_TRACK is the track viewer
extern bool ss_sonic1_palette_cycle;
static void Screen_Special(void) {
    ss_sonic1_palette_cycle = getenv("SONIC2_SS_SONIC1") != NULL;
    if (ss_sonic1_palette_cycle)
        GM_Special();
    else
        GM_SpecialTrack();
    ss_sonic1_palette_cycle = true;
}

// SONIC2_START_SS=<stage> starts in the special stage (for the captures and the checks)
static void Screen_Sega(void) {
    const char *start = getenv("SONIC2_START_SS");
    if (start != NULL) {
        ss4_stage = atoi(start);
        gamemode = GameMode_Special;
        return;
    }
    GM_Sega();
}

const ScreenFunc game_screens[] = {
    [GameMode_Sega] = Screen_Sega,
    [GameMode_Title] = GM_Title,
    [GameMode_Demo] = Screen_Level,
    [GameMode_Level] = Screen_Level,
    [GameMode_Editor] = Screen_Level,
    [GameMode_Special] = Screen_Special,
    [GameMode_Continue] = GM_Continue,
    [GameMode_Ending] = GM_Ending,
    [GameMode_Credits] = GM_Credits,
#ifdef SCP_SPLASH
    [GameMode_SSRG] = GM_SSRG,
#endif
#ifdef SCP_COUNTDOWN
    [GameMode_Countdown] = GM_Countdown,
#endif
};
const int game_screen_count = (int)(sizeof(game_screens) / sizeof(game_screens[0]));
const uint8_t game_boot_screen = GameMode_Sega;
