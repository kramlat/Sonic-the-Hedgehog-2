#include "Object/TitleCard.h"
#include "GM_Options.h"
#include "Object/Tails.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"
extern uint8_t true_two_player; // (Game.h: the versus play)

#include "Constants.h"
#include "Game.h"
#include "Level.h"
#include "PLC.h"
#include "Sound.h"

#include "Resource/Mappings/TitleCard.h"
#include "Resource/Mappings/TitleCardNames.h"
#include "Resource/Art/TitleCardZone.h"
#include "Resource/Art/TitleCardLetters.h"
#include "Nemesis.h"
#include "Backend/VDP.h"
#include "ScrollEngine.h"
#include "SplitScreen.h"
#include <string.h>
#include "Mappings.h"
#include "Resource/Mappings/LevelResults.h"
#include "Object/HUD.h"
#include "Palette.h"
#include "Resource/Mappings/SpecialResult.h"

// Title card configuration. The positions depend on the width and height of the picture, which are chosen at run time, so
// the tables are built where they are used (see the title card's creation routine).
struct TitleCard_Item {
    int16_t y;
    uint8_t routine, frame;
};

struct TitleCard_Config {
    int16_t x0, x1;
};

// What each act leads to, as the ROM has it: a row for each zone slot and a column for each act, the level (written as the level select writes it) that is loaded when the act is done; LEVEL_END where there is no next
// level. First the one player order, then the versus play's; the rows of the zones that a table leaves out are 0 (the first act of Emerald Hill), as the original's are.
#define LEVEL_END 0xFFFF // (a placeholder where the real order is not set yet: finishing such a level ends the game)
#define ORDER_ZONES 0x11
#define ORDER_ACTS 4
static const uint16_t order_one_player[ORDER_ZONES][ORDER_ACTS] = {
    [ZoneId_EHZ] = { LEVEL_ID(ZoneId_EHZ, 1), LEVEL_ID(ZoneId_CPZ, 0), LEVEL_END, LEVEL_END },
    [ZoneId_WZ]  = { LEVEL_ID(ZoneId_WZ, 1),  LEVEL_END,               LEVEL_END, LEVEL_END },
    [ZoneId_SSZ] = { LEVEL_ID(ZoneId_SSZ, 1), LEVEL_END,               LEVEL_END, LEVEL_END },
    [ZoneId_MTZ] = { LEVEL_ID(ZoneId_MTZ, 1), LEVEL_ID(ZoneId_MTZ, 2), LEVEL_ID(ZoneId_SCZ, 0), LEVEL_END },
    [ZoneId_SFZ] = { LEVEL_ID(ZoneId_SFZ, 1), LEVEL_ID(ZoneId_DEZ, 0), LEVEL_END, LEVEL_END },
    [ZoneId_HTZ] = { LEVEL_ID(ZoneId_HTZ, 1), LEVEL_ID(ZoneId_MCZ, 0), LEVEL_END, LEVEL_END },
    [ZoneId_HPZ] = { LEVEL_ID(ZoneId_HPZ, 1), LEVEL_END,               LEVEL_END, LEVEL_END },
    [ZoneId_GCZ] = { LEVEL_ID(ZoneId_GCZ, 1), LEVEL_END,               LEVEL_END, LEVEL_END },
    [ZoneId_OOZ] = { LEVEL_ID(ZoneId_OOZ, 1), LEVEL_ID(ZoneId_MTZ, 0), LEVEL_END, LEVEL_END },
    [ZoneId_MCZ] = { LEVEL_ID(ZoneId_MCZ, 1), LEVEL_ID(ZoneId_OOZ, 0), LEVEL_END, LEVEL_END },
    [ZoneId_CNZ] = { LEVEL_ID(ZoneId_CNZ, 1), LEVEL_ID(ZoneId_HTZ, 0), LEVEL_END, LEVEL_END },
    [ZoneId_CPZ] = { LEVEL_ID(ZoneId_CPZ, 1), LEVEL_ID(ZoneId_ARZ, 0), LEVEL_END, LEVEL_END },
    [ZoneId_DEZ] = { LEVEL_END, LEVEL_END, LEVEL_END, LEVEL_END },
    [ZoneId_ARZ] = { LEVEL_ID(ZoneId_ARZ, 1), LEVEL_ID(ZoneId_CNZ, 0), LEVEL_END, LEVEL_END },
    [ZoneId_SCZ] = { LEVEL_ID(ZoneId_SFZ, 0), LEVEL_END,               LEVEL_END, LEVEL_END },
};

static const uint16_t order_versus[ORDER_ZONES][ORDER_ACTS] = {
    [ZoneId_EHZ] = { LEVEL_ID(ZoneId_EHZ, 1), LEVEL_ID(ZoneId_CNZ, 0), LEVEL_END, LEVEL_END },
    [ZoneId_MCZ] = { LEVEL_ID(ZoneId_MCZ, 1), LEVEL_END,               LEVEL_END, LEVEL_END },
    [ZoneId_CNZ] = { LEVEL_ID(ZoneId_CNZ, 1), LEVEL_ID(ZoneId_MCZ, 0), LEVEL_END, LEVEL_END },
};

