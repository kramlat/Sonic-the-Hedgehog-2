#ifndef _SPECIALSTAGETRACK_H
#define _SPECIALSTAGETRACK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// The pre-beta's special stage track, stage 3a of docs/Sonic2SpecialStage.md: how a segment's data becomes the words of the track's name table, and how the stage goes from segment to segment. (How the words get
// to the screen, the scrolling and the palette are stage 3b.)
//
// A segment is three bit streams (SpecialStageTables.h), read most significant bit first: a stream of flags (1: a single tile, 0: a run), the tiles' stream (a bit: 1 a 10-bit number, 0 a 6-bit one; the number,
// plus $40 for the 10-bit ones, picks a word of the table at $6E3A) and the runs' stream (a bit: 1 a 7-bit number, 0 a 6-bit one; plus $40 for the 7-bit ones, a pair of the table at $7302: a tile and how many times
// it is written, less one). A byte $FF in the runs' stream where a byte begins, or the 7-bit number $7F, ends a "unit". The words are the name table's: $6000 (palette 3) is put on a tile's word and $E000 (palette 3,
// priority) on a run's. A segment is 28 units, each 128 words (4 rows of the plane's 32 cells): 3,584 words. The stage's code decodes 7 units a frame (a slice) for four frames, to four places of the name table.
// When the track is turned over (the pre-beta's flipped state) each unit's words go in the other order, as the code that writes backwards into RAM does, and a single tile's word is XORed with $6800 (not ORed with $6000): the tile is turned the other way across.

#define SS_TRACK_UNIT_WORDS   128
#define SS_TRACK_SLICE_UNITS  7
#define SS_TRACK_SLICES       4
#define SS_TRACK_SLICE_WORDS  (SS_TRACK_UNIT_WORDS * SS_TRACK_SLICE_UNITS)
#define SS_TRACK_SEGMENT_WORDS (SS_TRACK_SLICE_WORDS * SS_TRACK_SLICES)

typedef struct {
    const uint8_t *data;
    size_t size, pos;
    int left;          // the bits left in `current`
    uint8_t current;
} SSBits;

// A segment being decoded a slice at a time (what the stage's code keeps between its four calls)
typedef struct {
    SSBits flags, tiles, runs;
    int slice;         // the next slice to decode
    bool valid;
} SSDecoder;

bool SSDecoder_Start(SSDecoder *decoder, int segment);
// The next slice's 7 units into `out` (SS_TRACK_SLICE_WORDS words); false when it is done or the data is wrong (a unit that is not 128 words, a stream that runs out)
bool SSDecoder_Slice(SSDecoder *decoder, bool flipped, uint16_t *out);
// The whole segment (4 slices, SS_TRACK_SEGMENT_WORDS words)
bool SS_DecodeSegment(int segment, bool flipped, uint16_t *out);

// The order of the segments: the pre-beta's step (Obj 9B0A), place in the step's list (9B0B) and what it chooses (Offset_0x005F16's first half), and the flipped state (Offset_0x008B9E)
typedef struct {
    int step;          // the stage's sequence (SS_SequenceStep)
    int index;         // the place in the step's list
    int list;          // the list the current segment came from
    int segment;       // the current segment (-1: none yet)
    bool flipped;      // the track is turned over
} SSTrack;

void SSTrack_Reset(SSTrack *track);
// Whether the current segment is one that turns the track over or back ($11, $0D and $1A), and to which: the state it would have, which the display takes up at the end of the segment's time
bool SSTrack_Turns(const SSTrack *track, bool *flipped);
// The next segment of the stage (and the flipped state it brings)
int SSTrack_NextSegment(SSTrack *track);

#endif //_SPECIALSTAGETRACK_H
