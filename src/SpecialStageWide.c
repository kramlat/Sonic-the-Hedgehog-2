// The pre-beta's special stage track for a wide screen: see SpecialStageWide.h
#include "SpecialStageWide.h"
#include <math.h>
#include <string.h>
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
    bool lane;                   // a straight one (a lane's edge: a ray from V)
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

// ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
// The pipe's rings (one point perspective: the pipe's cross sections are circles about the vanishing point C, a ring at the depth z has the radius k / z, so along every ray from C a ring is at the same 1/r; rings the
// same distance apart in depth are the same step apart in 1/r). The boundaries between the colours are fitted with circles (each pair of colours its own); those that are circles (most of the pixels within a couple
// of pixels of it) give C. Along the rays from C through the edge column the last boundary before the edge (w0, in 1/r) and the one before it (w1) are where the last two rings are; beyond the edge the rings go
// on outward the same step apart (w1 - w0: "the same distance", the user), each the thin band (B, the colour at the edge: the green arcs where the next half ring of spheres is) and the wide one (A, the colour
// inside it) after it.

typedef struct {
    double cx, cy;     // C
    double w0, step;   // the last ring's 1/r and the step between rings
    double thin;       // the thin band's part of the step
    int a, b;          // the colours: A (between the rings) and B (the ring)
} Rings;

typedef struct { double n, sx, sy, sxx, syy, sxy, sxz, syz, sz, err; double cx, cy, r; } CircleSums;

static void AddCirclePoint(CircleSums *s, double x, double y) {
    const double z = x * x + y * y;
    s->n += 1;
    s->sx += x; s->sy += y; s->sxx += x * x; s->syy += y * y; s->sxy += x * y;
    s->sxz += x * z; s->syz += y * z; s->sz += z;
}

static double Det3(const double m[3][3]) {
    return m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) - m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0]) + m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
}

// the circle x x + y y + D x + E y + F = 0 nearest the points (least squares, by Cramer's rule)
static bool SolveCircle(CircleSums *s) {
    const double m[3][3] = {{s->sxx, s->sxy, s->sx}, {s->sxy, s->syy, s->sy}, {s->sx, s->sy, s->n}};
    const double v[3] = {-s->sxz, -s->syz, -s->sz};
    const double det = Det3(m);
    if (fabs(det) < 1e-6)
        return false;
    double sol[3];
    for (int i = 0; i < 3; i++) {
        double t[3][3];
        for (int r = 0; r < 3; r++)
            for (int c = 0; c < 3; c++)
                t[r][c] = c == i ? v[r] : m[r][c];
        sol[i] = Det3(t) / det;
    }
    s->cx = -sol[0] / 2;
    s->cy = -sol[1] / 2;
    const double rr = s->cx * s->cx + s->cy * s->cy - sol[2];
    if (rr <= 0)
        return false;
    s->r = sqrt(rr);
    return true;
}

static bool IsBoundary(const uint8_t img[SS_IMG_H][SS_IMG_W], int x, int y, int *pair, double *px, double *py) {
    const int c = img[y][x];
    if (x > 0 && img[y][x - 1] != c) {
        const int o = img[y][x - 1];
        *pair = c < o ? c * 16 + o : o * 16 + c;
        *px = x - 0.5;
        *py = y;
        return true;
    }
    if (y > 0 && img[y - 1][x] != c) {
        const int o = img[y - 1][x];
        *pair = c < o ? c * 16 + o : o * 16 + c;
        *px = x;
        *py = y - 0.5;
        return true;
    }
    return false;
}

// the colour runs along the ray from the edge column's point (0, ye) to C: the colour at the edge and where (the distance from C) each run inward begins; runs under 3 pixels are not counted (outlines)
static int RayRuns(const uint8_t img[SS_IMG_H][SS_IMG_W], double cx, double cy, double ye, int *colour, double *dist, int max) {
    const double dx = cx, dy = cy - ye, len = sqrt(dx * dx + dy * dy);
    int raw_c[512];
    double raw_d[512];
    int raw_len[512], m = 0;
    for (double t = 0; t < len - 2 && m < 512; t += 0.5) {
        const int x = (int)(t * dx / len), y = (int)(ye + t * dy / len);
        if (x < 0 || x >= SS_IMG_W || y < 0 || y >= SS_IMG_H)
            continue;
        const int c = img[y][x];
        if (m > 0 && raw_c[m - 1] == c) {
            raw_len[m - 1]++;
            continue;
        }
        raw_c[m] = c;
        raw_d[m] = len - t;
        raw_len[m] = 1;
        m++;
    }
    int n = 0;
    for (int i = 0; i < m && n < max; i++) {
        if (i > 0 && raw_len[i] < 6) // (under 3 pixels: an outline, not a ring)
            continue;
        if (n > 0 && colour[n - 1] == raw_c[i])
            continue;
        colour[n] = raw_c[i];
        dist[n] = raw_d[i];
        n++;
    }
    return n; // dist[i]: the distance from C where run i (from the edge inward) begins; dist[0] is the edge's
}

