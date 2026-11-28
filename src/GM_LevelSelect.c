// The level select: see GM_LevelSelect.h
#include "GM_LevelSelect.h"
#include "Constants.h"
#include "Sound.h"
#include "Game.h"
#include "Level.h"
#include "LevelScroll.h"
#include "Enigma.h"
#include "Nemesis.h"
#include "Palette.h"
#include "Video.h"
#include "Backend/VDP.h"
#include "MenuBackground.h"
#include "MenuIcons.h"
#include "SoundIds.h"
#include "ZoneIds.h"

#include "Resource/S2Menu/LevelSelectMap.h"

#include <string.h>


#define MAP_W 40
#define MAP_H 28
#define NAME_FIELDS 15   // the cells of a zone's name that light up
#define ICON_ROW 22
#define ICON_COL 27
#define SOUND_COL 34
#define SOUND_ROW 18
#define HOLD_DELAY 0xB
#define LS_YELLOW 0x6000

// One entry of the menu (a zone's act, the special stage or the sound test): the zone slot and act, the icon, where the name of the zone is (the row of its first act) and the act's number
typedef struct { uint16_t level; uint8_t icon, name_row, name_col, act_row, act_col; } Item;
#define LS_L 3   // (the columns of the names and of the act numbers)
#define LS_R 22
#define NUM_L 18
#define NUM_R 36
#define SPECIAL_ITEM 30
#define SOUND_ITEM 31
#define ITEM_COUNT 32

// In the order of the zone slots, as the layout draws them. Zones with no icon of their own show the spare one (3). Aquatic Ruin (slot $0F) is not in this layout
static const Item items[ITEM_COUNT] = {
    { LEVEL_ID(ZoneId_EHZ, 0), 0, 1, LS_L, 1, NUM_L }, { LEVEL_ID(ZoneId_EHZ, 1), 0, 1, LS_L, 2, NUM_L },
    { LEVEL_ID(0x01, 0), 3, 3, LS_L, 3, NUM_L }, { LEVEL_ID(0x01, 1), 3, 3, LS_L, 4, NUM_L },          // Rock World
    { LEVEL_ID(ZoneId_WZ, 0), 3, 5, LS_L, 5, NUM_L }, { LEVEL_ID(ZoneId_WZ, 1), 3, 5, LS_L, 6, NUM_L },
    { LEVEL_ID(ZoneId_SSZ, 0), 3, 7, LS_L, 7, NUM_L }, { LEVEL_ID(ZoneId_SSZ, 1), 3, 7, LS_L, 8, NUM_L },
    { LEVEL_ID(ZoneId_MTZ, 0), 1, 9, LS_L, 9, NUM_L }, { LEVEL_ID(ZoneId_MTZ, 1), 1, 9, LS_L, 10, NUM_L }, { LEVEL_ID(ZoneId_MTZ, 2), 1, 9, LS_L, 11, NUM_L },
    { LEVEL_ID(0x05, 0), 3, 12, LS_L, 12, NUM_L }, { LEVEL_ID(0x05, 1), 3, 12, LS_L, 13, NUM_L },       // Ocean Wind (slot 5)
    { LEVEL_ID(ZoneId_SFZ, 0), 10, 14, LS_L, 0, 0 },
    { LEVEL_ID(ZoneId_HTZ, 0), 2, 16, LS_L, 16, NUM_L }, { LEVEL_ID(ZoneId_HTZ, 1), 2, 16, LS_L, 17, NUM_L },
    { LEVEL_ID(ZoneId_HPZ, 0), 3, 18, LS_L, 18, NUM_L }, { LEVEL_ID(ZoneId_HPZ, 1), 3, 18, LS_L, 19, NUM_L },
    { LEVEL_ID(ZoneId_GCZ, 0), 3, 1, LS_R, 1, NUM_R }, { LEVEL_ID(ZoneId_GCZ, 1), 3, 1, LS_R, 2, NUM_R },
    { LEVEL_ID(ZoneId_OOZ, 0), 4, 3, LS_R, 3, NUM_R }, { LEVEL_ID(ZoneId_OOZ, 1), 4, 3, LS_R, 4, NUM_R },
    { LEVEL_ID(ZoneId_MCZ, 0), 5, 5, LS_R, 5, NUM_R }, { LEVEL_ID(ZoneId_MCZ, 1), 5, 5, LS_R, 6, NUM_R },
    { LEVEL_ID(ZoneId_CNZ, 0), 6, 7, LS_R, 7, NUM_R }, { LEVEL_ID(ZoneId_CNZ, 1), 6, 7, LS_R, 8, NUM_R },
    { LEVEL_ID(ZoneId_CPZ, 0), 7, 9, LS_R, 9, NUM_R }, { LEVEL_ID(ZoneId_CPZ, 1), 7, 9, LS_R, 10, NUM_R },
    { LEVEL_ID(ZoneId_DEZ, 0), 11, 11, LS_R, 0, 0 },
    { LEVEL_ID(ZoneId_SCZ, 0), 9, 13, LS_R, 0, 0 },
    { 0, 12, 16, LS_R, 0, 0 },                                                                          // Special Stage
    { 0, MENUICON_SOUND_TEST, 18, LS_R, 0, 0 },                                                         // Sound Test
};

// Left and Right: the item on the other side (by the row)
static const uint8_t side[ITEM_COUNT] = {
    18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 27, 28, 29, 29, 30, 30, 31, 31,   // left to right
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 11, 13, 14, 16,                              // right to left
};

