// Beta 4's special stage objects: see SpecialStageB4Objects.h
#include "Object/SpecialStageB4Objects.h"
#include "Object/SpecialStageB4Art.h"
#include "SpecialStageB4Character.h"
#include "SpecialStageB4Items.h"
#include "SpecialStageB4Tables.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"
#include "Constants.h"
#include "Macros.h"
#include "Game.h"
#include "Nemesis.h"
#include "Sound.h"
#include "SoundIds.h"
#include "MathUtil.h"
#include "Object/Tails.h"
#include "Backend/VDP.h"
#include "GameInterface.h"
#include "EnginePalette.h"

#include <string.h>
#include <stdlib.h>
#include <stdlib.h>
#include <stdio.h>

#include "Resource/Mappings/SSB4Sonic.h"
#include "Resource/Mappings/SSB4Hud.h"
#include "Resource/Mappings/SSB4Start.h"
#include "Resource/Mappings/SSB4Counter.h"
#include "Resource/Mappings/SSB4Letters.h"
#include "Resource/Mappings/SSB4Checkpoint.h"
#include "Resource/Mappings/SSB4Rings.h"
#include "Resource/Mappings/SSB4Bombs.h"
#include "Resource/Art/SSB4SonicMiles.h"

// What the pre-beta's stage code defines and these share (SpecialStageObjects.c): the animations of the characters and the mappings of Miles, his tails and the shadows are the same in Beta 4, byte for byte
extern const uint8_t Animation_SpecialStageSonic[];
extern const uint8_t Animation_SpecialStageMiles[];
extern const uint8_t Animation_SpecialStageMilesTail[];
extern const uint8_t Mappings_SpecialStageMiles[];
extern const uint8_t Mappings_SpecialStageMilesTail[];
extern const uint8_t Mappings_SpecialStageShadow[];

#define SFX(id) (sfx_Jump + ((id) - 0xA0))

static SS4Player players[2];
static SS4World wrld;
static SS4Items items;
static SS4World2 w2;
static uint8_t picture[0x8000]; // (SoncMils decoded: the characters' tiles, which a frame at a time are copied to VRAM)
static uint8_t cached[3];
static uint8_t sst_of[SS4_ITEM_MAX];
static int stage_number;
static bool started, over, emerald;
static uint16_t speed_word;
static int checkpoint;
// The versus play (Beta 4's `$FFFFFE00`, set by the entry from the match): a round at each of the stage's three checkpoints, in which each character's rings are his own and the one with more wins it
static bool versus;
static int round_count;                 // $FFFFFE17: the rounds played
static uint16_t round_rings[3][2];      // $FFFFFFA0 on: each round's rings, Sonic's then Miles'
static uint8_t round_wins[2];           // $FFFFDB93: the rounds each has won (it is the high nibble for Sonic, the low for Miles)
bool SS4Stage_Versus(void) { return versus; }
int SS4Stage_RoundsPlayed(void) { return round_count; }
uint16_t SS4Stage_RoundRings(int round, int who) { return round_rings[round][who]; }
static uint16_t pad_history[16]; // Sonic's pad, the newest first: Miles does what he did 16 frames ago when nobody works his own (loc_2FE20, loc_30E24)
static uint16_t miles_wait;
static int segments_seen_step = -1;
static int gate_step = -1;    // the gate's row (-1: none)
static int gate_waiting;
static int pass_pending;     // a checkpoint was passed: frames until the next one's words
static bool pad_locked;
static int end_timer, end_state;
static uint16_t ss_frames; // the stage's own frame counter (the level's `$FFFFFE0F`)
static int prev_segment = -1, cur_segment = -1; // the segment shown now and the one before it ()

// The rings that the stage asks for at each of the checkpoints (loc_8298: four for each stage)
static const uint8_t required[40] = {
    0x14, 0x32, 0x46, 0x46, 0x23, 0x4B, 0x82, 0xBE, 0x28, 0x55, 0x96, 0xDC, 0x2D, 0x5F, 0xAA, 0xFA, 0x32, 0x69, 0xBE, 0xFA,
    0x32, 0x69, 0xBE, 0xFA, 0x32, 0x69, 0xBE, 0xFA, 0x28, 0x50, 0x8C, 0xC8, 0x28, 0x50, 0x8C, 0xC8, 0x28, 0x50, 0x8C, 0xC8,
};

int SS4Stage_Required(int stage, int cp) {
    return required[(stage * 4 + cp) % 40];
}

void SS4Stage_Begin(int stage, bool versus_play) {
    stage_number = stage;
    versus = versus_play;
    round_count = 0;
    memset(round_rings, 0, sizeof(round_rings));
    memset(round_wins, 0, sizeof(round_wins));
    SS4Items_Reset(&items, stage);
    memset(&w2, 0, sizeof(w2));
    memset(&wrld, 0, sizeof(wrld));
    wrld.period_less = 4;
    SS4Player_Init(&players[0], false);
    SS4Player_Init(&players[1], true);
    w2.player[0] = &players[0];
    w2.player[1] = &players[1];
    NemDecToRAM(Art_SSB4SonicMiles, picture);
    memset(cached, 0xFF, sizeof(cached));
    memset(sst_of, 0, sizeof(sst_of));
    memset(pad_history, 0, sizeof(pad_history));
    miles_wait = 0;
    started = over = emerald = false;
    if (getenv("SONIC2_SS_CHEAT") != NULL) // (a check of the end: plenty of rings)
        w2.rings[0] = (uint16_t)atoi(getenv("SONIC2_SS_CHEAT"));
    speed_word = 0x0C;
    checkpoint = 0;
    segments_seen_step = -1;
    prev_segment = cur_segment = -1;
    gate_step = -1;
    gate_waiting = 0;
    pass_pending = 0;
    pad_locked = false;
    end_state = 0;
}

