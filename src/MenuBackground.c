// The shared Sonic and Miles menu background: see MenuBackground.h
#include "MenuBackground.h"
#include "Constants.h"
#include "Enigma.h"
#include "Video.h"
#include "Backend/VDP.h"
#include "Nemesis.h"
#include "Palette.h"
#include <string.h>

#include "Resource/S2Menu/SonicMiles.h"
#include "Resource/S2Menu/VsMenuBg.h"
#include "Resource/S2Menu/MenuFont.h"
#include "Resource/S2Palette/VersusMenu.h"

#define MAP_W 40
#define MAP_H 28

static int anim_index, anim_time;

static void DrawBackground(void) {
    static uint8_t map[MAP_W * MAP_H * 2];
    EniDec(S2Menu_VsMenuBg, map, 0x6000); // (palette line 3: its own tiles 1-10 are the animation's)
    const int left = PLANE_WIDEADD / 2;
    for (int y = 0; y < PLANE_HEIGHT; y++) {
        for (int x = 0; x < PLANE_WIDTH; x++) {
            const int row = (y - (int)(PLANE_TALLADD / PLANE_ROW_BYTES) + MAP_H * 4) % MAP_H;
            const int col = (((x - left) % 6) + 6) % 6; // (the stripe repeats every 6 cells: the wide screen's sides carry it on)
            const uint8_t *m = map + ((size_t)row * MAP_W + (size_t)col) * 2;
            Plane_Put(&screen1p.plane_b, (size_t)y * PLANE_ROW_BYTES + (size_t)x * 2, (uint16_t)((m[0] << 8) | m[1]));
        }
    }
}

void MenuBackground_Init(void) {
    anim_index = 0;
    anim_time = 0;
    VDP_SeekVRAM(1 * 32);
    VDP_WriteVRAM(S2Menu_SonicMiles, 10 * 32);
    DrawBackground();
}

// The Sonic and Miles animation: frames of 10 tiles at tile 1 (Menu_Animate: frame 0 for 200 frames, then 1, 2 (6 each), 3 (200), 2, 1)
void MenuBackground_Animate(void) {
    static const uint8_t frame[6] = { 0, 10, 20, 30, 20, 10 };
    static const uint8_t time[6] = { 0xC7, 5, 5, 0xC7, 5, 5 };
    if (anim_time-- > 0)
        return;
    VDP_SeekVRAM(1 * 32);
    VDP_WriteVRAM(S2Menu_SonicMiles + (size_t)frame[anim_index] * 32, 10 * 32);
    anim_time = time[anim_index];
    anim_index = (anim_index + 1) % 6;
}

void MenuBackground_LoadCommon(void) {
    VDP_SetBackgroundColour(0);
    VDP_SeekVRAM(0x0200);
    NemDec(S2Menu_MenuFont); // (tile $10 on: the digits, then A-Z from $1E)
    memset(dry_palette_dup, 0, sizeof(dry_palette_dup));
    for (int i = 0; i < 4 * 16; i++) {
        const uint8_t *p = S2Palette_VersusMenu + (size_t)i * 2;
        dry_palette_dup[i / 16][i % 16] = (uint16_t)((p[0] << 8) | p[1]);
    }
}
