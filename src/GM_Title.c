#include "SstLoad.h"
#include "GM_Title.h"
#include "Constants.h"

#include "Sound.h"
#include "Game.h"
#include "Level.h"
#include "LevelDraw.h"
#include "LevelScroll.h"
#include "Enigma.h"
#include "Nemesis.h"
#include "PLC.h"
#include "Palette.h"
#include "PaletteCycle.h"
#include "SpecialStage.h"
#include "GM_SpecialTrack.h"
#include "Video.h"
#include "SplitScreen.h"

#include "Backend/VDP.h"

#include <stdio.h>
#include <string.h>

// Level select's own uncompressed font (also used for HUD digits, see
// HUD_WriteHex) -- shared with the rest of the game, not a level-select-only
// asset. Declared (not #included -- Resource/Art/Text.h defines the array,
// so including it here too would be a duplicate definition at link time)
// since Game.c is the only place that includes the real header.
extern const uint8_t Art_Text[];

// Title screen state
uint8_t demo_num;

// ===========================================================================
// Level select: the Simon Wai prototype's (LevelSelect, LevelSelect_Controls and LevelSelect_TextLoad in its disassembly). It plays its own music while it is open, lists every stage of
// the zones it has and a sound test of every id from $80 to $FF, and starts a level in the split screen when B is the button that picks it. Hold A and press Start on the title screen to get here.
// ===========================================================================
#define LEVSEL_ROWS         30   // 28 stages, Special Stage and Sound Select (the prototype's 25, Sand Shower's two, and Sky Chase, Sky Fortress and Death Egg with one each)
#define LEVSEL_SS_ROW       28
#define LEVSEL_SNDTEST_ROW  29
#define LEVSEL_VISIBLE      27   // lines that fit the screen (rows 1 to 27 of its 28): the list scrolls when the highlight leaves them
#define LEVSEL_LINE_LENGTH  27
#define LEVSEL_START_ROW    1    // (the prototype draws its text at $E08C on the plane: row 1, column 6)
#define LEVSEL_START_COL    6
#define LEVSEL_VRAM_MAIN    ((LEVSEL_START_ROW << 7) + (LEVSEL_START_COL << 1)) // (a byte offset in plane A)
#define LEVSEL_FONT_TILE    0x700 // (above the title's art, which the menu stands on: Sonic 1's tile $680 is among the wings')
#define LEVSEL_FONT_VRAM    ART_VRAM(LEVSEL_FONT_TILE)
#define LEVSEL_SNDTEST_COL  18   // where the sound number goes in the Sound Select line (the prototype's $EDB0)
#define LEVSEL_HOLD_DELAY   0xB

// The menu's text (Level_Select_Text): each line 27 characters. The zones' names are those of the prototype's time
static const char *const levsel_text[LEVSEL_ROWS] = {
    "GREEN HILL ZONE     STAGE 0",
    "                    STAGE 1",
    "WOOD ZONE           STAGE 0",
    "                    STAGE 1",
    "SAND SHOWER ZONE    STAGE 0",
    "                    STAGE 1",
    "METROPOLIS ZONE     STAGE 0",
    "                    STAGE 1",
    "                    STAGE 2",
    "HILL TOP ZONE       STAGE 0",
    "                    STAGE 1",
    "HIDDEN PALACE ZONE  STAGE 0",
    "                    STAGE 1",
    "OIL OCEAN ZONE      STAGE 0",
    "                    STAGE 1",
    "DUST HILL ZONE      STAGE 0",
    "                    STAGE 1",
    "CASINO NIGHT ZONE   STAGE 0",
    "                    STAGE 1",
    "CHEMICAL PLANT ZONE STAGE 0",
    "                    STAGE 1",
    "GENOCIDE CITY ZONE  STAGE 0",
    "                    STAGE 1",
    "NEO GREEN HILL ZONE STAGE 0",
    "                    STAGE 1",
    "SKY CHASE ZONE      STAGE 0",
    "SKY FORTRESS ZONE   STAGE 0",
    "DEATH EGG ZONE      STAGE 0",
    "SPECIAL STAGE",
    "SOUND SELECT",
};

