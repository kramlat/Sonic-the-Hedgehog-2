#ifndef _GM_VERSUSMENU_H
#define _GM_VERSUSMENU_H

#include <stdbool.h>

// The 2 player versus menus of Beta 4 (M5; docs/Sonic2M5.md): the session menu (LOCAL SPLITSCREEN, HOST SESSION, JOIN SESSION), the code screens, the wait, loading, refused and time out screens, and then
// the zone menu (to come: for now the first zone is taken). All play the Level select / Options theme. Returns true with `level_id` set and the two player flags on when a match is to start, false when the player
// went back (B on the session menu). A session (HOST or JOIN) that has connected is ended here for now: it is not connected to the game loop yet (the netplay milestone).
bool VersusMenu_Run(void);
// After a true VersusMenu_Run: the special stage box was chosen (level_id is not a level then)
bool VersusMenu_ChoseSpecialStage(void);

#endif //_GM_VERSUSMENU_H
