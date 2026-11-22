// Beta 4's special stage results screen: see SpecialResults.h
#include "SpecialResults.h"
#include "GM_SpecialTrack.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"
#include "Constants.h"
#include "Macros.h"
#include "Game.h"
#include "Level.h"
#include "Palette.h"
#include "EnginePalette.h"
#include "PaletteIds.h"
#include "PLC.h"
#include "Nemesis.h"
#include "Sound.h"
#include "SoundIds.h"
#include "Viewport.h"
#include "Backend/VDP.h"

#include "Resource/Mappings/SSB4ResultsCard.h"
#include "Resource/Art/SSB4Results.h"
#include "Resource/Art/SSB4Emeralds.h"

#define SFX(id) (sfx_Jump + ((id) - 0xA0))

extern const uint8_t Art_HUD[];
extern const uint8_t *EndCard_ZoneArt(void); // (Sonic2PLC.c: the title card's ZONE letters, which this card shares)
void HUD_WriteBonusCard(uint16_t first, uint16_t second, uint16_t third);
void Obj_SpecialResultsPiece(Sst *self);

// loc_1156A: the 13 pieces: where each starts and where it goes (X), its Y, its routine and its frame
static const struct { uint16_t x, target, y; uint8_t routine, frame; } pieces[13] = {
    { 0x230, 0x120, 0xAA, 0x02, 0x00 }, { 0x010, 0x120, 0x98, 0x04, 0x01 }, { 0x118, 0, 0xC0, 0x06, 0x05 }, { 0x130, 0, 0xCC, 0x08, 0x06 }, { 0x130, 0, 0xE4, 0x0A, 0x07 },
    { 0x118, 0, 0xF0, 0x0C, 0x08 }, { 0x100, 0, 0xE4, 0x0E, 0x09 }, { 0x100, 0, 0xCC, 0x10, 0x0A }, { 0x118, 0, 0xD8, 0x12, 0x0B }, { 0x330, 0x120, 0x108, 0x14, 0x0C },
    { 0x340, 0x120, 0x118, 0x16, 0x0D }, { 0x350, 0x120, 0x128, 0x18, 0x0F }, { 0x360, 0x120, 0x138, 0x1A, 0x10 },
};

static int sonic_left, miles_left, bonus_left; // $FFFFF7D2, $FFFFF7D4 and $FFFFFF8E: what is still to be counted
static bool emerald_won, finished, show_numbers;
static uint16_t frames;
static int16_t card_offset; // (a wider screen than the card's 320: the card is in the middle)

static void Slide(Sst *self) { // loc_10DDC: 16 pixels a frame toward the place it is for (once it is within sight it is drawn)
    int16_t x = self->x_pos.f.u;
    const int16_t target = (int16_t)self->own.u16[0];
    if (x != target) {
        x = (int16_t)(x + (x > target ? -16 : 16));
        self->x_pos.f.u = x;
    }
    if ((uint16_t)x <= 0x200 + card_offset)
        Sst_Draw(self);
}

