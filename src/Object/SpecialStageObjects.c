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
#include "EnginePalette.h"
#include "MathUtil.h"
#include "Resource/Mappings/SpecialStageSonic.h"
#include "Resource/Mappings/SpecialStageMiles.h"
#include "Resource/Mappings/SpecialStageMilesTail.h"
#include "Resource/Mappings/SpecialStageShadow.h"
#include "Resource/Mappings/SpecialStageRings.h"
#include "Resource/Mappings/SpecialStageBombs.h"
#include "Resource/Mappings/SpecialStageHud.h"
#include "Resource/Mappings/SpecialStageStart.h"
#include "Resource/Mappings/SpecialStageCounter.h"
#include "Resource/Art/SpecialStageBombs.h"
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
static SSItems items;
static const uint8_t *track_block;
static size_t track_block_size;
static bool track_flipped;
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

static void SpillRings(SSPlayer *p);

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
    SpillRings(p);
    SSPlayer_Animate(p, &track, Animation_SpecialStageSonic);
    if (!p->hit)
        SSItems_Touch(&items, p);
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
    SpillRings(p);
    SSPlayer_Animate(p, &track, Animation_SpecialStageMiles);
    if (!p->hit)
        SSItems_Touch(&items, p);
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

void SpecialStageObjects_SetItems(int step, const uint8_t *block, size_t block_size, bool flipped, bool load) {
    track_block = block;
    track_block_size = block_size;
    track_flipped = flipped;
    if (load)
        SSItems_Load(&items, step);
}

SSItems *SpecialStageObjects_Items(void) {
    return &items;
}

void SpecialStageObjects_UpdateItems(void) {
    SSItems_Update(&items, track_block, track_block_size, track_flipped);
}

// The bombs' pictures (Offset_0x0302CC): a frame's tiles are copied to VRAM tile $3C0 from the decoded art, the frames' places from the script's entries $12 on (a single word each)
static uint8_t bomb_buffer[ART_BYTES];
static uint8_t bomb_cached = 0xFF;
static bool bomb_loaded;

static void LoadBombFrame(uint8_t frame) {
    if (!bomb_loaded) {
        NemDecToRAM(Art_SpecialStageBombs, bomb_buffer);
        bomb_loaded = true;
    }
    if (frame == bomb_cached)
        return;
    bomb_cached = frame;
    const uint8_t *entry_bytes = ScriptEntry(frame + 0x12);
    const uint16_t entry = (uint16_t)((entry_bytes[0] << 8) | entry_bytes[1]);
    const size_t tiles = (size_t)((entry >> 12) & 0xF) + 1;
    const size_t from = (size_t)(entry & 0x0FFF) * 2;
    VDP_SeekVRAM((size_t)ArtTile_SpecialStageBombs * 32);
    VDP_WriteVRAM(bomb_buffer + from, tiles * 32);
}

void Obj_SpecialStageItem(Sst *self) { // Offset_0x030316 and Offset_0x030218: the item of the slot named by the subtype
    const SSItem *item = &items.item[self->subtype];
    if (self->routine == 0) {
        self->routine = 2;
        self->render_flags |= SST_RENDER_LEVEL;
        self->priority = 3 * 0x80;
    }
    if (item->kind == SS_ITEM_NONE || !item->visible) {
        self->render_flags &= (uint8_t)~SST_RENDER_ON_SCREEN;
        return;
    }
    self->x_pos.f.u = item->x;
    self->y_pos.f.u = item->y;
    self->mapping_frame = item->frame;
    if (item->kind == SS_ITEM_RING) {
        self->mappings = Mappings_SpecialStageRings;
        self->art_tile = TILE_MAP(0, 2, 0, 0, ArtTile_SpecialStageRings);
    } else {
        self->mappings = Mappings_SpecialStageBombs;
        if (item->anim == 0x0A) {
            self->art_tile = TILE_MAP(0, 2, 0, 0, ArtTile_SpecialStageBombsExplosion);
        } else {
            self->art_tile = TILE_MAP(0, 1, 0, 0, ArtTile_SpecialStageBombs);
            LoadBombFrame(item->frame);
        }
    }
    Sst_Draw(self);
}