static bool FitRings(const uint8_t img[SS_IMG_H][SS_IMG_W], Rings *rings) {
    static CircleSums sums[256];
    memset(sums, 0, sizeof sums);
    int pair;
    double px, py;
    for (int y = 0; y < SS_IMG_H; y++)
        for (int x = 0; x < SS_IMG_W; x++)
            if (IsBoundary(img, x, y, &pair, &px, &py))
                AddCirclePoint(&sums[pair], px, py);
    bool fitted[256] = {false};
    for (int p = 0; p < 256; p++)
        fitted[p] = sums[p].n >= 80 && SolveCircle(&sums[p]) && sums[p].r > 24 && sums[p].r < 400;
    for (int y = 0; y < SS_IMG_H; y++) // how far the points are from their circle
        for (int x = 0; x < SS_IMG_W; x++)
            if (IsBoundary(img, x, y, &pair, &px, &py) && fitted[pair])
                sums[pair].err += fabs(sqrt((px - sums[pair].cx) * (px - sums[pair].cx) + (py - sums[pair].cy) * (py - sums[pair].cy)) - sums[pair].r);
    double wsum = 0, cx = 0, cy = 0;
    int good[256], ngood = 0;
    for (int p = 0; p < 256; p++)
        if (fitted[p] && sums[p].err / sums[p].n < 2.5 && sums[p].cx > 32 && sums[p].cx < 224 && sums[p].cy > -120 && sums[p].cy < 200)
            good[ngood++] = p;
    // a pipe: two circles or more about the same centre, of different sizes (one circle alone can be anything round)
    bool pipe = false;
    for (int i = 0; i < ngood; i++)
        for (int j = i + 1; j < ngood; j++)
            if (fabs(sums[good[i]].cx - sums[good[j]].cx) < 8 && fabs(sums[good[i]].cy - sums[good[j]].cy) < 8 && fabs(sums[good[i]].r - sums[good[j]].r) > 15)
                pipe = true;
    if (!pipe)
        return false;
    for (int i = 0; i < ngood; i++) {
        const int p = good[i];
        wsum += sums[p].n;
        cx += sums[p].cx * sums[p].n;
        cy += sums[p].cy * sums[p].n;
    }
    rings->cx = cx / wsum;
    rings->cy = cy / wsum;
    // the last two rings: along the rays from C through the edge column, the colour at the edge (B), the next run in (A) and where each begins; the pair most rays agree on, at the median of their 1/r
    static int votes[256];
    static double w0s[256][SS_IMG_H], w1s[256][SS_IMG_H];
    memset(votes, 0, sizeof votes);
    for (int ye = 0; ye < SS_IMG_H; ye += 2) {
        int colour[8];
        double dist[8];
        const int m = RayRuns(img, rings->cx, rings->cy, ye, colour, dist, 8);
        if (m < 3)
            continue;
        const int key = colour[1] * 16 + colour[0];
        w0s[key][votes[key]] = 1 / dist[1];
        w1s[key][votes[key]] = 1 / dist[2];
        votes[key]++;
    }
    int best = -1;
    for (int k = 0; k < 256; k++)
        if (votes[k] >= 4 && (best < 0 || votes[k] > votes[best]))
            best = k;
    if (best < 0)
        return false;
    for (int i = 1; i < votes[best]; i++) // (sorted, for the medians)
        for (int j = i; j > 0 && w0s[best][j] < w0s[best][j - 1]; j--) {
            const double t = w0s[best][j]; w0s[best][j] = w0s[best][j - 1]; w0s[best][j - 1] = t;
        }
    for (int i = 1; i < votes[best]; i++)
        for (int j = i; j > 0 && w1s[best][j] < w1s[best][j - 1]; j--) {
            const double t = w1s[best][j]; w1s[best][j] = w1s[best][j - 1]; w1s[best][j - 1] = t;
        }
    rings->w0 = w0s[best][votes[best] / 2];
    rings->step = w1s[best][votes[best] / 2] - rings->w0;
    rings->a = best / 16;
    rings->b = best % 16;
    rings->thin = 0.25; // (the user's drawing: the green arc is about a quarter of the step)
    return rings->step > 1e-4;
}

// The colour of the wall at (x, y) beyond the edge (x negative): its depth along its ray from C, the rings going on the same step apart
static int RingColour(const uint8_t img[SS_IMG_H][SS_IMG_W], const Rings *rings, double x, double y) {
    const double w = 1 / sqrt((x - rings->cx) * (x - rings->cx) + (y - rings->cy) * (y - rings->cy));
    const double ye = rings->cy + (y - rings->cy) * rings->cx / (rings->cx - x); // where its ray from C crosses the edge
    if (ye < 0 || ye > SS_IMG_H - 1)
        return -1;
    const int edge = img[(int)ye][0];
    if (edge != rings->a && edge != rings->b) // (the half pipe is not there: the sky, the rim, a lane)
        return -1;
    if (w >= rings->w0 + rings->step) // (deeper than the last two rings: what the ray has at the edge)
        return edge;
    if (w >= rings->w0)
        return rings->a;
    return fmod(rings->w0 - w, rings->step) < rings->thin * rings->step ? rings->b : rings->a;
}

