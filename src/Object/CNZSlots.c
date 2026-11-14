// Casino Night's slot machine of Sonic 2 Beta 4 (checked against `Load_Slot_Machine_Sprites`, Sonic2B4.asm $27918 to $27E3F, and its object $D6, on 2026-11-14; the pre-beta's had another machine: a state for each
// step of each reel, speeds as words). The machine is a state machine of $FF4E (here `m`, 18 bytes, Beta 4's own layout so that the translation stays line for line):
//   m[0] state (0 init, 4 first draw, 8 set going, $C draw and wait, $10 the reels, $14 done, $18 resting), m[1] how many full drawings to wait, m[2] a wait in frames, m[3] which reel is drawn next (0, 4, 8),
//   m[4], m[5] the stops the spin was set up for (the symbols of the three reels are the nibbles of the word at 4, the lowest first) and, when it ends, the reward word,
//   three records of four bytes at 6, $A and $E, one for each reel: the position (a word), the speed (a signed byte) and the reel's own state (0 waiting, 4 slowing, 8 stopped, $C done).
#include "Object/CNZSlots.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"
#include "Constants.h"

#include "Game.h"
#include "Level.h"
#include "Video.h"
#include "Backend/VDP.h"
#include "Object/CharControl.h"
#include "Object/Sonic.h"
#include "Object/Tails.h"
#include "Solid.h"
#include "Sound.h"
#include "VersusStats.h"

#include <string.h>

#include "Macros.h"

#include "Resource/Animation/CNZSlotStarter.h"
#include "Resource/Art/CNZSlotCheck.h"
#include "Resource/Mappings/CNZSlotStarter.h"

void Obj_EnemyPoints(Sst *self);

#define ArtTile_CNZSlotStarter 0x388 // ($7100: the zone's list)

static uint8_t m[0x12];

static uint16_t W(int off) { return (uint16_t)((m[off] << 8) | m[off + 1]); }
static void SetW(int off, uint16_t v) {
    m[off] = (uint8_t)(v >> 8);
    m[off + 1] = (uint8_t)v;
}

void SlotMachine_Reset(void) {
    memset(m, 0, sizeof(m));
}

uint8_t SlotMachine_State(void) {
    return m[0];
}

void SlotMachine_Start(void) { // (the starter: a character was taken, and the machine was resting)
    if (m[0] == 0x18)
        m[0] = 0x08;
}

uint16_t SlotMachine_Result(void) {
    return W(4);
}

// the symbols of each reel's strips (Offset_0x026EFF, 026F07 and 026F0F)
static const uint8_t reel_symbols[3][8] = {
    { 3, 0, 1, 4, 2, 5, 4, 1 },
    { 3, 0, 1, 4, 2, 5, 0, 2 },
    { 3, 0, 1, 4, 2, 5, 4, 1 },
};
// loc_27E1A: the stops a spin is set up for, by the frame counter: it counts down the thresholds; $0F, $FF is "any"
static const uint8_t spin_choices[7][3] = { { 0x08, 0x03, 0x33 }, { 0x12, 0x00, 0x00 }, { 0x12, 0x01, 0x11 }, { 0x24, 0x02, 0x22 }, { 0x1E, 0x04, 0x44 }, { 0x1E, 0x05, 0x55 }, { 0xFF, 0x0F, 0xFF } };
// Offset_0x026EDE: the reward for three of each symbol (BCD; $FFFF is the jackpot)
static const uint16_t rewards[6] = { 0x0030, 0x0025, 0xFFFF, 0x0150, 0x0010, 0x0020 };

