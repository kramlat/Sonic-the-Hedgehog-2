// The pre-beta's special stage track as the screen shows it: see SpecialStageDisplay.h
#include "SpecialStageDisplay.h"
#include "SpecialStageTables.h"

#include "Enigma.h"
#include "MathUtil.h"

#include <string.h>

// Offset_0x008C22: the scroll buffers. A line of the first buffer has the stripe of its line group: 0, -$100, -$200, -$300 for two lines each, over and over; the buffer of the turned track has them the other way round
static void ScrollBuffers(SSDisplay *d) {
    for (int line = 0; line < SS_SCROLL_LINES; line++) {
        const int group = (line / 2) % 4;
        d->hscroll[0][line][0] = (int16_t)(-0x100 * group);
        d->hscroll[0][line][1] = 0;
        d->hscroll[1][line][0] = (int16_t)(-0x100 * (3 - group));
        d->hscroll[1][line][1] = 0;
    }
}

void SSDisplay_Reset(SSDisplay *d) {
    memset(d, 0, sizeof(*d));
    d->speed = 0x000C; // (move.l #$000C0000, 9B12)
    SSTrack_Reset(&d->track);
    d->segment = -1;
    ScrollBuffers(d);
    // the background: the Enigma map (tiles from $700) four times across the plane (Offset_0x008C9C)
    size_t size;
    const uint8_t *map = SS_BackgroundMap(0, &size);
    uint8_t bytes[32 * 32 * 2 + 64];
    memset(bytes, 0, sizeof(bytes));
    EniDec(map, bytes, 0x700); // (the top half; the second map, Offset_0x0DCA9C, is decoded  bytes on: the bottom half, 32 by 16 cells each)
    size_t size2;
    const uint8_t *map2 = SS_BackgroundMap(1, &size2);
    EniDec(map2, bytes + 0x400, 0x700);
    uint16_t block[32 * 32];
    for (int i = 0; i < 32 * 32; i++)
        block[i] = (uint16_t)((bytes[i * 2] << 8) | bytes[i * 2 + 1]);
    for (int row = 0; row < 32; row++)
        for (int column = 0; column < SS_PLANE_WIDTH; column++)
            d->plane_b[row * SS_PLANE_WIDTH + column] = block[row * 32 + (column % 32)];
}

void SSDisplay_ResetStage(SSDisplay *d, int stage) {
    SSDisplay_Reset(d);
    d->b4 = true;
    SSTrack_ResetStage(&d->track, stage);
}

// Offset_0x00112C
int SSDisplay_Pace(SSDisplay *d) {
    if (d->current_speed != d->speed) { // (the speed changed: the pattern starts again)
        d->current_speed = d->speed;
        d->current_frac = d->speed_frac;
        d->counter = 0;
        d->pattern_index = 0;
    }
    d->counter--;
    if ((int8_t)d->counter > 0)
        return d->period_less + 1;
    if (d->b4) { // loc_11BE: the speed picks the period
        static const uint8_t periods[8] = { 0x3C, 0x1E, 0x0F, 0x0A, 0x08, 0x06, 0x05, 0x00 };
        const int period = periods[(d->current_speed >> 1) & 7];
        d->counter = (uint8_t)period;
        d->period_less = (uint8_t)(period - 1);
        return period;
    }
    int pattern = (int)d->current_speed - 8;
    if (pattern > 4) {
        pattern += 4;
    } else {
        if (pattern < 0)
            pattern = 0; // (the stage's speeds are 8 and over)
        pattern = pattern * 2 + ((d->current_frac >> 15) & 1);
    }
    if (pattern >= SS_PACE_PATTERNS)
        pattern = SS_PACE_PATTERNS - 1; // (the stage's speeds stop at $F)
    const uint8_t step = d->pattern_index++;
    // the pattern goes round after twice the speed (and one more when the fraction is half or more)
    const uint32_t doubled = (((uint32_t)d->current_speed << 16) | d->current_frac) << 1;
    if ((uint8_t)(doubled >> 16) == d->pattern_index)
        d->pattern_index = 0;
    size_t size;
    const uint8_t *frames = SS_PacePattern(pattern, &size);
    const int period = step < size ? frames[step] : frames[0];
    d->counter = (uint8_t)period;
    d->period_less = (uint8_t)(period - 1);
    return period;
}

