#ifndef _STARPOST_H
#define _STARPOST_H

#include "Sst.h"

// Object $79 of Sonic 2 Beta 4: the star post (the checkpoint). Its art is 16 tiles at ArtTile_StarPost.
#define ArtTile_StarPost 0x47C // (Beta 4's own place: the monitor art, 60 tiles from $680, reaches $6BC, so $6B0 was overwritten by its last icons)

void Obj_StarPost(Sst *self);

// The second player's checkpoint (the versus play): the star post he last passed
extern uint8_t last_star_p2;
extern int16_t star_p2_x, star_p2_y;
// Set when a bonus star is touched: the special stage is entered from the post (Beta 4's $FFFFF7CD)
extern bool star_bonus_stage;

#endif //_STARPOST_H
