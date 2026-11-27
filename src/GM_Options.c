// The final game's options screen: see GM_Options.h
#include "GM_Options.h"
#include "Constants.h"
#include "Sound.h"
#include "Game.h"
#include "Level.h"
#include "Enigma.h"
#include "LevelScroll.h"
#include "Nemesis.h"
#include "Palette.h"
#include "Video.h"
#include "Backend/VDP.h"
#include "MenuBackground.h"
#include "SoundIds.h"
#include "PLC.h"
#include "SplitScreen.h"
extern bool hud_player_two; // (Object/HUD.c)

#include "Resource/S2Menu/MenuBox.h"
#include "Resource/S2Menu/OptionsMap.h"

#include <string.h>

uint8_t options_player = 0;
uint8_t options_vs_items = 0;
uint8_t options_miles_swap = 0; // (the Miles swap cheat: the lives counters, the end of act card and the like say MILES)
static int sound_test = 0; // ($00-$FF: the sound id itself, as the number shows it)


#define BOX_W 22
#define BOX_H 8
#define BOX_COL 9
#define BOX_COUNT 3
#define FONT_TILE 0x10
#define BOX_TILE 0x70
#define WORD_YELLOW 0x6000

static const char *const box_title[BOX_COUNT] = { "* PLAYER SELECT *", "* VS MODE ITEMS *", "*  SOUND TEST   *" };
static const char *const player_text[3] = { "SONIC AND TAILS", "SONIC ALONE    ", "TAILS ALONE    " };
static const char *const items_text[2] = { "ALL KINDS ITEMS", "TELEPORT ONLY  " };
static const int box_row[BOX_COUNT] = { 3, 11, 19 };
static const int box_choices[BOX_COUNT] = { 3, 2, 0x100 };

static uint16_t box_selected[BOX_W * BOX_H], box_plain[BOX_W * BOX_H];

static uint8_t CharTile(char c) {
    if (c >= '0' && c <= '9')
        return (uint8_t)(FONT_TILE + (c - '0'));
    if (c >= 'A' && c <= 'Z')
        return (uint8_t)(0x1E + (c - 'A'));
    if (c == '*')
        return 0x1A;
    return 0; // (a space)
}

static void PutText(uint16_t *cells, int offset, const char *text, uint16_t line) {
    for (int i = 0; text[i] != 0; i++)
        cells[offset + i] = (uint16_t)(line | CharTile(text[i]));
}

static size_t Cell(int row, int col) {
    return (size_t)PLANE_WIDEADD + PLANE_TALLADD + (size_t)row * PLANE_ROW_BYTES + (size_t)col * 2;
}

static int Value(int box) {
    return box == 0 ? options_player : box == 1 ? options_vs_items : sound_test;
}

static void DrawBox(int box, bool selected) {
    uint16_t cells[BOX_W * BOX_H];
    memcpy(cells, selected ? box_selected : box_plain, sizeof(cells));
    const uint16_t line = selected ? WORD_YELLOW : 0;
    PutText(cells, 1 * BOX_W + 2, box_title[box], line);
    if (box == 0)
        PutText(cells, 4 * BOX_W + 3, player_text[options_player], line);
    else if (box == 1)
        PutText(cells, 4 * BOX_W + 3, items_text[options_vs_items], line);
    else {
        PutText(cells, 4 * BOX_W + 3, "      00       ", line);
        for (int n = 0; n < 2; n++) { // (the number in hex: the digits' tiles, then A-F after the colon, as the final's HexDump does)
            int d = (sound_test >> (4 * (1 - n))) & 0xF;
            cells[4 * BOX_W + 9 + n] = (uint16_t)(line | (FONT_TILE + d + (d >= 10 ? 4 : 0)));
        }
    }
    for (int y = 0; y < BOX_H; y++)
        for (int x = 0; x < BOX_W; x++)
            Plane_Put(&screen1p.plane_a, Cell(box_row[box] + y, BOX_COL + x), cells[y * BOX_W + x]);
}

static void DrawAll(int current) {
    for (int i = 0; i < BOX_COUNT; i++)
        DrawBox(i, i == current);
}

