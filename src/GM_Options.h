#ifndef _GM_OPTIONS_H
#define _GM_OPTIONS_H

#include <stdint.h>

// The final game's options screen (Obj of MenuScreen_Options): PLAYER SELECT (Sonic and Tails / Sonic alone / Tails alone), VS MODE ITEMS (all kinds / teleport only) and SOUND TEST, on the Sonic and
// Miles background. Up and Down pick the box, Left and Right change it (A adds $10 to the sound test), B or C play the sound, Start takes the box: 1 player game, 2 player versus, or (the sound test's) the SEGA screen.
typedef enum { Options_Start1P, Options_StartVersus, Options_Sega } OptionsResult;
OptionsResult Options_Run(void);

// The choices, kept across visits (the final's Player_option and Two_player_items)
extern uint8_t options_player;    // 0 Sonic and Tails, 1 Sonic alone, 2 Tails alone
extern uint8_t options_vs_items;  // 0 all kinds of items, 1 teleport only

#endif //_GM_OPTIONS_H
