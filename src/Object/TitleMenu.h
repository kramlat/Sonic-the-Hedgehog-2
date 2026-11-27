#ifndef _TITLEMENU_H
#define _TITLEMENU_H

#include <stdbool.h>

#include "Sst.h"

void Obj_TitleMenu(Sst *self);
bool TitleMenu_TwoPlayers(void); // the title's menu has 2 PLAYER VS lit
int TitleMenu_Choice(void); // 0 1 PLAYER, 1 2 PLAYER VS, 2 OPTIONS
void TitleMenu_Reset(void); // the choice is 1 PLAYER again

#endif //_TITLEMENU_H