void Obj_SpecialStageItemShadow(Sst *self) { // Offset_0x03058A: a shadow under the item (its size by the item's, its look by the angle)
    const SSItem *item = &items.item[self->subtype];
    if (self->routine == 0) {
        self->routine = 2;
        self->mappings = Mappings_SpecialStageShadow;
        self->render_flags |= SST_RENDER_LEVEL;
        self->priority = 5 * 0x80;
    }
    if (item->kind == SS_ITEM_NONE || !item->visible || item->anim > 9)
        return;
    static const uint16_t art[3] = { 0x6279, 0x629F, 0x62D9 };
    self->x_pos.f.u = item->x;
    self->y_pos.f.u = item->y;
    self->art_tile = art[item->shadow_look];
    self->mapping_frame = (uint8_t)((9 - item->anim) * 3 + item->shadow_look);
    self->render_flags = (uint8_t)((self->render_flags & 0xFC) | (item->shadow_flipped ? 1 : 0));
    Sst_Draw(self);
}

// ---- The head up display (stage 5b) ----

static bool stage_started; // $FFFF9B23: the START banner has gone, the stage runs and the clock counts
static bool time_over;
static int8_t clock_tens, clock_ones;
static uint16_t clock_ticks;
static Sst *banner;

bool SpecialStageObjects_Started(void) {
    return stage_started;
}

bool SpecialStageObjects_TimeUp(void) {
    return time_over;
}

void SpecialStageObjects_ResetHud(void) {
    stage_started = false;
    time_over = false;
    clock_tens = clock_ones = 9;
    clock_ticks = 0x3B;
    banner = NULL;
}

static void ShowAt(Sst *self, int16_t x, int16_t y, uint8_t frame) {
    self->x_pos.f.u = x;
    self->y_pos.f.u = y;
    self->mapping_frame = frame;
    Sst_Draw(self);
}

void Obj_SpecialStageHud(Sst *self) { // Offset_0x00907A: the words RINGS and TIME and the second ring counter's frame, one picture
    if (self->routine == 0) {
        self->routine = 2;
        self->mappings = Mappings_SpecialStageHud;
        self->art_tile = TILE_MAP(0, 2, 0, 0, ArtTile_SpecialStageHudFont);
        self->render_flags |= SST_RENDER_LEVEL;
        self->priority = 1 * 0x80;
    }
    ShowAt(self, 0x80, 0, 0);
}

void Obj_SpecialStageCounter(Sst *self) { // Offset_0x0094FA: one digit of Sonic's (subtype 0 to 2) or Miles' (3 to 5) ring count: hundreds, tens, ones
    if (self->routine == 0) {
        self->routine = 2;
        self->mappings = Mappings_SpecialStageCounter;
        self->art_tile = TILE_MAP(0, 2, 0, 0, ArtTile_SpecialStageHudFont);
        self->render_flags |= SST_RENDER_LEVEL;
        self->priority = 0;
    }
    const int who = self->subtype / 3, place = self->subtype % 3;
    const unsigned rings = players[who].touched % 1000;
    const unsigned digit = place == 0 ? rings / 100 : place == 1 ? rings / 10 % 10 : rings % 10;
    ShowAt(self, (int16_t)((who == 0 ? 0x44 : 0xDC) + place * 8), 0x20, (uint8_t)digit);
}

void Obj_SpecialStageClock(Sst *self) { // Offset_0x0093BE: the two digits of the time, counting down a second at a time once the stage has started (the tens are subtype 0, the ones 1)
    if (self->routine == 0) {
        self->routine = 2;
        self->mappings = Mappings_SpecialStageCounter;
        self->art_tile = TILE_MAP(0, 2, 0, 0, ArtTile_SpecialStageHudFont);
        self->render_flags |= SST_RENDER_LEVEL;
        self->priority = 1 * 0x80;
    }
    if (self->subtype == 0 && stage_started && !time_over && --clock_ticks == 0) { // (the tens' object keeps the time for both)
        clock_ticks = 0x3C;
        if (--clock_ones < 0) {
            clock_ones = 9;
            if (--clock_tens < 0) {
                clock_tens = clock_ones = 0;
                time_over = true;
            }
        }
    }
    ShowAt(self, self->subtype == 0 ? 0x7C : 0x84, 0x20, (uint8_t)(self->subtype == 0 ? clock_tens : clock_ones));
}