// The level each of the first 28 lines starts (LevelSelect_Order), in the prototype's zone ids, which are the zone slots (ZoneIds.h). A zone that has not been built yet (it has no header in the level
// tables) does nothing when picked, as an unused entry of the prototype's order does
static const uint16_t levsel_levels[LEVSEL_SS_ROW] = {
    LEVEL_ID(0x00, 0), LEVEL_ID(0x00, 1),
    LEVEL_ID(0x02, 0), LEVEL_ID(0x02, 1),
    LEVEL_ID(0x03, 0), LEVEL_ID(0x03, 1),
    LEVEL_ID(0x04, 0), LEVEL_ID(0x04, 1), LEVEL_ID(0x04, 2),
    LEVEL_ID(0x07, 0), LEVEL_ID(0x07, 1),
    LEVEL_ID(0x08, 0), LEVEL_ID(0x08, 1),
    LEVEL_ID(0x0A, 0), LEVEL_ID(0x0A, 1),
    LEVEL_ID(0x0B, 0), LEVEL_ID(0x0B, 1),
    LEVEL_ID(0x0C, 0), LEVEL_ID(0x0C, 1),
    LEVEL_ID(0x0D, 0), LEVEL_ID(0x0D, 1),
    LEVEL_ID(0x09, 0), LEVEL_ID(0x09, 1),
    LEVEL_ID(0x0F, 0), LEVEL_ID(0x0F, 1),
    LEVEL_ID(0x10, 0),
    LEVEL_ID(0x06, 0), // Sky Fortress: one act
    LEVEL_ID(0x0E, 0), // Death Egg: one act (not built yet: the slot is empty and it does nothing when picked)
};

static bool LevSelZoneBuilt(uint16_t level) {
    return LEVEL_ZONE(level) < ZoneId_Num && level_load_block[LEVEL_ZONE(level)][LEVEL_ACT(level)].art[0] != NULL;
}

// The sound test number (0-$7F, shown as $80-$FF) and the highlighted line persist across visits, as the prototype's RAM variables do
static int levsel_sound = 0;
static int levsel_item = 0;

// What the sound test plays for the id $80+value: the music ($81-$9F, the final game's) and effects ($A0-$F8, the final game's, Sonic 1's versions and Tails' flight) are the sounds of this port's ids; the rest is silence
uint8_t SoundTest_Id(int value) {
    int id = 0x80 + value;
    if (id >= 0x81 && id <= 0x9F)
        return (uint8_t)(mus_VsResults + (id - 0x81));
    if (id >= 0xA0 && id <= 0xF8) // (the final game's effects to $F0, Sonic 1's versions $F1-$F5, then Tails' flight: $F6-$F8)
        return (uint8_t)(sfx_Jump + (id - 0xA0));
    if (id == 0xF9)
        return mus_MzSwap; // (the port's own song, after the effects)
    return 0;
}

// The tile of a character: the digits as they are, then Y and Z, then A to X (the font sheet's order)
static uint8_t LevSelCharToTile(char c) {
    if (c >= '0' && c <= '9') return (uint8_t)(c - '0');
    if (c == 'Y') return 0x0F;
    if (c == 'Z') return 0x10;
    if (c >= 'A' && c <= 'X') return (uint8_t)(0x11 + (c - 'A'));
    return 0xFF; // a blank
}

static void LevSelWord(size_t address, uint8_t tile, int palette) {
    if (tile == 0xFF) // a space: the cell keeps what the title drew there
        return;
    uint16_t word = TILE_MAP(1, palette, 0, 0, (LEVSEL_FONT_TILE + tile));
    Plane_Put(&screen1p.plane_a, address, word);
}

// What the title drew under the text (the cells of a line's spaces keep it), kept to put back when the list scrolls, and the first line shown
static uint16_t levsel_backdrop[LEVSEL_VISIBLE][LEVSEL_LINE_LENGTH];
static int levsel_top = 0;

static void LevSelBackdropSave(void) {
    for (int row = 0; row < LEVSEL_VISIBLE; row++)
        for (int col = 0; col < LEVSEL_LINE_LENGTH; col++)
            levsel_backdrop[row][col] = Plane_Word(&screen1p.plane_a, LEVSEL_VRAM_MAIN + (row * PLANE_ROW_BYTES) + (col << 1));
}