void SSDisplay_VBlank(SSDisplay *d) {
    const int period = SSDisplay_Pace(d);
    d->frame++;
    if (d->frame >= period) {
        d->frame = 0;
        d->shown = d->bank; // (the plane's address register: $8230 for the buffer at $C000, $8220 for $8000)
        d->bank ^= 1;
    }
}

// Offset_0x008B9E: three segments turn the track over or back; the scroll buffer for the turned track is put on when the segment's time is nearly out
static void Turn(SSDisplay *d) {
    d->prev_display_flipped = d->display_flipped;
    bool flipped;
    if (SSTrack_Turns(&d->track, &flipped) && d->frame >= d->period_less)
        d->display_flipped = flipped;
}

void SSDisplay_Decode(SSDisplay *d) {
    if (d->frame >= SS_TRACK_SLICES) {
        Turn(d);
        return;
    }
    if (d->frame == 0) { // (a new segment: the step and the place in its list go on)
        const int before = d->track.index;
        d->segment = SSTrack_NextSegment(&d->track);
        d->list = d->track.list;
        d->index = before;
        SSDecoder_Start(&d->decoder, d->segment);
    }
    Turn(d);
    SSDecoder_Slice(&d->decoder, d->track.flipped, &d->plane_a[d->bank][d->frame * SS_TRACK_SLICE_WORDS]);
}

// A curve's step (Offset_0x008E22 and Offset_0x008F52): the entry's high byte and the sine of its low byte, as a number of 8 more bits
static int32_t CurveStep(uint16_t entry) {
    int16_t sin, cos;
    CalcSine((uint8_t)(entry & 0xFF), &sin, &cos);
    const uint16_t word = (uint16_t)((entry & 0xFF00) | (uint16_t)sin);
    return (int32_t)((uint32_t)word << 8);
}

static int CurveEntry(const SSDisplay *d) { // (the entry for how many frames the segment lasts: 4 to 8; 9B21 less 3)
    int entry = (int)d->period_less - 3;
    return entry < 0 ? 0 : entry > 4 ? 4 : entry;
}

// Offset_0x008EF8: the background's X
static void ScrollBackgroundX(SSDisplay *d) {
    int curve;
    switch (d->list) {
    case 0:
        if (d->index < 6) curve = 0;
        else if (d->index < 0x0B) curve = 1;
        else if (d->index < 0x0C) curve = 2;
        else return;
        break;
    case 1:
        if (d->index < 0x0C) curve = 0;
        else return;
        break;
    case 2:
        if (d->index < 3) curve = 0;
        else return;
        break;
    default:
        return;
    }
    const int32_t step = CurveStep(SS_HCurveEntry(curve, CurveEntry(d)));
    if (d->display_flipped)
        d->b_hscroll += step;
    else
        d->b_hscroll -= step;
    const int16_t x = (int16_t)(d->b_hscroll >> 16);
    for (int line = 0; line < SS_SCROLL_LINES; line++)
        d->hscroll[d->display_flipped ? 1 : 0][line][1] = x;
}

// Offset_0x008D96: the background's Y
static void ScrollBackgroundY(SSDisplay *d) {
    int curve;
    bool add = false;
    switch (d->list) {
    case 0:
        if (d->index < 0x0C) return;
        if (d->index < 0x0D) { curve = 0; }
        else if (d->index < 0x11) { curve = 1; }
        else if (d->index < 0x12) { curve = 2; }
        else { add = true; curve = d->index < 0x16 ? 3 : 4; } // (D7 is set from $12 on)
        break;
    case 1:
        if (d->index < 0x0C) return;
        add = true;
        if (d->index < 0x0D) { curve = 0; }
        else if (d->index < 0x0E) { curve = 5; }
        else if (d->index < 0x12) { curve = 6; }
        else { add = false; curve = 5; } // (the pre-beta compares D1 where it means D2 here: always this one)
        break;
    default:
        return;
    }
    const int32_t step = CurveStep(SS_VCurveEntry(curve, CurveEntry(d)));
    if (add)
        d->b_vscroll += step;
    else
        d->b_vscroll -= step;
}

