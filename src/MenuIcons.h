#ifndef _MENUICONS_H
#define _MENUICONS_H

#include <stdint.h>

// The zone icons of the final game's menus (the versus zone menu, the level select): 15 pictures of 4 by 3 cells, each with a palette line of its own, which goes on line 2
#define MENUICON_WORDS 12
#define MENUICON_SOUND_TEST 14 // (the picture of the sound test; 12 is the special stage's, 13 the crossed-out "played" one)
void MenuIcons_Load(void);                  // the art, at tile $80 on
void MenuIcons_SetPalette(int icon);        // the icon's colours into line 2 (now, not faded)
uint16_t MenuIcons_Word(int word);          // a word of the icons' map: icon * MENUICON_WORDS + cell (tile numbers from $80)

#endif //_MENUICONS_H
