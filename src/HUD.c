// Sonic 2's HUD updates: Sonic 1's HUD.c (a copy) without the time over kill (nopped out in Nick Arcade); the HUD's art, lives counter and digits are Nick Arcade's (res/Art).
#include "HUD.h"
#include "Constants.h"

#include "Game.h"
#include "Level.h"
#include "LevelScroll.h"
#include "Object/Sonic.h"
#include "Video.h"
#include "TileBank.h"
#include "Backend/VDP.h"

#include <stdint.h>

// HUD assets
#include "VersusStats.h"
#include "Resource/Art/HUDNum.h"
#include "Resource/Art/LifeNum.h"

// HUD art writing
static const uint32_t hud_dec[] = {
    100000,
    10000,
    1000,
    100,
    10,
    1
};

static const uint8_t hud_cmd_base[] = {
    /*Hud_TilesBase:*/ 0x16, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x14, 0x00, 0x00,
    /*Hud_TilesZero:*/ 0xFF, 0xFF, 0x00
};
static const uint8_t* hud_cmd_ringbase = &hud_cmd_base[12];

void HUD_WriteCmd(size_t offset, const uint8_t* cmd, size_t cmds) {
    VDP_SeekVRAM(offset);

    do {
        // Get tile
        int16_t tile = (int8_t)*cmd++;
        if (tile >= 0) {
            const uint8_t* art = Art_HUDNum + (tile <<= 5);
            VDP_WriteVRAM(art, 64);
        } else {
            VDP_FillVRAM(0, 64);
        }
    } while (cmds-- > 0);
}

void HUD_WriteNumber(size_t offset, uint32_t value, const uint32_t* dec, size_t decs) {
    bool digit_write = false;
    do {
        // Get digit
        uint16_t digit;
        for (digit = 0; value >= *dec; digit++, value -= *dec)
            ;

        // Write digit
        if (digit)
            digit_write = true;
        if (digit_write) {
            const uint8_t* art = Art_HUDNum + (digit <<= 6);
            VDP_SeekVRAM(offset);
            VDP_WriteVRAM(art, 64);
        }

        // Increment offset and decimal
        offset += 64;
        dec++;
    } while (decs-- > 0);
}

void HUD_WriteNumber2(size_t offset, uint32_t value, const uint32_t* dec, size_t decs) {
    do {
        // Get digit
        uint16_t digit;
        for (digit = 0; value >= *dec; digit++, value -= *dec)
            ;

        // Write digit
        const uint8_t* art = Art_HUDNum + (digit <<= 6);
        VDP_SeekVRAM(offset);
        VDP_WriteVRAM(art, 64);

        // Increment offset and decimal
        offset += 64;
        dec++;
    } while (decs-- > 0);
}

void HUD_WriteHex(size_t offset, uint32_t value) {
    VDP_SeekVRAM(offset);

    size_t decs = 7;
    do {
        // Get digit
        value = (value & 0xFFFF0000) | (((value & 0x0FFF) << 4) | ((value & 0xF000) >> 12));

        uint16_t digit = value & 0xF;
        if (digit >= 0xA)
            digit += 7;

        // Write digit
        const uint8_t* art = Art_Text + (digit <<= 5);
        VDP_WriteVRAM(art, 32);
        value = (value >> 16) | (value << 16);
    } while (decs-- > 0);
}

// HUD functions
static bool second_synced; // (the second view's HUD art has the words copied: see HUD_UpdateSecond)
extern uint16_t endact_total; // (Object/TitleCard.c)
extern uint8_t two_player_mode; // (SplitScreen.c)
extern bool hud_player_two;     // (Object/HUD.c)
#define MILES_DIGITS_VRAM 0xFD20 // (the lives digits in Miles' name: the tiles after its icon, 0x7E4 + 5)

