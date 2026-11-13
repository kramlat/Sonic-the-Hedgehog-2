// The pre-beta's special stage objects: see SpecialStageObjects.h
#include "Object/SpecialStageObjects.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"
#include "Constants.h"
#include "Object/SpecialStageArt.h"
#include "SpecialStageTables.h"

#include "Game.h"
#include "Nemesis.h"
#include "Backend/VDP.h"

#include <string.h>

#include "Macros.h"

#include "Resource/Animation/SpecialStageSonic.h"
#include "Resource/Animation/SpecialStageMiles.h"
#include "Resource/Animation/SpecialStageMilesTail.h"
#include "Resource/Mappings/SpecialStageSonic.h"
#include "Resource/Mappings/SpecialStageMiles.h"
#include "Resource/Mappings/SpecialStageMilesTail.h"
#include "Resource/Mappings/SpecialStageShadow.h"
#include "Resource/Art/SpecialStageSonic.h"
#include "Resource/Art/SpecialStageSonicDiagonal.h"
#include "Resource/Art/SpecialStageSonicHorizontal.h"
#include "Resource/Art/SpecialStageSonicSpin.h"
#include "Resource/Art/SpecialStageMiles.h"
#include "Resource/Art/SpecialStageMilesDiagonal.h"
#include "Resource/Art/SpecialStageMilesHorizontal.h"
#include "Resource/Art/SpecialStageMilesSpin.h"
#include "Resource/Art/SpecialStageMilesTail.h"
#include "Resource/Art/SpecialStageMilesTailDiagonal.h"
#include "Resource/Art/SpecialStageMilesTailHorizontal.h"

enum { Who_Sonic = 0, Who_Miles = 1 };

static SSPlayer players[2];
static SSTrackState track = { 12, 4 };

// What Sonic's pad has been, a frame at a time (Offset_0x02ED44, $FFFF9B62 to $FFFF9B80: the newest first): Miles does what Sonic did 15 frames ago, if nobody works his own pad (Offset_0x02FD5C)
static uint16_t pad_history[16];
static uint16_t miles_wait;

// The characters' pictures, decoded (Offset_0x008D78 puts them in RAM at the start of the stage): each set's plain, diagonal, horizontal and spin ones, and the tails' plain, diagonal and horizontal
#define ART_BYTES 0x4000
typedef struct {
    uint8_t buffer[4][ART_BYTES];
    uint8_t cached;       // the frame whose tiles are in VRAM ($FFFFF766 and $FFFFF7DE)
    int vram_tile;
    int script_offset;    // where in the script the set's frames begin ($32 for Miles)
} CharacterArt;

static CharacterArt sonic_art = { .vram_tile = ArtTile_SpecialStageSonic, .script_offset = 0 };
static CharacterArt miles_art = { .vram_tile = ArtTile_SpecialStageMiles, .script_offset = 0x32 };
static uint8_t tail_buffer[3][ART_BYTES];
static uint8_t tail_cached = 0xFF;
static bool art_loaded;

static void LoadArtBuffers(void) {
    NemDecToRAM(Art_SpecialStageSonic, sonic_art.buffer[0]);
    NemDecToRAM(Art_SpecialStageSonicDiagonal, sonic_art.buffer[1]);
    NemDecToRAM(Art_SpecialStageSonicHorizontal, sonic_art.buffer[2]);
    NemDecToRAM(Art_SpecialStageSonicSpin, sonic_art.buffer[3]);
    NemDecToRAM(Art_SpecialStageMiles, miles_art.buffer[0]);
    NemDecToRAM(Art_SpecialStageMilesDiagonal, miles_art.buffer[1]);
    NemDecToRAM(Art_SpecialStageMilesHorizontal, miles_art.buffer[2]);
    NemDecToRAM(Art_SpecialStageMilesSpin, miles_art.buffer[3]);
    NemDecToRAM(Art_SpecialStageMilesTail, tail_buffer[0]);
    NemDecToRAM(Art_SpecialStageMilesTailDiagonal, tail_buffer[1]);
    NemDecToRAM(Art_SpecialStageMilesTailHorizontal, tail_buffer[2]);
    art_loaded = true;
}

static const uint8_t *ScriptEntry(int index) {
    size_t size;
    const uint8_t *table = SS_DynamicScript(&size);
    return table + (uint16_t)((table[index * 2] << 8) | table[index * 2 + 1]);
}