bool SS4Stage_Started(void) { return started; }
bool SS4Stage_Over(void) { return over; }
bool SS4Stage_GotEmerald(void) { return emerald; }
uint16_t SS4Stage_Speed(void) { return speed_word; }
int SS4Stage_Rings(void) { return w2.rings[0] + w2.rings[1]; }
int SS4Stage_RingsOf(int who) { return w2.rings[who]; }

static Sst *Make(SstCode code, uint8_t subtype) {
    Sst *o = Sst_Allocate();
    if (o != NULL) {
        o->code = code;
        o->subtype = subtype;
    }
    return o;
}

void SS4Stage_Spawn(void) {
    player->code = Obj_SS4Character;
    player->subtype = 0;
    TAILS_OBJ->code = Obj_SS4Character;
    TAILS_OBJ->subtype = 1;
    Make(Obj_SS4Hud, 0);
    Make(Obj_SS4Start, 0);
    Make(Obj_SS4Counter, 0);
    Make(Obj_SS4Counter, versus ? 2 : 1); // (the versus play has no total: the rounds each has won instead)
    Make(Obj_SS4Controller, 0);
}

// loc_6056: after a $FC in the stage's list the track's edge colour (colour 7 of the third line) flashes through 16 colours, quicker each time a list begins; it goes on until the colour is back to $EE
static uint8_t flash_mask = 7, flash_index;
static void PaletteFlash(const SSDisplay *d) {
    static const uint16_t table[16] = { 0xEE, 0x2EE, 0x4EE, 0x6EE, 0x8EE, 0xAEE, 0xCEE, 0xEEE, 0x0C, 0xAEA, 0x48E, 0x6E6, 0x6E8, 0x4EA, 0x2EC, 0xEC };
    if (!items.flash) {
        if (dry_palette[3][7] == 0xEE) {
            flash_mask = 7;
            flash_index = 0;
            return;
        }
    }
    if ((ss_frames & flash_mask) == 0) {
        dry_palette[3][7] = table[flash_index++ & 0xF];
        if (d->track.index == 0 && d->frame == 0) {
            flash_mask = (uint8_t)(flash_mask - 4);
            if (flash_mask & 0x80)
                flash_mask = 1;
        }
    }
}