// Draws the lines that fit in the normal palette, the highlighted one in the yellow one, and the sound number; the list scrolls to keep the highlight in view
static void LevSelTextLoad(int selected) {
    if (selected < levsel_top)
        levsel_top = selected;
    if (selected >= levsel_top + LEVSEL_VISIBLE)
        levsel_top = selected - LEVSEL_VISIBLE + 1;
    if (levsel_top > LEVSEL_ROWS - LEVSEL_VISIBLE)
        levsel_top = LEVSEL_ROWS - LEVSEL_VISIBLE;
    for (int line = 0; line < LEVSEL_VISIBLE; line++) {
        const int row = levsel_top + line;
        const char *text = levsel_text[row];
        size_t len = strlen(text);
        for (int col = 0; col < LEVSEL_LINE_LENGTH; col++) {
            const size_t at = LEVSEL_VRAM_MAIN + (line * PLANE_ROW_BYTES) + (col << 1);
            Plane_Put(&screen1p.plane_a, at, levsel_backdrop[line][col]);
            LevSelWord(at, LevSelCharToTile(col < (int)len ? text[col] : ' '), row == selected ? 2 : 0);
        }
    }
    const int sound_line = LEVSEL_SNDTEST_ROW - levsel_top;
    if (sound_line >= 0 && sound_line < LEVSEL_VISIBLE) {
        int shown = 0x80 + levsel_sound;
        int palette = selected == LEVSEL_SNDTEST_ROW ? 2 : 0;
        for (int nibble = 1; nibble >= 0; nibble--) {
            int d = (shown >> (nibble * 4)) & 0xF;
            LevSelWord(LEVSEL_VRAM_MAIN + (sound_line * PLANE_ROW_BYTES) + ((LEVSEL_SNDTEST_COL + 1 - nibble) << 1), (uint8_t)(d < 10 ? d : d + 7), palette);
        }
    }
}

static void PlayLevel(bool new_game);

// Up and Down move the highlight (with a repeat while held), Left and Right step the sound number on the Sound Select line (A adds $10), and B, C, A or Start picks (B starts the level in
// the split screen, on the Sound Select line they all play the sound but A, which only changes the number)
static void LevelSelect(void) {
    PlayMusic(mus_Menu);

    // The menu stands on the title screen as it is (its wings, emblem, Sonic and Tails on plane A and in the sprites), over an empty plane B (its landscape goes, the backdrop stays the title's: colour 0 of line 2, blue).
    // The text is drawn on plane A over the art, leaving the cells of its spaces as they are, in white (line 0, colour 15 of the level select palette) and, when highlighted, yellow (line 2)
    Plane_Fill(&screen1p.plane_b, 0, (PLANE_WIDTH * PLANE_HEIGHT) << 1, 0);
    PalLoad2(PalId_LevelSel); // (the whole picture in the level select's brown tones, as the prototype shows it)

    memset(hscroll_buffer, 0, sizeof(hscroll_buffer));
    Viewport_UploadHScroll(&screen1p, hscroll_buffer, sizeof(hscroll_buffer));

    VDP_SeekVRAM(LEVSEL_FONT_VRAM);
    // 41 tiles * 32 bytes/tile -- can't sizeof() an extern array with no declared size, and Text.h itself is only ever #included once, from Game.c (re-including it here would double-define
    // Art_Text at link time). The resource pipeline appends 8 bytes of padding after the real data, which is harmless to also write here.
    VDP_WriteVRAM(Art_Text, 41 * 32);

    LevSelBackdropSave();
    levsel_top = 0;
    int item = levsel_item;
    int hold_timer = 0;
    LevSelTextLoad(item);

    for (;;) {
        vbla_routine = 0x04;
        WaitForVBla();

        // LevelSelect_Controls
        bool redraw = false;
        if ((jpad1_press1 & (JPAD_UP | JPAD_DOWN)) || --hold_timer < 0) {
            hold_timer = LEVSEL_HOLD_DELAY;
            uint8_t held = jpad1_hold1 & (JPAD_UP | JPAD_DOWN);
            if (held) {
                if (held & JPAD_UP)
                    item = item > 0 ? item - 1 : LEVSEL_ROWS - 1;
                if (held & JPAD_DOWN)
                    item = item < LEVSEL_ROWS - 1 ? item + 1 : 0;
                redraw = true;
            }
        }
        if (!redraw && item == LEVSEL_SNDTEST_ROW) { // (a frame that moved the highlight does not also step the number)
            int value = levsel_sound;
            if (jpad1_press1 & JPAD_LEFT)
                value = value > 0 ? value - 1 : 0x7F;
            if (jpad1_press1 & JPAD_RIGHT)
                value = value < 0x7F ? value + 1 : 0;
            if (jpad1_press1 & JPAD_A)
                value = (value + 0x10) & 0x7F;
            levsel_sound = value;
            redraw = true;
        }
        if (redraw)
            LevSelTextLoad(item);

        RunPLC();
        if (plc_buffer[0].art != NULL)
            continue; // (not while art is still loading)
        if (!(jpad1_press1 & (JPAD_A | JPAD_B | JPAD_C | JPAD_START)))
            continue;

        two_player_mode = (jpad1_hold1 & JPAD_B) != 0;
        true_two_player = two_player_mode; // (the level select's B is the versus: a second human plays Tails)
        if (item == LEVSEL_SNDTEST_ROW) {
            if (jpad1_press1 & JPAD_A)
                continue; // (A only changes the number)
            uint8_t id = SoundTest_Id(levsel_sound);
            if (id >= mus_VsResults && id <= mus_Drowning || id == mus_MzSwap)
                PlayMusic(id);
            else if (id != 0)
                PlaySound(id);
            continue;
        }
        if (item == LEVSEL_SS_ROW)
            break;
        if (LevSelZoneBuilt(levsel_levels[item]))
            break;
    }

    levsel_item = item; // remembered for the next visit

    if (item == LEVSEL_SS_ROW) {
        gamemode = GameMode_Special;
        level_id = 0;
        // (last_special and the emeralds are left alone, like the original: the stage number moves on with each
        // visit and skips emeralds you have. Only starting a level, or the attract-mode demo, clears them.)
        lives = 3;
        rings = 0;
        level_time.pad = level_time.min = level_time.sec = level_time.frame = 0;
        score = 0;
        score_life = 5000;
    } else {
        level_id = levsel_levels[item];
        PlayLevel(false);
    }
}

