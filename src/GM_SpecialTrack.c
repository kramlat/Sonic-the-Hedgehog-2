// The pre-beta's special stage track on the screen (a viewer, stage 3b): see GM_SpecialTrack.h
#include "GM_SpecialTrack.h"
#include "SpecialStageDisplay.h"
#include "SpecialStageTables.h"

#include "Game.h"
#include "Palette.h"
#include "PLC.h"
#include "Video.h"
#include "Viewport.h"
#include "Sound.h"
#include "Backend/VDP.h"
#include "Backend/Joypad.h"

#include <string.h>

// The track's tiles at the start of the tile space ($008C6C: each is a row of 8 pixels, eight times over)
static void LoadTrackTiles(void) {
    size_t count;
    const uint8_t *rows = SS_TrackTiles(&count);
    static uint8_t tiles[0x200 * 32];
    if (count > 0x200)
        count = 0x200;
    for (size_t t = 0; t < count; t++)
        for (int line = 0; line < 8; line++)
            memcpy(tiles + t * 32 + line * 4, rows + t * 4, 4);
    VDP_SeekVRAM(0);
    VDP_WriteVRAM(tiles, count * 32);
}

// The plane's name tables are in the scratch plane memory where the pre-beta had them in VRAM: plane A's two buffers at $8000 and $C000, plane B at $A000
static void PutBuffer(const SSDisplay *d, int bank, int first_word, int words) {
    plane_t buffer;
    Plane_UseScratchAt(&buffer, bank ? 0xC000 : 0x8000);
    for (int i = first_word; i < first_word + words; i++)
        Plane_Put(&buffer, (size_t)i * 2, d->plane_a[bank][i]);
}

static void ShowPlanes(SSDisplay *d) {
    Plane_UseScratchAt(&screen1p.plane_a, d->shown ? 0xC000 : 0x8000);
    Plane_UseScratchAt(&screen1p.plane_b, 0xA000);
    screen1p.vsram.a = 0;
    screen1p.vsram.b = (int16_t)(d->b_vscroll >> 16);
    static int16_t table[224][2];
    for (int line = 0; line < 224; line++) {
        table[line][0] = d->hscroll[d->display_flipped ? 1 : 0][line][0];
        table[line][1] = d->hscroll[d->display_flipped ? 1 : 0][line][1];
    }
    Viewport_UploadHScroll(&screen1p, table, sizeof(table));
}

void GM_SpecialTrack(void) {
    static SSDisplay d;
    PaletteWhiteOut(); // (the palette goes to white and then fades in to the stage's)
    ClearScreen();
    VDP_SeekVRAM(0);
    VDP_FillVRAM(0, 0x10000);
    Viewport_SetSize(256, SCREEN_HEIGHT, SS_PLANE_WIDTH, SS_PLANE_HEIGHT); // (the stage runs in the 32-cell wide mode, $8C08: its stripes are 256 pixels)
    LoadTrackTiles();
    QuickPLC(PlcId_SSMain); // (the background's tiles go to $E000, the HUD's font and the rest)
    PalLoad1(PalId_SpecialStage2);
    SSDisplay_Reset(&d);
    {
        plane_t b;
        Plane_UseScratchAt(&b, 0xA000);
        for (int i = 0; i < SS_PLANE_WORDS; i++)
            Plane_Put(&b, (size_t)i * 2, d.plane_b[i]);
    }
    PlayMusic(bgm_SS);
    SSDisplay_Prime(&d);
    PutBuffer(&d, 0, 0, SS_PLANE_WORDS);
    PutBuffer(&d, 1, 0, SS_PLANE_WORDS);
    ShowPlanes(&d);
    PaletteWhiteIn();
    for (;;) {
        vbla_routine = 0x0A;
        WaitForVBla();
        if (jpad1_press1 & JPAD_START)
            break;
        if ((jpad1_press1 & JPAD_UP) && d.speed < 0x0F)
            d.speed++;
        if ((jpad1_press1 & JPAD_DOWN) && d.speed > 8)
            d.speed--;
        if (jpad1_press1 & JPAD_RIGHT)
            d.speed_frac ^= 0x8000;
        SSDisplay_Frame(&d);
        if (d.frame < SS_TRACK_SLICES) // (a slice came into the buffer being decoded)
            PutBuffer(&d, d.bank, d.frame * SS_TRACK_SLICE_WORDS, SS_TRACK_SLICE_WORDS);
        ShowPlanes(&d);
    }
    Viewport_SetSize(SCREEN_WIDTH, SCREEN_HEIGHT, PLANE_WIDTH, PLANE_HEIGHT);
    gamemode = GameMode_Sega;
}
