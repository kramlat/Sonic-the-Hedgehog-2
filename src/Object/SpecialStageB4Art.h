#ifndef _SPECIALSTAGEB4ART_H
#define _SPECIALSTAGEB4ART_H

// Where Beta 4's special stage puts its art in VRAM (tile numbers; its lists $3C and $3D, Sonic2PLC.c). The characters' art is streamed a frame at a time at $5CA0, $6000 and $62C0.
#define ArtTile_SS4Emerald      0x174 // $2E80
#define ArtTile_SS4WingsHand    0x1A2 // $3440
#define ArtTile_SS4HudFont      0x1FA // $3F40
#define ArtTile_SS4Shadow       0x23C // $4780 (the player's; $623C on the items' and characters' shadows)
#define ArtTile_SS4ShadowDiag   0x262 // $4C40
#define ArtTile_SS4ShadowVert   0x29C // $5380
#define ArtTile_SS4Explosion    0x2B5 // $56A0
#define ArtTile_SS4Sonic        0x2E5 // $5CA0
#define ArtTile_SS4Miles        0x300 // $6000
#define ArtTile_SS4MilesTail    0x316 // $62C0
#define ArtTile_SS4Rings        0x322 // $6440
#define ArtTile_SS4Start        0x38A // $7140 (the START banner; the bombs go over it in list $3D)
#define ArtTile_SS4Bombs        0x38A
#define ArtTile_SS4PaymwVs      0x3DF // $7BE0
#define ArtTile_SS4Background   0x700 // $E000
#define ArtTile_SS4Stars        0x77F // $EFE0

#endif //_SPECIALSTAGEB4ART_H