// The act after this one, in the form the level id has it, or 0xFFFF for the end of the game
uint16_t TitleCard_NextLevel(void) {
    const int zone = LEVEL_ZONE(level_id), act = LEVEL_ACT(level_id);
    if (zone >= ORDER_ZONES || act >= ORDER_ACTS)
        return 0;
    return (true_two_player ? order_versus : order_one_player)[zone][act]; // (the versus play's order: the true 2 player flag, not the split screen)
}

const uint8_t *EndCard_ZoneArt(void); // (Sonic2PLC.c, which has the art)

// ---- The end of act card: object $3A of Beta 4 (End_Level_Results_Mappings by tools/import_s2b4_cards.pl; art: the ZONE letters at $580, the words at $5B0, the continue icon at $5F4) ----
// Seven elements slide in (the first, this object, runs the rest: it waits, tallies the bonuses into the score, waits, and goes to the next level), and when the bonuses add up to 1000 or more
// a flashing icon of Sonic gives a continue. The numbers are the HUD's art (HUD_Update draws the total, the time bonus and the ring bonus into tiles $520 on).
enum { GOT_FIRST = 0, GOT_PASSED = 3, GOT_ZONE = 5, GOT_ACT1 = 6, GOT_TIME = 9, GOT_RING = 10, GOT_TOTAL = 11, GOT_CONTINUE = 12 };
#define GOT_TOTAL_LIMIT 1000 // (bonuses that add up to this or more give a continue)
uint16_t endact_total;       // the bonuses added to the score so far

static const struct GotCard_Item {
    int16_t x0, x1, y;
    uint8_t routine, frame;
} gotcard_item[7] = { // Level_Results_Screen_Pos
    { 0x0020, 0x0120, 0x00B8, 2, 0 },           // the first line (the master)
    { 0x0200, 0x0100, 0x00CA, 4, GOT_PASSED },  // the second line
    { 0x0240, 0x0140, 0x00CA, 6, 4 },           // (the word before the act: ZONE, or nothing in the zones without acts)
    { 0x0280, 0x0180, 0x00BE, 8, GOT_ACT1 },    // the act number (dynamic frame)
    { 0x0320, 0x0120, 0x0114, 4, GOT_TIME },    // TIME BONUS
    { 0x0330, 0x0120, 0x00F0, 4, GOT_RING },    // RING BONUS
    { 0x0340, 0x0120, 0x0100, 4, GOT_TOTAL },   // the total
};

// The first line (the final's loc_14102): SONIC, or for Tails alone TAILS, or MILES with the Miles swap cheat
static uint8_t GotCard_FirstLine(void) {
    if (!Tails_IsPlayer())
        return 0;
    return options_miles_swap ? 1 : 2;
}

// (the zones that are not played in acts: Sky Chase, Wing Fortress and Death Egg have no act number)
static bool GotCard_NoAct(void) {
    const int zone = LEVEL_ZONE(level_id);
    return zone == ZoneId_SCZ || zone == ZoneId_SFZ || zone == ZoneId_DEZ;
}

// loc_10DDC: 16 pixels a frame towards where it stops; drawn unless it is past the right edge
static void GotCard_Slide(Sst *self, Own_TitleCard *own) {
    if (self->x_pos.f.u != own->main_x) {
        if (self->x_pos.f.u > own->main_x)
            self->x_pos.f.u -= 16;
        else
            self->x_pos.f.u += 16;
        if ((uint16_t)self->x_pos.f.u > 0x200 + SCREEN_WIDEADD)
            return;
    }
    Sst_Draw(self);
}