// The pixels beyond the left edge: out[y][k] is the pixel k + 1 to the left of the picture
static void ExtendLeft(const uint8_t img[SS_IMG_H][SS_IMG_W], uint8_t out[SS_IMG_H][SS_WIDE_MC * 8]) {
    static Boundary bd[SS_MAX_BOUNDARIES];
    Rings ring_fit;
    const Rings *rings = FitRings(img, &ring_fit) ? &ring_fit : NULL;
    int ys[SS_MAX_BOUNDARIES], above[SS_MAX_BOUNDARIES], below[SS_MAX_BOUNDARIES];
    int n = ColumnBoundaries(img, 0, ys, above, below, SS_MAX_BOUNDARIES);
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
    bool have_v = false;
    {
        // where the straight boundaries cross: the least squares of vy = a + b vx over them (the point nearest all their lines); the middle of the screen on a straight track, up or down with the pitch, and to the side the track turns
        double sbb = 0, sb = 0, sab = 0, sa = 0;
        int cnt = 0;
        double smin = 9, smax = -9;
        for (int k = 0; k < n; k++) {
            bd[k].lane = !(bd[k].length < 24 || fabs(bd[k].c) > 0.004 || fabs(bd[k].b) < 0.1);
            if (!bd[k].lane)
                continue;
            sbb += bd[k].b * bd[k].b;
            sb += bd[k].b;
            sab += bd[k].a * bd[k].b;
            sa += bd[k].a;
            smin = bd[k].b < smin ? bd[k].b : smin;
            smax = bd[k].b > smax ? bd[k].b : smax;
            cnt++;
        }
        const double det = sbb * cnt - sb * sb;
        if (cnt >= 2 && det > 1e-3 && smax - smin > 0.15) {
            have_v = true;
            vx = (-sab * cnt + sb * sa) / det; // (Cramer: sbb vx - sb vy = -sab and -sb vx + cnt vy = sa)
            vy = (sbb * sa - sb * sab) / det;
        }
        vx = vx < 48 ? 48 : vx > 208 ? 208 : vx;
        vy = vy < -40 ? -40 : vy > 180 ? 180 : vy;
    }
    // The boundaries that are not at the edge column but a little way in (the thin outlines): each is a ray from V too, so where it would cross the edge column is where it starts, if no boundary there is its own already
    int total = n;
    const int seed_limit = n + 12; // (not many: a wrong vanishing point would fill the margin with stripes)
    for (int xs = 4; xs <= 48 && have_v && total < seed_limit && vx > 72 && vx < 184; xs += 4) {
        int sy[SS_MAX_BOUNDARIES], sa[SS_MAX_BOUNDARIES], sb[SS_MAX_BOUNDARIES];
        const int m = ColumnBoundaries(img, xs, sy, sa, sb, SS_MAX_BOUNDARIES);
        for (int i = 0; i < m && total < seed_limit; i++) {
            const double y0 = vy + (sy[i] - vy) * vx / (vx - xs);
            if (y0 < -60 || y0 > 290)
                continue;
            bool known = false;
            for (int k = 0; k < total; k++)
                if (bd[k].above == sa[i] && bd[k].below == sb[i] && fabs(bd[k].a - y0) <= 3)
                    known = true;
            if (known)
                continue;
            bd[total].above = sa[i];
            bd[total].below = sb[i];
            bd[total].a = y0;
            bd[total].length = 0;
            bd[total].lane = false;
            bd[total].b = bd[total].c = 0;
            total++;
        }
    }
    for (int i = 1; i < total; i++) // (by height, so that the bands are in order)
        for (int j = i; j > 0 && bd[j].a < bd[j - 1].a; j--) {
            const Boundary t = bd[j];
            bd[j] = bd[j - 1];
            bd[j - 1] = t;
        }
    n = total;
    for (int col = 0; col < SS_WIDE_MC * 8; col++) {
        const double x = -(col + 1);
        double pos[SS_MAX_BOUNDARIES];
        for (int k = 0; k < n; k++) // vector math: the line from the vanishing point V through the boundary's point at the track's edge (x = 0), carried out to x (negative: past the edge)
            pos[k] = bd[k].a + ((vy - bd[k].a) / vx) * x;
        // the floor: below the first lane ray under the rings' centre (the lanes run on the floor; above them is the pipe's wall, which is the rings')
        double floor_top = 1e9;
        for (int k = 0; k < n && rings; k++)
            if (bd[k].lane && bd[k].a > rings->cy) {
                floor_top = pos[k];
                break;
            }
        for (int y = 0; y < SS_IMG_H; y++) {
            const int ring = rings != NULL && y < floor_top ? RingColour(img, rings, x, y) : -1;
            if (ring >= 0) { // the wall: the ring the point is on, by its depth
                out[y][col] = (uint8_t)ring;
                continue;
            }
            int colour = top_colour; // the colour of the band the point is in (rays from one point keep their order beyond the edge)
            for (int k = 0; k < n && y >= pos[k]; k++)
                colour = bd[k].below;
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
