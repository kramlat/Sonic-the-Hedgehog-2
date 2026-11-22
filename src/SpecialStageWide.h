#ifndef _SPECIALSTAGEWIDE_H
#define _SPECIALSTAGEWIDE_H

#include <stdint.h>

// The pre-beta's special stage for a screen wider than its 256 pixels (the user: the track stays pixel-exact in the middle 256; each line's edge pixel is carried 8 pixels out at both sides to make the walls look solid, and the
// rest of the sides is cropped so that the background is seen whole). The plane is made wider: a row holds the track's four stripes each with margins of cells on both sides, and the line scroll puts each stripe in the
// middle of the screen; the background (plane B) is drawn the whole width.
//
//   plane cells across = 4 * stride; stripe w starts at cell w * stride; its 32 cells of the track are at [stride_start + left_cells, +32); the margin cell next to the track at each side is one of the 16 solid tiles
//   (ArtTile_SpecialStageSolid + the edge pixel's colour), the others the blank tile (ArtTile_SpecialStageBlank)

#define SS_WIDE_MC 10   // the margin cells at each side that the track's geometry is carried into (80 pixels: what 16:9 needs)
#define SS_WIDE_ROWS 28 // the rows of the screen

typedef struct {
    int screen_width;
    int x0;           // where the track's 256 pixels begin on the screen
    int stride;       // cells of a stripe with its margins (32 when the screen is 256: a power of two otherwise)
    int left_cells;   // margin cells before the track's cells
    int plane_width;  // 4 * stride
} SSWide;

void SSWide_Layout(SSWide *wide, int screen_width);
// A row (the 128 words the decoder gives, `row` its number) into a row of the wide plane (wide->plane_width words): the margins are the cells that SSWide_BuildMargins makes
void SSWide_ExpandRow(const SSWide *wide, const uint16_t *unit, int row, uint16_t *out);
// The margins of what is shown (the plane's words and the line scroll's stripes): 2 * SS_WIDE_ROWS * SS_WIDE_MC tiles of 32 bytes, the left side's first, to be written to VRAM at ArtTile_SpecialStageMarginLeft
void SSWide_BuildMargins(const uint16_t *plane, const int16_t (*hscroll)[2], uint8_t *tiles);
#define SS_WIDE_MARGIN_BYTES (2 * SS_WIDE_ROWS * SS_WIDE_MC * 32)
// The line scroll for a stripe: the plane's X where the stripe's track begins is put at the screen's x0 (the unwide table's is -$100 times the stripe's number)
int16_t SSWide_ScrollA(const SSWide *wide, int stripe);
// A background's scroll with the track's 256 pixels put at x0
int16_t SSWide_ScrollB(const SSWide *wide, int16_t b);

#endif //_SPECIALSTAGEWIDE_H