// The alpha's level select code (its Level_Select_Code_US and _J: Up, Down, Down, Down, Down, Up) for the level select. The debug mode cheat (C, C, C, C, then that code) is this project's own entry
// gesture, built on it: the alpha instead counts the C presses whenever they are pressed (in $FFFFFFD6) and what the count is decides what the code switches on, which this port does not replicate (its
// slow motion is not implemented). In both cases, presses of unrelated buttons in between are allowed/ignored, and entering the sequence's first button again after a mismatch restarts the count from
// there rather than requiring a clean restart.
static const uint8_t levsel_cheat_sequence[6] = {JPAD_UP, JPAD_DOWN, JPAD_DOWN, JPAD_DOWN, JPAD_DOWN, JPAD_UP};
#ifdef NDEBUG
// Only used in release builds -- debug builds skip entry entirely (see
// GM_Title() below).
static const uint8_t debug_cheat_sequence[10] = {
    JPAD_C, JPAD_C, JPAD_C, JPAD_C, JPAD_UP, JPAD_DOWN, JPAD_DOWN, JPAD_DOWN, JPAD_DOWN, JPAD_UP,
};
#endif

static bool TitleCheatStep(uint8_t *progress, const uint8_t *sequence, uint8_t length) {
    uint8_t input = jpad1_press1 & (JPAD_UP | JPAD_DOWN | JPAD_LEFT | JPAD_RIGHT | JPAD_C);
    if (input == 0)
        return false;
    if (input == sequence[*progress]) {
        if (++*progress >= length) {
            *progress = 0;
            return true;
        }
    } else {
        *progress = (input == sequence[0]) ? 1 : 0;
    }
    return false;
}

// Title screen demo list
// (the final game's DemoLevels, in order: Emerald Hill (the split screen versus), Chemical Plant, Aquatic Ruin, Casino Night; the pre-beta's were Chemical Plant, Emerald Hill, Casino Night, Hill Top; Nick Arcade's third was Hidden Palace, which the pre-beta has taken out: its demo's data is the same, the
// recording of the third is Casino Night's now)
static const uint16_t title_demos[] = {
    LEVEL_ID(ZoneId_EHZ, 0), // (the split screen versus)
    LEVEL_ID(ZoneId_CPZ, 0),
    LEVEL_ID(ZoneId_ARZ, 0),
    LEVEL_ID(ZoneId_CNZ, 0),
};