void HUD_Lives(void) {
    const bool miles_counter = two_player_mode || hud_player_two;
    // (the second player's counter, with the name TAILS or MILES, is what the level's start loads: Options_LoadLifeCounter, the final's PLCs MilesLife2P and TailsLife2P; the digits are copied into its name here)
    size_t offset = 0xFBA0;

    uint32_t value = lives;

    const uint32_t* dec = &hud_dec[4];
    size_t decs = 1;

    bool digit_write = false;
    do {
        // Get digit
        uint16_t digit;
        for (digit = 0; value >= *dec; digit++, value -= *dec)
            ;

        // Write digit
        if (digit)
            digit_write = true;
        for (int copy = 0; copy < (miles_counter ? 2 : 1); copy++) {
            const size_t at = copy == 0 ? offset : offset - 0xFBA0 + MILES_DIGITS_VRAM; // (the second copy: in Miles' name)
            if (digit_write || decs == 0) {
                const uint8_t* art = Art_LifeNum + ((size_t)digit << 5);
                VDP_SeekVRAM(at);
                VDP_WriteVRAM(art, 32);
            } else {
                VDP_SeekVRAM(at);
                VDP_FillVRAM(0, 32);
            }
        }

        // Increment offset and decimal
        offset += 64;
        dec++;
    } while (decs-- > 0);
}

// The special stage results' three numbers (Beta 4's `$FFFFFF8E`, `$FFFFF7D2` and `$FFFFF7D4`: the emerald bonus, Sonic's rings and Miles') at the same place as the act results'
void HUD_WriteBonusCard(uint16_t first, uint16_t second, uint16_t third);

// Hud_TimeRingBonus: writes a 4-digit bonus as 8x16 digits at the current VRAM position, blanking leading zeroes.
static void HUD_WriteBonus(uint16_t value) {
    static const uint16_t places[4] = { 1000, 100, 10, 1 };
    static const uint8_t blank[64] = { 0 };
    bool started = false;
    for (int i = 0; i < 4; i++) {
        uint16_t digit = 0;
        while (value >= places[i]) {
            value = (uint16_t)(value - places[i]);
            digit++;
        }
        if (digit != 0)
            started = true;
        VDP_WriteVRAM(started ? Art_HUDNum + (digit << 6) : blank, 64);
    }
}

void HUD_Base(void) {
    second_synced = false; // (the second view's copy of the words is taken again)
    // Write lives and initial HUD cmd
    HUD_Lives();
    HUD_WriteCmd(0xDC40, hud_cmd_base, 14);
}


// The second view of a split screen has numbers of its own: a tile bank (SplitScreen.c makes it) with a copy of the HUD's art, where Miles' score, rings and time are written as they change; the HUD's second sprite
// (Object/HUD.c) draws from it. The words are the main bank's copies, taken when the HUD's base has been written.
extern const tilebank_t *hud_second_bank;
extern bool hud_second_rings_zero, hud_second_minutes_nine;
static uint32_t second_score;
static uint16_t second_rings;
static uint8_t second_min, second_sec;

static void HUD_BankNumber(size_t first_tile, uint32_t value, const uint32_t *dec, size_t decs, bool blank_leading, bool zero_shows) {
    static const uint8_t blank[64];
    tilebank_t *bank = (tilebank_t *)hud_second_bank;
    bool started = !blank_leading;
    size_t tile = first_tile;
    do {
        uint16_t digit;
        for (digit = 0; value >= *dec; digit++, value -= *dec)
            ;
        if (digit || (decs == 0 && zero_shows)) // (the rings' last digit shows a 0 too; the score's has the 0 of its mapping after it)
            started = true;
        TileBank_Write(bank, tile, started ? Art_HUDNum + (digit << 6) : blank, 64);
        tile += 2;
        dec++;
    } while (decs-- > 0);
}

static void HUD_UpdateSecond(void) {
    if (hud_second_bank == NULL)
        return;
    if (!second_synced) {
        TileBank_Write((tilebank_t *)hud_second_bank, ArtTile_HUD, VDP_PeekVRAM() + ArtTile_HUD * TILE_SIZE, 0x3A * TILE_SIZE);
        second_synced = true;
        second_score = ~0u;
        second_rings = 0xFFFF;
        second_min = second_sec = 0xFF;
    }
    uint32_t s;
    uint16_t r;
    uint8_t min, sec;
    VsStats_SecondHud(&s, &r, &min, &sec);
    if (s != second_score) {
        second_score = s;
        HUD_BankNumber(ArtTile_HUDScore, s, &hud_dec[0], 5, true, false);
    }
    if (r != second_rings) {
        second_rings = r;
        HUD_BankNumber(ArtTile_HUDRings, r, &hud_dec[3], 2, true, true);
    }
    if (min != second_min) {
        second_min = min;
        HUD_BankNumber(ArtTile_HUDTimeMins, min, &hud_dec[5], 0, false, true);
    }
    if (sec != second_sec) {
        second_sec = sec;
        HUD_BankNumber(ArtTile_HUDTimeSecs, sec, &hud_dec[4], 1, false, true);
    }
    hud_second_rings_zero = r == 0;
    hud_second_minutes_nine = min == 9;
}