// The end of act card's object (and its elements)
void Obj_GotThroughCard(Sst *self) {
    Own_TitleCard *own = (Own_TitleCard *)self->own.u8;

    switch (self->routine) {
    case 0: { // Waits for the card's art, then makes the seven elements (this object is the first)
        if (plc_buffer[0].art != NULL)
            return;
        endact_total = 0;
        { // the ZONE art and the fixed letters, the first 48 tiles of it: the rest is the title card's banner, which would cover the results' words
            static uint8_t zone_art[0x60 * TILE_SIZE];
            NemDecToRAM(EndCard_ZoneArt(), zone_art);
            VDP_SeekVRAM(ART_VRAM(ArtTile_Title_Card));
            VDP_WriteVRAM(zone_art, 0x30 * TILE_SIZE);
        }
        const int16_t to_add = SCREEN_WIDEADD2;
        const int16_t from_add = to_add + ((SCREEN_WIDEADD2 + 0xF) & ~0xF);
        const int16_t from_sub = (0x10 - to_add) & 0xF;
        Sst *a1 = self;
        for (int i = 0; i < 7; i++) {
            if (i > 0) {
                a1 = Sst_AllocateAfter(a1);
                if (a1 == NULL)
                    break;
            }
            const struct GotCard_Item *item = &gotcard_item[i];
            Own_TitleCard *scratch = (Own_TitleCard *)a1->own.u8;
            a1->code = Obj_GotThroughCard;
            a1->x_pos.f.u = (int16_t)(item->x0 + (item->x0 > item->x1 ? from_add : -from_sub));
            scratch->main_x = (int16_t)(item->x1 + to_add);
            a1->y_pos.f.u = (int16_t)(item->y + SCREEN_TALLADD2);
            a1->routine = item->routine;
            a1->mapping_frame = i == 0 ? GotCard_FirstLine() : item->frame;
            a1->mappings = Mappings_LevelResults;
            a1->art_tile = 0; // (the pieces hold their tiles and their priority)
            a1->render_flags = 0;
            a1->priority = 0 * 0x80;
        }
        return;
    }
    case 2: // the first line: slides in, then the bonuses wait a second and a half
        GotCard_Slide(self, own);
        if (self->x_pos.f.u == own->main_x) {
            self->routine = 0xA;
            own->timer = 0xB4;
        }
        return;
    case 4:
        GotCard_Slide(self, own);
        return;
    case 6:
        if (GotCard_NoAct())
            self->mapping_frame = GOT_ZONE;
        GotCard_Slide(self, own);
        return;
    case 8: // the act number
        if (GotCard_NoAct()) {
            Sst_Delete(self);
            return;
        }
        self->mapping_frame = (uint8_t)(LEVEL_ZONE(level_id) == ZoneId_MTZ && LEVEL_ACT(level_id) == 2 ? 8 : GOT_ACT1 + LEVEL_ACT(level_id));
        GotCard_Slide(self, own);
        return;
    case 0xA: // waits before the tally
    case 0xE: // waits after it
        if (--own->timer == 0)
            self->routine += 2;
        Sst_Draw(self);
        return;
    case 0xC: { // the tally: ten points a frame from each bonus into the score
        Sst_Draw(self);
        endact_bonus = true;
        uint16_t ticked = 0;
        if (time_bonus) {
            ticked += 10;
            time_bonus -= 10;
        }
        if (ring_bonus) {
            ticked += 10;
            ring_bonus -= 10;
        }
        endact_total += ticked;
        if (ticked == 0) {
            PlaySound(sfx_Cash);
            self->routine += 2;
            own->timer = 0xB4;
            if (endact_total >= GOT_TOTAL_LIMIT) { // a continue: its icon comes, flashing
                own->timer = 0x12C;
                Sst *icon = Sst_Allocate();
                if (icon != NULL) {
                    Own_TitleCard *scratch = (Own_TitleCard *)icon->own.u8;
                    icon->code = Obj_GotThroughCard;
                    icon->routine = 0x12;
                    icon->x_pos.f.u = (int16_t)(0x188 + SCREEN_WIDEADD2);
                    icon->y_pos.f.u = (int16_t)(0x10C + SCREEN_TALLADD2);
                    icon->mappings = Mappings_LevelResults;
                    icon->mapping_frame = 0;
                    icon->art_tile = 0;
                    icon->render_flags = 0;
                    icon->priority = 0 * 0x80;
                    scratch->timer = 0x3C;
                    scratch->final_x = 0; // (the icon's blink)
                    continues++;
                }
            }
            return;
        }
        AddPoints(ticked);
        if ((frame_count & 3) == 0)
            PlaySound(sfx_Switch);
        return;
    }
    case 0x10: { // on to the next level
        const uint16_t next = TitleCard_NextLevel();
        last_lamp = 0;
        if (next == 0xFFFF) { // (the end of the game: the Sega screen)
            gamemode = GameMode_Sega;
            return;
        }
        level_id = next;
        if (big_ring) // (Sonic jumped into a giant ring: the next screen is a special stage)
            gamemode = GameMode_Special;
        else
            restart = 1;
        return;
    }
    case 0x12: // the continue icon: waits, then the sound
        if (own->timer) {
            own->timer--;
            return;
        }
        self->routine += 2;
        PlaySound(sfx_Continue);
        // Fallthrough
    case 0x14: // and flashes (two frames, switching every 20 frames; shown on half the frames)
        if ((int16_t)--own->timer < 0) {
            own->timer = 0x13;
            own->final_x = (int16_t)(own->final_x ^ 1);
        }
        self->mapping_frame = (uint8_t)(GOT_CONTINUE + own->final_x);
        if (frame_count & 0x10)
            Sst_Draw(self);
        return;
    }
}

// The elements of the level's title card (Beta 4's object $34): the name, ZONE, the act number, the curtain (the bars on the window plane), the banner and the zigzag edge; found by their subtype
enum { TCE_NAME, TCE_ZONE, TCE_ACT, TCE_CURTAIN, TCE_BANNER, TCE_STRIPES, TCE_COUNT };
static Sst *TitleCard_Element(int n) {
    for (int i = 0; i < SST_COUNT; i++)
        if (sst[i].code != NULL && sst[i].code == Obj_TitleCard && sst[i].subtype == n)
            return &sst[i];
    return NULL;
}

bool TitleCard_Arrived(void) {
    const Sst *zone = TitleCard_Element(TCE_NAME); // (the level waits for the name: `cmp.w ($FFFFB0B0).w,D0`)
    return zone == NULL || zone->x_pos.f.u == ((const Own_TitleCard *)zone->own.u8)->main_x;
}

static int tc_wipe = -1; // frames since the level asked the card to go (-1: not yet)

