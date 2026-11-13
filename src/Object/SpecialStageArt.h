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
#define ArtTile_SpecialStageBombs           0x3C0 // $7800: the bombs' frame, copied in a frame at a time
#define ArtTile_SpecialStageBombsExplosion  0x3D0 // $7A00
#define ArtTile_SpecialStageSonic         0x2F2 // $5E40: Sonic's art, streamed a frame at a time from the buffers (Offset_0x02EED4)
#define ArtTile_SpecialStageMiles         0x381 // $7020: Miles' art
#define ArtTile_SpecialStageMilesTail     0x375 // $6EA0: his tails'
#define ArtTile_SpecialStageBlank         0x7C0 // (this port's: a tile with nothing in it, which the widescreen track's sides are made of: the background shows)
#define ArtTile_SpecialStageSolid         0x7D0 // (this port's: 16 tiles, each of one colour: the walls' padding at the sides of the widescreen track is the edge pixel of each line carried out, 8 pixels)

#endif //_SPECIALSTAGEART_H
