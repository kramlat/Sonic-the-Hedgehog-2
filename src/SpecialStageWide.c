// The pre-beta's special stage track for a wide screen: see SpecialStageWide.h
#include "SpecialStageWide.h"
#include "SpecialStageTables.h"
#include "Object/SpecialStageArt.h"
#include <stdbool.h>

void SSWide_Layout(SSWide *wide, int screen_width) {
    wide->screen_width = screen_width;
    if (screen_width <= 256) {
        wide->x0 = 0;
        wide->stride = 32;
        wide->left_cells = 0;
        wide->plane_width = 128;
        return;
    }
    wide->x0 = (screen_width - 256) / 2;
    const int right_pixels = screen_width - 256 - wide->x0;
    const int left_cells = (wide->x0 + 7) / 8 + 1;      // (a cell more: the scroll is not a whole number of cells)
    const int right_cells = (right_pixels + 7) / 8 + 1;
    int stride = 64;
    while (stride < 32 + left_cells + right_cells)
        stride *= 2;
    wide->stride = stride;
    wide->left_cells = (stride - 32) / 2 >= left_cells ? (stride - 32) / 2 : left_cells; // (the stripe in the middle of its room where there is room)
    wide->plane_width = 4 * stride;
}

// The pixel at one side of a tile's row (a word's tile: its first or last pixel as it is drawn, turned across when the word's h-flip bit is set)
static int EdgeColour(uint16_t word, bool right) {
    size_t count;
    const uint8_t *rows = SS_TrackTiles(&count);
    const size_t tile = word & 0x07FF;
    if (tile >= count)
        return 0;
    const uint8_t *r = rows + tile * 4;
    const uint32_t row = ((uint32_t)r[0] << 24) | ((uint32_t)r[1] << 16) | ((uint32_t)r[2] << 8) | r[3];
    const int first = (int)(row >> 28), last = (int)(row & 0xF);
    return (right != ((word & 0x0800) != 0)) ? last : first;
}

uint16_t SSWide_MarginWord(uint16_t edge_word, int colour) {
    if (colour == 0)
        return 0x8000 | ArtTile_SpecialStageBlank; /* (with the priority bit, as every tile of the track has it: the shadow mode leaves the background as it is) */ // (nothing: the background shows)
    return (uint16_t)((edge_word & 0xE000) | (ArtTile_SpecialStageSolid + colour));
}

void SSWide_ExpandUnit(const SSWide *wide, const uint16_t *unit, uint16_t *row) {
    for (int w = 0; w < 4; w++) {
        const uint16_t *core = unit + w * 32;
        uint16_t *to = row + w * wide->stride;
        const uint16_t blank = 0x8000 | ArtTile_SpecialStageBlank;
        const uint16_t left = SSWide_MarginWord(core[0], EdgeColour(core[0], false));
        const uint16_t right = SSWide_MarginWord(core[31], EdgeColour(core[31], true));
        for (int c = 0; c < wide->left_cells; c++)
            to[c] = c >= wide->left_cells - SS_WIDE_PAD_CELLS ? left : blank;
        for (int c = 0; c < 32; c++)
            to[wide->left_cells + c] = core[c];
        for (int c = wide->left_cells + 32; c < wide->stride; c++)
            to[c] = c < wide->left_cells + 32 + SS_WIDE_PAD_CELLS ? right : blank;
    }
}

int16_t SSWide_ScrollA(const SSWide *wide, int stripe) {
    return (int16_t)(wide->x0 - (stripe * wide->stride + wide->left_cells) * 8);
}

int16_t SSWide_ScrollB(const SSWide *wide, int16_t b) {
    return (int16_t)(b + wide->x0);
}
