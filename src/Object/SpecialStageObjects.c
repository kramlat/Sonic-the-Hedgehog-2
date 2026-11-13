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
#include "Resource/Mappings/SpecialStageSonic.h"
#include "Resource/Mappings/SpecialStageShadow.h"
#include "Resource/Art/SpecialStageSonic.h"
#include "Resource/Art/SpecialStageSonicDiagonal.h"
#include "Resource/Art/SpecialStageSonicHorizontal.h"
#include "Resource/Art/SpecialStageSonicSpin.h"

static SSPlayer sonic;
static SSTrackState track = { 12, 4 };
static uint8_t cached_frame = 0xFF;

// Sonic's four sets of pictures, decoded (Offset_0x008D78 puts them in RAM at the start of the stage): the plain ones, the diagonal, the horizontal and the spin
static uint8_t art_buffer[4][0x3000];
static bool art_loaded;

static void LoadArtBuffers(void) {
    NemDecToRAM(Art_SpecialStageSonic, art_buffer[0]);
    NemDecToRAM(Art_SpecialStageSonicDiagonal, art_buffer[1]);
    NemDecToRAM(Art_SpecialStageSonicHorizontal, art_buffer[2]);
    NemDecToRAM(Art_SpecialStageSonicSpin, art_buffer[3]);
    art_loaded = true;
}

// SS_Load_Sonic_Dynamic_PLC (Offset_0x02EED4): when the frame has changed its tiles are copied into VRAM: from the buffer of its set (frames 0 to 3, 4 to $B, $C to $F, the rest) as the frame's script says, one after
// another from tile $2F2. A script entry is a count of words and then words each with a number of tiles (the top 4 bits, less one) and where they begin (12 bits, in words)
static void LoadFrameArt(uint8_t frame) {
    if (frame == cached_frame)
        return;
    cached_frame = frame;
    const uint8_t *buffer = art_buffer[frame < 4 ? 0 : frame < 0x0C ? 1 : frame < 0x10 ? 2 : 3];
    size_t size;
    const uint8_t *table = SS_DynamicScript(&size);
    const uint16_t offset = (uint16_t)((table[frame * 2] << 8) | table[frame * 2 + 1]);
    const uint8_t *script = table + offset;
    int count = (script[0] << 8) | script[1];
    size_t vram = (size_t)ArtTile_SpecialStageSonic * 32;
    for (int i = 0; i < count; i++) {
        const uint16_t entry = (uint16_t)((script[2 + i * 2] << 8) | script[3 + i * 2]);
        const size_t tiles = (size_t)((entry >> 12) & 0xF) + 1;
        const size_t from = (size_t)(entry & 0x0FFF) * 2;
        VDP_SeekVRAM(vram);
        VDP_WriteVRAM(buffer + from, tiles * 32);
        vram += tiles * 32;
    }
}

void SpecialStageObjects_SetTrack(uint16_t speed, uint8_t period_less) {
    track.speed = speed;
    track.period_less = period_less;
}

SSPlayer *SpecialStageObjects_Sonic(void) {
    return &sonic;
}

void Obj_SpecialStageSonic(Sst *self) {
    if (self->routine == 0) { // Offset_0x02ED58
        SSPlayer_Init(&sonic);
        if (!art_loaded)
            LoadArtBuffers();
        cached_frame = 0xFF;
        self->routine = 2;
        self->mappings = Mappings_SpecialStageSonic;
        self->art_tile = TILE_MAP(0, 1, 0, 0, ArtTile_SpecialStageSonic);
        self->render_flags |= SST_RENDER_LEVEL;
        self->priority = 2 * 0x80;
        self->x_pos.f.u = (int16_t)(sonic.x >> 16);
        self->y_pos.f.u = (int16_t)(sonic.y >> 16);
        Sst *shadow = Sst_Allocate();
        if (shadow != NULL) {
            shadow->code = Obj_SpecialStageShadow;
            shadow->x_pos.f.u = self->x_pos.f.u;
            shadow->y_pos.f.u = (int16_t)(self->y_pos.f.u + 0x18);
            shadow->parent = (uint16_t)(self - sst + 1);
        }
        return;
    }
    SSPlayer_Step(&sonic, &track, jpad1_hold1, jpad1_press1);
    SSPlayer_Animate(&sonic, &track, Animation_SpecialStageSonic);
    self->x_pos.f.u = (int16_t)(sonic.x >> 16);
    self->y_pos.f.u = (int16_t)(sonic.y >> 16);
    self->mapping_frame = sonic.frame;
    self->render_flags = (uint8_t)((self->render_flags & 0xFC) | (sonic.flips & 3));
    LoadFrameArt(sonic.frame);
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
    SSPlayer_Shadow(&sonic, &shadow);
    self->x_pos.f.u = shadow.x;
    self->y_pos.f.u = shadow.y;
    self->mapping_frame = shadow.frame;
    self->art_tile = shadow.art_tile;
    self->render_flags = (uint8_t)((self->render_flags & 0xFC) | (shadow.flips & 3));
    Sst_Draw(self);
}
