#ifndef _SPECIALSTAGEDISPLAY_H
#define _SPECIALSTAGEDISPLAY_H

#include "SpecialStageTrack.h"

#include <stdbool.h>
#include <stdint.h>

// The pre-beta's special stage track as the screen shows it, stage 3b of docs/Sonic2SpecialStage.md: what the stage's code keeps in RAM ($FFFF9B00 on, the two plane buffers, the scroll buffers) and what it does with it
// each frame, so that the track can run with no characters. The numbers in the comments are the pre-beta's addresses.
//
// The picture is plane A, 128 cells wide and 32 high, in two buffers (the plane's address register is flipped between $8000 and $C000 as a segment's time is out): a row of it holds four 32-cell stripes, and the
// line scroll (a table of an A and a B X for each of 256 lines, that is 4 lines in 8 of each stripe, set up at $8C22) shows each pixel line from the stripe it belongs to. That is how the pre-beta draws the pipe.
// The background (plane B) is a 32 by 32 map of its own over the same width, moved by the curves of the track.

#define SS_PLANE_WIDTH  128
#define SS_PLANE_HEIGHT 32
#define SS_PLANE_WORDS  (SS_PLANE_WIDTH * SS_PLANE_HEIGHT)
#define SS_SCROLL_LINES 256

typedef struct {
    // the speed (the stage's word and fraction, 9B12 and 9B14) and what the pace has made of it (9B16, 9B18, 9B1F, 9B1E, 9B21)
    uint16_t speed, speed_frac;
    uint16_t current_speed, current_frac;
    uint8_t counter;       // 9B1F: the frames of this segment that are left
    uint8_t pattern_index; // 9B1E: where in the pattern of frame counts it is
    uint8_t period_less;   // 9B21: how many frames the segment lasts, less one

    SSTrack track;         // 9B0A, 9B0B, 9B08, 9B10 and 9B0E (the pre-beta's order of segments, and its flipped decoding)
    SSDecoder decoder;     // the stream places the stage keeps between its four calls
    int frame;             // 9B0D: the frame of the segment's time (4 or more: nothing is decoded)
    int bank;              // 9B0C: the buffer the next segment is decoded into
    int shown;             // the buffer plane A is at ($8000 or $C000)
    bool display_flipped;  // 9B0F: the scroll buffer for the turned track is the one shown
    bool prev_display_flipped; // 9B11
    int segment;           // 9B10: the current segment (-1: none yet)
    int list, index;       // 9B08, 9B09: the current segment's list and its place in it
    int32_t b_hscroll;     // 9B24: the background's X (16.16)
    int32_t b_vscroll;     // 9B30: its Y
    bool b4;               // Beta 4's stage: its pace (one period for the speed) and its scrolling of the background by byte tables
    bool locked;           // Beta 4's end has locked the pads: the vertical scroll of the third list stops (loc_7B48)

    uint16_t plane_a[2][SS_PLANE_WORDS];
    int16_t hscroll[2][SS_SCROLL_LINES][2]; // [0] the table at $FFFFE000, [1] the turned track's at $FFFF9700: each line's A and B X (the VDP's: how far the plane is moved, a negative number to show cells further on)
    uint16_t plane_b[SS_PLANE_WORDS];       // the background: the Enigma map ($DCBCA, tile numbers from $700) four times across
} SSDisplay;

void SSDisplay_Reset(SSDisplay *display);
// Beta 4's stage `stage`: the same display with Beta 4's order of segments, its pace ($FFFFDB12's word picks a period: $3C, $1E, $F, $A, 8, 6, 5 frames for the speeds 0, 2 .. $C) and its background scrolling (loc_7A38 and loc_7ADC)
void SSDisplay_ResetStage(SSDisplay *display, int stage);
// The pace (Offset_0x00112C): how many frames the segment now being shown lasts
int SSDisplay_Pace(SSDisplay *display);
// The blank's part of a frame (Offset_0x000F5C on): the pace, and the frame of the time on; when the time is out the plane is changed to the buffer decoded and the other is decoded next
void SSDisplay_VBlank(SSDisplay *display);
// The first half of Offset_0x005F16: the next slice of the track decoded (a new segment first, on the frame 0), and the turning
void SSDisplay_Decode(SSDisplay *display);
// Offset_0x008D8C: the background moved by the curves
void SSDisplay_Scroll(SSDisplay *display);
// What the stage does from its start until the track is there (Offset_0x005D8A to Offset_0x005DE2): the first segment decoded and the time at the end of its first period
void SSDisplay_Prime(SSDisplay *display);
// A frame of the stage's running (the blank and then the main loop's parts)
void SSDisplay_Frame(SSDisplay *display);

#endif //_SPECIALSTAGEDISPLAY_H
