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
static void DrawPage(const uint8_t *eni) {
    EniDec(eni, page_map, 0);
    for (int y = 0; y < MAP_H; y++)
        for (int x = 0; x < MAP_W; x++) {
            const uint8_t *m = page_map + ((size_t)y * MAP_W + (size_t)x) * 2;
            Plane_Put(&screen1p.plane_a, Cell(y, x), (uint16_t)((m[0] << 8) | m[1]));
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
    memset(dry_palette_dup, 0, sizeof(dry_palette_dup));
    VDP_SeekVRAM(0x0200);
    NemDec(S2Menu_MenuFont); // (tile $10 on)
    VDP_SeekVRAM(0x0C00);
    NemDec(S2Menu_MenuFrame); // (tile $60 on)
    for (int i = 0; i < 4 * 16; i++) {
        const uint8_t *p = S2Palette_VersusMenu + (size_t)i * 2;
        dry_palette_dup[i / 16][i % 16] = (uint16_t)((p[0] << 8) | p[1]);
    }
    anim_index = 0;
    anim_time = 0;
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
    static const int rows[3] = { 7, 10, 13 };
    DrawPage(S2Menu_TwoPMenu);
    int item = 0;
    for (;;) {
        for (int i = 0; i < 3; i++)
            ColourRow(rows[i], 0, MAP_W - 1, i == item ? WORD_YELLOW : WORD_WHITE);
        Frame();
        if (jpad1_press1 & JPAD_UP)
            item = (item + 2) % 3;
        if (jpad1_press1 & JPAD_DOWN)
            item = (item + 1) % 3;
        if (jpad1_press1 & (JPAD_START | JPAD_A | JPAD_C))
            return (SessionChoice)item;
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
    DrawPage(eni);
    if (code != NULL)
        PutCode(code);
    for (int frames = 0; frames < limit; frames++) {
        NetPlay_Poll(net);
        Frame();
        const NetPlayStatus status = NetPlay_Status(net);
        if (status == wanted || (wanted == NETPLAY_READY && status == NETPLAY_RUNNING))
            return Wait_Done;
        if (status == NETPLAY_CLOSED)
            return WasRefused(net) ? Wait_Refused : Wait_TimedOut;
        if (jpad1_press1 & JPAD_B)
            return Wait_Cancelled;
    }
    return Wait_TimedOut;
}

// CONNECTION REFUSED or SESSION TIME OUT: any button goes back to the session menu
static void ErrorScreen(bool refused) {
    DrawPage(refused ? S2Menu_TwoPRefused : S2Menu_TwoPTimeOut);
    PlaySound(sfx_Bonus);
    do
        Frame();
    while (!AnyButton());
}

// ENTER CODE: the 8 by 5 grid of letters and digits, A or C puts one in the field, B takes the last one out (or goes back), Start joins when all 20 are in. Returns false when it went back
static bool CodeScreen(char code[21]) {
    DrawPage(S2Menu_TwoPCode);
    int length = 0, row = 0, col = 0;
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
            PlaySound(sfx_Spring);
        }
        if (jpad1_press1 & JPAD_B) {
            if (length == 0)
                return false;
            code[--length] = 0;
        }
        if ((jpad1_press1 & JPAD_START) && length == 20)
            return true;
    }
}

// The zone menu (to come: s2b4's four boxes, with a second page later): for now the first zone
static bool ZoneMenu(void) {
    level_id = LEVEL_ID(ZoneId_EHZ, 0);
    return true;
}

bool VersusMenu_Run(void) {
    EnterScreens();
    PlayMusic(mus_Menu);
    PaletteFadeIn();
    bool result = false;
    for (;;) {
        const SessionChoice choice = SessionMenu();
        if (choice == Session_Back)
            break;
        if (choice == Session_Local) {
            result = ZoneMenu();
            break;
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
    PaletteFadeOut();
    return result;
}