// Japanese credits
#include "Resource/Art/JapaneseCredits.h"
#include "Resource/Tilemap/JapaneseCredits.h"

// Credits font
#include "Resource/Art/CreditsFont.h"

// Sonic 2's title (the final game's: tools/import_s2final_title.pl): the emblem, background and Sonic and Tails art, the menu's blocks and words, the font; the plane maps and the palette
#include "Resource/Art/TitleFMain.h"
#include "Resource/Art/TitleFSprites.h"
#include "Resource/Art/TitleFMenu.h"
#include "Resource/Art/TitleFPlayers.h"
#include "Resource/Art/TitleFFont.h"
#include "Resource/Art/TitleFCredit.h"
#include "Resource/S2TitleF/PalText.h"
#include "Resource/S2TitleF/MapBG.h"
#include "Resource/S2TitleF/MapBack.h"
#include "Resource/S2TitleF/MapLogo.h"
#include "Resource/S2TitleF/PalTitle.h"
#include "Object/TitleIntro.h"

// Title
#include "Resource/Art/TitleFG.h"
#include "Resource/Art/TitleSonic.h"
#include "Object/TitleMenu.h"
#include "GM_Options.h"
#include "GM_VersusMenu.h"
#include "VersusMatch.h"
#include "Resource/Tilemap/TitleFG.h"

// Level stuff
// new_game: starting from the title screen. A level picked from the level select behaves like arriving through a
// giant ring does: the special stage counter and the emeralds you have are left alone (the original resets them
// here too, but only the attract-mode demo should force the first stage).
static void PlayLevel(bool new_game) {
    // Matches the disassembly's PlayLevel exactly -- always a normal level,
    // no special-casing here. (An earlier version of this port used
    // "A held -> Special Stage" as a stand-in shortcut before level select
    // existed; now that A is required just to *enter* level select, that
    // check would misfire on every level picked from the menu, since A is
    // already held by the time PlayLevel() runs. Special Stage is now its
    // own row in the menu, same as on real hardware.)
    gamemode = GameMode_Level;
    ss4_versus = false; // (a single player's play, or the match's level: not the versus special stage)
    lives = 3;
    rings = 0;
    level_time.pad = level_time.min = level_time.sec = level_time.frame = 0;
    score = 0;
    if (new_game) {
        last_special = 0;
        emeralds = 0;
        memset(emerald_list, 0, sizeof(emerald_list));
        ss4_stage = 0; // (Beta 4's stage: PlayLevel clears Current_Special_Stage and the emeralds' flags)
        memset(ss4_emerald, 0, sizeof(ss4_emerald));
    }
    continues = 0;
    score_life = 5000;
   FadeOutMusic();
}

// Sonic 2's title Start: the level select cheat is on from the start (hold A and press Start, no code to enter), and a plain Start begins in Emerald Hill (Beta 4's title sets the level to 0; the pre-beta's set Neo Green Hill
// (zone slot $0F, Aquatic Ruin in the final game), which is the third zone of the order here as in the final game).
extern bool hud_player_two; // (Object/HUD.c)

static void Tit_StartGame(bool versus);

static void Tit_ChkLevSel(bool level_select_cheat) {
    (void)level_select_cheat;
    hud_player_two = false; // (a game of this machine alone, unless the versus menus say it is the guest's)
    // The alpha asks for A and Start together and nothing else held (its pad byte is exactly $C0); the prototype only looks at A
    if ((jpad1_hold1 & 0xFF) == (JPAD_A | JPAD_START)) {
        LevelSelect();
    } else {
        Tit_StartGame(TitleMenu_TwoPlayers()); // (the menu's choice: Up and Down turn it over, Start takes it)
    }
}