void TitleCard_Leave(void) {
    Sst *curtain = TitleCard_Element(TCE_CURTAIN);
    if (curtain != NULL) { // the bars come off (Beta 4 runs the objects until they are, before play begins), then the names wait and go
        tc_wipe = 0;
        while (TitleCard_Element(TCE_CURTAIN) != NULL) {
            vbla_routine = 0x0C;
            WaitForVBla();
            ExecuteObjects();
            BuildSprites(NULL);
            RunPLC();
        }
        return;
    }
    for (int i = 0; i < 3; i++) { // (the split screen's: the three names, which wait and go)
        Sst *e = TitleCard_Element(i);
        if (e != NULL) {
            e->routine = 6;
            ((Own_TitleCard *)e->own.u8)->timer = 60;
        }
    }
}

// The title cards of the Sonic 2 pre-beta (September 14, 1992: its object $34, Obj_0x34_Title_Cards; art, mappings and letter tables from tools/import_s2prebeta_titlecards.pl). Three elements slide
// in: the name of the zone (from the right), ZONE and the act number (from the left); they wait a second after the level has been loaded and slide away, the name's end loading the explosion's and the
// animals' art. Its frames: the names (the pre-beta's own, and Wood's, Sand Shower's, Cyber City's and Hidden Palace's written from its alphabet: the user), ZONE, ACT 1 to 3
enum { TC_FRAME_ZONE = 15, TC_FRAME_ACT1 = 16 };
extern const uint8_t titlecard_letters[][20];
#define TC_ART_FIXED 0x580   // the fixed part (E, N, O, ZONE, the numbers)
#define TC_ART_LETTERS 0x5DE // where the letters a name needs are copied to

// The name's frame by zone slot (a zone that has no name of its own is Green Hill, as the pre-beta's placeholder slots are)
static uint8_t TitleCard_NameFrame(int zone) {
    switch (zone) {
    case ZoneId_MTZ: return 1;
    case ZoneId_HTZ: return 2;
    case ZoneId_OOZ: return 3;
    case ZoneId_MCZ: return 4;  // Dust Hill
    case ZoneId_CNZ: return 5;
    case ZoneId_CPZ: return 6;
    case ZoneId_ARZ: return 7;  // Neo Green Hill
    case ZoneId_SCZ: return 8;
    case ZoneId_SFZ: return 9;  // Sky Fortress
    case ZoneId_DEZ: return 10; // Death Egg
    case ZoneId_WZ: return 11;
    case ZoneId_SSZ: return 12;
    case ZoneId_GCZ: return 13;
    case ZoneId_HPZ: return 14;
    default: return 0;          // Green Hill (Emerald Hill's card says so too: the pre-beta has no other name for its first slot)
    }
}

// The card's art goes into VRAM when the level's first pattern load cues have finished, not when the card is made: those lists (the water zones' bubbles at $5E8, Hidden Palace's bat at $5A8) have
// art in tiles the card uses, and the card would show what they write over it
static bool titlecard_art_loaded;

// Load_Title_Cards_Art: the fixed part at tile $580, then the letters the name uses, from the alphabet, one after the other from tile $5DE
static void TitleCard_LoadArt(uint8_t name) {
    static uint8_t alphabet[0x1000];
    VDP_SeekVRAM(ART_VRAM(TC_ART_FIXED));
    NemDec(Art_TitleCardZone);
    NemDecToRAM(Art_TitleCardLetters, alphabet);
    VDP_SeekVRAM(ART_VRAM(TC_ART_LETTERS));
    for (const uint8_t *l = titlecard_letters[name]; *l != 0xFF; l += 2)
        VDP_WriteVRAM(alphabet + l[0] * TILE_SIZE, (size_t)l[1] * TILE_SIZE);
}

// Whether a pattern load cue list writes into the card's tiles: the fixed part from $580 and the name's letters after it (from $5DE), or in the split screen the squeezed pieces too (from $600, as far as
// $700 is taken to be theirs)
bool TitleCard_ArtClashes(PlcId plc) {
    const PLCList *list = plcs[plc];
    if (list == NULL)
        return false;
    unsigned end = TC_ART_LETTERS;
    for (const uint8_t *l = titlecard_letters[TitleCard_NameFrame(LEVEL_ZONE(level_id))]; *l != 0xFF; l += 2)
        end += l[1];
    if (SplitScreen_SideBySide())
        end = 0x700;
    for (size_t i = 0; i < list->plcs; i++) {
        const unsigned first = (unsigned)(list->plc[i].off / TILE_SIZE);
        const unsigned tiles = ((list->plc[i].art[0] << 8) | list->plc[i].art[1]) & 0x7FFF; // (the Nemesis header's tile count)
        if (first < end && first + tiles > TC_ART_FIXED)
            return true;
    }
    return false;
}

// The card in the split screen, side by side: each view is half as wide as the picture and the card is drawn in both (an object that is not in the level is at the same place in each view), so it has
// to fit and sit in the middle of each. Its art is squeezed to half its width (the pieces of every element are made again from the unscaled ones: a letter is two cells wide, so it becomes one; where two
// pixels meet the brighter one stays, the white of the letter before its outline) and the elements are put so that the name, and ZONE with the number after it, are centred in the view.
#define TC_SCALED_TILES 0x600 // where the squeezed pieces' tiles go
static uint8_t titlecard_scaled_map[6][4 + 24 * SPRITE_PIECE_SIZE_S3]; // (by element: the name, ZONE, the act, [the curtain], the banner, the zigzag edge)