static uint8_t frame_lo(void) {
    return (uint8_t)frame_count;
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// The drawing of a reel (Offset_0x026C76): one reel for each call, in turn; its strip is made into 16 tiles of the 4 columns of the symbols that are at its position
// ---------------------------------------------------------------------------------------------------------------------------------------
static const uint8_t *StripSource(const uint8_t *table, uint16_t d3) { // Offset_0x026D68
    const unsigned symbol = table[(d3 >> 8) & 7] & 7;
    return Art_CNZSlotCheck + (symbol << 9) + ((d3 & 0xF8) >> 1);
}

static void DrawReel(int pos_off, int speed_off, const uint8_t *table, uint16_t vram) {
    const uint16_t old = W(pos_off);
    SetW(pos_off, (uint16_t)(old - (int16_t)(int8_t)m[speed_off])); // (the speed is a signed byte)
    uint16_t d3 = W(pos_off) & 0x7F8;
    if (d3 == (old & 0x7F8))
        return;
    static uint8_t buffer[0x200];
    const uint8_t *src = StripSource(table, d3);
    for (int i = 0; i < 0x20; i++) {
        for (int k = 1; k < 4; k++)
            memcpy(&buffer[k * 0x80 + i * 4], src + k * 0x80, 4);
        memcpy(&buffer[i * 4], src, 4);
        src += 4;
        d3 = (uint16_t)((d3 & 0xFF00) | ((d3 + 8) & 0xFF));
        if ((d3 & 0xFF) == 0) {
            d3 = (uint16_t)((d3 + 0x100) & 0x700);
            src = StripSource(table, d3);
        }
    }
    VDP_SeekVRAM(vram);
    VDP_WriteVRAM(buffer, sizeof(buffer));
}

static void Draw(void) {
    switch (m[3]) {
    case 0:
        m[3] += 4;
        DrawReel(6, 8, reel_symbols[0], 0xAA00);
        break;
    case 4:
        m[3] += 4;
        DrawReel(0xA, 0xC, reel_symbols[1], 0xAC00);
        break;
    default: // 8
        m[3] = 0;
        m[1]--;
        DrawReel(0xE, 0x10, reel_symbols[2], 0xAE00);
        break;
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// The result (Offset_0x026D94 and what it calls): three of a kind pay by the symbol, a symbol 3 (the wild one) with two others pays on, and fives alone are counted
// ---------------------------------------------------------------------------------------------------------------------------------------
static uint16_t Bcd(uint16_t v) { // (three digits)
    return (uint16_t)(((v / 100) << 8) | (((v / 10) % 10) << 4) | (v % 10));
}

static uint16_t Unbcd(uint16_t b) {
    return (uint16_t)(((b >> 8) & 0xF) * 100 + ((b >> 4) & 0xF) * 10 + (b & 0xF));
}

static uint16_t Reward(unsigned symbol) { // Offset_0x026E80
    return rewards[symbol];
}

static uint16_t Times4(uint16_t d0) { // Offset_0x026E8E
    if ((int16_t)d0 < 0)
        return (uint16_t)(d0 << 2);
    return Bcd((uint16_t)(Unbcd(d0) * 4));
}

static uint16_t Times2(uint16_t d0) { // Offset_0x026EB2
    if ((int16_t)d0 < 0)
        return (uint16_t)(d0 + d0);
    return Bcd((uint16_t)(Unbcd(d0) * 2));
}

static void Evaluate(void) {
    const uint8_t d2 = (m[5] & 0xF0) >> 4, d3 = m[5] & 0xF; // (the second and third reels)
    const uint8_t first = m[4];
    int match = 0;
    if (d2 == first)
        match += 4;
    if (d3 == first)
        match += 8;
    uint16_t result;

    if (match == 12) { // all three the same
        SetW(4, Reward(d2));
        return;
    }
    if (match == 8) { // the first and the third
        if (d3 == 3) {
            SetW(4, Times4(Reward(d2)));
            return;
        }
        if (d2 == 3) {
            SetW(4, Times2(Reward(d3)));
            return;
        }
    } else if (match == 4) { // the first and the second
        if (d2 == 3) {
            SetW(4, Times4(Reward(d3)));
            return;
        }
        if (d3 == 3) {
            SetW(4, Times2(Reward(d2)));
            return;
        }
    }
    // Offset_0x026E2A: the second and the third
    if (d2 == d3) {
        if (first == 3) {
            SetW(4, Times2(Reward(d2)));
            return;
        }
        if (d2 == 3) {
            SetW(4, Times4(Reward(first & 0xF)));
            return;
        }
    }
    // Offset_0x026E5E: the fives
    result = 0;
    if (first == 5)
        result += 2;
    if (d2 == 5)
        result += 2;
    if (d3 == 5)
        result += 2;
    SetW(4, result);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// The machine (Load_Slot_Machine_Sprites)
// ---------------------------------------------------------------------------------------------------------------------------------------
// loc_27BD4: the reels are still, and the result is made
static void SlotMachine_Finish(void) {
    SetW(8, 0);
    SetW(0xC, 0);
    SetW(0x10, 0);
    m[1] = 0;
    Evaluate();
    m[0] = 0x18;
}

static uint8_t Ror8(uint8_t v, int n) {
    return (uint8_t)((v >> n) | (v << (8 - n)));
}

// Beta 4 picks the first reel's stop for the frame counters beyond the table's thresholds with an index that is not masked (loc_279FE's fall-through, `move.b 0(a1,d0),4(a4)` with d0 up to $74, where the
// code means d1): it reads past the three strips into what follows them in the ROM, which is these bytes (the strips, a pad byte and then code, from loc_27E2F), so that the first reel often has no symbol to stop at
static const uint8_t first_reel_spill[0x78] = {
    0x03, 0x00, 0x01, 0x04, 0x02, 0x05, 0x04, 0x01, 0x03, 0x00, 0x01, 0x04, 0x02, 0x05, 0x00, 0x02, 0x03, 0x00, 0x01, 0x04, 0x02, 0x05, 0x04, 0x01, 0x00, 0x4E, 0xF9, 0x00, 0x01, 0x49, 0x0A, 0x4E,
    0xF9, 0x00, 0x01, 0x2D, 0x14, 0x4E, 0xF9, 0x00, 0x01, 0x2E, 0x86, 0x4E, 0xF9, 0x00, 0x01, 0x36, 0xBE, 0x4E, 0xF9, 0x00, 0x01, 0x57, 0xD2, 0x00, 0x00, 0x70, 0x00, 0x10, 0x28, 0x00, 0x24, 0x32,
    0x3B, 0x00, 0x06, 0x4E, 0xFB, 0x10, 0x02, 0x00, 0x04, 0x00, 0x56, 0x54, 0x28, 0x00, 0x24, 0x21, 0x7C, 0x00, 0x02, 0x80, 0x46, 0x00, 0x04, 0x31, 0x7C, 0x43, 0x94, 0x00, 0x02, 0x61, 0x00, 0x02,
    0x38, 0x11, 0x7C, 0x00, 0x04, 0x00, 0x01, 0x11, 0x7C, 0x00, 0x10, 0x00, 0x19, 0x11, 0x7C, 0x00, 0x01, 0x00, 0x18, 0x11, 0x7C, 0x00, 0xCA, 0x00,
};

// loc_27ADC: the symbol the reel in turn (m[3]) is to stop on
static unsigned Target(void) {
    uint16_t d1 = W(4);
    if (m[3])
        d1 >>= m[3];
    d1 &= 7;
    if ((int16_t)d1 > 5)
        d1 -= 2;
    return d1;
}

// loc_27AF8: the symbol the reel stopped on goes into its place in the word at 4
static void StoreSymbol(unsigned d1) {
    uint16_t d2 = 0xFFF0;
    d1 &= 0xF;
    const int d0 = m[3];
    if (d0) {
        d1 = (uint16_t)(d1 << d0);
        d2 = (uint16_t)((d2 << d0) | (d2 >> (16 - d0)));
    }
    SetW(4, (uint16_t)((W(4) & d2) | d1));
    SetW(4, (uint16_t)(W(4) & 0x777));
}

// loc_27AAC: what the reel in turn does (its own state, byte 3 of its record)
static void ReelStep(void) {
    uint8_t *const a1 = &m[6 + m[3]];
    const uint8_t *const a3 = reel_symbols[m[3] >> 2];
    const uint16_t pos = (uint16_t)((a1[0] << 8) | a1[1]);

    switch (a1[3]) {
    case 0x00: // loc_27B1A: waits for the reel before it to be going, then for its symbol to come round
        if (m[3] == 0) {
            if ((int8_t)m[1] >= 0)
                return;
        } else if ((int8_t)a1[-1] < 8) {
            return;
        }
        if (a3[((uint16_t)(pos - 0xA0) >> 8) & 7] == Target()) {
            a1[3] += 4;
            a1[2] = 0x60;
        }
        return;

    case 0x04: { // loc_27B56: slows, and stops on its symbol
        const unsigned want = Target();
        if (a3[((uint16_t)(pos + 0xF0) & 0x700) >> 8] == want) { // loc_27B94
            const uint16_t d0 = (uint16_t)(pos + 0x80);
            const uint16_t d1 = (uint16_t)((d0 & 0x700) - 0x10);
            a1[0] = (uint8_t)(d1 >> 8);
            a1[1] = (uint8_t)d1;
            StoreSymbol(a3[(d0 >> 8) & 7]);
            a1[2] = 0xF8;
            a1[3] += 4;
            return;
        }
        if (a1[2] > 0x20)
            a1[2] -= 0x0C;
        if ((int8_t)a1[2] > 0x18 && a1[1] <= 0x80)
            a1[2] -= 2;
        return;
    }

    case 0x08: // loc_27BC0: stopped; it is done when it has come to rest at a whole position
        if (a1[1] == 0) {
            a1[2] = 0;
            a1[3] += 4;
        }
        return;

    default: // $C: done
        return;
    }
}

void SlotMachine_Process(void) {
    switch (m[0]) {
    case 0x00: { // loc_27932
        memset(m, 0, sizeof(m));
        uint8_t d0 = frame_lo();
        m[6] = d0;
        d0 = Ror8(d0, 3);
        m[0xA] = d0;
        d0 = Ror8(d0, 3);
        m[0xE] = d0;
        m[7] = m[0xB] = m[0xF] = 8;
        m[8] = m[0xC] = m[0x10] = 8;
        m[1] = 1;
        m[0] += 4;
        break;
    }

    case 0x04: // loc_27980: the first drawing, and then it rests
        Draw();
        if (m[1] != 0)
            break;
        m[0] = 0x18;
        SetW(8, 0);
        SetW(0xC, 0);
        SetW(0x10, 0);
        break;

    case 0x08: { // loc_279A0: it is set going: the speeds, and where the spin is to end
        m[8] = (uint8_t)(((frame_lo() & 7) - 4) + 0x30);
        m[0xC] = (uint8_t)((((uint8_t)((frame_lo() << 4) | (frame_lo() >> 4)) & 7) - 4) + 0x30);
        m[0x10] = (uint8_t)((((frame_count >> 8) & 7) - 4) + 0x30);
        m[1] = 2;
        m[3] = 0;
        m[9] = m[0xD] = m[0x11] = 0;
        m[0] += 4;
        uint8_t d0 = Ror8(frame_lo(), 3);
        const uint8_t *choice = spin_choices[0];
        for (;; choice += 3) {
            const bool borrow = d0 < choice[0];
            d0 = (uint8_t)(d0 - choice[0]);
            if (borrow)
                break;
        }
        if (choice[0] != 0xFF) {
            m[4] = choice[1];
            m[5] = choice[2];
            break;
        }
        // (any: each reel takes a symbol of its strip)
#if SCP_FIX_BUGS
        m[4] = reel_symbols[0][d0 & 7];
#else
        m[4] = first_reel_spill[d0]; // (not masked: see first_reel_spill)
#endif
        d0 = Ror8(d0, 3);
        uint8_t d2 = (uint8_t)(reel_symbols[1][d0 & 7] << 4);
        d0 = Ror8(d0, 3);
        d2 |= reel_symbols[2][d0 & 7];
        m[5] = d2;
        break;
    }

    case 0x0C: // loc_27A56: the reels are drawn a while at the speeds they were given
        Draw();
        if (m[1] != 0)
            break;
        m[8] = (uint8_t)(m[8] + 0x30);
        m[0xC] = (uint8_t)(m[0xC] + 0x30);
        m[0x10] = (uint8_t)(m[0x10] + 0x30);
        m[1] = (uint8_t)((frame_lo() & 0xF) + 0x0C);
        m[2] = 0;
        m[0] += 4;
        break;

    case 0x10: // loc_27A8E: the reels, each by its own state, until all three are done
        Draw();
        if (m[9] == 0x0C && m[0xD] == 0x0C && m[0x11] == 0x0C) {
            SlotMachine_Finish();
            break;
        }
        ReelStep();
        break;

    case 0x14: // loc_27BD4
        SlotMachine_Finish();
        break;

    default: // $18: resting
        break;
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object D6: the starter
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    uint8_t subtype;
    uint8_t pad[7];
    uint16_t state[2]; // 0x30, 0x34: for Sonic and for Tails: 0 waiting, 2 held, 4 let go (the next word of each is its timer)
    uint16_t timer[2]; // 0x32, 0x36
} Own_SlotStarter;

// A character is let go (loc_27824)
static void SlotStarter_Release(Sst *self, Own_SlotStarter *own, Sst *chr, int who) {
    self->anim = 0;
    OBJ_CONTROL(chr) = 0;
    self->status &= (uint8_t) ~(1 << (3 + who));
    chr->status &= (uint8_t)~SST_PSTATUS_OBJECT_STAND;
    chr->status |= SST_PSTATUS_IN_AIR;
    chr->y_vel = 0x400;
    own->state[who] = 4;
    own->timer[who] = 0x3C;
}

static void SlotStarter_Character(Sst *self, Own_SlotStarter *own, Sst *chr, int who) {
    const int32_t side = Solid_Character(self, chr, who, 0x23, 0x10, 0x11, self->x_pos.f.u, NULL);
    switch (own->state[who]) {
    case 0:
        if (side != -1)
            return;
        chr->x_pos.f.u = self->x_pos.f.u;
        chr->y_pos.f.u = self->y_pos.f.u;
        chr->x_vel = 0;
        chr->y_vel = 0;
        chr->ground_vel = 0;
        OBJ_CONTROL(chr) = 0x81;
        chr->status |= SST_PSTATUS_IN_BALL;
        chr->y_radius = 0xE;
        chr->x_radius = 7;
        chr->anim = SonAnimId_Roll;
        self->anim = 1;
        own->state[who] = 2;
        own->timer[who] = 0x78;
        if (self->subtype != 0) // (only a starter with a subtype starts the machine: the others only pay)
            SlotMachine_Start();
        break;

    case 2:
        if (self->subtype != 0) { // holds him until the machine is at rest again, a sound every sixteenth frame
            if (SlotMachine_State() == 0x18) {
                SlotStarter_Release(self, own, chr, who);
            } else if ((frame_count & 0xF) == 0) {
                PlaySound(sfx_Basaran); // ($C0)
            }
            break;
        }
        if ((int16_t)--own->timer[who] >= 0) {
            if ((own->timer[who] & 0xF) == 0) {
                PlaySound(sfx_Basaran);
                VsStats_AddPointsTo(chr, 0xA);
                Sst *points = Sst_SpawnCode(Obj_EnemyPoints, self->x_pos.f.u, self->y_pos.f.u);
                if (points != NULL)
                    points->mapping_frame = 0;
            }
            break;
        }
        SlotStarter_Release(self, own, chr, who);
        break;

    default: // 4: a rest before it takes anyone again
        if ((int16_t)--own->timer[who] < 0)
            own->state[who] = 0;
        break;
    }
}

void Obj_CNZSlotStarter(Sst *self) {
    Own_SlotStarter *own = (Own_SlotStarter *)self->own.u8;

    if (self->routine == 0) {
        self->routine += 2;
        self->mappings = Mappings_CNZSlotStarter;
        self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_CNZSlotStarter);
        self->render_flags |= SST_RENDER_LEVEL;
        self->width_pixels = 0x18;
        self->priority = 1 * 0x80;
    }

    SlotStarter_Character(self, own, player, SolidChar_Sonic);
    if (TAILS_OBJ->code != NULL)
        SlotStarter_Character(self, own, TAILS_OBJ, SolidChar_Tails);
    Sst_Animate(self, Animation_CNZSlotStarter);
    SstOnScreenTest(self);
}