static void Tit_StartGame(bool versus) {
    {
        two_player_mode = versus;
        true_two_player = two_player_mode;
        level_id = LEVEL_ID(ZoneId_EHZ, 0);
        if (two_player_mode && !VersusMenu_Run()) { // (the session and zone menus; B on the first goes back to the title)
            two_player_mode = true_two_player = false;
            return;
        }
        if (true_two_player) {
            two_player_mode = VersusMenu_ChoseLocal(); // (the split screen only for the local match: the true 2 player mode is the versus, with or without it)
            VersusMatch_Begin(); // (the match: its acts' results and counters)
        }
        if (true_two_player && VersusMenu_ChoseSpecialStage()) { // (its box: the special stage with the two players)
            PlayLevel(true);
            VersusMatch_BeginSpecial(); // (the versus stages are 7 to 9: the match's flow stands on the special stage box)
            gamemode = GameMode_Special;
            level_id = 0;
            return;
        }
        PlayLevel(true);
    }
}

void Deform_TitleScreen(void); // Sonic 2's LevelScroll.c
void LevelScroll_BackgroundOff(void);

// Copies a part of the background into plane B, row by row, wrapping at the plane's edge: the two 32-cell parts fill the whole 64-cell plane, so with the widescreen
// margin the right part runs off the end of each row and has to come back round to its start (not spill into the next row)
static void CopyTilemapWrapped(const uint8_t *map, int col, int width, int height) {
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int c = (PLANE_WIDEADD / 2 + col + x) % PLANE_WIDTH;
            const uint8_t *m = map + ((y * width + x) << 1);
            uint16_t v = (m[0] << 8) | m[1];
            Plane_Put(&screen1p.plane_b, PLANE_TALLADD + ((y * PLANE_WIDTH + c) << 1), v);
        }
    }
}

// The title's background on plane B, in two parts side by side: 40 cells and 24 (the level select stands on it too)
static void DrawTitleBackground(void) {
    static uint8_t map[40 * 28 * 2];
    EniDec(S2TitleF_MapBG, map, 0x4000); // (palette line 3)
    CopyTilemapWrapped(map, 0, 40, 28);
    EniDec(S2TitleF_MapBack, map, 0x4000);
    CopyTilemapWrapped(map, 40, 24, 28);
}

// "SONIC AND MILES "TAILS" PROWER IN": the card the final game shows while the title's art loads. Its letters are two tiles wide (a capital's left half and its right half), the I one; the font's tiles are at $500
static void CreditCardText(uint8_t *map, int col, int line, const char *text) {
    static const uint8_t left[27] = { 0x3A, 1, 3, 5, 7, 9, 0xB, 0xD, 0xF, 0x11, 0x12, 0x14, 0x16, 0x18, 0x1A, 0x1C, 0x1E, 0x20, 0x22, 0x24, 0x26, 0x28, 0x2A, 0x2C, 0x2E, 0x30, 0x32 };
    static const uint8_t right[26] = { 2, 4, 6, 8, 0xA, 0xC, 0xE, 0x10, 0x11, 0x13, 0x15, 0x17, 0x19, 0x1B, 0x1D, 0x1F, 0x21, 0x23, 0x25, 0x27, 0x29, 0x2B, 0x2D, 0x2F, 0x31, 0x33 };
    for (; *text != 0; text++) {
        uint8_t codes[2];
        int n = 0;
        if (*text == ' ')
            codes[n++] = 0;
        else if (*text == 'I')
            codes[n++] = 0x11; // (the I is one narrow tile)
        else if (*text == '\'')
            codes[n++] = 0x38; // (the double quote: the intro's charset puts '"' on $38)
        else if (*text >= 'A' && *text <= 'Z') {
            codes[n++] = left[*text - '@'];
            codes[n++] = right[*text - 'A'];
        } else
            codes[n++] = (uint8_t)*text;
        for (int i = 0; i < n; i++, col++) {
            const unsigned word = 0x500 + codes[i];
            map[(line * 40 + col) * 2] = (uint8_t)(word >> 8);
            map[(line * 40 + col) * 2 + 1] = (uint8_t)word;
        }
    }
}