static void PlaySoundTest(void) {
    const int id = sound_test; // (the number is the engine's own sound id: music from 01, effects after it, silence past the last one)
    if (id >= mus_VsResults && id <= mus_Drowning || id == mus_MzSwap)
        PlayMusic((uint8_t)id);
    else if (id >= sfx_Jump && id <= SOUND_ID_LAST)
        PlaySound((uint8_t)id);
}

uint8_t Options_PlayerChoice(void) {
    if (true_two_player || two_player_mode || demo != 0)
        return 0;
    return options_player;
}

OptionsResult Options_Run(void) {
    PaletteFadeOut();
    ClearScreen();
    MenuBackground_LoadCommon();
    VDP_SeekVRAM(BOX_TILE * 32);
    NemDec(S2Menu_MenuBox);
    static uint8_t map[BOX_W * BOX_H * 2];
    EniDec(S2Menu_OptionsMap, map, BOX_TILE);          // (palette 0: the selected one)
    for (int i = 0; i < BOX_W * BOX_H; i++)
        box_selected[i] = (uint16_t)((map[i * 2] << 8) | map[i * 2 + 1]);
    EniDec(S2Menu_OptionsMap, map, 0x2000 | BOX_TILE); // (palette 1: the others)
    for (int i = 0; i < BOX_W * BOX_H; i++)
        box_plain[i] = (uint16_t)((map[i * 2] << 8) | map[i * 2 + 1]);
    MenuBackground_Init();
    scrpos_x.v = 0;
    scrpos_y.v = 0;
    memset(hscroll_buffer, 0, sizeof(hscroll_buffer));
    Viewport_UploadHScroll(&screen1p, hscroll_buffer, sizeof(hscroll_buffer));
    LevelScroll_BackgroundReset();

    int current = 0;
    DrawAll(current);
    PlayMusic(mus_Menu);
    PaletteFadeIn();
    for (;;) {
        vbla_routine = 0x04;
        WaitForVBla();
        MenuBackground_Animate();
        const uint8_t press = (uint8_t)(jpad1_press1 | jpad2_press);
        if (press & JPAD_UP)
            current = current > 0 ? current - 1 : BOX_COUNT - 1;
        if (press & JPAD_DOWN)
            current = current < BOX_COUNT - 1 ? current + 1 : 0;
        int v = Value(current);
        const int max = box_choices[current] - 1;
        if (press & JPAD_LEFT)
            v = v > 0 ? v - 1 : max;
        if (press & JPAD_RIGHT)
            v = v < max ? v + 1 : 0;
        if (current == 2 && (press & JPAD_A))
            v = (v + 0x10) & 0xFF; // (the fixed version: only the sound test steps by $10)
        if (current == 0)
            options_player = (uint8_t)v;
        else if (current == 1)
            options_vs_items = (uint8_t)v;
        else
            sound_test = v;
        if (current == 2 && (press & (JPAD_B | JPAD_C)))
            PlaySoundTest();
        DrawAll(current);
        if (press & JPAD_START)
            break;
    }
    PaletteFadeOut();
    return current == 0 ? Options_Start1P : current == 1 ? Options_StartVersus : Options_Sega;
}

// The final's level start (Level_SetPlayerMode and what follows it): the split screen has the second player's counter, Tails alone has his on the main one. The queue is the level's, or at once (QuickPLC) where VRAM was wiped
void Options_LoadLifeCounter(bool quick) {
    // (as the final adds to the id: the four are in the order MilesLife2P, MilesLife, TailsLife2P, TailsLife, one after the other in PlcIdsExtra.h)
    int plc = PlcId_MilesLife2P;
    if (!two_player_mode && !hud_player_two) {
        if (Options_PlayerChoice() != 2)
            return;
        plc += PlcId_MilesLife - PlcId_MilesLife2P;
    }
    if (!options_miles_swap)
        plc += PlcId_TailsLife2P - PlcId_MilesLife2P;
    if (quick)
        QuickPLC(plc);
    else
        AddPLC(plc);
}