// The START banner (object $5F): it comes down from above, the three lights below it go red, yellow, green a second each, and then the banner bursts into its pieces (the flags, the letters) which fly off, the A falling
static void ParkedTrafficLights(Sst *self) {
    static const uint16_t lights[2][3] = { { 0x00EA, 0x00C6, 0x0082 }, { 0x0AEE, 0x00CE, 0x008A } }; // (green, then yellow: colours 11 to 13 of the third line)
    if (self->own.s16[0]-- > 0) {
        Sst_Draw(self);
        return;
    }
    self->own.s16[0] = 0x3C;
    if (--self->own.s16[1] == 0) { // (the green has been on its second: the banner is told to go)
        if (banner != NULL)
            banner->own.s16[0] = -1;
        Sst_Delete(self);
        return;
    }
    memcpy(&dry_palette[3][11], lights[self->own.s16[1] - 1], sizeof(lights[0]));
    memcpy(&dry_palette_dup[3][11], lights[self->own.s16[1] - 1], sizeof(lights[0]));
    self->y_pos.f.u += 0x10;
    Sst_Draw(self);
}

void Obj_SpecialStageStart(Sst *self) { // Offset_0x0090E8
    static const struct { uint8_t frame; int8_t speed; } pieces[8] = { { 2, -0x60 }, { 3, -0x40 }, { 4, -0x20 }, { 5, 0 }, { 6, 0x20 }, { 7, 0x40 }, { 8, 0x60 }, { 9, 0x60 } };
    switch (self->routine) {
    case 0: // Offset_0x009170: above the screen, coming down
        self->x_pos.f.u = 0x80;
        self->y_pos.f.u = -0x40;
        self->y_vel = 0x100;
        self->mappings = Mappings_SpecialStageStart;
        self->art_tile = ArtTile_SpecialStageTrafficLights;
        self->render_flags |= SST_RENDER_LEVEL;
        self->priority = 1 * 0x80;
        self->own.s16[0] = 0;
        banner = self;
        self->routine = 2;
        // fallthrough
    case 2: // Offset_0x0091A2
        Sst_MoveSprite2(self);
        if (self->y_pos.f.u < 0x48) {
            Sst_Draw(self);
            return;
        }
        {
            Sst *lights = Sst_Allocate(); // Offset_0x0092A2: the lights' own object
            if (lights != NULL) {
                lights->code = Obj_SpecialStageStart;
                lights->x_pos.f.u = 0xE0;
                lights->y_pos.f.u = 0x48;
                lights->mappings = Mappings_SpecialStageStart;
                lights->art_tile = TILE_MAP(0, 3, 0, 0, ArtTile_SpecialStageTrafficLights);
                lights->render_flags |= SST_RENDER_LEVEL;
                lights->priority = 0;
                lights->routine = 8;
                lights->own.s16[0] = 0x3C;
                lights->own.s16[1] = 3;
                lights->mapping_frame = 1;
            }
        }
        self->y_vel = 0;
        self->y_pos.f.u = 0x48;
        self->routine = 4;
        Sst_Draw(self);
        return;
    case 4: // Offset_0x0091CA: waits for the lights, then bursts
        if (self->own.s16[0] == 0) {
            Sst_Draw(self);
            return;
        }
        for (int i = 0; i < 8; i++) {
            Sst *piece = Sst_Allocate();
            if (piece == NULL)
                break;
            piece->code = Obj_SpecialStageStart;
            piece->x_pos.f.u = self->x_pos.f.u;
            piece->y_pos.f.u = self->y_pos.f.u;
            piece->mappings = self->mappings;
            piece->art_tile = self->art_tile;
            piece->render_flags = self->render_flags;
            piece->priority = self->priority;
            piece->routine = 6;
            piece->mapping_frame = pieces[i].frame;
            piece->x_vel = (int16_t)(pieces[i].speed * 16);
            if (i == 3) { // (the A falls)
                piece->x_vel = 0;
                piece->y_vel = 0x600;
            }
        }
        self->routine = 0x0A;
        self->own.s16[0] = 0x1E;
        self->mapping_frame = 0;
        return; // (the banner itself is not drawn again: its pieces are)
    case 6: // Offset_0x009102: a piece flying
        if (self->x_vel == 0 && self->y_vel == 0) {
            // (nothing)
        } else if (self->x_vel == 0 || self->y_vel != 0) {
            self->y_vel = (int16_t)(self->y_vel + 0x40 > 0x800 ? 0x800 : self->y_vel + 0x40);
        } else if (self->x_vel > 0) {
            self->x_vel = (int16_t)(self->x_vel + 0x40 > 0x800 ? 0x800 : self->x_vel + 0x40);
        } else {
            self->x_vel = (int16_t)(self->x_vel - 0x40 < -0x800 ? -0x800 : self->x_vel - 0x40);
        }
        Sst_MoveSprite2(self);
        if (self->x_pos.f.u < -0x18 || self->x_pos.f.u > 0x118 || self->y_pos.f.u > 0x100) {
            Sst_Delete(self);
            return;
        }
        Sst_Draw(self);
        return;
    case 8:
        ParkedTrafficLights(self);
        return;
    case 0x0A: // Offset_0x00923A: the banner is gone; a half second more and the stage starts
        if (--self->own.s16[0] < 0) {
            stage_started = true;
            banner = NULL;
            Sst_Delete(self);
        }
        return;
    }
}

