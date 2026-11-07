#ifndef _BOSSART_H
#define _BOSSART_H

// Where the art every boss shares is in VRAM (as tile numbers: the lists put it at the same places, $A000, $AC00 and $B000)
#define ArtTile_BossShip      0x500 // Robotnik's ship ($A000)
#define ArtTile_ShipBoost     0x560 // its boost ($AC00)
#define ArtTile_BossExplosion 0x580 // the explosions ($B000)
#define ArtTile_OOZBoss       0x38C // Oil Ocean's boss (: list $2F)
#define ArtTile_MTZBoss       0x37C // Metropolis's boss, its balls and the small Robotniks (F80: list $2E)

#endif //_BOSSART_H
