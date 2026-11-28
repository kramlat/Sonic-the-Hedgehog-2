#ifndef _GM_OPTIONS_H
#define _GM_OPTIONS_H

#include <stdint.h>
#include <stdbool.h>

// The final game's options screen (Obj of MenuScreen_Options): PLAYER SELECT (Sonic and Tails / Sonic alone / Tails alone), VS MODE ITEMS (all kinds / teleport only) and SOUND TEST, on the Sonic and
// Miles background. Up and Down pick the box, Left and Right change it (A adds $10 to the sound test), B or C play the sound, Start takes the box: 1 player game, 2 player versus, or (the sound test's) the SEGA screen.
typedef enum { Options_Start1P, Options_StartVersus, Options_StartCoop, Options_Sega } OptionsResult;
// (Start with A held on the PLAYER SELECT box is the cooperative game: the split screen with Tails beside Sonic, no versus)
OptionsResult Options_Run(void);

// The choices, kept across visits (the final's Player_option and Two_player_items)
extern uint8_t options_player;    // 0 Sonic and Tails, 1 Sonic alone, 2 Tails alone
// What the choice means for the level that is starting: the character selector is ignored (0, Sonic and Tails) in the versus (true_two_player) and in the split screen (two_player_mode), each of them
// on its own (they are two flags that are not the same: the versus can be online with no split screen, and a level started with B on the level select is both), and in a demo, which was recorded with its characters
uint8_t Options_PlayerChoice(void);
// The online versus's guest (not the host): set by the session menus when this machine joined someone else's match, so it is player 2, and player 2 is Tails: the choice is Tails alone for it, whatever the
// selector says. Only meaningful with true_two_player and no split screen (that pair is an online game; both flags together are a split screen game).
extern uint8_t versus_guest;
extern uint8_t options_vs_items;  // 0 all kinds of items, 1 teleport only

// The name Tails goes by: TAILS, unless the Miles swap cheat is on (the final's Graphics_Flags bit 7 chooses between its two names the same way: its Japanese region has MILES). Nothing sets it yet: the cheat comes later
extern uint8_t options_miles_swap;

// The sound test's codes (B or C plays each number; the ring sounds when the whole row has been played): 19, 65, 09, 17 turn the level select on (A and Start on the title), 01, 01, 02, 04 give 14 continues
extern uint8_t options_level_select_cheat;
extern uint8_t options_continues_cheat;

// The life counter art of Tails (the final's PLCs MilesLife, TailsLife and the 2P "1up patches"), as the level's start loads it: the second player's in the split screen, the main counter's for Tails alone
// (Sonic's is PlcId_Main's). QuickPLC at once where the art was wiped (after the special stage), else queued.
void Options_LoadLifeCounter(bool quick);

#endif //_GM_OPTIONS_H
