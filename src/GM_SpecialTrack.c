// The pre-beta's special stage track on the screen (a viewer, stage 3b): see GM_SpecialTrack.h
#include "GM_SpecialTrack.h"
#include "SpecialStageDisplay.h"
#include "SpecialStageTables.h"
#include "SpecialStageWide.h"
#include "Object/SpecialStageB4Objects.h"
#include "Object/SpecialStageArt.h"
#include "Object/SpecialStageObjects.h"
#include "Object/Tails.h"
#include "EngineObject.h"
#include "SstLoad.h"
#include "Object.h"

#include "Game.h"
#include "Palette.h"
#include "PLC.h"
#include "Video.h"
#include "Viewport.h"
#include "Sound.h"
#include "Backend/VDP.h"
#include "Backend/Joypad.h"

#include "GM_Level.h"

#include <stdlib.h>
#include <string.h>

// The track's tiles at the start of the tile space ($008C6C: each is a row of 8 pixels, eight times over), and this port's 16 tiles of one colour that carry the track's edge pixels out at the sides of a wide screen (the blank tile beside them is the tile space as it was cleared)
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
    static uint8_t solid[16 * 32];
    for (int colour = 0; colour < 16; colour++)
        memset(solid + colour * 32, colour * 0x11, 32);
    VDP_SeekVRAM((size_t)ArtTile_SpecialStageSolid * 32);
    VDP_WriteVRAM(solid, sizeof(solid));
}

static SSWide wide;

// The plane's name tables are in the scratch plane memory, each as many bytes as the plane (the wide plane's cells across, 32 rows): plane A's two buffers and then plane B
static size_t PlaneBytes(void) {
    return (size_t)wide.plane_width * SS_PLANE_HEIGHT * 2;
}

static size_t BufferAt(int bank) {
    return (size_t)bank * PlaneBytes();
}

static size_t BackgroundAt(void) {
    return 2 * PlaneBytes();
}

// The rows of the slice just decoded, a row at a time into the wide plane (the track's cells in the middle of each stripe and the sides carried on)
static void PutRows(const SSDisplay *d, int bank, int first_row, int rows) {
    plane_t buffer;
    Plane_UseScratchAt(&buffer, BufferAt(bank));
    static uint16_t row[512];
    for (int r = first_row; r < first_row + rows; r++) {
        SSWide_ExpandUnit(&wide, &d->plane_a[bank][r * SS_PLANE_WIDTH], row);
        for (int i = 0; i < wide.plane_width; i++)
            Plane_Put(&buffer, ((size_t)r * wide.plane_width + i) * 2, row[i]);
    }
}

static void ShowPlanes(SSDisplay *d) {
    Plane_UseScratchAt(&screen1p.plane_a, BufferAt(d->shown));
    Plane_UseScratchAt(&screen1p.plane_b, BackgroundAt());
    // (the game's blank uploads its own scroll buffer and vertical scrolls every frame: they are what the picture is made from)
    vid_scrpos_y_dup = 0;
    vid_bg_scrpos_y_dup = (int16_t)(d->b_vscroll >> 16);
    for (int line = 0; line < SCREEN_HEIGHT && line < SS_SCROLL_LINES; line++) {
        const int16_t a = d->hscroll[d->display_flipped ? 1 : 0][line][0];
        hscroll_buffer[line][0] = SSWide_ScrollA(&wide, -a / 0x100);
        hscroll_buffer[line][1] = SSWide_ScrollB(&wide, d->hscroll[d->display_flipped ? 1 : 0][line][1]);
    }
}

int ss4_stage; // Beta 4's stage to play: 0 to 6 in the one player play (the number of emeralds won on the way: it goes on only when one is won), 7 to 9 are the versus stages
bool ss4_emerald[7];  // $FFFFFFB2 on: which emeralds are won

// loc_82D4: a stage number of 7 or more (the entry from the level) is the first stage whose emerald is not won; with all seven won the game starts again (false)
static bool ChooseStage(void) {
    if (ss4_stage >= 7) {
        int n = 0;
        while (n < 7 && ss4_emerald[n])
            n++;
        if (n == 7)
            return false;
        ss4_stage = n;
    }
    return true;
}