static bool TitleCard_SplitWanted(void) {
    return SplitScreen_SideBySide();
}

// One element's pieces squeezed: its mapping in titlecard_scaled_map[index] and its extent (the pieces' leftmost and rightmost pixel); `next_tile` is where the next piece's tiles go
static void TitleCard_Squeeze(int index, uint8_t frame, unsigned *next_tile, int *left, int *right) {
    const uint8_t *pieces;
    const uint16_t count = Mappings_FramePieces(Mappings_TitleCardNames, frame, &pieces);
    uint8_t *map = titlecard_scaled_map[index];
    map[0] = 0;
    map[1] = 2;
    map[2] = 0;
    map[3] = (uint8_t)count;
    uint8_t *out = map + 4;
    const uint8_t *vram = VDP_PeekVRAM();
    *left = 0x7FFF;
    *right = -0x7FFF;
    for (unsigned p = 0; p < count && p < 24; p++, pieces += SPRITE_PIECE_SIZE_S3, out += SPRITE_PIECE_SIZE_S3) {
        const uint8_t size = Mappings_PieceSize(pieces);
        const int wc = ((size >> 2) & 3) + 1, hc = (size & 3) + 1, wc2 = (wc + 1) / 2;
        const unsigned tile_word = Mappings_PieceTile(pieces), tile = tile_word & 0x7FF; // (the word's flags and palette line stay: the number is on line 1)
        // the source, a pixel at a time (the cells go down a column and then across), and the squeezed cells
        static uint8_t cells[16 * TILE_SIZE];
        memset(cells, 0, sizeof(cells));
        int first = 8 * wc2, last = -1;
        for (int y = 0; y < hc * 8; y++) {
            for (int dx = 0; dx < wc2 * 8; dx++) {
                uint8_t best = 0;
                for (int k = 0; k < 2; k++) {
                    const int sx = dx * 2 + k;
                    if (sx >= wc * 8)
                        continue;
                    const uint8_t *row = vram + (size_t)(tile + (size_t)(sx >> 3) * hc + (size_t)(y >> 3)) * TILE_SIZE + (size_t)(y & 7) * 4;
                    const uint8_t b = row[(sx & 7) >> 1];
                    const uint8_t px = (sx & 1) ? (b & 0xF) : (b >> 4);
                    if (px > best)
                        best = px;
                }
                if (best) {
                    if (dx < first) first = dx;
                    if (dx > last) last = dx;
                }
                uint8_t *dst = cells + (size_t)((dx >> 3) * hc + (y >> 3)) * TILE_SIZE + (size_t)(y & 7) * 4 + (size_t)((dx & 7) >> 1);
                *dst = (dx & 1) ? (uint8_t)((*dst & 0xF0) | best) : (uint8_t)((*dst & 0x0F) | (best << 4));
            }
        }
        VDP_SeekVRAM(ART_VRAM(*next_tile));
        VDP_WriteVRAM(cells, (size_t)wc2 * hc * TILE_SIZE);
        const int x = (Mappings_PieceXS3(pieces) >> 1);
        out[0] = pieces[0];
        out[1] = (uint8_t)(((wc2 - 1) << 2) | (hc - 1));
        out[2] = (uint8_t)(((tile_word & 0xF800) | *next_tile) >> 8);
        out[3] = (uint8_t)*next_tile;
        out[4] = (uint8_t)((uint16_t)x >> 8);
        out[5] = (uint8_t)x;
        *next_tile += (unsigned)(wc2 * hc);
        if (last >= first) {
            if (x + first < *left) *left = x + first;
            if (x + last + 1 > *right) *right = x + last + 1;
        }
    }
    if (*left > *right)
        *left = *right = 0;
}

