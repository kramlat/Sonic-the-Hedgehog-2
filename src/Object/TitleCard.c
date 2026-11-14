#include "Object/TitleCard.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"

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

// What each act leads to: the pre-beta's Level_Sequence_List (and Level_Sequence_List_2P, for the versus play), one pair of entries for each of its zone numbers (the first act's, then the second's); the high byte of an entry
// is the zone and the low byte the act. Emerald Hill 2 goes to Chemical Plant, which goes to Neo Green Hill, Casino Night, Hill Top, Dust Hill, Oil Ocean, Metropolis, then Sky Chase (the pre-beta's own entry), Sky
// Fortress and Death Egg, whose entry ends the game ($FFFF: the Sega screen, as `Game_Mode = 0`). The entries of the zones the pre-beta has none of are 0 (the first act of Emerald Hill, as its own table does). Its third
// act of Metropolis is the pre-beta's zone 5 (row 5 here), which leads to Neo Green Hill.
static const uint16_t level_order[2][17][2] = {
    { // one player
        { 0x0001, 0x0D00 }, { 0, 0 }, { 0x0201, 0x0400 }, { 0, 0 }, { 0x0401, 0x1000 }, { 0x0F00, 0 }, { 0x0E00, 0 }, { 0x0701, 0x0B00 }, { 0x0801, 0x0A00 },
        { 0, 0 }, { 0x0A01, 0x0400 }, { 0x0B01, 0x0A00 }, { 0x0C01, 0x0700 }, { 0x0D01, 0x0F00 }, { 0xFFFF, 0 }, { 0x0F01, 0x0C00 }, { 0x0600, 0 },
    },
    { // two players (versus)
        { 0x0001, 0x0C00 }, { 0, 0 }, { 0x0201, 0x0400 }, { 0, 0 }, { 0x0401, 0x1000 }, { 0x0F00, 0 }, { 0x0E00, 0 }, { 0x0701, 0x0B00 }, { 0x0801, 0x0A00 },
        { 0, 0 }, { 0x0A01, 0x0400 }, { 0x0B01, 0xFFFF }, { 0x0C01, 0x0B00 }, { 0x0D01, 0x0F00 }, { 0xFFFF, 0 }, { 0x0F01, 0x0C00 }, { 0x0600, 0 },
    },
};

