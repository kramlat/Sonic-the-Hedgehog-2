#ifndef _SPECIALSTAGEART_H
#define _SPECIALSTAGEART_H

// Where the pre-beta's special stage puts its art in VRAM (as tile numbers; the stage's lists are $3C and $3D: Sonic2PLC.c). The characters' art is not here: it is streamed frame by frame (stage 4).
#define ArtTile_SpecialStageEmerald         0x174 // $2E80
#define ArtTile_SpecialStageWingsHand       0x1A2 // $3440
#define ArtTile_SpecialStageBackground      0x700 // $E000: the track's background (the stage's plane B)
#define ArtTile_SpecialStageHudFont         0x237 // $46E0
#define ArtTile_SpecialStagePlayerShadow    0x279 // $4F20
#define ArtTile_SpecialStageDiagonalShadow  0x29F // $53E0
#define ArtTile_SpecialStageVerticalShadow  0x2D9 // $5B20
#define ArtTile_SpecialStageTrafficLights   0x30D // $61A0: the lights of the start; the rings' art goes over them (list $3D) once it has gone
#define ArtTile_SpecialStageRings           0x30D // $61A0
#define ArtTile_SpecialStageBombsExplosion  0x3D0 // $7A00

#endif //_SPECIALSTAGEART_H
