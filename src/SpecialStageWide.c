// The pre-beta's special stage track for a wide screen: see SpecialStageWide.h
#include "SpecialStageWide.h"
#include <math.h>
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


// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// The track's own geometry beyond its 256 pixels (the user's example of the final in widescreen: the walls' stripes go on rising to the top corners). The picture of the track as it is shown (the words of the shown plane
// and the line scroll: 256 by 224 pixels of colours) is put together. In the column at the edge the colours run in bands, with a boundary between two bands; each boundary is followed inward, column by column, as far as
// it can be (the nearest boundary between the same two colours, a few lines away at most), and a curve of the second degree is fitted to where it was; the margin's columns have the same bands in the same order, each
// boundary where its curve is, one that has gone off the screen gone. The margin is made of patterns of its own (a tile for each row and each cell of it, at each side), written to VRAM every frame, and the margins of
// the name table (SSWide_ExpandRow) point at them.

#define SS_IMG_W 256
#define SS_IMG_H 224
#define SS_TRACK_COLUMNS 96 // how far in a boundary is followed
#define SS_MAX_BOUNDARIES 64
#define SS_CURVE_COLUMNS 32 // how far out the fitted curve is followed before it goes on straight
#define SS_THIN_BAND 6        // a band this thin (lines) at the edge keeps its thickness in the margin
#define SS_MAX_SLOPE 0.9  // the most a boundary rises a column at the track's edge (the example's rim rises about 0.7)
#define SS_MAX_BEND 0.006 // and the most its slope grows a column

static void ComposeImage(const uint16_t *plane, const int16_t (*hscroll)[2], uint8_t img[SS_IMG_H][SS_IMG_W]) {
    size_t count;
    const uint8_t *rows = SS_TrackTiles(&count);
    for (int y = 0; y < SS_IMG_H; y++) {
        const int stripe = ((-hscroll[y][0]) / 0x100) & 3;
        const uint16_t *cells = plane + (y / 8) * 128 + stripe * 32;
        for (int c = 0; c < 32; c++) {
            const uint16_t word = cells[c];
            const size_t tile = word & 0x07FF;
            const uint8_t *r = tile < count ? rows + tile * 4 : NULL;
            for (int p = 0; p < 8; p++) {
                const int q = (word & 0x0800) ? 7 - p : p;
                img[y][c * 8 + p] = r == NULL ? 0 : (uint8_t)((q & 1) ? (r[q / 2] & 0xF) : (r[q / 2] >> 4));
            }
        }
    }
}

typedef struct {
    int above, below;            // the colours either side (a boundary is between the line y - 1 and the line y)
    double y[SS_TRACK_COLUMNS];  // where it was in each column going in (column 0 is the edge's)
    int length;                  // how many columns it was followed
    double a, b, c;              // y(x) = a + b x + c x x, x going in; the margin is at negative x
} Boundary;

// The boundaries of one column, from the top
static int ColumnBoundaries(const uint8_t img[SS_IMG_H][SS_IMG_W], int x, int *ys, int *above, int *below, int max) {
    int n = 0;
    for (int y = 1; y < SS_IMG_H && n < max; y++)
        if (img[y - 1][x] != img[y][x]) {
            ys[n] = y;
            above[n] = img[y - 1][x];
            below[n] = img[y][x];
            n++;
        }
    return n;
}

static void FitBoundary(Boundary *bd) {
    const int n = bd->length;
    if (n < 6) { // (too short to say: it stays level)
        bd->a = bd->y[0];
        bd->b = bd->c = 0;
        return;
    }
    // least squares of y = a + b x + c x x over x = 0..n-1 (the normal equations, solved by Cramer's rule)
    double s0 = n, s1 = 0, s2 = 0, s3 = 0, s4 = 0, t0 = 0, t1 = 0, t2 = 0;
    for (int x = 0; x < n; x++) {
        const double xd = x, v = bd->y[x];
        s1 += xd; s2 += xd * xd; s3 += xd * xd * xd; s4 += xd * xd * xd * xd;
        t0 += v; t1 += xd * v; t2 += xd * xd * v;
    }
    const double det = s0 * (s2 * s4 - s3 * s3) - s1 * (s1 * s4 - s3 * s2) + s2 * (s1 * s3 - s2 * s2);
    if (det > -1e-9 && det < 1e-9) {
        bd->a = bd->y[0];
        bd->b = bd->c = 0;
        return;
    }
    bd->a = (t0 * (s2 * s4 - s3 * s3) - s1 * (t1 * s4 - s3 * t2) + s2 * (t1 * s3 - s2 * t2)) / det;
    bd->b = (s0 * (t1 * s4 - s3 * t2) - t0 * (s1 * s4 - s3 * s2) + s2 * (s1 * t2 - t1 * s2)) / det;
    bd->c = (s0 * (s2 * t2 - t1 * s3) - s1 * (s1 * t2 - t1 * s2) + t0 * (s1 * s3 - s2 * s2)) / det;
}

