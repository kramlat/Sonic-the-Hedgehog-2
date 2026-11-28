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
#include "Object/Ring.h"
#include "MathUtil.h"
#include "Solid.h"
#include "Sound.h"
#include "VersusStats.h"

#include <string.h>

#include "Macros.h"

#include "Resource/Animation/CNZSlotStarter.h"
#include "Resource/Art/CNZSlotCheck.h"
#include "Resource/Mappings/CNZSlotStarter.h"
#include "Resource/Mappings/CNZBombPrize.h"
extern const uint8_t Mappings_RingREV01[]; // (Ring.c's: the ring's eight frames)

void Obj_EnemyPoints(Sst *self);

#define ArtTile_CNZSlotStarter 0x388 // ($7100: the zone's list)
#define ArtTile_CNZBombPrize 0x380   // (ArtNem_CNZBonusSpike: the slot machine's spike ball art, $7000)

static uint8_t m[0x12];
static bool slot_in_use;          // SlotMachineInUse: a starter that starts the machine has a character, until it has let him go
static uint16_t bonus_countdown;  // Bonus_Countdown_3: the frames the bombs have been waiting, so that their sound is not made at every one

static uint16_t W(int off) { return (uint16_t)((m[off] << 8) | m[off + 1]); }
static void SetW(int off, uint16_t v) {
    m[off] = (uint8_t)(v >> 8);
    m[off + 1] = (uint8_t)v;
}