static uint16_t ls_map[MAP_W * MAP_H];
static int sound_test = 0; // ($00-$FF)
static int last_item = 0;

static size_t Cell(int row, int col) {
    return (size_t)PLANE_WIDEADD + PLANE_TALLADD + (size_t)row * PLANE_ROW_BYTES + (size_t)col * 2;
}

static bool Built(uint16_t level) {
    return LEVEL_ZONE(level) < ZoneId_Num && level_load_block[LEVEL_ZONE(level)][LEVEL_ACT(level)].art[0] != NULL;
}

static void PutMapWord(int row, int col, uint16_t line) {
    Plane_Put(&screen1p.plane_a, Cell(row, col), (uint16_t)(ls_map[row * MAP_W + col] + line));
}

static void DrawSoundNumber(uint16_t line) {
    for (int n = 0; n < 2; n++) {
        const int d = (sound_test >> (4 * (1 - n))) & 0xF;
        Plane_Put(&screen1p.plane_a, Cell(SOUND_ROW, SOUND_COL + n), (uint16_t)(line + 0x10 + d + (d >= 10 ? 4 : 0)));
    }
}

// Marks (yellow) or unmarks an item's name, its act's number and, for the sound test, the number
static void Mark(int item, uint16_t line) {
    const Item *it = &items[item];
    for (int i = 0; i < NAME_FIELDS; i++)
        PutMapWord(it->name_row, it->name_col + i, line);
    if (it->act_col != 0)
        PutMapWord(it->act_row, it->act_col, line);
    if (item == SOUND_ITEM)
        DrawSoundNumber(line);
}

static void DrawIcon(int item) {
    MenuIcons_SetPalette(items[item].icon);
    for (int i = 0; i < MENUICON_WORDS; i++)
        Plane_Put(&screen1p.plane_a, Cell(ICON_ROW + i / 4, ICON_COL + i % 4), MenuIcons_Word(items[item].icon * MENUICON_WORDS + i));
}

static void PlaySoundTest(void) {
    const int id = sound_test; // (the number is the engine's own sound id: music from 01, effects after it, silence past the last one)
    if (id >= mus_VsResults && id <= mus_Drowning || id == mus_MzSwap)
        PlayMusic((uint8_t)id);
    else if (id >= sfx_Jump && id <= SOUND_ID_LAST)
        PlaySound((uint8_t)id);
}

LevelSelectPick LevelSelect_Run(void) {
    PaletteFadeOut();
    ClearScreen();
    MenuBackground_LoadCommon();
    MenuIcons_Load();
    static uint8_t map[MAP_W * MAP_H * 2];
    EniDec(S2Menu_LevelSelectMap, map, 0);
    for (int i = 0; i < MAP_W * MAP_H; i++)
        ls_map[i] = (uint16_t)((map[i * 2] << 8) | map[i * 2 + 1]);
    MenuBackground_Init();
    scrpos_x.v = 0;
    scrpos_y.v = 0;
    memset(hscroll_buffer, 0, sizeof(hscroll_buffer));
    Viewport_UploadHScroll(&screen1p, hscroll_buffer, sizeof(hscroll_buffer));
    LevelScroll_BackgroundReset();
    for (int y = 0; y < MAP_H; y++)
        for (int x = 0; x < MAP_W; x++)
            PutMapWord(y, x, 0);
    DrawSoundNumber(0);

    int item = last_item, hold = 0;
    Mark(item, LS_YELLOW);
    DrawIcon(item);
    PlayMusic(mus_Menu);
    PaletteFadeIn();

    LevelSelectPick pick = { LevelSelect_Sega, 0, false };
    for (;;) {
        vbla_routine = 0x04;
        WaitForVBla();
        MenuBackground_Animate();
        const int before = item;
        if ((jpad1_press1 & (JPAD_UP | JPAD_DOWN)) || --hold < 0) {
            hold = HOLD_DELAY;
            const uint8_t held = jpad1_hold1 & (JPAD_UP | JPAD_DOWN);
            if (held & JPAD_UP)
                item = item > 0 ? item - 1 : ITEM_COUNT - 1;
            if (held & JPAD_DOWN)
                item = item < ITEM_COUNT - 1 ? item + 1 : 0;
        } else if (item == SOUND_ITEM) {
            int v = sound_test;
            if (jpad1_press1 & JPAD_LEFT)
                v = v > 0 ? v - 1 : 0xFF;
            if (jpad1_press1 & JPAD_RIGHT)
                v = v < 0xFF ? v + 1 : 0;
            if (jpad1_press1 & JPAD_A)
                v = (v + 0x10) & 0xFF;
            sound_test = v;
            if (jpad1_press1 & (JPAD_B | JPAD_C))
                PlaySoundTest();
        } else if (jpad1_press1 & (JPAD_LEFT | JPAD_RIGHT)) {
            item = side[item];
        }
        Mark(before, 0);
        Mark(item, LS_YELLOW);
        if (item != before)
            DrawIcon(item);

        if (jpad1_press1 & JPAD_START) {
            if (item == SOUND_ITEM) {
                pick.kind = LevelSelect_Sega;
                break;
            }
            if (item == SPECIAL_ITEM) {
                pick.kind = LevelSelect_Special;
                break;
            }
            if (Built(items[item].level)) {
                pick.kind = LevelSelect_Level;
                pick.level = items[item].level;
                break;
            }
        }
    }
    pick.split = (jpad1_hold1 & JPAD_B) != 0;
    last_item = item;
    PaletteFadeOut();
    return pick;
}
