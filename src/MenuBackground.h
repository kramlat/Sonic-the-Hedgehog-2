#ifndef _MENUBACKGROUND_H
#define _MENUBACKGROUND_H

// The Sonic and Miles background every menu of the final game stands on (the options screen, the versus menus and results, the level select to come): the striped plane B and the 10 tiles
// of the morphing picture. One component so they all look and time alike. Call Init once the screen is clear and the palette is loaded, Animate every frame.
void MenuBackground_LoadCommon(void); // the menu font (tile $10 on) and the menu palette, faded in by the caller
void MenuBackground_Init(void);
void MenuBackground_Animate(void);

#endif //_MENUBACKGROUND_H