void Obj_SpecialResultsPiece(Sst *self) {
    const int i = self->subtype;
    if (self->routine == 0) { // loc_11372
        self->routine = pieces[i].routine;
        self->mappings = Mappings_SSB4ResultsCard;
        self->art_tile = 0;
        self->x_pos.f.u = (int16_t)(pieces[i].x + card_offset);
        self->y_pos.f.u = (int16_t)pieces[i].y;
        self->own.u16[0] = (uint16_t)(pieces[i].target + card_offset);
        self->mapping_frame = pieces[i].frame;
        self->render_flags |= 0; // (the card's pieces are in the VDP's own sprite coordinates, 128 more than the screen's, not the level's)
        self->priority = 0;
    }
    switch (self->routine) {
    case 0x02: // loc_113B6: the title, which is a controller as well: it slides in, waits $B4 frames, and then the counting begins
        if (emerald_won)
            self->mapping_frame = 4;
        if ((int16_t)self->x_pos.f.u == (int16_t)self->own.u16[0]) {
            self->routine = 0x1C;
            self->own.u16[1] = 0xB4;
        }
        Slide(self);
        return;
    case 0x04: // loc_113DC
        if (!emerald_won) {
            Sst_Delete(self);
            return;
        }
        Slide(self);
        return;
    case 0x06: case 0x08: case 0x0A: case 0x0C: case 0x0E: case 0x10: case 0x12: { // loc_113F4 on: an emerald: shown, blinking, if it is won
        const int n = (self->routine - 6) / 2;
        if (!ss4_emerald[n]) {
            Sst_Delete(self);
            return;
        }
        if (frames & 1)
            Sst_Draw(self);
        return;
    }
    case 0x14: Slide(self); return; // loc_11462
    case 0x16: // loc_11452: Sonic's line: the frame with his rings, or the one without once they are counted
        self->mapping_frame = (uint8_t)(sonic_left == 0 ? 0x12 : 0x0D);
        Slide(self);
        return;
    case 0x18: // loc_1140E: Miles' line
        self->mapping_frame = (uint8_t)(miles_left == 0 ? 0x14 : 0x0F);
        Slide(self);
        return;
    case 0x1A: // loc_11466: the emerald's bonus line
        if (!emerald_won) {
            Sst_Delete(self);
            return;
        }
        Slide(self);
        return;
    case 0x1C: // loc_11470: waiting
    case 0x20:
        if (--self->own.u16[1] == 0)
            self->routine += 2;
        Sst_Draw(self);
        return;
    case 0x1E: { // loc_1147E: the counting: every frame one ring of each character and ten of the bonus go to the score (ten points a ring: the added is $A for each that is left)
        Sst_Draw(self);
        show_numbers = true;
        int points = 0;
        if (sonic_left) {
            points += 10;
            sonic_left--;
        }
        if (miles_left) {
            points += 10;
            miles_left--;
        }
        if (bonus_left) {
            points += 10;
            bonus_left -= 10;
        }
        if (points != 0) {
            AddPoints((uint16_t)points);
            if ((frames & 3) == 0)
                QueueSound2(SFX(0xCD));
            return;
        }
        QueueSound2(SFX(0xC5)); // (counted: the cash sound)
        self->routine += 2;
        self->own.u16[1] = 0x78;
        return;
    }
    case 0x22: // loc_114FC: done
        finished = true;
        Sst_Draw(self);
        return;
    }
}

void SpecialResults_Run(int sonic_rings, int miles_rings, bool got_emerald) {
    sonic_left = sonic_rings;
    miles_left = miles_rings;
    emerald_won = got_emerald;
    bonus_left = got_emerald ? 1000 : 0; // ($3E8 with an emerald)
    finished = false;
    show_numbers = true;
    frames = 0;
    PaletteFadeOut();
    Viewport_SetSize(SCREEN_WIDTH, SCREEN_HEIGHT, PLANE_WIDTH, PLANE_HEIGHT);
    ClearScreen();
    VDP_SeekVRAM(0);
    VDP_FillVRAM(0, 0x10000);
    VDP_SeekVRAM(0x580 * 32);
    NemDec(EndCard_ZoneArt());                // $B000: the ZONE letters
    VDP_SeekVRAM(0x590 * 32);
    NemDec(Art_SSB4Results);                  // $B200: the card's words
    VDP_SeekVRAM(0x6B0 * 32);
    NemDec(Art_SSB4Emeralds);                 // $D600: the emeralds
    VDP_SeekVRAM(ArtTile_HUD * 32);
    NemDec(Art_HUD);                          // the HUD's own letters (SCORE, RINGS), which the card's labels are made of (the level list 0)
    PalLoad1(PalId_SSB4Results);
    Sst_ResetAll();
    for (int i = 0; i < 13; i++) {
        Sst *o = Sst_Allocate();
        if (o != NULL) {
            o->code = Obj_SpecialResultsPiece;
            o->subtype = (uint8_t)i;
        }
    }
    PlayMusic(mus_ActClear);
    HUD_WriteBonusCard((uint16_t)bonus_left, (uint16_t)sonic_left, (uint16_t)miles_left);
    card_offset = (int16_t)((SCREEN_WIDTH - 320) / 2);
    scrpos_x.v = 0;
    scrpos_y.v = 0;
    ExecuteObjects();
    BuildSprites(NULL);
    PaletteFadeIn();
    while (!finished && frames < 1200) {
        vbla_routine = 0x16;
        WaitForVBla();
        frames++;
        ExecuteObjects();
        if (show_numbers) {
            show_numbers = false;
            HUD_WriteBonusCard((uint16_t)bonus_left, (uint16_t)sonic_left, (uint16_t)miles_left);
        }
        BuildSprites(NULL);
    }
    PaletteFadeOut();
}
