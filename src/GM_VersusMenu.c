// The 2 player versus menus of Beta 4: see GM_VersusMenu.h
#include "GM_VersusMenu.h"
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
#include "Backend/NetPlay.h"
#include "VersusFlow.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"
#include "Mappings.h"

#include "Resource/S2Menu/MenuFont.h"
#include "Resource/S2Menu/MenuFrame.h"
#include "Resource/S2Menu/SonicMiles.h"
#include "Resource/S2Menu/VsMenuBg.h"
#include "Resource/S2Menu/TwoPMenu.h"
#include "Resource/S2Menu/TwoPCode.h"
#include "Resource/S2Menu/TwoPHostWait.h"
#include "Resource/S2Menu/TwoPGuestWait.h"
#include "Resource/S2Menu/TwoPLoading.h"
#include "Resource/S2Menu/TwoPRefused.h"
#include "Resource/S2Menu/TwoPTimeOut.h"
#include "Resource/S2Palette/VersusMenu.h"
#include "Resource/S2Menu/VsResultFont.h"
#include "Resource/S2Menu/VsActRes.h"
#include "Resource/S2Menu/VsLvlRes.h"
#include "Resource/S2Menu/VsGblRes.h"
#include "Resource/S2Menu/VsSSRes.h"
#include "Resource/S2Menu/VsAllSS.h"
#include "Resource/Mappings/VsBanner.h"
#include "Resource/S2Palette/LevelIcons.h"
#include "Resource/S2Menu/VsMenu.h"
#include "Resource/S2Menu/VsMenu2.h"
#include "Resource/S2Menu/LevelIcons.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#define MAP_W 40
#define MAP_H 28
#define RESOURCE_PADDING 8 // (paradoxmake puts 8 zero bytes after every resource)

#define GAME_NAME "sonic2"
#define SEARCH_FRAMES 1800  // the guest looks for the host for 30 seconds
#define HOST_FRAMES 7200    // the host waits for a guest for 2 minutes
#define CONNECT_FRAMES 600  // the handshake and the start have 10 seconds

// Tile words: the menu font's tiles are $10-$19 (digits) and $1E-$37 (A-Z), on palette line 1 (white) or 3 (yellow, the selected one)
#define WORD_WHITE  0x2000
#define WORD_YELLOW 0x6000