// SS_Load_Sonic_Dynamic_PLC and SS_Load_Miles_Dynamic_PLC (Offset_0x02EED4, Offset_0x02FDA4): when the frame has changed its tiles are copied into VRAM: from the buffer of its set (frames 0 to 3, 4 to $B, $C to $F,
// the rest), as the frame's script says, one after another. A script entry is a count of words and then words each with a number of tiles (the top 4 bits, less one) and where they begin (12 bits, in words)
static void LoadFrameArt(CharacterArt *art, uint8_t frame) {
    if (frame == art->cached)
        return;
    art->cached = frame;
    const uint8_t *buffer = art->buffer[frame < 4 ? 0 : frame < 0x0C ? 1 : frame < 0x10 ? 2 : 3];
    const uint8_t *script = ScriptEntry(frame + art->script_offset);
    const int count = (script[0] << 8) | script[1];
    size_t vram = (size_t)art->vram_tile * 32;
    for (int i = 0; i < count; i++) {
        const uint16_t entry = (uint16_t)((script[2 + i * 2] << 8) | script[3 + i * 2]);
        const size_t tiles = (size_t)((entry >> 12) & 0xF) + 1;
        const size_t from = (size_t)(entry & 0x0FFF) * 2;
        VDP_SeekVRAM(vram);
        VDP_WriteVRAM(buffer + from, tiles * 32);
        vram += tiles * 32;
    }
}

// Offset_0x02FE28: Miles' tails have one entry for a frame, a single word (not a count and words), from the entries $44 on
static void LoadTailArt(uint8_t frame) {
    if (frame == tail_cached)
        return;
    tail_cached = frame;
    const uint8_t *buffer = tail_buffer[frame < 7 ? 0 : frame < 0x0E ? 1 : 2];
    const uint8_t *entry_bytes = ScriptEntry(frame + 0x44);
    const uint16_t entry = (uint16_t)((entry_bytes[0] << 8) | entry_bytes[1]);
    const size_t tiles = (size_t)((entry >> 12) & 0xF) + 1;
    const size_t from = (size_t)(entry & 0x0FFF) * 2;
    VDP_SeekVRAM((size_t)ArtTile_SpecialStageMilesTail * 32);
    VDP_WriteVRAM(buffer + from, tiles * 32);
}

void SpecialStageObjects_SetTrack(uint16_t speed, uint8_t period_less) {
    track.speed = speed;
    track.period_less = period_less;
}

SSPlayer *SpecialStageObjects_Sonic(void) {
    return &players[Who_Sonic];
}

SSPlayer *SpecialStageObjects_Miles(void) {
    return &players[Who_Miles];
}

static void Spawn(Sst *self, int who, const uint8_t *mappings, uint16_t art_tile, int priority) {
    SSPlayer *p = &players[who];
    if (who == Who_Sonic)
        SSPlayer_Init(p);
    else
        SSPlayer_InitMiles(p);
    if (!art_loaded)
        LoadArtBuffers();
    (who == Who_Sonic ? &sonic_art : &miles_art)->cached = 0xFF;
    self->routine = 2;
    self->subtype = (uint8_t)who;
    self->mappings = mappings;
    self->art_tile = art_tile;
    self->render_flags |= SST_RENDER_LEVEL;
    self->priority = (uint16_t)(priority * 0x80);
    self->x_pos.f.u = (int16_t)(p->x >> 16);
    self->y_pos.f.u = (int16_t)(p->y >> 16);
    Sst *shadow = Sst_Allocate();
    if (shadow != NULL) {
        shadow->code = Obj_SpecialStageShadow;
        shadow->subtype = (uint8_t)who;
        shadow->x_pos.f.u = self->x_pos.f.u;
        shadow->y_pos.f.u = (int16_t)(self->y_pos.f.u + 0x18);
    }
}

static void Show(Sst *self, SSPlayer *p, CharacterArt *art) {
    self->x_pos.f.u = (int16_t)(p->x >> 16);
    self->y_pos.f.u = (int16_t)(p->y >> 16);
    self->mapping_frame = p->frame;
    self->render_flags = (uint8_t)((self->render_flags & 0xFC) | (p->flips & 3));
    self->status = p->status;
    self->anim = p->anim;
    LoadFrameArt(art, p->frame);
    Sst_Draw(self);
}