void SS4Stage_Frame(const SSDisplay *d) {
    ss_frames++;
    PaletteFlash(d);
    wrld.period_less = d->period_less;
    w2.frame_four = d->frame == 4;
    w2.flipped = d->display_flipped;
    w2.segment = d->segment < 0 ? 0 : d->segment;
    if (d->segment != cur_segment) {
        prev_segment = cur_segment;
        cur_segment = d->segment;
    }
    speed_word = d->speed;
    if (d->frame == 4 && d->segment >= 0) { // loc_6148: a new step has come: its objects are made
        size_t count;
        const uint8_t *steps = SS4_StageSteps(stage_number, &count);
        if ((size_t)d->track.step < count)
            SS4Items_MakeStep(&items, d->track.step, SS4_KindLength(steps[d->track.step] & 0x7F));
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// The characters

// loc_2FFC8, loc_30E74, loc_30F36: the art of the frame, from the picture to the character's place in VRAM, once for each frame
static void LoadFrameArt(int who, uint8_t frame) {
    if (cached[who] == frame)
        return;
    cached[who] = frame;
    SS4ArtPiece pieces[16];
    const int n = SS4_FramePieces(who, frame, pieces, 16);
    size_t vram = (size_t)(who == 0 ? ArtTile_SS4Sonic : who == 1 ? ArtTile_SS4Miles : ArtTile_SS4MilesTail) * 32;
    for (int i = 0; i < n; i++) {
        VDP_SeekVRAM(vram);
        VDP_WriteVRAM(picture + pieces[i].source, pieces[i].bytes);
        vram += pieces[i].bytes;
    }
}

void Obj_SS4Character(Sst *self) {
    const int who = self->subtype;
    SS4Player *p = &players[who];
    if (self->routine == 0) {
        self->routine = 2;
        self->mappings = who == 0 ? Mappings_SSB4Sonic : Mappings_SpecialStageMiles;
        const int palette = who == 0 ? 1 : 2, tile = who == 0 ? ArtTile_SS4Sonic : ArtTile_SS4Miles;
        self->art_tile = TILE_MAP(0, palette, 0, 0, tile);
        self->render_flags |= SST_RENDER_LEVEL;
        Make(Obj_SS4Shadow, (uint8_t)who);
        if (who == 1)
            Make(Obj_SS4Tail, 0);
    }
    uint8_t held, pressed;
    if (who == 0) {
        memmove(pad_history + 1, pad_history, 15 * sizeof(uint16_t));
        pad_history[0] = (uint16_t)((jpad1_hold1 << 8) | jpad1_press1);
        held = jpad1_hold1;
        pressed = jpad1_press1;
    } else {
        held = jpad2_hold;
        pressed = jpad2_press;
        if (held & 0x7F) { // (his own pad is worked: he does what it says, and for three seconds more)
            memset(pad_history, 0, sizeof(pad_history));
            miles_wait = 0xB4;
        } else if (miles_wait) {
            miles_wait--;
        } else {
            held = (uint8_t)(pad_history[15] >> 8);
            pressed = (uint8_t)pad_history[15];
        }
    }
    if (pad_locked) { // (the end: the pads do nothing but Start: loc_5EEA)
        held &= 0x80;
        pressed &= 0x80;
    }
    SS4Player_Step(p, &wrld, held, pressed, w2.rings[who], who == 0 ? Animation_SpecialStageSonic : Animation_SpecialStageMiles);
    if (p->sound_jump) {
        QueueSound2(SFX(0xA0));
        p->sound_jump = false;
    }
    if (p->sound_ring_loss) {
        QueueSound2(SFX(0xC6));
        p->sound_ring_loss = false;
    }
    if (p->spill) {
        p->spill = false;
        SS4Items_Spill(&items, &w2, who);
    }
    wrld.front = p->other_in_front || wrld.front;
    self->x_pos.f.u = p->screen_x;
    self->y_pos.f.u = p->screen_y;
    self->mapping_frame = p->frame;
    self->render_flags = (uint8_t)((self->render_flags & 0xFC) | (p->flips & 3));
    self->priority = (uint16_t)(p->priority * 0x80);
    LoadFrameArt(who, p->frame);
    Sst_Draw(self);
}

void Obj_SS4Tail(Sst *self) { // loc_30EE2: Miles' tails, with him, in the animation of the tails while he runs, and gone while he jumps
    const SS4Player *p = &players[1];
    if (self->routine == 0) {
        self->routine = 2;
        self->mappings = Mappings_SpecialStageMilesTail;
        self->art_tile = TILE_MAP(0, 2, 0, 0, ArtTile_SS4MilesTail);
        self->render_flags |= SST_RENDER_LEVEL;
    }
    self->x_pos.f.u = p->screen_x;
    self->y_pos.f.u = p->screen_y;
    self->render_flags = (uint8_t)((self->render_flags & 0xFC) | (p->flips & 3));
    self->priority = (uint16_t)((p->priority - 1) * 0x80);
    self->anim = p->anim;
    if (self->anim >= 3)
        return;
    Sst_Animate(self, Animation_SpecialStageMilesTail);
    LoadFrameArt(2, self->mapping_frame);
    Sst_Draw(self);
}

void Obj_SS4Shadow(Sst *self) { // loc_3058E
    const SS4Player *p = &players[self->subtype];
    SS4Shadow s;
    SS4Player_Shadow(p, &s);
    if (self->routine == 0) {
        self->routine = 2;
        self->mappings = Mappings_SpecialStageShadow;
        self->render_flags |= SST_RENDER_LEVEL;
        self->priority = 4 * 0x80;
    }
    self->art_tile = s.art_tile;
    self->x_pos.f.u = s.x;
    self->y_pos.f.u = s.y;
    self->mapping_frame = s.frame;
    self->render_flags = (uint8_t)((self->render_flags & 0xFC) | (s.flips & 3));
    Sst_Draw(self);
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// The items

void Obj_SS4Item(Sst *self) {
    const SS4Item *it = &items.item[self->subtype];
    if (it->kind == SS4Kind_None) {
        Sst_Delete(self);
        return;
    }
    if (self->routine == 0) {
        self->routine = 2;
        self->render_flags |= SST_RENDER_LEVEL;
    }
    switch (it->kind) {
    case SS4Kind_Ring:
    case SS4Kind_Spilled: self->mappings = Mappings_SSB4Rings; break;
    case SS4Kind_Bomb: self->mappings = Mappings_SSB4Bombs; break;
    default: self->mappings = Mappings_SpecialStageShadow; break;
    }
    if (!it->visible)
        return;
    self->art_tile = it->art_tile;
    self->x_pos.f.u = it->x;
    self->y_pos.f.u = it->y;
    self->mapping_frame = it->frame;
    self->priority = (uint16_t)(it->priority * 0x80);
    self->render_flags = (uint8_t)((self->render_flags & 0xFC) | (it->flips & 3));
    Sst_Draw(self);
}

// The stage's end (what the objects $5A and $59 do after a step's list asks for them): the check of the rings at a checkpoint (the two characters' together against what the checkpoint asks: too few and the stage
// ends there), and at the last the emerald: the stage slows, and with enough rings it comes down the pipe and is the character's
static int32_t emerald_distance;

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// The words of the stage (loc_31E14, loc_31F48, loc_31ED8): what it asks, and what a checkpoint says

static int required_now;      // $FFFFDB8C: the rings the next check asks (the two characters' together)

// The letters of each chunk of words (loc_31E62: 17 of them), and which chunks make each of the stage's sentences (loc_31FBE; the byte is the chunk's number times 2)
static const uint8_t chunks[17][7] = {
    { 0, 1, 2, 0xFF }, { 3, 4, 5, 0, 6, 0xFF }, { 7, 8, 8, 9, 0x11, 0xFF }, { 5, 8, 2, 0xFF }, { 1, 5, 8, 0xA, 0, 0xB, 0xFF }, { 0xC, 9, 0xD, 0xE, 1, 3, 0xFF }, { 0xF, 8, 6, 2, 0xFF },
    { 0x10, 4, 5, 6, 0x11, 0xFF }, { 6, 8, 5, 4, 7, 0xFF }, { 0xF, 4, 9, 1, 6, 0xFF }, { 2, 4, 1, 0xFF }, { 0x10, 4, 5, 0xFF }, { 2, 0x10, 4, 7, 1, 0xFF }, { 0xD, 9, 9, 0x11, 0xFF },
    { 0x11, 0xFF }, { 0x12, 0x12, 0x12, 0xFF }, { 0x13, 0xFF },
};
static const uint8_t sentences[10][6] = {
    { 0xFF }, { 2, 0x1C, 0xFF }, { 4, 0xFF }, { 6, 8, 2, 0x1E, 0xFF }, { 0xA, 0x20, 0xA, 0xFF }, { 0xC, 2, 0xE, 0xFF }, { 0x10, 0xE, 0xFF }, { 0x12, 0xE, 0xFF }, { 0x14, 0xFF }, { 0x16, 0x18, 0x16, 0x1A, 0xFF },
};

// A piece of the words: a letter (or a digit: `digit`) that stays for $3C frames and then flies off away from the middle (loc_31D5A, loc_31D96)
void Obj_SS4Banner(Sst *self) {
    switch (self->routine) {
    case 4:
        if (--self->own.u16[1] != 0) {
            Sst_Draw(self);
            return;
        }
        if (self->mapping_frame > 0x13 && self->own.u8[0] == 0) {
            Sst_Delete(self);
            return;
        }
        self->routine = 8;
        self->angle = (uint8_t)CalcAngle((int16_t)(self->x_pos.f.u - 0x80), (int16_t)(self->y_pos.f.u - 0x70));
        Sst_Draw(self);
        return;
    case 8: {
        int16_t sin, cos;
        CalcSine(self->angle, &sin, &cos);
        self->x_pos.f.u = (int16_t)(self->x_pos.f.u + ((cos * 8) >> 8));
        self->y_pos.f.u = (int16_t)(self->y_pos.f.u + ((sin * 8) >> 8));
        if (self->x_pos.f.u < 0 || self->x_pos.f.u > 0x100 || self->y_pos.f.u < 0) {
            Sst_Delete(self);
            return;
        }
        Sst_Draw(self);
        return;
    }
    }
}

static Sst *SpawnPiece(int frame, bool digit, int x, int y) { // loc_31DD6 and the letters of loc_31E14
    Sst *o = Sst_Allocate();
    if (o == NULL)
        return NULL;
    o->code = Obj_SS4Banner;
    o->routine = 4;
    o->mapping_frame = (uint8_t)frame;
    o->mappings = digit ? Mappings_SSB4Counter : Mappings_SSB4Letters;
    o->art_tile = digit ? TILE_MAP(0, 2, 0, 0, ArtTile_SS4HudFont) : TILE_MAP(0, 2, 0, 0, ArtTile_SS4WingsHand);
    o->render_flags |= SST_RENDER_LEVEL;
    o->priority = 1 * 0x80;
    o->x_pos.f.u = (int16_t)x;
    o->y_pos.f.u = (int16_t)y;
    o->own.u16[1] = 0x3C;
    o->own.u8[0] = digit ? 1 : 0;
    return o;
}

static int SpawnChunk(int byte, int x, int y) { // loc_31E14: a chunk's letters, 8 apart; the x it ends at
    for (const uint8_t *l = chunks[byte >> 1]; *l != 0xFF; l++, x += 8)
        SpawnPiece(*l, false, x, y);
    return x;
}

static void SpawnSentence(int index, int x, int y) { // loc_31F50
    if (index != 2)
        x = 0x5E;
    for (const uint8_t *s = sentences[index]; *s != 0xFF; s++) {
        if (*s == 2) {
            x = 0x5E;
            y = 0x7E;
        }
        x = SpawnChunk(*s, x, y) + 8;
    }
}

// loc_31F8C: a sentence of the versus play (`index` is the original's d0: 8 and $A the words before the round and at its start, $C Sonic's round, $E Miles', $10 a draw, $12 the next round), from the x of its table
static void VersusSentence(int d0) {
    static const uint8_t xs[6] = { 0x44, 0x44, 0x58, 0x58, 0x74, 0x3C };
    int x = xs[(d0 - 8) >> 1];
    for (const uint8_t *c = sentences[d0 >> 1]; *c != 0xFF; c++)
        x = SpawnChunk(*c, x, 0x48) + 8;
}

// loc_31ED8 with loc_8248: the rings the next check asks, in words: the number's digits, with words before and after it
static void ShowRequirement(void) {
    required_now = SS4Stage_Required(stage_number, checkpoint);
    checkpoint++;
    int n = required_now;
    const int digits = n >= 100 ? 3 : 2;
    int x = digits == 3 ? 0x80 : 0x78;
    SpawnChunk(0, 0x54, 0x48);
    for (int k = 0; k < digits; k++, x -= 8) {
        SpawnPiece(n % 10, true, x, 0x48);
        n /= 10;
    }
    int tx = digits == 3 ? 0x8C : 0x84; // (the sentence after it: loc_31FBE's second: the chunks 2 and $1C)
    for (const uint8_t *s = sentences[1]; *s != 0xFF; s++)
        tx = SpawnChunk(*s, tx, 0x48) + 8;
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// The gate of a checkpoint ($5A): seven rings come in an arch from the middle to the sides (the gate's table, one row for each step of a segment's time) and, at the end, the rings are counted

static const uint8_t gate_table[9][14] = {
    { 0xF6, 0xF6, 0x70, 0x5E, 0x76, 0x58, 0x7E, 0x56, 0x88, 0x58, 0x8E, 0x5E, 0xF6, 0xF6 }, { 0xF6, 0xF6, 0x6D, 0x5A, 0x74, 0x54, 0x7E, 0x50, 0x8A, 0x54, 0x92, 0x5A, 0xF6, 0xF6 },
    { 0xF6, 0xF6, 0x6A, 0x58, 0x72, 0x50, 0x7E, 0x4C, 0x8C, 0x50, 0x94, 0x58, 0xF6, 0xF6 }, { 0xF6, 0xF6, 0x68, 0x56, 0x70, 0x4C, 0x7E, 0x48, 0x8E, 0x4C, 0x96, 0x56, 0xF6, 0xF6 },
    { 0x62, 0x5E, 0x66, 0x50, 0x70, 0x46, 0x7E, 0x42, 0x8E, 0x46, 0x98, 0x50, 0x9C, 0x5E }, { 0x5C, 0x5A, 0x62, 0x4A, 0x70, 0x3E, 0x7E, 0x38, 0x8E, 0x3E, 0x9C, 0x4A, 0xA2, 0x5A },
    { 0x54, 0x54, 0x5A, 0x3E, 0x6A, 0x30, 0x7E, 0x2A, 0x94, 0x30, 0xA4, 0x3E, 0xAA, 0x54 }, { 0x42, 0x4A, 0x4C, 0x28, 0x62, 0x12, 0x7E, 0x0A, 0x9C, 0x12, 0xB2, 0x28, 0xBC, 0x4A },
    { 0x16, 0x26, 0x28, 0xFC, 0xEC, 0xEC, 0xEC, 0xEC, 0xEC, 0xEC, 0xD6, 0xFC, 0xE8, 0x26 },
};
static const uint8_t gate_frames[9] = { 0, 1, 1, 1, 2, 4, 6, 8, 9 };
static int gate_wait;           // the check is waiting for the segment the gate begins in
static int after_gate;          // frames until the words of the result are done

void Obj_SS4GateRing(Sst *self) {
    if (gate_step < 0 || gate_step > 8) {
        Sst_Delete(self);
        return;
    }
    const uint8_t *row = gate_table[gate_step];
    const int k = self->subtype;
    if (self->routine == 0) {
        self->routine = 2;
        self->mappings = Mappings_SSB4Rings;
        self->art_tile = TILE_MAP(0, 3, 0, 0, ArtTile_SS4Rings);
        self->render_flags |= SST_RENDER_LEVEL;
        self->priority = 5 * 0x80;
    }
    if (row[k * 2] == 0xF6)
        return;
    self->x_pos.f.u = row[k * 2];
    self->y_pos.f.u = row[k * 2 + 1];
    self->mapping_frame = gate_frames[gate_step];
    Sst_Draw(self);
}

// The emblem of a gate's result (loc_31CB6 and loc_31C02): the wings (frame $14) with the hand (frame $15) in front, which bobs a few pixels; a fail turns the hand over (a thumb down). They stay $3C frames
void Obj_SS4Emblem(Sst *self) {
    const bool hand = self->subtype & 1, fail = self->subtype & 2;
    if (self->routine == 0) {
        self->routine = 2;
        self->mappings = Mappings_SSB4Letters;
        self->art_tile = TILE_MAP(0, 1, 0, 0, ArtTile_SS4WingsHand);
        self->render_flags |= SST_RENDER_LEVEL;
        self->priority = (hand ? 0 : 1) * 0x80;
        self->mapping_frame = hand ? 0x15 : 0x14;
        const int y = (self->subtype & 4) ? 0x1C : 0x48; // (the versus play's is higher: loc_31CB6's d4)
        if (self->subtype & 8)
            self->art_tile = TILE_MAP(0, 2, 0, 0, ArtTile_SS4WingsHand); // (a round Miles won: the other palette line, loc_31BA2)
        self->x_pos.f.u = 0x80;
        self->y_pos.f.u = (int16_t)y;
        self->own.u16[1] = 0x3C;
        self->own.u16[2] = (uint16_t)y; // the place it bobs about
        if (hand && fail)
            self->render_flags |= SST_RENDER_Y_FLIP;
    }
    if (hand) {
        int16_t y = self->y_pos.f.u;
        if (self->own.u8[0] == 0) {
            y--;
            if (y < (int16_t)(self->own.u16[2] - 4)) {
                y = (int16_t)(self->own.u16[2] - 3);
                self->own.u8[0] = 1;
            }
        } else {
            y++;
            if (y > (int16_t)(self->own.u16[2] + 4)) {
                y = (int16_t)(self->own.u16[2] + 3);
                self->own.u8[0] = 0;
            }
        }
        self->y_pos.f.u = y;
    }
    if (--self->own.u16[1] == 0) {
        if (hand && versus) { // loc_31C7A: the third round ends the stage, any other has the words of the next one
            if (round_count >= 3) {
                PlayMusic(bgm_Stop);
                over = true;
            } else {
                VersusSentence(0x12);
            }
        }
        Sst_Delete(self);
        return;
    }
    Sst_Draw(self);
}

// loc_60F8 on: while the gate is on the track's three lights cycle through four sets of colours every 4 frames; loc_319A4 puts the plain ones back
static int gate_cycle;
static bool gate_lit;
static void GateColours(bool on) {
    static const uint16_t sets[4][3] = { { 0xEE, 0xCC, 0x88 }, { 0xE0, 0xC0, 0x80 }, { 0xEE0, 0xCC0, 0x880 }, { 0xE0E, 0xC0C, 0x808 } };
    static const uint16_t plain[3] = { 0xEE, 0x88, 0x44 };
    gate_lit = on;
    if (on && (ss_frames & 3) != 0)
        return;
    for (int i = 0; i < 3; i++)
        dry_palette[3][11 + i] = on ? sets[gate_cycle & 3][i] : plain[i];
    if (on)
        gate_cycle++;
}

static void BeginGate(void) {
    gate_step = 0;
    GateColours(true);
    for (int k = 0; k < 7; k++)
        Make(Obj_SS4GateRing, (uint8_t)k);
}

// What the gate's end does (loc_31B0C, loc_31BE4 and what follows): the sound, the colours back, the rings against what is asked; the words of the result; a fail ends the stage after them
static void EndVersusRound(void) { // loc_31B0C on, the versus branch: the round's rings are kept and counted again from nothing; the character with more has won it
    const int sonic = w2.rings[0], miles = w2.rings[1];
    round_rings[round_count][0] = (uint16_t)sonic;
    round_rings[round_count][1] = (uint16_t)miles;
    w2.rings[0] = w2.rings[1] = 0;
    round_count++;
    if (sonic == miles) { // (a draw: only its words, and after the third round the stage is over at once)
        VersusSentence(0x10);
        if (round_count >= 3) {
            PlayMusic(bgm_Stop);
            over = true;
        }
        return;
    }
    const bool sonic_won = sonic > miles;
    round_wins[sonic_won ? 0 : 1]++;
    for (int k = 0; k < 2; k++)
        Make(Obj_SS4Emblem, (uint8_t)(k | 4 | (sonic_won ? 0 : 8)));
    VersusSentence(sonic_won ? 0xC : 0xE);
}

static void EndGate(void) {
    GateColours(false);
    items.flash = false;
    gate_step = -1;
    if (versus) {
        QueueSound2(SFX(0xA1));
        EndVersusRound();
        return;
    }
    const bool failed = SS4Stage_Rings() < required_now;
    // (Beta 4 plays $A1, the lamp post, for both; the final plays $EE, what this build calls sfx_Error, for an earlier gate's fail: the one byte the user named)
    QueueSound2(failed ? sfx_Error : SFX(0xA1));
    for (int k = 0; k < 2; k++)
        Make(Obj_SS4Emblem, (uint8_t)(k | (failed ? 2 : 0)));
    if (failed) {
        SpawnSentence(3, 0x5E, 0x68);
        end_state = 4;
        end_timer = 0x3C; // (loc_31C5E: the emblem is gone, the music stops, the stage is over)
    } else {
        SpawnSentence(2, 0x74, 0x68);
        pass_pending = 0x3C;
        end_state = 0;
    }
}

static void StartCheck(int kind) {
    if (kind == 0x59) {
        end_state = 1;
        end_timer = 0;
    } else {
        gate_waiting = 1; // (the gate begins when the segment that has the gate's place has gone: loc_319DC)
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// The emerald ($59 after its speed is cut): it comes down the middle of the pipe by the segment's table of places, as an item does but slower ($4445 a frame), a quarter smaller, with its shadow; the colours of the
// track's three lights are the stage's emerald's (loc_3214A)

bool SS4Stage_PadLocked(void) { return pad_locked; }

static void EmeraldColours(void) {
    static const uint16_t colours[7][3] = {
        { 0xEA, 0xC6, 0x82 }, { 0xAEE, 0xCE, 0x8A }, { 0xA6E, 0x0E, 0x0A }, { 0xEAE, 0xE6E, 0xE4E }, { 0xE6A, 0xC08, 0x806 }, { 0xEE8, 0xEC0, 0xE80 }, { 0xCCC, 0xAAA, 0x888 },
    };
    for (int i = 0; i < 3; i++)
        dry_palette[3][11 + i] = colours[stage_number % 7][i];
}

void Obj_SS4Emerald(Sst *self) {
    if (end_state < 2 || end_state > 4) {
        Sst_Delete(self);
        return;
    }
    static const uint8_t anim_by_distance[32] = { 9, 9, 9, 8, 8, 7, 7, 6, 6, 5, 5, 4, 4, 3, 3, 3, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0 };
    const bool shadow = self->subtype == 1;
    if (self->routine == 0) {
        self->routine = 2;
        self->mappings = shadow ? Mappings_SpecialStageShadow : Mappings_SSB4Checkpoint;
        self->art_tile = shadow ? TILE_MAP(1, 3, 0, 0, ArtTile_SS4Shadow) : TILE_MAP(0, 3, 0, 0, ArtTile_SS4Emerald);
        self->render_flags |= SST_RENDER_LEVEL;
        self->priority = (shadow ? 5 : 4) * 0x80;
    }
    int n = (int)(emerald_distance >> 16);
    if (n < 1 || n > SS4_PlaceCount(w2.segment))
        return;
    const uint8_t *e = SS4_Place(w2.segment, n - 1);
    int d3 = e[1] >= 0x80 ? (int8_t)e[1] : e[1];
    int d4 = (e[2] * 3) >> 2, d5 = (e[3] * 3) >> 2;
    int anim = anim_by_distance[n > 0x1D ? 0x1E : n];
    if (shadow) {
        d4 += d4 >> 2;
        d5 += d5 >> 2;
        self->x_pos.f.u = (int16_t)(e[0]);
        self->y_pos.f.u = (int16_t)(d5 + d3);
        self->mapping_frame = (uint8_t)((10 - anim == 10 ? 9 : 10 - anim) * 3);
    } else {
        self->x_pos.f.u = (int16_t)(e[0]);
        self->y_pos.f.u = (int16_t)(((e[3] * 3) >> 2) + d3);
        self->mapping_frame = (uint8_t)anim;
    }
    (void)d4;
    Sst_Draw(self);
}

static void RunEnd(void) {
    switch (end_state) {
    case 1: // loc_321A8: the pad is locked and the speed is cut at four moments
        pad_locked = true;
        end_timer--;
        if (end_timer == -0x1E || end_timer == -0x3C || end_timer == -0x5A)
            speed_word = (uint16_t)(speed_word - 2);
        if (end_timer == -0x78) {
            speed_word = (uint16_t)(speed_word - 2);
            end_timer = 0x78;
            end_state = 2;
            emerald_distance = (int32_t)0x36 << 16;
            EmeraldColours();
            Make(Obj_SS4Emerald, 1);
            Make(Obj_SS4Emerald, 0);
        }
        break;
    case 2: // loc_322D6: the emerald comes; at the end of its time it is the rings' check again
        emerald_distance -= 0x4445;
        if (--end_timer < 0) {
            if (SS4Stage_Rings() >= required_now) {
                if ((emerald_distance >> 16) < 3) {
                    end_timer = 0x1D;
                    end_state = 3;
                    PlayMusic(mus_Emerald);
                }
            } else {
                speed_word = (uint16_t)(speed_word - 4);
                SpawnSentence(3, 0x5E, 0x68); // (the final gate's fail: the words only, no emblem and no sound: loc_3230C calls loc_31F50 itself; the emerald stays where it is)
                end_timer = 0x45;
                end_state = 4;
            }
        }
        break;
    case 3: // loc_3234C: the emerald is won
        if (--end_timer < 0) {
            emerald = true;
            over = true;
            end_state = 0;
        }
        break;
    case 4: // loc_32328: not enough: the stage ends
        if (--end_timer < 0) {
            PlayMusic(bgm_Stop);
            over = true;
            end_state = 0;
        }
        break;
    }
}

void Obj_SS4Controller(Sst *self) {
    (void)self;
    SS4Items_Run(&items, &w2);
    for (int i = 0; i < w2.sounds; i++)
        QueueSound2(SFX(w2.sound[i]));
    w2.sounds = 0;
    if (items.check != 0) {
        StartCheck(items.check);
        items.check = 0;
    }
    RunEnd();
    if (gate_waiting && prev_segment == 0x14 && w2.frame_four) {
        gate_waiting = 0;
        BeginGate();
    } else if (gate_step >= 0 && w2.frame_four) {
        if (gate_step == 8)
            EndGate();
        else
            gate_step++;
    }
    if (gate_lit || gate_step >= 0)
        GateColours(gate_step >= 0);
    if (pass_pending && --pass_pending == 0)
        ShowRequirement();
    for (int i = 0; i < SS4_ITEM_MAX; i++) { // an object for each item that is in the pool
        if (items.item[i].kind == SS4Kind_None)
            continue;
        const int slot = sst_of[i] - 1;
        if (slot >= 0 && sst[slot].code == Obj_SS4Item && sst[slot].subtype == i)
            continue;
        Sst *o = Make(Obj_SS4Item, (uint8_t)i);
        if (o != NULL)
            sst_of[i] = (uint8_t)(o - sst + 1);
    }
}

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// The HUD, the ring counters and the START banner

void Obj_SS4Hud(Sst *self) { // $5E
    if (self->routine == 0) {
        self->routine = 1;
        self->mappings = Mappings_SSB4Hud;
        self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_SS4HudFont);
        self->render_flags |= SST_RENDER_LEVEL;
        self->priority = 0;
        self->x_pos.f.u = 0x80;
        self->y_pos.f.u = 0;
        self->mapping_frame = versus ? 1 : 0; // (the frame without the TOTAL box)
    }
    Sst_Draw(self);
}

// $87: the digits of the rings: subtype 0 Sonic's and Miles' (at $48 and $E0, the leading zeros left out), subtype 1 the two together in the middle
void Obj_SS4Counter(Sst *self) {
    if (self->routine == 0) {
        self->routine = 2;
        self->mappings = Mappings_SSB4Counter;
        self->art_tile = TILE_MAP(0, 2, 0, 0, ArtTile_SS4HudFont);
        self->render_flags |= SST_RENDER_LEVEL | SST_RENDER_MULTI_DRAW;
        self->priority = 0;
        self->x_pos.f.u = 0;
        self->y_pos.f.u = 0;
        self->mapping_frame = 0;
    }
    int n = 0;
    if (self->subtype == 0) {
        for (int who = 0; who < 2; who++) {
            const uint8_t *d = w2.digits[who];
            int x = who == 0 ? 0x48 : 0xE0, first = 0;
            for (int k = 0; k < 3; k++) {
                if (k < 2 && !first && d[k] == 0)
                    continue;
                first = 1;
                self->children[n].x = (int16_t)x;
                self->children[n].y = 0x20;
                self->children[n].frame = d[k];
                x += 8;
                n++;
            }
        }
    } else if (self->subtype == 2) { // loc_814A: an icon for each round a character has won, from his side of the HUD toward the middle ($20 and $30 for Sonic, $B8 and $C8 for Miles; frame $A)
        for (int who = 0; who < 2; who++)
            for (int k = 0; k < round_wins[who]; k++) {
                self->children[n].x = (int16_t)(who == 0 ? 0x20 + 0x10 * k : 0xB8 + 0x10 * k);
                self->children[n].y = 0x2C;
                self->children[n].frame = 0xA;
                n++;
            }
    } else {
        int ones = w2.digits[0][2] + w2.digits[1][2], tens = w2.digits[0][1] + w2.digits[1][1], hundreds = w2.digits[0][0] + w2.digits[1][0];
        if (ones >= 10) {
            tens++;
            ones -= 10;
        }
        if (tens >= 10) {
            hundreds++;
            tens -= 10;
        }
        const int digits = hundreds ? 3 : tens ? 2 : 1;
        const int digit[3] = { hundreds, tens, ones };
        int x = digits == 3 ? 0x78 : digits == 2 ? 0x7C : 0x80;
        for (int k = 3 - digits; k < 3; k++) {
            self->children[n].x = (int16_t)x;
            self->children[n].y = 0x20;
            self->children[n].frame = (uint8_t)digit[k];
            x += 8;
            n++;
        }
    }
    self->child_count = (uint8_t)n;
    Sst_Draw(self);
}

// $5F: the banner drops in from above at $100 a frame to y $48, waits $F frames, and breaks into its seven pieces, which fly away at 8 by their angle from the middle; $1E frames more and the stage begins
void Obj_SS4Start(Sst *self) {
    switch (self->routine) {
    case 0:
        if (versus)
            VersusSentence(8); // (the words before the round: loc_7CEE)
        self->routine = 2;
        self->x_pos.f.u = 0x80;
        self->y_pos.f.u = (int16_t)-0x40;
        self->y_vel = 0x100;
        self->mappings = Mappings_SSB4Start;
        self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_SS4Start);
        self->render_flags |= SST_RENDER_LEVEL;
        self->priority = 1 * 0x80;
        Sst_MoveSprite2(self);
        break;
    case 2:
        Sst_MoveSprite2(self);
        if (self->y_pos.f.u >= 0x48) {
            self->y_vel = 0;
            self->y_pos.f.u = 0x48;
            self->routine = 4;
            self->own.u8[2] = 0x0F; // ($2A)
        }
        break;
    case 4:
        if (--self->own.u8[2] == 0) {
            for (int i = 0; i < 7; i++) { // the pieces: the frames 2 to 8, each flying from where it is across the banner
                Sst *piece = Make(Obj_SS4Letter, (uint8_t)(2 + i));
                if (piece == NULL)
                    continue;
                piece->mappings = Mappings_SSB4Start;
                piece->art_tile = self->art_tile;
                piece->render_flags |= SST_RENDER_LEVEL;
                piece->priority = self->priority;
                piece->x_pos.f.u = self->x_pos.f.u;
                piece->y_pos.f.u = self->y_pos.f.u;
                piece->mapping_frame = (uint8_t)(2 + i);
                piece->routine = 2;
            }
            self->routine = 6;
            self->own.u16[1] = 0x1E; // ($2A word)
            self->mapping_frame = 0;
            self->mapping_frame = 0xFF; // (the banner itself is gone: only its pieces fly)
        }
        break;
    case 6:
        if ((int16_t)--self->own.u16[1] < 0) {
            started = true;
            if (versus)
                VersusSentence(0xA);
            else
                ShowRequirement();
            Sst_Delete(self);
            return;
        }
        return;
    }
    Sst_Draw(self);
}

void Obj_SS4Letter(Sst *self) { // a piece of the banner: it flies off to the side it is on
    static const int8_t dx[9] = { 0, 0, -0x40, -0x20, -0x10, 0, 0x10, 0x20, 0x40 };
    int16_t angle_x = dx[self->subtype < 9 ? self->subtype : 8];
    self->x_pos.f.u = (int16_t)(self->x_pos.f.u + (angle_x < 0 ? -8 : angle_x > 0 ? 8 : 0));
    self->y_pos.f.u = (int16_t)(self->y_pos.f.u - 2);
    if (self->x_pos.f.u < 0 || self->x_pos.f.u > 0x100 || self->y_pos.f.u < 0) {
        Sst_Delete(self);
        return;
    }
    Sst_Draw(self);
}