// ---- The rings a bomb knocks out (object $5B, Offset_0x030646) ----

void Obj_SpecialStageRingOut(Sst *self) { // a ring flying: up and out, then down in front of the character
    Sst_MoveSprite(self); // (ObjectFall: the speeds, then gravity $38)
    if (self->y_vel >= 0) { // Offset_0x030724: on its way down it goes in front, as its other picture
        self->priority = 0;
        self->mapping_frame = 9;
    }
    if (self->x_pos.f.u < 0 || self->x_pos.f.u >= 0x100 || (uint16_t)self->y_pos.f.u >= 0xE0) {
        Sst_Delete(self);
        return;
    }
    Sst_Draw(self);
}

static void SpillRings(SSPlayer *p) {
    if (!p->spill)
        return;
    p->spill = false;
    static const uint8_t ten[10] = { 0x38, 0x48, 0x2A, 0x56, 0x1C, 0x64, 0x0E, 0x72, 0x00, 0x80 };
    static const uint8_t few[10] = { 0x40, 0x30, 0x50, 0x20, 0x60, 0x10, 0x70, 0x00, 0x80, 0x00 };
    const unsigned before = p->touched;
    p->touched = before > 10 ? before - 10 : 0; // (it keeps what is over ten; with ten or fewer it loses them all)
    unsigned count;
    const uint8_t *angles;
    if (p->touched != 0) {
        count = 10;
        angles = ten;
    } else {
        count = before;
        angles = (before & 1) ? few : ten;
    }
    for (unsigned i = 0; i < count && i < 10; i++) {
        Sst *ring = Sst_Allocate();
        if (ring == NULL)
            break;
        int16_t sine, cosine;
        CalcSine((uint8_t)(p->angle + 0x40 + angles[i]), &sine, &cosine);
        ring->code = Obj_SpecialStageRingOut;
        ring->routine = 2;
        ring->mappings = Mappings_SpecialStageRings;
        ring->art_tile = TILE_MAP(0, 2, 0, 0, ArtTile_SpecialStageRings);
        ring->render_flags |= SST_RENDER_LEVEL;
        ring->priority = 5 * 0x80;
        ring->mapping_frame = 8;
        ring->x_pos.f.u = (int16_t)(p->x >> 16);
        ring->y_pos.f.u = (int16_t)(p->y >> 16);
        ring->x_vel = (int16_t)((cosine * 0x400) >> 8);
        ring->y_vel = (int16_t)((sine * 0x800) >> 8);
    }
}