// Beta 4's curves (loc_79EA: 11 of 5 bytes, one for each frame of a segment's time) and the lists of which curve a place in a segment's list uses
static const uint8_t b4_curves[11][5] = {
    { 2, 2, 2, 2, 2 }, { 4, 4, 5, 4, 5 }, { 0xB, 0xB, 0xB, 0xB, 0xC }, { 0, 0, 1, 0, 0 }, { 1, 1, 1, 1, 1 }, { 9, 9, 8, 9, 9 },
    { 9, 9, 9, 9, 0xA }, { 7, 7, 6, 7, 7 }, { 0, 1, 1, 1, 0 }, { 4, 3, 3, 3, 4 }, { 0, 0, 0xFF, 0, 0 },
};

// loc_7A38: the background's X: the same curves for the first three lists by the place in the list; every line of the buffer in use goes by it (the turned track's buffer the other way)
static void ScrollBackgroundXB4(SSDisplay *d) {
    if (d->list > 2)
        return;
    int curve;
    if (d->index < 1) curve = 0;
    else if (d->index < 2) curve = 1;
    else if (d->index < 0x0A) curve = 2;
    else if (d->index < 0x0B) curve = 1;
    else if (d->index < 0x0C) curve = 0;
    else return;
    int step = b4_curves[curve][d->frame < 5 ? d->frame : 4];
    if (d->display_flipped)
        step = -step;
    for (int line = 0; line < SS_SCROLL_LINES; line++)
        d->hscroll[d->display_flipped ? 1 : 0][line][1] = (int16_t)(d->hscroll[d->display_flipped ? 1 : 0][line][1] - step);
}

// loc_7ADC: the background's Y: the list the segment is of and its place give a curve (or none), which is taken from the Y (the second list adds)
static void ScrollBackgroundYB4(SSDisplay *d) {
    static const int8_t first[24] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 4, 4, 1, 2, 2, 2, 2, 2, 2, 5, 6, 7, 9, 8 };
    static const int8_t second[24] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 8, 9, 7, 6, 5, 2, 2, 2, 2, 2, 2, 1, 0 };
    static const int8_t fourth[16] = { 3, 3, 10, 10, 3, 3, 10, 10, 3, 3, 10, 10, 3, 3, 10, 10 };
    int curve = -1;
    bool add = false;
    const int i = d->index;
    switch (d->list) {
    case 0: curve = i < 24 ? first[i] : -1; break;
    case 1: add = true; curve = i < 24 ? second[i] : -1; break;
    case 3: curve = i < 16 ? fourth[i] : -1; break;
    default: break;
    }
    if (curve < 0)
        return;
    const int step = b4_curves[curve][d->frame < 5 ? d->frame : 4];
    d->b_vscroll += (int32_t)(add ? step : -step) * 65536;
}

void SSDisplay_Scroll(SSDisplay *d) {
    if (d->b4) {
        ScrollBackgroundXB4(d);
        ScrollBackgroundYB4(d);
        return;
    }
    ScrollBackgroundX(d);
    ScrollBackgroundY(d);
}

void SSDisplay_Prime(SSDisplay *d) {
    do { // (Offset_0x005D8A: until the time is at its start)
        SSDisplay_VBlank(d);
    } while (d->frame != 0);
    SSDisplay_Decode(d);
    do { // (Offset_0x005D9E: until the first segment's time is nearly out)
        SSDisplay_VBlank(d);
        SSDisplay_Decode(d);
    } while ((int)d->counter - 1 != 0);
    SSDisplay_Scroll(d);
}

void SSDisplay_Frame(SSDisplay *d) {
    SSDisplay_VBlank(d);
    SSDisplay_Decode(d);
    SSDisplay_Scroll(d);
}