static void ShowCreditCard(void) {
    static uint8_t map[40 * 28 * 2];
    memset(map, 0, sizeof(map));
    CreditCardText(map, 0xF, 9, "SONIC");
    CreditCardText(map, 0x11, 0xC, "AND");
    CreditCardText(map, 3, 0xF, "MILES 'TAILS' PROWER");
    CreditCardText(map, 0x12, 0x12, "IN");
    VDP_SeekVRAM(ART_VRAM(0x500));
    NemDec(Art_TitleFCredit);
    CopyTilemap(map, &screen1p.plane_a, PLANE_WIDEADD + PLANE_TALLADD, 40, 28);
    memset(dry_palette_dup, 0, sizeof(dry_palette_dup));
    for (int i = 0; i < 16; i++)
        dry_palette_dup[0][i] = LESWAP_16(((const uint16_t *)S2TitleF_PalText)[i]);
    PaletteFadeIn();
    for (int f = 0; f < 90; f++) { // (what the machine spends loading the title's art)
        vbla_routine = 0x04;
        WaitForVBla();
    }
    PaletteFadeOut();
    ClearScreen();
}

// Title gamemode
void GM_Title(void) {
    // Stop music
    StopAllSound();

    // The hidden Japanese credits (hold A+B+C+Down on the "Sonic Team Presents" screen) come with the debug cheat: always in debug builds, once its
    // code has been entered in release builds (the original only had them through a build option).
#ifndef NDEBUG
    credits_cheat = true;
#endif

    // Clear the pattern load queue and fade out
    ClearPLC();
    PaletteFadeOut();
    VDP_ClearVRAM(); // (the title's pictures rely on the tiles they do not load being blank, as after a boot: a demo's level art must not show through)

    // Set VDP state
    VDP_SetBackgroundColour(0x20); // Line 2, entry 0

    VDPDisableWaterSplit();
    LevelScroll_BackgroundOff(); // (the title's vblank draws a level's background layer: not the one of the demo that has just ended)

    // Clear screen
    ClearScreen();

    // (Sonic 1 shows its "SONIC TEAM PRESENTS" screen here, with the hidden Japanese credits. Sonic 2 skips it; "MILES TAILS PROWER IN" is to take its place later.)
    memset(dry_palette_dup, 0, sizeof(dry_palette_dup));
    PalLoad1(PalId_Sonic);
    ExecuteObjects();
    BuildSprites(NULL);

    ShowCreditCard();

    // Load the title's art (the final game's): the emblem and background at tile 0, Sonic and Tails and the stars at $150, the menu's blocks at $3F2 and its words at $402, the font at $680
    VDP_SeekVRAM(ART_VRAM(0x000));
    NemDec(Art_TitleFMain);
    VDP_SeekVRAM(ART_VRAM(0x150));
    NemDec(Art_TitleFSprites);
    VDP_SeekVRAM(ART_VRAM(0x3F2));
    NemDec(Art_TitleFMenu);
    VDP_SeekVRAM(ART_VRAM(0x402));
    NemDec(Art_TitleFPlayers);
    VDP_SeekVRAM(ART_VRAM(0x680));
    NemDec(Art_TitleFFont);

    // Reset game state
    last_lamp = 0;
    debug_use = false;
    demo = 0;
    level_id = LEVEL_ID(ZoneId_EHZ, 0);
    pcyc_time = 0;

    // The title has a camera of its own (Deform_TitleScreen runs it on from -$280)
    scrpos_x.f.u = -0x280;
    bg_scrpos_y.v = 0;
    TitleMenu_Reset();

    // Fade out
    PaletteFadeOut();
    ClearScreen();

    // Plane A: the emblem and the copyright line; plane B: the background, in two parts side by side
    {
        static uint8_t map[40 * 28 * 2];
        EniDec(S2TitleF_MapLogo, map, 0xE000); // (palette line 4, in front of the sprites)
        static const char copyright[] = "@ 1992 SEGA"; // (the font's tiles: digits from 0, '@' $B, letters from $E)
        for (int i = 0; copyright[i] != 0; i++) {
            const char c = copyright[i];
            const unsigned tile = c == ' ' ? 0 : c == '@' ? 0x680 + 0xB : (c >= '0' && c <= '9') ? 0x680 + (c - '0') : 0x680 + 0xE + (c - 'A');
            map[(26 * 40 + 28 + i) * 2] = (uint8_t)(tile >> 8);
            map[(26 * 40 + 28 + i) * 2 + 1] = (uint8_t)tile;
        }
        CopyTilemap(map, &screen1p.plane_a, PLANE_WIDEADD + PLANE_TALLADD, 40, 28);
    }
    DrawTitleBackground();

    // The palette: only the line the title shows first is loaded (the others fade in with the intro)
    memset(dry_palette_dup, 0, sizeof(dry_palette_dup));
    for (int i = 0; i < 16; i++)
        dry_palette_dup[1][i] = LESWAP_16(((const uint16_t *)S2TitleF_PalTitle)[i]);

    // Run title screen for 640 frames (a little over ten seconds)
    demo_length = 60 * 10 + 40;

    // Load title objects: the intro, which makes the rest (Sonic, Tails, the stars, the menu)
    Sst_ResetAll(); // (the level's Sonic and Tails must not stay on the title screen)
    TitleIntro_Start();

    ExecuteObjects();
    BuildSprites(NULL);

    NewPLC(PlcId_Main);

    // Fade in
    PaletteFadeIn();

    // Level select cheat entry state (see TitleCheatStep/Tit_ChkLevSel).
    uint8_t cheat_progress = 0;
    bool level_select_cheat = false;

    // Debug/object-placement mode cheat entry state. debug_cheat itself is
    // the real, global flag (Game.h) that GM_Level.c/GM_Special.c already
    // check when a level starts. In debug builds, both the cheat entry and
    // the "hold A" requirement below are skipped entirely, so debug mode is
    // always one level-start away while testing.
#ifndef NDEBUG
    debug_cheat = !cli_no_debug;
#else
    uint8_t debug_progress = 0;
#endif

    // Loop
    for (;;) {
        // Render frame
        vbla_routine = 0x04;
        WaitForVBla();

        // The mode was changed from outside (e.g. an in-app demo recording request): leave.
        if ((gamemode & 0x7F) != GameMode_Title)
            return;

        // Run the intro (and the menu it brings up) and load PLCs
        ExecuteObjects();
        Deform_TitleScreen(); // (the title's own camera runs on by one a frame, the final game's SwScrl_Title)
        BuildSprites(NULL);
        RunPLC();
        PCycle_Title();

        // Check for level select cheat entry (Up, Down, Down, Down, Down, Up)
        if (TitleCheatStep(&cheat_progress, levsel_cheat_sequence, 6)) {
            level_select_cheat = true;
            PlaySound(sfx_Ring);
        }
#ifdef NDEBUG
        // Check for debug mode cheat entry (C, C, C, C, Up, Down, Down, Down, Down, Up)
        if (TitleCheatStep(&debug_progress, debug_cheat_sequence, 10)) {
            debug_cheat = true;
            credits_cheat = true;
            PlaySound(sfx_Ring);
        }
#endif

        // The time is up: a demo
        if (!demo_length) {
            // Load demo
            FadeOutMusic();

            level_id = title_demos[demo_num & 7];
            if (++demo_num >= 4)
                demo_num = 0;

            // Enter demo gamemode
            demo = 1;
            if (level_id != LEVEL_ID(ZoneId_SS, 0)) { // (none of the demos is the special stage's now)
                // Regular level
                gamemode = GameMode_Demo;
            } else {
                // Special stage
                gamemode = GameMode_Special;
                level_id = 0;
                last_special = 0;
                ss4_stage = 0;
                memset(ss4_emerald, 0, sizeof(ss4_emerald));
            }

            two_player_mode = level_id == LEVEL_ID(ZoneId_EHZ, 0); // (only the Emerald Hill demo is a two player one, in the split screen)
            true_two_player = two_player_mode; // (its Tails is the recorded pad 2)

            // Set game state
            lives = 3;
            rings = 0;
            level_time.pad = level_time.min = level_time.sec = level_time.frame = 0;
            score = 0;
            score_life = 5000;
            return;
        }

        // Start takes the menu's choice once the intro has run (before that, it skips the intro to its end: the objects take that press)
        if ((jpad1_press1 & JPAD_START) && TitleIntro_Complete()) {
            if (TitleMenu_Choice() == 2) { // OPTIONS: its own screen; Start there takes a 1 player game, the versus, or goes back to the SEGA screen (the sound test's)
                const OptionsResult result = Options_Run();
                if (result == Options_Sega) {
                    gamemode = GameMode_Sega;
                    return;
                }
                hud_player_two = false;
                Tit_StartGame(result == Options_StartVersus);
                return;
            }
            Tit_ChkLevSel(level_select_cheat);
            return;
        }
    }
}