void SlotMachine_Reset(void) {
    memset(m, 0, sizeof(m));
    slot_in_use = false;
    bonus_countdown = 0;
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
// SlotRingRewards (the final game's: rings, plain numbers; the third symbol, the bomb, is -1, and what is below zero is bombs, not rings)
static const int16_t rewards[6] = { 30, 25, -1, 150, 10, 20 };

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
// The result (SlotMachine_ChooseReward and what it calls): three of a kind pay by the symbol, a symbol 3 (the wild one) with two others pays on, and bars alone are counted
// ---------------------------------------------------------------------------------------------------------------------------------------
static uint16_t Reward(unsigned symbol) { // SlotMachine_GetReward
    return (uint16_t)rewards[symbol < 6 ? symbol : 0];
}

static uint16_t Times4(uint16_t d0) { // SlotMachine_QuadrupleUp
    return (uint16_t)(d0 << 2);
}

static uint16_t Times2(uint16_t d0) { // SlotMachine_DoubleUp
    return (uint16_t)(d0 + d0);
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

typedef struct {
    uint8_t subtype;
    uint8_t pad[7];
    uint16_t state[2]; // 0x30, 0x34: for Sonic and for Tails: 0 waiting, 2 held, 4 let go (the next word of each is its timer)
    uint16_t timer[2]; // 0x32, 0x36
    uint16_t bombs;    // 0x2A: the bombs still to send (-1: not set yet)
    uint16_t flying;   // 0x2C: the prizes that are on their way
    uint16_t angle;    // 0x2E: where the next prize starts on the circle round the machine
} Own_SlotStarter;

// ---------------------------------------------------------------------------------------------------------------------------------------
// The prizes: the ring prize ($DC) and the bomb prize ($D3) of the final game. The starter sends them out on a circle round the machine, and each flies in to the machine a sixteenth of the way a frame; when its
// delay is up it is paid (a ring is counted, with the ring's sparkle) or collected (a bomb takes a ring from the character and the spikes' sound)
// ---------------------------------------------------------------------------------------------------------------------------------------
typedef struct {
    int32_t x, y;        // 0x30, 0x34: the position with the fractions (the high words are the place)
    uint16_t *flying;    // 0x2A: the starter's count of the prizes that are on their way
    Sst *chr;            // parent: the character the machine holds
    int16_t machine_x, machine_y; // 0x38, 0x3A
    int16_t delay;       // 0x3C: the frames before it is paid
    uint8_t age;         // (the spin's own frame counter)
} Own_Prize;

// The place and the way in, as both prizes do it
static void Prize_Fly(Sst *self, Own_Prize *own) {
    const int32_t mx = (int32_t)((uint32_t)(uint16_t)own->machine_x << 16);
    const int32_t my = (int32_t)((uint32_t)(uint16_t)own->machine_y << 16);
    own->x -= (own->x - mx) >> 4;
    self->x_pos.f.u = (int16_t)(own->x >> 16);
    own->y -= (own->y - my) >> 4;
    self->y_pos.f.u = (int16_t)(own->y >> 16);
}

// What the character has of rings: in the versus play the second player's are his own set (swapped in for the time)
static void Prize_Rings(const Sst *chr, bool add) {
    const bool second = vs_stats_active && chr == TAILS_OBJ;
    if (second)
        VsStats_Swap();
    if (add) {
        Ring_Collect();
    } else if (rings != 0) {
        rings--;
        ring_count |= 1; // (the HUD's ring count is updated)
    }
    if (second)
        VsStats_Swap();
}

void Obj_CNZRingPrize(Sst *self) { // ObjDC
    Own_Prize *own = (Own_Prize *)self->own.u8;
    switch (self->routine) {
    case 0:
        Prize_Fly(self, own);
        self->mapping_frame = (uint8_t)((own->age++ >> 1) & 3); // (Ani_objDC: the four spinning frames, two frames each)
        if (--own->delay != 0) {
            Sst_Draw(self);
            return;
        }
        (*own->flying)--;
        Prize_Rings(own->chr, true); // (CollectRing)
        self->routine = 2;
        own->age = 0;
        // Fallthrough
    case 2: // Ani_Ring: the sparkle, six frames to each of its four, and it is gone
        self->mapping_frame = (uint8_t)(4 + own->age / 6);
        if (++own->age >= 24) {
            Sst_Delete(self);
            return;
        }
        Sst_Draw(self);
        return;
    }
}

void Obj_CNZBombPrize(Sst *self) { // ObjD3
    Own_Prize *own = (Own_Prize *)self->own.u8;
    Prize_Fly(self, own);
    if (--own->delay != 0) {
        Sst_Draw(self);
        return;
    }
    (*own->flying)--;
    if (bonus_countdown >= 5) { // (a sound at the most every fifth frame that a bomb arrives)
        bonus_countdown = 0;
        QueueSound2(sfx_HitSpikes);
    }
    Prize_Rings(own->chr, false);
    Sst_Delete(self);
}

// A prize is sent out from the starter: where it starts is the circle round the machine at the starter's angle, which the next one's is a step on (SlotStarter's loc_2BC86 and loc_2BD4E)
static bool Prize_Send(Sst *self, Own_SlotStarter *own, Sst *chr, bool bomb) {
    Sst *prize = Sst_SpawnCode(bomb ? Obj_CNZBombPrize : Obj_CNZRingPrize, self->x_pos.f.u, self->y_pos.f.u);
    if (prize == NULL)
        return false;
    Own_Prize *pr = (Own_Prize *)prize->own.u8;
    prize->mappings = bomb ? Mappings_CNZBombPrize : Mappings_RingREV01;
    prize->art_tile = bomb ? TILE_MAP(0, 0, 0, 0, ArtTile_CNZBombPrize) : TILE_MAP(0, 1, 0, 0, ArtTile_Ring);
    prize->render_flags = SST_RENDER_LEVEL;
    prize->priority = (bomb ? 4 : 3) * 0x80;
    prize->width_pixels = bomb ? 0x10 : 8;
    pr->delay = bomb ? 0x1E : 0x1A;
    pr->machine_x = self->x_pos.f.u;
    pr->machine_y = self->y_pos.f.u;
    int16_t sin, cos;
    CalcSine((uint8_t)own->angle, &sin, &cos);
    own->angle = (uint16_t)(own->angle + (bomb ? 0x90 : 0x89));
    pr->x = (int32_t)((uint32_t)(uint16_t)(self->x_pos.f.u + (cos >> 1)) << 16);
    pr->y = (int32_t)((uint32_t)(uint16_t)(self->y_pos.f.u + (sin >> 1)) << 16);
    prize->x_pos.f.u = (int16_t)(pr->x >> 16);
    prize->y_pos.f.u = (int16_t)(pr->y >> 16);
    pr->flying = &own->flying;
    pr->chr = chr;
    own->flying++;
    return true;
}

// The prizes of the result, once the machine is at rest (loc_2BC86): ring prizes for a reward of rings, bomb prizes (a hundred) for a bomb's; one every other frame, sixteen on their way at the most. True when
// everything has been paid and has arrived: the character is let go
static bool SlotStarter_Prizes(Sst *self, Own_SlotStarter *own, Sst *chr) {
    const int16_t reward = (int16_t)SlotMachine_Result();
    if (reward < 0) {
        if ((int16_t)own->bombs < 0)
            own->bombs = 100;
        if (own->bombs != 0) {
            if (!(frame_count & 1) || own->flying >= 16 || !Prize_Send(self, own, chr, true)) {
                bonus_countdown++;
                return false;
            }
            own->bombs--;
        }
        if (own->flying == 0)
            return true;
        bonus_countdown++;
        return false;
    }
    if (reward != 0) {
        if (!(frame_count & 1) || own->flying >= 16 || !Prize_Send(self, own, chr, false))
            return false;
        SetW(4, (uint16_t)(reward - 1));
    }
    return own->flying == 0;
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// Object D6: the starter
// ---------------------------------------------------------------------------------------------------------------------------------------


// A character is let go (loc_27824)
static void SlotStarter_Release(Sst *self, Own_SlotStarter *own, Sst *chr, int who) {
    self->anim = 0;
    OBJ_CONTROL(chr) = 0;
    self->status &= (uint8_t) ~(1 << (3 + who));
    chr->status &= (uint8_t)~SST_PSTATUS_OBJECT_STAND;
    chr->status |= SST_PSTATUS_IN_AIR;
    chr->y_vel = 0x400;
    own->state[who] = 4;
    own->timer[who] = 0x1E; // (the final game's rest: Beta 4's was $3C)
}

static void SlotStarter_Character(Sst *self, Own_SlotStarter *own, Sst *chr, int who) {
    const int32_t side = Solid_Character(self, chr, who, 0x23, 0x10, 0x11, self->x_pos.f.u, NULL);
    switch (own->state[who]) {
    case 0:
        if (OBJ_CONTROL(chr) != 0) // (a character the machine or another object holds)
            return;
        if (self->subtype != 0 && slot_in_use) // (one that starts the machine waits while another has it)
            return;
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
        if (self->subtype != 0 && SlotMachine_State() == 0x18) { // (only a starter with a subtype starts the machine: the others only pay)
            SlotMachine_Start();
            own->bombs = (uint16_t)-1;
            own->flying = 0;
            own->angle = 0;
            slot_in_use = true;
        }
        break;

    case 2:
        if (self->subtype != 0) { // holds him until the machine is at rest again, a sound every sixteenth frame, and then until its prizes have been paid
            if (SlotMachine_State() == 0x18) {
                if (SlotStarter_Prizes(self, own, chr))
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
        if ((int16_t)--own->timer[who] < 0) {
            own->state[who] = 0;
            if (self->subtype != 0)
                slot_in_use = false;
        }
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