// Makes the card fit the views, once the split screen is on (the card is made before it is: the level is loaded under it): the sprite elements are squeezed to half their width. The name is put in the middle
// of the view, ZONE and the number after it as one in the middle, the banner on the yellow and the zigzag edge at the edge of the red, which are half as many columns (TitleCard_DrawBars)
static void TitleCard_FitSplit(void) {
    static const int all[5] = { TCE_NAME, TCE_ZONE, TCE_ACT, TCE_BANNER, TCE_STRIPES };
    Sst *e[6] = { 0 };
    for (int i = 0; i < 5; i++)
        e[all[i]] = TitleCard_Element(all[i]);
    if (e[0] == NULL || e[0]->routine_secondary)
        return;
    const int view_w = SCREEN_WIDTH / 2;
    const int red_px = 56; // (the red is 7 columns wide in the views: the text is put in what is to the right of it, as the zigzag edge is at its edge)
    const int centre = 128 + red_px + (view_w - red_px) / 2; // (an object's x counts from 128 to the view's left edge)
    unsigned next_tile = TC_SCALED_TILES;
    int left[6] = { 0 }, right[6] = { 0 };
    for (int i = 0; i < 5; i++)
        if (e[all[i]] != NULL)
            TitleCard_Squeeze(all[i], e[all[i]]->status_secondary, &next_tile, &left[all[i]], &right[all[i]]);
    // where each stands when it has arrived
    int target[6] = { 0 };
    target[0] = centre - (left[0] + right[0]) / 2;
    const int zone_w = right[1] - left[1], number_w = e[2] != NULL ? right[2] - left[2] : 0, gap = e[2] != NULL ? 6 : 0;
    const int start = centre - (zone_w + gap + number_w) / 2;
    target[1] = start - left[1];
    target[2] = start + zone_w + gap - left[2];
    for (int k = 0; k < 5; k++) {
        const int i = all[k];
        if (e[i] == NULL)
            continue;
        Own_TitleCard *own = (Own_TitleCard *)e[i]->own.u8;
        if (i == TCE_BANNER || i == TCE_STRIPES) { // (half the way: from where they start to the edge of the red, or to the yellow)
            const int x1 = i == TCE_BANNER ? 0x168 : 0x0F0, x0 = i == TCE_BANNER ? 0x2A8 : 0x080;
            // (how far along it has come is kept: the card is made as the whole picture's and these have usually arrived when the views come)
            const int old_start = own->final_x, old_target = own->main_x, old_x = e[i]->x_pos.f.u;
            const int span = old_target - old_start;
            const int progress = span != 0 ? ((old_x - old_start) * 256) / span : 256;
            const int new_start = 128 + (x0 - 128) / 2, new_target = 128 + (x1 - 128) / 2;
            own->main_x = (int16_t)new_target;
            own->final_x = (int16_t)new_start;
            e[i]->x_pos.f.u = (int16_t)(new_start + ((new_target - new_start) * progress) / 256);
        } else {
            const int dx = target[i] - own->main_x;     // (the element keeps how far it still has to go, and how far out it starts and goes)
            e[i]->x_pos.f.u = (int16_t)(e[i]->x_pos.f.u + dx);
            own->final_x = (int16_t)(own->final_x + dx);
            own->main_x = (int16_t)target[i];
        }
        e[i]->mappings = titlecard_scaled_map[i];
        e[i]->mapping_frame = 0;
        e[i]->routine_secondary = 1;
    }
}

// ---- the bars: Beta 4 draws them on plane A with the VBlank's list writers; here they are the window plane, which does not scroll and can be taken off from the top down (the level wipes in under it) ----
#define TC_BLUE   0x85DA // (the solid tiles of the card's art: blue on line 0, yellow on line 1, red on line 0)
#define TC_YELLOW 0xA5DC
#define TC_RED    0x85D8

static int tc_age; // frames the card has been there

// Whether the views are side by side (each half as wide as the picture): the card is half as wide in them (the stacked views are the whole picture, squashed: the card is as it is)
static bool TitleCard_Halves(void) {
    return SplitScreen_SideBySide(); // (the card is made before the split screen is on, as the whole picture's: it changes when the views come)
}

// The picture's cells: the card's columns are the 320-wide picture's, in the middle of a wider one; in the side by side views every column count is half, from the view's left edge
static void TitleCard_DrawBars(int yellow_from, int blue_until, int red_columns, int covered_px) {
    const bool halves = TitleCard_Halves();
    const int ox = halves ? 0 : PLANE_WIDEADD / 2, oy = PLANE_TALLADD / PLANE_ROW_BYTES;
    if (halves) {
        yellow_from = (yellow_from + 1) / 2;
        blue_until = (blue_until + 1) / 2;
        red_columns = (red_columns + 1) / 2;
    }
    viewport_t *views[2] = { &screen1p, &screen2p };
    for (int v = 0; v < (two_player_mode ? 2 : 1); v++) {
        viewport_t *vp = views[v];
        for (int row = 0; row < (int)PLANE_HEIGHT; row++) {
            for (int col = 0; col < PLANE_WIDTH; col++) {
                const int lr = row - oy, lc = col - ox;
                uint16_t word = TC_BLUE;
                if (lr >= 20 && lc >= yellow_from && lc >= blue_until)
                    word = TC_YELLOW;
                if (lc < red_columns)
                    word = TC_RED;
                Plane_Put(&vp->window, (size_t)row * PLANE_ROW_BYTES + (size_t)col * 2, word);
            }
        }
        if (covered_px >= SCREEN_HEIGHT) {
            Viewport_SetWindow(vp, (window_region_t){ .use_x = true, .x = 0, .right = true });
        } else if (covered_px > 0) {
            Viewport_SetWindow(vp, (window_region_t){ .use_y = true, .y = (uint16_t)covered_px, .below = false });
        } else {
            Viewport_ClearWindow(vp);
        }
    }
}

// The element's slide: 16 pixels a frame (32 on the way out) towards `target`; drawn unless it is past the right edge
static bool TitleCard_Slide(Sst *self, int16_t target, int16_t speed, bool out) {
    if (self->x_pos.f.u != target) {
        self->x_pos.f.u += self->x_pos.f.u > target ? -speed : speed;
        if ((uint16_t)self->x_pos.f.u > 0x200 + SCREEN_WIDEADD)
            return false;
        if (out && self->x_pos.f.u == target)
            return false; // (back where it started: not drawn)
    }
    Sst_Draw(self);
    return true;
}

