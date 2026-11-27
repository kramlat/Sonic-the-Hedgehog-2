#ifndef _TITLEINTRO_H
#define _TITLEINTRO_H

#include <stdbool.h>

#include "Sst.h"

void Obj_TitleIntro(Sst *self);
/// Starts the intro: the object that moves Sonic and Tails in over the emblem (the final game's object $0E, with the palette changers' work), and clears what a previous run left
void TitleIntro_Start(void);
/// The intro has run (or Start skipped it): the menu is up and Start takes the choice
bool TitleIntro_Complete(void);
/// The palette lines the intro fades in begin black: the title loads only the one it shows first
void TitleIntro_LoadMenuPalette(void);

#endif //_TITLEINTRO_H
