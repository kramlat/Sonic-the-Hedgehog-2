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

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "Resource/Mappings/SSB4Sonic.h"
#include "Resource/Mappings/SSB4Hud.h"
#include "Resource/Mappings/SSB4Start.h"
#include "Resource/Mappings/SSB4Counter.h"
#include "Resource/Mappings/SSB4Letters.h"
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
static uint16_t pad_history[16]; // Sonic's pad, the newest first: Miles does what he did 16 frames ago when nobody works his own (loc_2FE20, loc_30E24)
static uint16_t miles_wait;
static int segments_seen_step = -1;

// The rings that the stage asks for at each of the checkpoints (loc_8298: four for each stage)
static const uint8_t required[40] = {
    0x14, 0x32, 0x46, 0x46, 0x23, 0x4B, 0x82, 0xBE, 0x28, 0x55, 0x96, 0xDC, 0x2D, 0x5F, 0xAA, 0xFA, 0x32, 0x69, 0xBE, 0xFA,
    0x32, 0x69, 0xBE, 0xFA, 0x32, 0x69, 0xBE, 0xFA, 0x28, 0x50, 0x8C, 0xC8, 0x28, 0x50, 0x8C, 0xC8, 0x28, 0x50, 0x8C, 0xC8,
};

int SS4Stage_Required(int stage, int cp) {
    return required[(stage * 4 + cp) % 40];
}

void SS4Stage_Begin(int stage) {
    stage_number = stage;
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
    speed_word = 0x0C;
    checkpoint = 0;
    segments_seen_step = -1;
}

bool SS4Stage_Started(void) { return started; }
bool SS4Stage_Over(void) { return over; }
bool SS4Stage_GotEmerald(void) { return emerald; }
uint16_t SS4Stage_Speed(void) { return speed_word; }
int SS4Stage_Rings(void) { return w2.rings[0] + w2.rings[1]; }

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
    Make(Obj_SS4Counter, 1);
    Make(Obj_SS4Controller, 0);
}

void SS4Stage_Frame(const SSDisplay *d) {
    wrld.period_less = d->period_less;
    w2.frame_four = d->frame == 4;
    w2.flipped = d->display_flipped;
    w2.segment = d->segment < 0 ? 0 : d->segment;
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
    if (!started) { // (nobody is held back by the banner but the stage's clock: the characters run)
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
static int end_timer, end_state;
static int32_t emerald_distance;

static void StartCheck(int kind) {
    if (kind == 0x59) {
        end_state = 1;
        end_timer = 0;
    } else {
        end_state = 10;
        end_timer = 0x78;
    }
}

static void RunEnd(void) {
    switch (end_state) {
    case 1: // loc_321A8: the pad is locked and the speed is cut at four moments
        end_timer--;
        if (end_timer == -0x1E || end_timer == -0x3C || end_timer == -0x5A)
            speed_word = (uint16_t)(speed_word - 2);
        if (end_timer == -0x78) {
            speed_word = (uint16_t)(speed_word - 2);
            end_timer = 0x78;
            end_state = 2;
            emerald_distance = (int32_t)0x36 << 16;
        }
        break;
    case 2: // loc_322D6: the emerald comes; at the end of its time it is the rings' check again
        emerald_distance -= 0x4445;
        if (--end_timer < 0) {
            if (SS4Stage_Rings() >= SS4Stage_Required(stage_number, 2)) {
                if ((emerald_distance >> 16) < 3) {
                    end_timer = 0x1D;
                    end_state = 3;
                    PlayMusic(mus_Emerald);
                }
            } else {
                speed_word = (uint16_t)(speed_word - 4);
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
            over = true;
            end_state = 0;
        }
        break;
    case 10: // loc_31BE4 on: a checkpoint: the rings against what it asks; too few and the stage ends after the words
        if (--end_timer < 0) {
            if (SS4Stage_Rings() < SS4Stage_Required(stage_number, checkpoint)) {
                over = true;
                end_state = 0;
            } else {
                end_state = 0;
            }
            checkpoint++;
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
    if (items.flash)
        items.flash = false;
    RunEnd();
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
        self->mapping_frame = 0;
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