// Line 1 of Beta 4's Sonic and Miles palette: the yellow of the card's band is its colour 12. The level's own line 1 fades in at the same time, so the card keeps this one until the yellow is gone
static const uint16_t tc_line1[16] = { 0x0C20, 0x0000, 0x0E62, 0x0A86, 0x0E86, 0x0044, 0x0EEE, 0x0AAA, 0x0888, 0x0444, 0x0666, 0x0E86, 0x00EE, 0x0088, 0x0EA8, 0x0ECA };

// The elements' timings (Beta 4's table at loc_10C94): where each starts and stops, and how many frames it waits before it moves
static const struct { int16_t x0, x1, y; uint8_t frame; uint8_t delay; bool wide_aligned; } tc_element[TCE_COUNT] = {
    { 0x0240, 0x0120, 0x00B8, 0, 0x1B, false },                     // the name (frame: by the zone)
    { 0x0028, 0x0148, 0x00D0, 15, 0x1C, false },                    // ZONE
    { 0x0068, 0x0188, 0x00D0, 16, 0x1C, false },                    // the act number (frame: by the act)
    { 0, 0, 0, 0, 0, false },                                       // the curtain
    { 0x02A8, 0x0168, 0x0120, 19, 8, true },                        // the banner (SONIC THE HEDGEHOG), on the yellow
    { 0x0080, 0x00F0, 0x00F0, 20, 0x15, true },                     // the zigzag edge of the red
};