void GM_SpecialTrack(void) {
    const bool b4 = getenv("SONIC2_SS_PREBETA") == NULL; // (Beta 4's stage is the stage; the pre-beta's stays while it is checked)
    if (b4 && !ChooseStage()) {
        gamemode = GameMode_Sega; // (all seven emeralds are won: the original starts the game again)
        return;
    }
    static SSDisplay d;
    PaletteWhiteOut(); // (the palette goes to white and then fades in to the stage's)
    ClearScreen();
    VDP_SeekVRAM(0);
    VDP_FillVRAM(0, 0x10000);
    SSWide_Layout(&wide, SCREEN_WIDTH);
    Viewport_SetSize(SCREEN_WIDTH, SCREEN_HEIGHT, wide.plane_width, SS_PLANE_HEIGHT);
    LoadTrackTiles();
    if (b4) {
        QuickPLC(PlcId_SSB4Main);
        PalLoad1(PalId_SSB4Main);
        PalLoad1((PaletteId)(PalId_SSB4Stage + (ss4_stage > 9 ? 0 : ss4_stage)));
        SSDisplay_ResetStage(&d, ss4_stage);
    } else {
        QuickPLC(PlcId_SSMain); // (the background's tiles go to $E000, the HUD's font and the rest)
        PalLoad1(PalId_SpecialStage2);
        SSDisplay_Reset(&d);
    }
    Sst_ResetAll();
    if (!b4) {
    player->code = Obj_SpecialStageSonic; // (Sonic and his shadow: the characters of stage 4)
    TAILS_OBJ->code = Obj_SpecialStageMiles; // (and Miles, with his tails, in the second character's place)
    }
    {
        // the background: its map across the whole plane
        plane_t b;
        Plane_UseScratchAt(&b, BackgroundAt());
        for (int row = 0; row < SS_PLANE_HEIGHT; row++)
            for (int column = 0; column < wide.plane_width; column++)
                Plane_Put(&b, ((size_t)row * wide.plane_width + column) * 2, d.plane_b[row * SS_PLANE_WIDTH + (column % 32)]);
    }
    if (b4) {
        SS4Stage_Begin(ss4_stage);
        SS4Stage_Spawn();
    } else {
    for (int i = 0; i < SS_ITEM_SLOTS; i++) { // (the rings and bombs, and their shadows: each follows a place in the layout)
        Sst *item = Sst_Allocate();
        if (item != NULL) { // (a slot is free until its code is set: the first is filled in before the next is asked for, or both would be the same slot)
            item->code = Obj_SpecialStageItem;
            item->subtype = (uint8_t)i;
        }
        Sst *shadow = Sst_Allocate();
        if (shadow != NULL) {
            shadow->code = Obj_SpecialStageItemShadow;
            shadow->subtype = (uint8_t)i;
        }
    }
    SSItems_Reset(SpecialStageObjects_Items());
    SpecialStageObjects_ResetHud();
    {
        static void (*const hud[])(Sst *) = { Obj_SpecialStageHud, Obj_SpecialStageStart };
        for (int i = 0; i < 2; i++) {
            Sst *o = Sst_Allocate();
            if (o != NULL)
                o->code = hud[i];
        }
        for (int i = 0; i < 6; i++) {
            Sst *o = Sst_Allocate();
            if (o != NULL) {
                o->code = Obj_SpecialStageCounter;
                o->subtype = (uint8_t)i;
            }
        }
        for (int i = 0; i < 2; i++) {
            Sst *o = Sst_Allocate();
            if (o != NULL) {
                o->code = Obj_SpecialStageClock;
                o->subtype = (uint8_t)i;
            }
        }
    }
    }
    PlayMusic(bgm_SS);
    SSDisplay_Prime(&d);
    PutRows(&d, 0, 0, SS_PLANE_HEIGHT);
    PutRows(&d, 1, 0, SS_PLANE_HEIGHT);
    ShowPlanes(&d);
    PaletteWhiteIn();
    bool rings_art = false;
    const bool viewer = getenv("SONIC2_SS_TRACK") != NULL; // (the track viewer's own keys: Up and Down for the speed, Start to leave)
    gamemode = GameMode_Special;
    VDP_SetShadowHighlight(true); // (the pre-beta turns it on: its shadows are colour 15 of the third line, the operator that darkens what is below it)
    for (;;) {
        vbla_routine = 0x0A;
        WaitForVBla();
        if (viewer) {
            if (jpad1_press1 & JPAD_START)
                break;
            if ((jpad1_press1 & JPAD_UP) && d.speed < 0x0F)
                d.speed++;
            if ((jpad1_press1 & JPAD_DOWN) && d.speed > 8)
                d.speed--;
        } else {
            PauseGame(); // (Start pauses, B and C step a frame, A leaves for the title: the pre-beta's stage has no other way out)
            if (gamemode != GameMode_Special)
                break;
        }
        if (!rings_art && (b4 ? SS4Stage_Started() : SpecialStageObjects_Started())) { // (the traffic lights, or the START banner, are gone: the rings' or the bombs' art goes over theirs, list $3D)
            QuickPLC(b4 ? PlcId_SSB4Bombs : PlcId_SSRings);
            rings_art = true;
        }
        d.locked = b4 && SS4Stage_PadLocked();
        SSDisplay_Frame(&d);
        if (d.frame < SS_TRACK_SLICES) // (a slice came into the buffer being decoded: its 7 rows)
            PutRows(&d, d.bank, d.frame * SS_TRACK_SLICE_UNITS, SS_TRACK_SLICE_UNITS);
        ShowPlanes(&d);
        if (b4) {
            SS4Stage_Frame(&d);
            if (SS4Stage_Over() && !viewer) {
                if (SS4Stage_GotEmerald() && ss4_stage < 7) { // (only an emerald takes the stage on)
                    ss4_emerald[ss4_stage] = true;
                    ss4_stage++;
                }
                gamemode = GameMode_Level; // (the way back to the level; the results are to come)
                break;
            }
        } else {
        SpecialStageObjects_SetTrack(d.speed, d.period_less);
        {
            size_t block_size = 0;
            const uint8_t *block = d.segment >= 0 ? SS_TrackBlock(d.segment, &block_size) : NULL;
            SpecialStageObjects_SetItems(d.track.step, block, block_size, d.track.flipped, d.frame == 4);
            SpecialStageObjects_UpdateItems();
        }
        }
        scrpos_x.v = (int32_t)(-wide.x0) << 16; // (the sprites are at the 256-pixel stage's places: the camera puts them in the middle of a wider screen)
        scrpos_y.v = 0;
        ExecuteObjects();
        uint8_t sprite_i;
        BuildSprites(&sprite_i);
    }
    Viewport_SetSize(SCREEN_WIDTH, SCREEN_HEIGHT, PLANE_WIDTH, PLANE_HEIGHT);
    VDP_SetShadowHighlight(false);
    if (viewer)
        gamemode = GameMode_Sega;
}