// The pixels beyond the left edge: out[y][k] is the pixel k + 1 to the left of the picture
static void ExtendLeft(const uint8_t img[SS_IMG_H][SS_IMG_W], uint8_t out[SS_IMG_H][SS_WIDE_MC * 8]) {
    static Boundary bd[SS_MAX_BOUNDARIES];
    int ys[SS_MAX_BOUNDARIES], above[SS_MAX_BOUNDARIES], below[SS_MAX_BOUNDARIES];
    const int n = ColumnBoundaries(img, 0, ys, above, below, SS_MAX_BOUNDARIES);
    const int top_colour = img[0][0];
    for (int k = 0; k < n; k++) { // follow each boundary inward
        bd[k].above = above[k];
        bd[k].below = below[k];
        bd[k].y[0] = ys[k];
        bd[k].length = 1;
        double last = ys[k];
        for (int x = 1; x < SS_TRACK_COLUMNS; x++) {
            int best = -1, best_d = 1000;
            for (int y = 1; y < SS_IMG_H; y++)
                if (img[y - 1][x] == bd[k].above && img[y][x] == bd[k].below) {
                    const int d = (int)(y > last ? y - last : last - y);
                    if (d < best_d) {
                        best_d = d;
                        best = y;
                    }
                }
            if (best < 0 || best_d > 5)
                break;
            bd[k].y[x] = best;
            bd[k].length = x + 1;
            last = best;
        }
        FitBoundary(&bd[k]);
    }
    // The vanishing point: the nearly straight boundaries are lines through it (the pipe's lanes), so where they meet is it; the middle of the screen, nudged the way the track turns. Those boundaries go on as lines through it
    double vx = 128, vy = 100;
    {
        double at[SS_MAX_BOUNDARIES]; // where each straight boundary is at the middle column: they meet at the vanishing point, so the median is its height (it follows the pitch: up when the track pitches up)
        int cnt = 0;
        for (int k = 0; k < n; k++) {
            if (bd[k].length < 24 || fabs(bd[k].c) > 0.004 || fabs(bd[k].b) < 0.1)
                continue;
            at[cnt++] = bd[k].a + bd[k].b * 128;
        }
        if (cnt >= 2) {
            for (int i = 1; i < cnt; i++) // (a median: sorted by insertion)
                for (int j = i; j > 0 && at[j] < at[j - 1]; j--) {
                    const double t = at[j];
                    at[j] = at[j - 1];
                    at[j - 1] = t;
                }
            vy = at[cnt / 2];
        }
        vy = vy < -40 ? -40 : vy > 180 ? 180 : vy;
    }
    bool radial[SS_MAX_BOUNDARIES];
    for (int k = 0; k < n; k++) {
        const double m_rad = (vy - bd[k].a) / (vx - 0);
        radial[k] = bd[k].length >= 24 && fabs(bd[k].c) <= 0.004 && fabs(bd[k].b - m_rad) <= 0.15;
    }
    for (int col = 0; col < SS_WIDE_MC * 8; col++) {
        const double x = -(col + 1);
        double pos[SS_MAX_BOUNDARIES];
        for (int k = 0; k < n; k++) {
            // (the curve acts for the first SS_CURVE_COLUMNS columns of the margin and goes on along its tangent after them: a turn's bend does not run away)
            // (and the pipe's walls rise toward its sides: a boundary never goes down going out, so a turn's fit that would warp it down is flattened: the slope and the bend are not less than nothing)
            if (radial[k]) { // (a line through the vanishing point)
                pos[k] = bd[k].a + ((vy - bd[k].a) / vx) * x;
                continue;
            }
            const double b = bd[k].b < 0 ? 0 : bd[k].b > SS_MAX_SLOPE ? SS_MAX_SLOPE : bd[k].b;
            const double c = bd[k].c < 0 ? 0 : bd[k].c > SS_MAX_BEND ? SS_MAX_BEND : bd[k].c;
            const double xc = x < -SS_CURVE_COLUMNS ? -SS_CURVE_COLUMNS : x;
            double v = bd[k].a + b * xc + c * xc * xc;
            if (x < xc)
                v += (b + 2 * c * xc) * (x - xc);
            if (v > bd[k].a)
                v = bd[k].a;
            v = v < -400 ? -400 : v > 700 ? 700 : v;
            pos[k] = v;
        }
        for (int k = 1; k < n; k++) { // (a thin band keeps its boundaries side by side, and gets thicker going out as fast as it did going in: the rim is seen more slantwise toward the sides)
            const double gap = bd[k].a - bd[k - 1].a;
            if (gap < 0 || gap > SS_THIN_BAND)
                continue;
            const int shared = bd[k].length < bd[k - 1].length ? bd[k].length : bd[k - 1].length;
            double grow = 0.2; // (a thin band is lost after a few columns: it is taken to widen a fifth of a line a column, as the rim does in the example)
            if (shared >= 8) {
                grow = (gap - (bd[k].y[shared - 1] - bd[k - 1].y[shared - 1])) / (shared - 1);
                grow = grow < 0 ? 0 : grow > 0.25 ? 0.25 : grow;
            }
            double thick = gap + grow * (-x);
            thick = thick > 24 ? 24 : thick;
            pos[k] = pos[k - 1] + thick;
        }
        for (int y = 0; y < SS_IMG_H; y++) { // the colour of the band the line is in: the bands keep their order (a boundary that has passed another is held at it)
            int colour = top_colour;
            double floor_y = -1000;
            for (int k = 0; k < n; k++) {
                const double p = pos[k] < floor_y ? floor_y : pos[k];
                floor_y = p;
                if (y >= p)
                    colour = bd[k].below;
                else
                    break;
            }
            out[y][col] = (uint8_t)colour;
        }
    }
}