// The act after this one, in the form the level id has it, or 0xFFFF for the end of the game
uint16_t TitleCard_NextLevel(void) {
    int zone = LEVEL_ZONE(level_id), act = LEVEL_ACT(level_id);
    if (zone == ZoneId_MTZ && act == 2) { // (the third act of Metropolis is the pre-beta's zone 5)
        zone = 5;
        act = 0;
    }
    if (zone > 0x10)
        return 0;
    return level_order[two_player_mode != 0][zone][act & 1];
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
            a1->mapping_frame = item->frame;
            a1->mappings = Mappings_LevelResults;
            a1->art_tile = TILE_MAP(1, 0, 0, 0, 0); // (priority; the pieces hold their tiles)
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
                    icon->art_tile = TILE_MAP(1, 0, 0, 0, 0);
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

// The elements of the level's title card, in the order they were made: the name, ZONE and the act number (NULL where the card is not there)
static Sst *TitleCard_Element(int n) {
    for (int i = 0; i < SST_COUNT; i++)
        if (sst[i].code != NULL && sst[i].code == Obj_TitleCard && n-- == 0)
            return &sst[i];
    return NULL;
}

bool TitleCard_Arrived(void) {
    const Sst *zone = TitleCard_Element(0); // (the level waits for the name: the pre-beta's `cmp.w ($FFFFB0B0).w,D0`)
    return zone == NULL || zone->x_pos.f.u == ((const Own_TitleCard *)zone->own.u8)->main_x;
}

void TitleCard_Leave(void) {
    static const uint8_t add[3] = { 2, 4, 4 }; // (the name loads the animals' art as it goes: routine 4; the others 6)
    for (int i = 0; i < 3; i++) {
        Sst *e = TitleCard_Element(i);
        if (e != NULL)
            e->routine = (uint8_t)(e->routine + add[i]);
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
static uint8_t titlecard_scaled_map[3][4 + 24 * SPRITE_PIECE_SIZE_S3];

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

// Makes the card fit the views: all three elements, once the split screen is on (the card is made before it is: the level is loaded under it)
static void TitleCard_FitSplit(void) {
    Sst *e[3] = { TitleCard_Element(0), TitleCard_Element(1), TitleCard_Element(2) };
    if (e[0] == NULL || e[0]->routine_secondary)
        return;
    const int view_w = SCREEN_WIDTH / 2;
    const int centre = 128 + view_w / 2; // (an object's x counts from 128 to the view's left edge)
    unsigned next_tile = TC_SCALED_TILES;
    int left[3] = { 0, 0, 0 }, right[3] = { 0, 0, 0 };
    for (int i = 0; i < 3; i++)
        if (e[i] != NULL)
            TitleCard_Squeeze(i, e[i]->status_secondary, &next_tile, &left[i], &right[i]);
    // where each stands when it has arrived: the name in the middle; ZONE and the number after it as one, in the middle
    int target[3];
    target[0] = centre - (left[0] + right[0]) / 2;
    const int zone_w = right[1] - left[1], number_w = e[2] != NULL ? right[2] - left[2] : 0, gap = e[2] != NULL ? 6 : 0;
    const int start = centre - (zone_w + gap + number_w) / 2;
    target[1] = start - left[1];
    target[2] = start + zone_w + gap - left[2];
    for (int i = 0; i < 3; i++) {
        if (e[i] == NULL)
            continue;
        Own_TitleCard *own = (Own_TitleCard *)e[i]->own.u8;
        const int dx = target[i] - own->main_x;       // (the element keeps how far it still has to go, and how far out it starts and goes)
        e[i]->x_pos.f.u = (int16_t)(e[i]->x_pos.f.u + dx);
        own->final_x = (int16_t)(own->final_x + dx);
        own->main_x = (int16_t)target[i];
        e[i]->mappings = titlecard_scaled_map[i];
        e[i]->mapping_frame = 0;
        e[i]->routine_secondary = 1;
    }
}

// Title card object
void Obj_TitleCard(Sst *self) {
    Own_TitleCard *own = (Own_TitleCard *)self->own.u8;

    switch (self->routine) {
    case 0: {
        const int zone = LEVEL_ZONE(level_id);
        const uint8_t name = TitleCard_NameFrame(zone);
        titlecard_art_loaded = false; // (the art goes in once the level's first pattern load cues are done: see routine 2)

        // Get configuration (built now: it depends on the size of the picture): where each element starts and where it stops
        const int16_t to_add = SCREEN_WIDEADD2;
        const int16_t from_add = to_add + ((SCREEN_WIDEADD2 + 0xF) & ~0xF);
        const int16_t from_sub = (0x10 - to_add) & 0xF;
        const struct { int16_t x0, x1; int16_t y; uint8_t frame; } card[3] = {
            { 0x0240 + from_add, 0x0120 + to_add, 0x00B8 + SCREEN_TALLADD2, name },
            { 0x0028 - from_sub, 0x0148 + to_add, 0x00D0 + SCREEN_TALLADD2, TC_FRAME_ZONE },
            { 0x0068 - from_sub, 0x0188 + to_add, 0x00D0 + SCREEN_TALLADD2, (uint8_t)(TC_FRAME_ACT1 + LEVEL_ACT(level_id)) },
        };
        // (Sky Chase, Sky Fortress and Death Egg are not played in acts: their card has no number)
        const int count = (zone == ZoneId_SCZ || zone == 6 || zone == 0x0E) ? 2 : 3;

        Sst *a1 = self;
        for (int i = 0; i < count; i++) {
            if (i > 0) {
                a1 = Sst_AllocateAfter(a1);
                if (a1 == NULL)
                    break;
            }
            Own_TitleCard *scratch_a1 = (Own_TitleCard *)a1->own.u8;
            a1->code = Obj_TitleCard;
            a1->x_pos.f.u = card[i].x0;
            scratch_a1->final_x = card[i].x0;
            scratch_a1->main_x = card[i].x1;
            a1->y_pos.f.u = card[i].y;
            a1->routine = 2;
            a1->mapping_frame = card[i].frame;
            a1->status_secondary = card[i].frame; // (the unscaled frame, for the split screen)
            a1->routine_secondary = 0;
            a1->mappings = Mappings_TitleCardNames;
            a1->art_tile = TILE_MAP(1, 0, 0, 0, 0); // (priority; the pieces hold their tiles: $580 on)
            a1->width_pixels = 0x60;
            a1->render_flags = 0;
            a1->priority = 0 * 0x80;
            a1->anim_frame_timer = 60;
        }
    }
        // Fallthrough
    case 2: // Moving to on-screen position, 16 pixels a frame
        if (TitleCard_SplitWanted())
            TitleCard_FitSplit();
        if (!titlecard_art_loaded) {
            if (plc_buffer[0].art != NULL)
                break; // (the queue is still being drained: nothing of the card shows yet)
            TitleCard_LoadArt(TitleCard_NameFrame(LEVEL_ZONE(level_id)));
            titlecard_art_loaded = true;
        }
        if (self->x_pos.f.u > own->main_x)
            self->x_pos.f.u -= 16;
        else if (self->x_pos.f.u < own->main_x)
            self->x_pos.f.u += 16;
        if (self->x_pos.f.u >= 0 && self->x_pos.f.u < (0x200 + SCREEN_WIDEADD))
            Sst_Draw(self);
        break;
    case 4: // Waits, then moves off-screen, 32 pixels a frame
    case 6:
        if (TitleCard_SplitWanted())
            TitleCard_FitSplit();
        if (self->anim_frame_timer) {
            self->anim_frame_timer--;
            Sst_Draw(self);
            break;
        }
        if (self->render_flags & SST_RENDER_ON_SCREEN) {
            if (self->x_pos.f.u != own->final_x) {
                if (self->x_pos.f.u > own->final_x)
                    self->x_pos.f.u -= 32;
                else
                    self->x_pos.f.u += 32;
                if (self->x_pos.f.u >= 0 && self->x_pos.f.u < (0x200 + SCREEN_WIDEADD))
                    Sst_Draw(self);
                break;
            }
        }
        // (gone, or back where it started) the name's end loads the explosions' and the animals' art
        if (self->routine == 4) {
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