void HUD_Update(void) {
    // As in Sonic & Knuckles, the HUD looks normal until Sonic is replaced by an object (debug_use) and is put back to normal when he is himself again
    static bool was_object;
    const bool is_object = debug_use != 0;
    if (was_object && !is_object) {
        HUD_Base();
        score_count = true;
        ring_count = true;
        HUD_WriteNumber2(0xDE40, level_time.min, &hud_dec[5], 0);
        HUD_WriteNumber2(0xDEC0, level_time.sec, &hud_dec[4], 1);
    }
    was_object = is_object;

    if (!is_object) {
        // Update score
        if (score_count) {
            score_count = false;
            HUD_WriteNumber(0xDC80, score, &hud_dec[0], 5);
        }

        // Update rings
        if (ring_count) {
            if (ring_count & 0x80) {
                // Set rings to 0
                HUD_WriteCmd(0xDF40, hud_cmd_ringbase, 2);
                // Fallthrough to the below code?
            }

            // Update rings count
            ring_count = false;
            HUD_WriteNumber(0xDF40, rings, &hud_dec[3], 2);
        }

        // Update time
        if (time_count && !pause_state) { // (the original skips the clock while f_pause is set)
            // (Nick Arcade has no time over: its check of 9:59:59 is a nop, the clock just stops at 9 minutes)
            // Increment time
            if (++level_time.frame >= 60) {
                level_time.frame = 0;
                if (++level_time.sec >= 60) {
                    level_time.sec = 0;
                    if (++level_time.min > 9)
                        level_time.min = 9;
                }
            }

            if (vs_stats_active)
                VsStats_Tick(); // (the second player's clock)

            // Write time
            HUD_WriteNumber2(0xDE40, level_time.min, &hud_dec[5], 0);
            HUD_WriteNumber2(0xDEC0, level_time.sec, &hud_dec[4], 1);
        }

        // Update lives
    } else {
        // Update position
        HUD_WriteHex(0xDC40, (scrpos_x.f.u << 16) | player->x_pos.f.u);
        HUD_WriteHex(0xDD40, (scrpos_y.f.u << 16) | player->y_pos.f.u);

        // Update rings
        if (ring_count) {
            if (ring_count & 0x80) {
                // Set rings to 0
                HUD_WriteCmd(0xDF40, hud_cmd_ringbase, 2);
                // Fallthrough to the below code?
            }

            // Update rings count
            ring_count = false;
            HUD_WriteNumber(0xDF40, rings, &hud_dec[3], 2);
        }

        // Update sprite count
        HUD_WriteNumber2(0xDEC0, sprite_count, &hud_dec[4], 1);
     }

    // Update lives
    if (life_count) {
        life_count = false;
        HUD_Lives();
    }

    HUD_UpdateSecond();

    // Update the time and ring bonus numbers of the end-of-act and special stage results cards
    if (endact_bonus) {
        endact_bonus = false;
        VDP_SeekVRAM(0x520 * TILE_SIZE); // (Beta 4's end of act card: the total, the time bonus and the ring bonus, four digits each)
        HUD_WriteBonus(endact_total);
        HUD_WriteBonus(time_bonus);
        HUD_WriteBonus(ring_bonus);
    }
}

void HUD_WriteBonusCard(uint16_t first, uint16_t second, uint16_t third) {
    VDP_SeekVRAM(0x520 * TILE_SIZE);
    HUD_WriteBonus(first);
    HUD_WriteBonus(second);
    HUD_WriteBonus(third);
}