static void PutTiles(uint8_t *tiles, const uint8_t out[SS_IMG_H][SS_WIDE_MC * 8], bool right) {
    for (int r = 0; r < SS_WIDE_ROWS; r++)
        for (int j = 0; j < SS_WIDE_MC; j++) {
            uint8_t *t = tiles + ((size_t)r * SS_WIDE_MC + j) * 32;
            for (int line = 0; line < 8; line++) {
                const uint8_t *src = out[r * 8 + line];
                for (int p = 0; p < 8; p += 2) {
                    const int k0 = right ? j * 8 + p : j * 8 + (7 - p), k1 = right ? j * 8 + p + 1 : j * 8 + (7 - (p + 1));
                    t[line * 4 + p / 2] = (uint8_t)((src[k0] << 4) | src[k1]);
                }
            }
        }
}

void SSWide_BuildMargins(const uint16_t *plane, const int16_t (*hscroll)[2], uint8_t *tiles) {
    static uint8_t img[SS_IMG_H][SS_IMG_W], mirrored[SS_IMG_H][SS_IMG_W], out[SS_IMG_H][SS_WIDE_MC * 8];
    ComposeImage(plane, hscroll, img);
    for (int y = 0; y < SS_IMG_H; y++)
        for (int x = 0; x < SS_IMG_W; x++)
            mirrored[y][x] = img[y][SS_IMG_W - 1 - x];
    ExtendLeft(img, out);
    PutTiles(tiles, out, false);
    ExtendLeft(mirrored, out);
    PutTiles(tiles + (size_t)SS_WIDE_ROWS * SS_WIDE_MC * 32, out, true);
}

void SSWide_ExpandRow(const SSWide *wide, const uint16_t *unit, int row, uint16_t *out) {
    for (int w = 0; w < 4; w++) {
        const uint16_t *core = unit + w * 32;
        uint16_t *to = out + w * wide->stride;
        const uint16_t blank = 0x8000 | ArtTile_SpecialStageBlank;
        for (int c = 0; c < wide->left_cells; c++) {
            const int j = wide->left_cells - 1 - c;
            to[c] = (row < SS_WIDE_ROWS && j < SS_WIDE_MC) ? (uint16_t)(0xE000 | (ArtTile_SpecialStageMarginLeft + row * SS_WIDE_MC + j)) : blank;
        }
        for (int c = 0; c < 32; c++)
            to[wide->left_cells + c] = core[c];
        for (int c = wide->left_cells + 32; c < wide->stride; c++) {
            const int j = c - wide->left_cells - 32;
            to[c] = (row < SS_WIDE_ROWS && j < SS_WIDE_MC) ? (uint16_t)(0xE000 | (ArtTile_SpecialStageMarginRight + row * SS_WIDE_MC + j)) : blank;
        }
    }
}

int16_t SSWide_ScrollA(const SSWide *wide, int stripe) {
    return (int16_t)(wide->x0 - (stripe * wide->stride + wide->left_cells) * 8);
}

int16_t SSWide_ScrollB(const SSWide *wide, int16_t b) {
    return (int16_t)(b + wide->x0);
}