void Obj_SpecialStageSonic(Sst *self) {
    if (self->routine == 0) { // Offset_0x02ED58
        Spawn(self, Who_Sonic, Mappings_SpecialStageSonic, TILE_MAP(0, 1, 0, 0, ArtTile_SpecialStageSonic), 2);
        return;
    }
    // (the pad's history goes on a place, Sonic's pad first: Offset_0x02ED44)
    memmove(pad_history + 1, pad_history, 15 * sizeof(uint16_t));
    pad_history[0] = (uint16_t)((jpad1_hold1 << 8) | jpad1_press1);
    SSPlayer *p = &players[Who_Sonic];
    SSPlayer_Step(p, &track, jpad1_hold1, jpad1_press1);
    SSPlayer_Animate(p, &track, Animation_SpecialStageSonic);
    Show(self, p, &sonic_art);
}

void Obj_SpecialStageMiles(Sst *self) {
    if (self->routine == 0) { // Offset_0x02FC30
        Spawn(self, Who_Miles, Mappings_SpecialStageMiles, TILE_MAP(0, 2, 0, 0, ArtTile_SpecialStageMiles), 1);
        Sst *tail = Sst_Allocate();
        if (tail != NULL) {
            tail->code = Obj_SpecialStageMilesTail;
            tail->mappings = Mappings_SpecialStageMilesTail;
            tail->art_tile = TILE_MAP(0, 2, 0, 0, ArtTile_SpecialStageMilesTail);
            tail->render_flags |= SST_RENDER_LEVEL;
            tail->priority = (uint16_t)(self->priority - 0x80);
            tail->routine = 2;
            tail_cached = 0xFF;
        }
        return;
    }
    // Offset_0x02FD5C: when his own pad is worked he does what it says (and for 3 seconds after); then he does what Sonic did 15 frames ago
    uint8_t held = jpad2_hold, pressed = jpad2_press;
    if (held & 0x7F) {
        memset(pad_history, 0, sizeof(pad_history));
        miles_wait = 0xB4;
    } else if (miles_wait) {
        miles_wait--;
    } else {
        held = (uint8_t)(pad_history[15] >> 8);
        pressed = (uint8_t)pad_history[15];
    }
    SSPlayer *p = &players[Who_Miles];
    SSPlayer_Step(p, &track, held, pressed);
    SSPlayer_Animate(p, &track, Animation_SpecialStageMiles);
    Show(self, p, &miles_art);
}

void Obj_SpecialStageMilesTail(Sst *self) { // Offset_0x02FDDE: with Miles, in the animation of the tails while he runs (the first three), and gone while he jumps
    const SSPlayer *p = &players[Who_Miles];
    self->x_pos.f.u = (int16_t)(p->x >> 16);
    self->y_pos.f.u = (int16_t)(p->y >> 16);
    self->render_flags = (uint8_t)((self->render_flags & 0xFC) | (p->flips & 3));
    self->anim = p->anim;
    if (self->anim >= 3)
        return;
    // the tails' own animation, a standard one (a frame lasts 3 frames)
    if (self->anim != self->prev_anim) {
        self->prev_anim = self->anim;
        self->anim_frame = 0;
        self->anim_frame_timer = 0;
    }
    if (--self->anim_frame_timer < 0) {
        const uint8_t *script = Animation_SpecialStageMilesTail + (uint16_t)((Animation_SpecialStageMilesTail[self->anim * 2] << 8) | Animation_SpecialStageMilesTail[self->anim * 2 + 1]);
        self->anim_frame_timer = (int8_t)script[0];
        uint8_t frame = script[1 + self->anim_frame];
        if (frame == 0xFF) {
            self->anim_frame = 0;
            frame = script[1];
        }
        self->mapping_frame = frame & 0x7F;
        self->anim_frame++;
    }
    LoadTailArt(self->mapping_frame);
    Sst_Draw(self);
}

void Obj_SpecialStageShadow(Sst *self) { // Offset_0x02F436
    if (self->routine == 0) {
        self->routine = 2;
        self->mappings = Mappings_SpecialStageShadow;
        self->render_flags |= SST_RENDER_LEVEL;
        self->priority = 4 * 0x80;
    }
    SSShadow shadow;
    SSPlayer_Shadow(&players[self->subtype], &shadow);
    self->x_pos.f.u = shadow.x;
    self->y_pos.f.u = shadow.y;
    self->mapping_frame = shadow.frame;
    self->art_tile = shadow.art_tile;
    self->render_flags = (uint8_t)((self->render_flags & 0xFC) | (shadow.flips & 3));
    Sst_Draw(self);
}