static const char symbols[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";

static uint8_t SymbolTile(char c) {
    if (c >= '0' && c <= '9')
        return (uint8_t)(0x10 + (c - '0'));
    if (c >= 'A' && c <= 'Z')
        return (uint8_t)(0x1E + (c - 'A'));
    return 0;
}

// ---- the menu's art: the cards and icons of the zone menu, which the session screens use for the icon near their bottom ----

static size_t Cell(int row, int col);

#define FRAME_TILE 0x140  // the card frame's tiles
#define BOX_W 17
#define BOX_H 12
#define ICON_WORDS 12     // an icon is 4 by 3 cells
#define PLAYED_ICON 156   // (the icon word where the crossed-out "played" one is in VsMenu2's map)
#define SOUND_TEST_ICON 168

static uint16_t card_selected[BOX_W * BOX_H], card_dim[BOX_W * BOX_H], icon_map[200];

static uint16_t Word(const uint8_t *m, size_t i) {
    return (uint16_t)((m[i * 2] << 8) | m[i * 2 + 1]);
}

static void SetIconPalette(int line) {
    for (int i = 0; i < 16; i++) {
        const uint16_t c = Word(S2Palette_LevelIcons, (size_t)line * 16 + i);
        dry_palette[2][i] = dry_palette_dup[2][i] = c;
    }
}

static void LoadMenuArt(void) {
    static uint8_t map[BOX_W * BOX_H * 2], icons[200 * 2];
    EniDec(S2Menu_VsMenu, map, FRAME_TILE);
    for (int i = 0; i < BOX_W * BOX_H; i++)
        card_selected[i] = Word(map, (size_t)i);
    EniDec(S2Menu_VsMenu, map, 0x2000 | FRAME_TILE);
    for (int i = 0; i < BOX_W * BOX_H; i++)
        card_dim[i] = Word(map, (size_t)i);
    EniDec(S2Menu_VsMenu2, icons, 0x80);
    for (int i = 0; i < 200; i++)
        icon_map[i] = Word(icons, (size_t)i);
    VDP_SeekVRAM(0x1000);
    NemDec(S2Menu_LevelIcons); // (tile $80 on)
}

// The session screens' icon: the card's icon frame (6 by 5 cells) at the bottom, centred (the emblem's ring is to be there), and the 4 by 3 icon in it (`icon`: a word of the map, `line`: its palette line)
static void ShowIcon(int icon, int line) {
    SetIconPalette(line);
    for (int y = 0; y < 5; y++)
        for (int x = 0; x < 6; x++)
            Plane_Put(&screen1p.plane_a, Cell(21 + y, 16 + x), card_selected[(5 + y) * BOX_W + 5 + x]);
    for (int i = 0; i < ICON_WORDS; i++)
        Plane_Put(&screen1p.plane_a, Cell(22 + i / 4, 17 + i % 4), icon_map[icon + i]);
}

static size_t Cell(int row, int col) {
    return (size_t)PLANE_WIDEADD + PLANE_TALLADD + (size_t)row * PLANE_ROW_BYTES + (size_t)col * 2;
}

// ---- the screen: the background (plane B, animated by the Sonic and Miles art), a page (plane A) ----

static uint8_t page_map[MAP_W * MAP_H * 2];
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

// The Sonic and Miles animation: frames of 10 tiles at tile 1 (Menu_Animate: frame 0 for 200 frames, then 1, 2 (6 each), 3 (200), 2, 1)
static void AnimateBackground(void) {
    static const uint8_t frame[6] = { 0, 10, 20, 30, 20, 10 };
    static const uint8_t time[6] = { 0xC7, 5, 5, 0xC7, 5, 5 };
    if (anim_time-- > 0)
        return;
    VDP_SeekVRAM(1 * 32);
    VDP_WriteVRAM(S2Menu_SonicMiles + (size_t)frame[anim_index] * 32, 10 * 32);
    anim_time = time[anim_index];
    anim_index = (anim_index + 1) % 6;
}

// Draws one of the menu maps on plane A (the page): the map's own words, the blank ones included
static void DrawScreen(const uint8_t *eni) {
    EniDec(eni, page_map, 0);
    for (int y = 0; y < MAP_H; y++)
        for (int x = 0; x < MAP_W; x++) {
            const uint8_t *m = page_map + ((size_t)y * MAP_W + (size_t)x) * 2;
            // (the maps' bottom block, from row 20, is the final game's emblem with its wings, whose art is not here yet: only its icon is drawn, in a frame of the cards')
            Plane_Put(&screen1p.plane_a, Cell(y, x), y >= 20 ? 0 : (uint16_t)((m[0] << 8) | m[1]));
        }
}

// Gives the cells of a row between two columns (the words of text, the blank ones left) a palette line: white or yellow
static void ColourRow(int row, int first, int last, uint16_t line) {
    for (int col = first; col <= last; col++) {
        const uint8_t *m = page_map + ((size_t)row * MAP_W + (size_t)col) * 2;
        const uint16_t word = (uint16_t)((m[0] << 8) | m[1]);
        if ((word & 0x7FF) == 0)
            continue;
        Plane_Put(&screen1p.plane_a, Cell(row, col), (uint16_t)((word & 0x9FFF) | line));
    }
}

static void Frame(void) {
    vbla_routine = 0x04;
    WaitForVBla();
    AnimateBackground();
}

static void EnterScreens(void) {
    ClearScreen();
    VDP_SetBackgroundColour(0); // (Beta 4's: colour 0 of line 0. The icons' palette goes on line 2, which the title's backdrop used)
    memset(dry_palette_dup, 0, sizeof(dry_palette_dup));
    VDP_SeekVRAM(0x0200);
    NemDec(S2Menu_MenuFont); // (tile $10 on)
    VDP_SeekVRAM(FRAME_TILE * 32);
    NemDec(S2Menu_MenuFrame); // (not at tile $60 as in Beta 4: the emblem of the session screens has those)
    for (int i = 0; i < 4 * 16; i++) {
        const uint8_t *p = S2Palette_VersusMenu + (size_t)i * 2;
        dry_palette_dup[i / 16][i % 16] = (uint16_t)((p[0] << 8) | p[1]);
    }
    anim_index = 0;
    anim_time = 0;
    LoadMenuArt();
    DrawBackground();
    scrpos_x.v = 0;
    scrpos_y.v = 0;
    memset(hscroll_buffer, 0, sizeof(hscroll_buffer));
    Viewport_UploadHScroll(&screen1p, hscroll_buffer, sizeof(hscroll_buffer));
    LevelScroll_BackgroundReset();
}

static bool AnyButton(void) {
    return (jpad1_press1 & (JPAD_A | JPAD_B | JPAD_C | JPAD_START | JPAD_UP | JPAD_DOWN | JPAD_LEFT | JPAD_RIGHT)) != 0;
}

// ---- the screens ----

typedef enum { Session_Local, Session_Host, Session_Join, Session_Back } SessionChoice;

// LOCAL SPLITSCREEN / HOST SESSION / JOIN SESSION: Up and Down, Start, A or C to choose, B to go back
static SessionChoice SessionMenu(void) {
    static const int rows[3] = { 6, 9, 12 };
    DrawScreen(S2Menu_TwoPMenu);
    static const struct { int icon, line; } icons[3] = { { 144, 12 }, { 12, 1 }, { 0, 0 } }; // (the special stage, Chemical Plant, Emerald Hill)
    int item = 0, shown = -1;
    for (;;) {
        if (shown != item) {
            ShowIcon(icons[item].icon, icons[item].line);
            shown = item;
        }
        for (int i = 0; i < 3; i++)
            ColourRow(rows[i], 0, MAP_W - 1, i == item ? WORD_YELLOW : WORD_WHITE);
        Frame();
        if (jpad1_press1 & JPAD_UP)
            item = (item + 2) % 3;
        if (jpad1_press1 & JPAD_DOWN)
            item = (item + 1) % 3;
        if (jpad1_press1 & (JPAD_START | JPAD_A | JPAD_C)) {
            PlaySound(sfx_Lamppost);
            return (SessionChoice)item;
        }
        if (jpad1_press1 & JPAD_B)
            return Session_Back;
    }
}

typedef enum { Wait_Done, Wait_Cancelled, Wait_Refused, Wait_TimedOut } WaitResult;

static void PutCode(const char *code) {
    int n = 0;
    for (const char *c = code; *c != 0 && n < 20; c++) {
        if (*c == '-' || *c == ' ')
            continue;
        const int row = 7 + n / 10, col = 13 + ((n % 10) / 5) * 6 + n % 5;
        Plane_Put(&screen1p.plane_a, Cell(row, col), (uint16_t)(WORD_WHITE | SymbolTile((char)toupper((unsigned char)*c))));
        n++;
    }
}

// Whether a closed session was turned away (the host denied it, or the connection was refused) as against one that just did not happen
static bool WasRefused(NetPlay *net) {
    char reason[160];
    snprintf(reason, sizeof(reason), "%s", NetPlay_Reason(net));
    for (char *c = reason; *c != 0; c++)
        *c = (char)tolower((unsigned char)*c);
    return strstr(reason, "refus") != NULL || strstr(reason, "password") != NULL || strstr(reason, "denied") != NULL || strstr(reason, "full") != NULL || strstr(reason, "another game") != NULL;
}

// A screen that waits for the session to reach `wanted` (or to be over): B cancels
static WaitResult WaitFor(NetPlay *net, NetPlayStatus wanted, int limit, const uint8_t *eni, const char *code) {
    DrawScreen(eni);
    if (code != NULL)
        PutCode(code);
    for (int frames = 0; frames < limit; frames++) {
        NetPlay_Poll(net);
        Frame();
        const NetPlayStatus status = NetPlay_Status(net);
        if (status == wanted || (wanted == NETPLAY_READY && status == NETPLAY_RUNNING)) {
            PlaySound(sfx_Lamppost); // (on to the next screen)
            return Wait_Done;
        }
        if (status == NETPLAY_CLOSED)
            return WasRefused(net) ? Wait_Refused : Wait_TimedOut;
        if (jpad1_press1 & JPAD_B)
            return Wait_Cancelled;
    }
    return Wait_TimedOut;
}

// CONNECTION REFUSED or SESSION TIME OUT: any button goes back to the session menu
static void ErrorScreen(bool refused) {
    DrawScreen(refused ? S2Menu_TwoPRefused : S2Menu_TwoPTimeOut);
    PlaySound(sfx_Error);
    do
        Frame();
    while (!AnyButton());
}

// ENTER CODE: the 8 by 5 grid of letters and digits, A or C puts one in the field, B takes the last one out (or goes back), Start joins when all 20 are in. Returns false when it went back
static bool CodeScreen(char code[21]) {
    DrawScreen(S2Menu_TwoPCode);
    int length = 0, row = 0, col = 0;
    ShowIcon(SOUND_TEST_ICON, 13);
    memset(code, 0, 21);
    for (;;) {
        // the field: the symbols typed and the '.' boxes left
        for (int n = 0; n < 20; n++) {
            const int r = 7 + n / 10, c = 13 + ((n % 10) / 5) * 6 + n % 5;
            const uint8_t tile = n < length ? SymbolTile(code[n]) : 0x1D;
            Plane_Put(&screen1p.plane_a, Cell(r, c), (uint16_t)(WORD_WHITE | tile));
        }
        // the grid: the cell the cursor is on is yellow
        for (int i = 0; i < 36; i++) {
            const int r = 11 + (i / 8) * 2, c = 11 + (i % 8) * 2;
            Plane_Put(&screen1p.plane_a, Cell(r, c), (uint16_t)(((i == row * 8 + col) ? WORD_YELLOW : WORD_WHITE) | SymbolTile(symbols[i])));
        }
        Frame();
        const int cells_in_row = row == 4 ? 4 : 8;
        if (jpad1_press1 & JPAD_LEFT)
            col = (col + cells_in_row - 1) % cells_in_row;
        if (jpad1_press1 & JPAD_RIGHT)
            col = (col + 1) % cells_in_row;
        if (jpad1_press1 & JPAD_UP)
            row = (row + 4) % 5;
        if (jpad1_press1 & JPAD_DOWN)
            row = (row + 1) % 5;
        if (col >= (row == 4 ? 4 : 8))
            col = row == 4 ? 3 : col;
        if ((jpad1_press1 & (JPAD_A | JPAD_C)) && length < 20) {
            code[length++] = symbols[row * 8 + col];
            PlaySound(sfx_Ring);
        }
        if (jpad1_press1 & JPAD_B) {
            if (length == 0)
                return false;
            code[--length] = 0;
        }
        if ((jpad1_press1 & JPAD_START) && length == 20) {
            PlaySound(sfx_Lamppost);
            return true;
        }
    }
}

// ---- the zone menu: boxes of 17 by 12 cells, four to a page (Beta 4's, made data-driven: a second page is more rows of the table) ----

#define PER_PAGE 4

typedef struct {
    const char *name;     // 12 cells: ' ' is the empty cell
    const char *kind;     // ZONE or STAGE
    int icon;             // the word of VsMenu2's map where its icon starts
    int palette;          // its line of the level icons' palettes
    uint16_t level;       // LEVEL_ID, or SPECIAL_STAGE
} ZoneBox;

#define SPECIAL_STAGE 0xFFFF

static const ZoneBox zone_boxes[] = {
    { "EMERALD HILL", "ZONE ", 0,   0,  LEVEL_ID(ZoneId_EHZ, 0) },
    { " MYSTIC CAVE", "ZONE ", 60,  5,  LEVEL_ID(ZoneId_MCZ, 0) },
    { "CASINO NIGHT", "ZONE ", 72,  6,  LEVEL_ID(ZoneId_CNZ, 0) },
    { "   SPECIAL   ", "STAGE", 144, 12, SPECIAL_STAGE },
};
#define BOX_COUNT ((int)(sizeof(zone_boxes) / sizeof(zone_boxes[0])))
#define PAGE_COUNT ((BOX_COUNT + PER_PAGE - 1) / PER_PAGE)

// What each box has been played to: the two scores' sum, negative when it has not been played (a played one cannot be picked again). The 2 player results are not kept yet.
static int16_t zone_played[BOX_COUNT];

static uint8_t WordTile(char c) {
    if (c == ' ')
        return 0;
    return SymbolTile(c);
}

static void DrawBox(int box, int slot, bool selected) {
    static const int origin_row[PER_PAGE] = { 2, 2, 15, 15 }, origin_col[PER_PAGE] = { 2, 22, 2, 22 };
    const ZoneBox *z = &zone_boxes[box];
    uint16_t cells[BOX_W * BOX_H];
    memcpy(cells, selected ? card_selected : card_dim, sizeof(cells));
    const uint16_t line = selected ? 0x6000 : 0; // (the text of the one chosen is yellow)
    for (int i = 0; i < 12; i++)
        cells[2 * BOX_W + 2 + i] = (uint16_t)(line | WordTile(z->name[i]));
    for (int i = 0; i < 5; i++)
        cells[4 * BOX_W + 6 + i] = (uint16_t)(line | WordTile(z->kind[i]));
    // The one chosen shows its icon, the others a blank box, a played one the crossed-out icon
    const int icon = zone_played[box] >= 0 ? PLAYED_ICON : z->icon;
    for (int i = 0; i < ICON_WORDS; i++) // (the ones not chosen are a blank box until played, like Beta 4's)
        cells[(6 + i / 4) * BOX_W + 6 + i % 4] = (!selected && zone_played[box] < 0) ? (0x2000 | (FRAME_TILE + 0x0B)) : icon_map[icon + i];
    for (int y = 0; y < BOX_H; y++)
        for (int x = 0; x < BOX_W; x++)
            Plane_Put(&screen1p.plane_a, Cell(origin_row[slot] + y, origin_col[slot] + x), cells[y * BOX_W + x]);
}

static void DrawPage(int page, int current) {
    for (int slot = 0; slot < PER_PAGE; slot++)
        if (page * PER_PAGE + slot < BOX_COUNT)
            DrawBox(page * PER_PAGE + slot, slot, page * PER_PAGE + slot == current);
}

// Returns true when a zone (or the special stage) was chosen: `level_id` is set (the special stage is SPECIAL_STAGE in it), false when the player backed out (B)
static bool ZoneMenu(void) {
    for (int i = 0; i < BOX_COUNT; i++)
        zone_played[i] = -1;

    for (int y = 0; y < MAP_H; y++) // (the session screens' words go)
        for (int x = 0; x < MAP_W; x++)
            Plane_Put(&screen1p.plane_a, Cell(y, x), 0);
    int current = 0, shown = -1;
    for (;;) {
        const int page = current / PER_PAGE;
        if (shown != current) {
            SetIconPalette(zone_boxes[current].palette);
            DrawPage(page, current);
            shown = current;
        }
        Frame();
        const uint8_t press = (uint8_t)(jpad1_press1 | jpad2_press);
        int next = current;
        const int slot = current % PER_PAGE;
        if (press & (JPAD_UP | JPAD_DOWN))
            next = page * PER_PAGE + (slot ^ 2);   // (Up and Down: the box above or below)
        if (press & (JPAD_LEFT | JPAD_RIGHT))
            next = page * PER_PAGE + (slot ^ 1);   // (Left and Right: the box beside)
        if (next < BOX_COUNT && next != current)
            current = next;
        if (press & JPAD_B)
            return false;
        if (press & JPAD_START) {
            if (zone_played[current] >= 0) {
                PlaySound(sfx_Error); // (played: it cannot be picked again, Beta 4's $ED)
                continue;
            }
            PlaySound(sfx_Lamppost);
            level_id = zone_boxes[current].level;
            return true;
        }
    }
}

static bool chose_special_stage;

bool VersusMenu_ChoseSpecialStage(void) {
    return chose_special_stage;
}

static bool chose_local;
bool VersusMenu_ChoseLocal(void) {
    return chose_local;
}

extern bool hud_player_two; // (Object/HUD.c)

bool VersusMenu_Run(void) {
    chose_special_stage = false;
    chose_local = false;
    hud_player_two = false;
    EnterScreens();
    PlayMusic(mus_Menu);
    PaletteFadeIn();
    bool result = false;
    for (;;) {
        const SessionChoice choice = SessionMenu();
        hud_player_two = false;
        if (choice == Session_Back)
            break;
        if (choice == Session_Local) {
            if (ZoneMenu()) {
                result = true;
                chose_local = true; // (the split screen: a session online has the one picture)
                break;
            }
            continue;
        }
        NetPlay *net = NetPlay_Create();
        WaitResult wait;
        if (choice == Session_Host) {
            if (!NetPlay_Host(net, GAME_NAME, NULL, 0, NULL, 4)) {
                NetPlay_Destroy(net);
                ErrorScreen(false);
                continue;
            }
            wait = WaitFor(net, NETPLAY_READY, HOST_FRAMES, S2Menu_TwoPHostWait, NetPlay_Password(net));
        } else {
            char code[21];
            if (!CodeScreen(code)) {
                NetPlay_Destroy(net);
                continue;
            }
            if (!NetPlay_JoinByCode(net, GAME_NAME, code)) {
                NetPlay_Destroy(net);
                ErrorScreen(false);
                continue;
            }
            wait = WaitFor(net, NETPLAY_READY, SEARCH_FRAMES + CONNECT_FRAMES, S2Menu_TwoPGuestWait, NULL);
            hud_player_two = true; // (the guest is the second player: his picture has Miles' counter)
        }
        if (wait == Wait_Done && choice == Session_Host) {
            // connected: the host picks the zone, the guest waits for it (the start message), then both load
            if (ZoneMenu()) {
                const uint8_t data[2] = { (uint8_t)(level_id >> 8), (uint8_t)level_id };
                NetPlay_StartMatch(net, data, sizeof(data));
                wait = WaitFor(net, NETPLAY_RUNNING, 60, S2Menu_TwoPLoading, NULL);
                result = true;
            }
        } else if (wait == Wait_Done) {
            wait = WaitFor(net, NETPLAY_RUNNING, CONNECT_FRAMES * 6, S2Menu_TwoPLoading, NULL);
            if (wait == Wait_Done) {
                size_t size = 0;
                const uint8_t *data = NetPlay_StartData(net, &size);
                if (size >= 2)
                    level_id = (uint16_t)((data[0] << 8) | data[1]);
                result = true;
            }
        }
        NetPlay_Stop(net); // (not connected to the game loop yet: the match is played here)
        NetPlay_Destroy(net);
        if (result && wait == Wait_Done)
            break;
        result = false;
        if (wait == Wait_Refused || wait == Wait_TimedOut)
            ErrorScreen(wait == Wait_Refused);
    }
    if (result && level_id == SPECIAL_STAGE)
        chose_special_stage = true;
    PaletteFadeOut();
    VDP_SetBackgroundColour(0x20); // (the title's again)
    return result;
}


// ---- the results pages (Beta 4's Versus_Mode_Results: the five maps, the numbers and the words written into the map before it is shown) ----

static uint16_t rbuf[MAP_W * MAP_H];

static const uint8_t *RTiles(const char *text, int *count) {
    static uint8_t tiles[16];
    int n = 0;
    for (const char *c = text; *c != 0 && n < 16; c++)
        tiles[n++] = *c == ' ' ? 0 : SymbolTile(*c);
    *count = n;
    return tiles;
}

// loc_9124: a word of letters written into the map at a byte offset, on a palette line
static void RText(int offset, const char *text, uint16_t line) {
    int n;
    const uint8_t *t = RTiles(text, &n);
    for (int i = 0; i < n; i++)
        rbuf[offset / 2 + i] = (uint16_t)(line | t[i]);
}

// loc_913C: three digits, the leading zeros left blank (the last one always shows)
static void RNum3(int offset, unsigned value, uint16_t line) {
    static const unsigned div[3] = { 100, 10, 1 };
    bool shown = false;
    for (int i = 0; i < 3; i++) {
        const unsigned d = value / div[i];
        value -= d * div[i];
        if (d != 0)
            shown = true;
        if (shown || i == 2)
            rbuf[offset / 2 + i] = (uint16_t)(line | (0x10 + d));
    }
}

// loc_9182: six digits, every leading zero blank (a 0 is blank)
static void RNum6(int offset, uint32_t value, uint16_t line) {
    static const uint32_t div[6] = { 100000, 10000, 1000, 100, 10, 1 };
    bool shown = false;
    for (int i = 0; i < 6; i++) {
        const unsigned d = (unsigned)(value / div[i]);
        value -= d * div[i];
        if (d != 0)
            shown = true;
        if (shown)
            rbuf[offset / 2 + i] = (uint16_t)(line | (0x10 + d));
    }
}

// loc_90DE: TIED, 1P or 2P (on the yellow line) from a difference, and the tally of the page
static void RWinner(int offset, int diff, int tally[2]) {
    if (diff == 0)
        RText(offset, "TIED", WORD_YELLOW);
    else if (diff > 0) {
        RText(offset, " 1P", WORD_YELLOW);
        tally[0]++;
    } else {
        RText(offset, " 2P", WORD_YELLOW);
        tally[1]++;
    }
}

static const char *const zone_names[4] = { "EMERALD HILL", " MYSTIC CAVE", "CASINO NIGHT", "SPECIAL STAGE" };

static int Cmp(uint32_t a, uint32_t b) {
    return a == b ? 0 : a > b ? 1 : -1;
}

static void RTime(int offset, uint32_t frames) { // min, sec, hundredths (the frames are 60ths: times 1.6875)
    RNum3(offset, frames / 3600, 0);
    RNum3(offset + 6, (frames / 60) % 60, 0);
    RNum3(offset + 12, ((frames % 60) * 0x1B0) >> 8, 0);
}

// loc_8FD8, loc_8F52 and loc_905C: a row of a zone's results: the categories each won and who won, or NO GAME / GAME OVER. `gap` is where the second number is, `win` where the winner
static void REntry(int offset, uint16_t entry, int second, int win, int none_win, int neg_shift, int tally[2]) {
    if (entry == VS_UNPLAYED || entry == VS_GAME_OVER)
        offset += neg_shift;
    if (entry == VS_UNPLAYED) {
        RText(offset, "NO GAME", 0);
        RText(offset + none_win, "    ", 0);
    } else if (entry == VS_GAME_OVER) {
        RText(offset, "GAME OVER", 0);
        RWinner(offset + none_win, (int)(entry >> 8) - (int)(entry & 0xFF), tally);
    } else {
        RNum3(offset, entry >> 8, 0);
        RNum3(offset + second, entry & 0xFF, 0);
        RWinner(offset + second + win, (int)(entry >> 8) - (int)(entry & 0xFF), tally);
    }
}

typedef struct {
    int tally[2];
} RTallies;

// Writes a page into `rbuf` (after its map) and gives the tallies the banner is chosen by
static void FillPage(const VsFlow *f, VsPage page, const VsPlayerStats *a, const VsPlayerStats *b, const uint16_t ss1[3], const uint16_t ss2[3], int round, int tally[2]) {
    switch (page) {
    case VS_PAGE_ACT: {
        RText(0x1F2, zone_names[f->zone], 0);
        RNum3(0x216, (unsigned)f->play + 1, 0);
        RNum6(0x33E, a->score, 0);
        RNum6(0x352, b->score, 0);
        RTime(0x3DA, a->time);
        RTime(0x3EE, b->time);
        RNum3(0x486, a->rings, 0);
        RNum3(0x49A, b->rings, 0);
        RNum3(0x526, a->total_rings, 0); // (the rows are TOTAL RING, then ITEM BOX)
        RNum3(0x53A, b->total_rings, 0);
        RNum3(0x5C6, a->items, 0);
        RNum3(0x5DA, b->items, 0);
        RWinner(0x364, Cmp(a->score, b->score), tally);
        RWinner(0x404, Cmp(b->time, a->time), tally);
        RWinner(0x4A4, Cmp(a->rings, b->rings), tally);
        RWinner(0x544, Cmp(a->total_rings, b->total_rings), tally);
        RWinner(0x5E4, Cmp(a->items, b->items), tally);
        RNum3(0x706, (unsigned)tally[0], 0);
        RNum3(0x70E, (unsigned)tally[1], 0);
        break;
    }
    case VS_PAGE_LEVEL: {
        RText(0x242, zone_names[f->zone], 0);
        static const int rows[3] = { 0x398, 0x488, 0x618 };
        for (int i = 0; i < 3; i++)
            REntry(rows[i], f->results[f->zone][i], 8, 0xC, 0x14, 0, tally);
        break;
    }
    case VS_PAGE_GLOBAL: {
        static const int rows[12] = { 0x208, 0x258, 0x2A8, 0x348, 0x398, 0x3E8, 0x488, 0x4D8, 0x528, 0x5C8, 0x618, 0x668 };
        for (int i = 0; i < 12; i++)
            REntry(rows[i], f->results[i / 3][i % 3], 8, 0x12, 0x16, 4, tally);
        RNum3(0x70A, (unsigned)tally[0], 0);
        RNum3(0x710, (unsigned)tally[1], 0);
        break;
    }
    case VS_PAGE_SPECIAL: {
        RNum3(0x266, (unsigned)round + 1, 0);
        static const int rows[3][2] = { { 0x4D6, 0x4E6 }, { 0x576, 0x586 }, { 0x616, 0x626 } };
        for (int i = 0; i < 3; i++) {
            RNum3(rows[i][0], ss1[i], 0);
            RNum3(rows[i][1], ss2[i], 0);
            RWinner(rows[i][0] + 0x1A, Cmp(ss1[i], ss2[i]), tally);
        }
        break;
    }
    case VS_PAGE_ALL_SPECIAL: {
        static const int rows[3] = { 0x4D4, 0x574, 0x614 };
        for (int i = 0; i < 3; i++)
            REntry(rows[i], f->results[3][i], 0xC, 0x10, 0x14, 0, tally);
        break;
    }
    }
}

// Object $21: the banner (1P WINS, 2P WINS, TIED), at a place for each page, flashing between two palette lines every 8 frames
static int banner_frame;
static const int16_t banner_pos[5][2] = { { 0xF0, 0x148 }, { 0xF0, 0x130 }, { 0xE0, 0x148 }, { 0xF0, 0x148 }, { 0xF0, 0x148 } };
static VsPage banner_page;

static void Obj_VsBanner(Sst *self) {
    if (self->routine == 0) {
        self->routine = 2;
        self->x_pos.f.u = (int16_t)(banner_pos[banner_page][0] + SCREEN_WIDEADD2);
        self->y_pos.f.u = (int16_t)(banner_pos[banner_page][1] + SCREEN_TALLADD2);
        self->mappings = Mappings_VsBanner;
        self->render_flags = 0;
        self->priority = 0;
    }
    self->mapping_frame = (uint8_t)banner_frame;
    self->art_tile = (uint16_t)(0x60 | ((frame_count & 8) ? 0x2000 : 0));
    Sst_Draw(self);
}

void VersusResults_Show(const VsFlow *f, VsPage page, const VsPlayerStats *p1, const VsPlayerStats *p2, const uint16_t ss1[3], const uint16_t ss2[3], int round) {
    static const uint8_t *const maps[5] = { S2Menu_VsActRes, S2Menu_VsLvlRes, S2Menu_VsGblRes, S2Menu_VsSSRes, S2Menu_VsAllSS };
    ClearScreen();
    memset(dry_palette_dup, 0, sizeof(dry_palette_dup));
    VDP_SetBackgroundColour(0);
    VDP_SeekVRAM(0x0200);
    NemDec(S2Menu_MenuFont);
    VDP_SeekVRAM(0x0C00);
    NemDec(S2Menu_VsResultFont); // (tile $60 on)
    for (int i = 0; i < 4 * 16; i++) {
        const uint8_t *p = S2Palette_VersusMenu + (size_t)i * 2;
        dry_palette_dup[i / 16][i % 16] = (uint16_t)((p[0] << 8) | p[1]);
    }
    anim_index = 0;
    anim_time = 0;
    VDP_SeekVRAM(1 * 32);
    VDP_WriteVRAM(S2Menu_SonicMiles, 10 * 32);
    DrawBackground();
    static uint8_t map[MAP_W * MAP_H * 2];
    EniDec(maps[page], map, 0);
    for (int i = 0; i < MAP_W * MAP_H; i++)
        rbuf[i] = Word(map, (size_t)i);
    int tally[2] = { 0, 0 };
    FillPage(f, page, p1, p2, ss1, ss2, round, tally);
    for (int y = 0; y < MAP_H; y++)
        for (int x = 0; x < MAP_W; x++)
            Plane_Put(&screen1p.plane_a, Cell(y, x), rbuf[y * MAP_W + x]);
    Sst_ResetAll();
    banner_page = page;
    banner_frame = tally[0] == tally[1] ? 2 : tally[0] < tally[1] ? 1 : 0;
    Sst *banner = Sst_Allocate();
    if (banner != NULL)
        banner->code = Obj_VsBanner;
    PlayMusic(mus_VsResults);
    PaletteFadeIn();
    for (;;) {
        vbla_routine = 0x0C;
        WaitForVBla();
        AnimateBackground();
        ExecuteObjects();
        BuildSprites(NULL);
        if (jpad1_press1 & JPAD_START)
            break;
    }
    PaletteFadeOut();
}