// Title card object
void Obj_TitleCard(Sst *self) {
    Own_TitleCard *own = (Own_TitleCard *)self->own.u8;

    switch (self->routine) {
    case 0: {
        const int zone = LEVEL_ZONE(level_id);
        const uint8_t name = TitleCard_NameFrame(zone);
        titlecard_art_loaded = false; // (the art goes in once the level's first pattern load cues are done: see routine 2)
        tc_age = 0;
        tc_wipe = -1;
        SplitScreen_ShowViews(); // (a versus level has its two views from the first frame of the card)
        const bool bars = true;
        hud_hold_back = bars; // (the HUD comes when the level is shown)

        const int16_t to_add = SCREEN_WIDEADD2;
        const int16_t from_add = to_add + ((SCREEN_WIDEADD2 + 0xF) & ~0xF);
        const int16_t from_sub = (0x10 - to_add) & 0xF;
        const int16_t cells_add = (int16_t)((PLANE_WIDEADD / 2) * 8); // (the bars are whole cells: what is put against them moves by the same)
        Sst *a1 = self;
        for (int i = 0; i < (bars ? TCE_COUNT : 3); i++) {
            if (i > 0) {
                a1 = Sst_AllocateAfter(a1);
                if (a1 == NULL)
                    break;
            }
            const int16_t add = tc_element[i].wide_aligned ? cells_add : to_add;
            const bool from_right = tc_element[i].x0 > tc_element[i].x1;
            Own_TitleCard *scratch = (Own_TitleCard *)a1->own.u8;
            a1->code = Obj_TitleCard;
            a1->subtype = (uint8_t)i;
            a1->x_pos.f.u = (int16_t)(tc_element[i].x0 + (tc_element[i].wide_aligned ? cells_add : (from_right ? from_add : -from_sub)));
            scratch->final_x = a1->x_pos.f.u;
            scratch->main_x = (int16_t)(tc_element[i].x1 + add);
            scratch->timer = tc_element[i].delay;
            a1->y_pos.f.u = (int16_t)(tc_element[i].y + SCREEN_TALLADD2);
            a1->routine = 2;
            uint8_t frame = tc_element[i].frame;
            if (i == TCE_NAME)
                frame = name;
            else if (i == TCE_ACT)
                frame = (uint8_t)(TC_FRAME_ACT1 + LEVEL_ACT(level_id));
            a1->mapping_frame = frame;
            a1->status_secondary = frame; // (the unscaled frame, for the split screen)
            a1->routine_secondary = 0;
            a1->mappings = Mappings_TitleCardNames;
            a1->art_tile = 0; // (the pieces hold their tiles and their priority: $580 on)
            a1->width_pixels = 0x60;
            a1->render_flags = 0;
            a1->priority = 0 * 0x80;
        }
    }
        // Fallthrough
    case 2: // each element: waits its frames, then slides in; the curtain draws the bars
        if (self->subtype == TCE_ACT) {
            const int z = LEVEL_ZONE(level_id);
            if (z == ZoneId_SCZ || z == ZoneId_SFZ || z == ZoneId_DEZ) { // (the zones that are not played in acts have no number)
                Sst_Delete(self);
                return;
            }
        }
        if (self->subtype == TCE_CURTAIN) {
            if (tc_wipe < 0 && !TitleCard_Arrived())
                memcpy(dry_palette_dup[1], tc_line1, sizeof(tc_line1)); // (the colours the screen fades to: until the level's own are made the target, when the level is loaded)
            if (tc_wipe < 14)
                memcpy(dry_palette[1], tc_line1, sizeof(tc_line1)); // (from the start: the level's own line 1 is there before the card's art is)
            else if (tc_wipe == 14)
                memcpy(dry_palette[1], dry_palette_dup[1], sizeof(tc_line1)); // (the yellow is gone: the level's colours)
        }
        if (!titlecard_art_loaded) {
            if (plc_buffer[0].art != NULL)
                break; // (the queue is still being drained: nothing of the card shows yet)
            TitleCard_LoadArt(TitleCard_NameFrame(LEVEL_ZONE(level_id)));
            titlecard_art_loaded = true;
        }
        if (TitleCard_SplitWanted())
            TitleCard_FitSplit(); // (squeezes what is in video memory: the art is there)
        if (self->subtype == TCE_CURTAIN) {
            const int age = tc_age++;
            // growing: the blue comes down two rows a frame, the yellow comes in from the right two columns a frame (after 8 frames), the red out from the left two columns a frame (after 21)
            const int rows = 2 * (age + 1);
            const int yellow = age < 7 ? 99 : (38 - 2 * (age - 7) < 8 ? 8 : 38 - 2 * (age - 7));
            const int red = age < 20 ? 0 : (2 * (age - 20 + 1) > 14 ? 14 : 2 * (age - 20 + 1));
            if (tc_wipe < 0) {
                TitleCard_DrawBars(yellow, 0, red, rows >= 32 ? SCREEN_HEIGHT : rows * 8 + (int)(PLANE_TALLADD / PLANE_ROW_BYTES) * 8);
                break;
            }
            // wiping (Beta 4's phase B): the red comes off from the right four columns a frame, then the yellow is painted blue from the left four columns a frame, then the level shows from the bottom up in bands of 32 pixels
            const int f = tc_wipe++;
            static const int red_keep[4] = { 10, 6, 2, 0 };
            const int red_now = f < 4 ? red_keep[f] : 0;
            const int yellow_from = f < 4 ? (red_keep[f] < 8 ? red_keep[f] : 8) : 0;
            const int blue_until = f < 5 ? 0 : (f < 14 ? 4 * (f - 4) : 1000);
            int covered = SCREEN_HEIGHT;
            if (f >= 16)
                covered = (208 - 32 * (f - 16)) + (int)(PLANE_TALLADD / PLANE_ROW_BYTES) * 8;
            if (f == 5) {
                Sst *banner = TitleCard_Element(TCE_BANNER);
                if (banner != NULL)
                    banner->routine = 4; // (the banner goes as the yellow is painted over)
            }
            if (f == 0) {
                Sst *stripes = TitleCard_Element(TCE_STRIPES);
                if (stripes != NULL)
                    stripes->routine = 4;
            }
            if (f == 4 || f == 15) { // (the red is off, and the yellow: the elements that were drawing them are done, wherever they have got to)
                Sst *gone = TitleCard_Element(f == 4 ? TCE_STRIPES : TCE_BANNER);
                if (gone != NULL)
                    Sst_Delete(gone);
            }
            if (f >= 16 + 8) { // (the level is all there: the names wait and go)
                Viewport_ClearWindow(&screen1p);
                Viewport_ClearWindow(&screen2p);
                hud_hold_back = false;
                for (int i = 0; i < 3; i++) {
                    Sst *e = TitleCard_Element(i);
                    if (e != NULL) {
                        e->routine = 6;
                        ((Own_TitleCard *)e->own.u8)->timer = 60;
                    }
                }
                Sst_Delete(self);
                break;
            }
            TitleCard_DrawBars(yellow_from, blue_until, red_now, covered);
            break;
        }
        if (own->timer > 1) {
            own->timer--;
            break;
        }
        TitleCard_Slide(self, own->main_x, TitleCard_Halves() && (self->subtype == TCE_BANNER || self->subtype == TCE_STRIPES) ? 8 : 16, false);
        break;
    case 4: // the banner and the zigzag edge going out (the names hold: routine 5)
        if (self->subtype == TCE_BANNER || self->subtype == TCE_STRIPES) {
            if (self->x_pos.f.u == own->final_x) {
                Sst_Delete(self);
                break;
            }
            TitleCard_Slide(self, own->final_x, TitleCard_Halves() ? 16 : 32, true);
            break;
        }
        // (the names hold on the card until the wipe is done)
        if (TitleCard_SplitWanted())
            TitleCard_FitSplit();
        Sst_Draw(self);
        break;
    case 6: // the names: wait a second, then slide out, 32 pixels a frame
        if (TitleCard_SplitWanted())
            TitleCard_FitSplit();
        if (own->timer) {
            own->timer--;
            Sst_Draw(self);
            break;
        }
        if (self->render_flags & SST_RENDER_ON_SCREEN) {
            if (self->x_pos.f.u != own->final_x) {
                TitleCard_Slide(self, own->final_x, 32, true);
                break;
            }
        }
        // (gone, or back where it started) the name's end loads the explosions' and the animals' art
        if (self->subtype == TCE_NAME) {
            const PlcId plc2 = LEVEL_LOAD_BLOCK(level_id)->plc2;
            if (plc2 != 0 && TitleCard_ArtClashes(plc2))
                AddPLC(plc2); // (the zone's second list, which LevelDataLoad leaves for here when it has art in the card's tiles: first, as it would be queued already otherwise)
            AddPLC(PlcId_Explode);
            AddPLC(Level_AnimalsPlc()); // (the zone's pair of the alpha's: Emerald Hill's is the pre-beta's too, Squirrel and BlueBird)
        }
        Sst_Delete(self);
        break;
    }
}
