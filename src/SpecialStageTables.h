#ifndef _SPECIALSTAGETABLES_H
#define _SPECIALSTAGETABLES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// The tables of the pre-beta's special stage (stage 2 of docs/Sonic2SpecialStage.md): the data the stage's code reads, as it is in the ROM, with the accessors the later stages use. They are the binaries of res/SSData
// (tools/asm_dc_region.pl from the disassembly, the ROM offset in the name), not copies in C. All words are big-endian as the ROM has them; the accessors read them so.

typedef struct {
    const uint8_t *data;
    size_t size;
} SSBlob;

// The track's 56 segments (the table at $6D5A: a segment's number is the byte the stage's lists hold). A segment is three sections, each a 4-byte header whose last word is the length of what follows it (the stage's code
// at $5F62 takes the sections' places from those words): the data is what follows the header.
#define SS_SEGMENT_COUNT 56
const SSBlob *SS_Segment(int segment);
bool SS_SegmentSections(int segment, SSBlob sections[3]);

// How the stage runs through the segments. A stage is a sequence of steps (the bytes at $5F08); a step's list (the low 3 bits, the high bit is a flag the pre-beta's code reads) is one of 5 lists of segment numbers (the
// tables at $6CFC) and the segments of a list are given in the lengths of $5F02. The step $FF ends the sequence.
#define SS_LIST_COUNT 5
int SS_ListLength(int list);                           // how many segments the stage runs through for the list ($5F02)
const uint8_t *SS_List(int list, size_t *size);        // the list's bytes ($6CFC; may have more bytes than SS_ListLength says: the last list has a pad)
size_t SS_SequenceLength(void);                        // the steps up to the end ($FF)
uint8_t SS_SequenceStep(size_t step);                  // the byte of a step: its list in the low bits (& $7F), the flag in the high bit

// The tile tables of the track's drawing ($6E3A and $7302, which the code at $6CD8 and the others reads): the first is a table of tile words (priority and flip bits and the tile), the second of pairs (a tile and how
// many times it is repeated, less one)
size_t SS_TileWordCount(void);
uint16_t SS_TileWord(size_t index);
size_t SS_RunCount(void);
uint16_t SS_RunTile(size_t index);
uint16_t SS_RunRepeat(size_t index);

// What the track is where the characters are ($751A: a word for each of the 56 segments, the place of its block from the table's start): the block's bytes, and the place the next block (or the end) is
const uint8_t *SS_TrackBlock(int segment, size_t *size);

// The rings and bombs of a part of the stage ($309C8, a layout for each of the 13 parts: the pre-beta's SS_Load_Objects): a count, and for each how far along the track it is (the low 7 bits of its first byte), whether it
// is a bomb (the high bit) and its second byte (which place across the track)
#define SS_PART_COUNT 13
int SS_ObjectCount(int part);
bool SS_ObjectEntry(int part, int index, int *position, bool *bomb, uint8_t *lane);

// How long each segment stays on the screen (($11B8): 12 patterns of the frames each of the segments takes, one after another, for the speeds from 8 on: a pattern as long as twice the speed, give or take)
#define SS_PACE_PATTERNS 12
const uint8_t *SS_PacePattern(int pattern, size_t *size);

// The curves of the background that the track's turns make ($8E5C: 13 for the vertical scroll, $8FEC: 10 for the horizontal; each 5 entries, one for each of the frames a segment can last, 4 to 8): an entry's high byte and its low byte (an angle)
#define SS_VCURVES 13
#define SS_HCURVES 10
uint16_t SS_VCurveEntry(int curve, int entry);
uint16_t SS_HCurveEntry(int curve, int entry);

// The track's tiles (($DA672): the count and then a row of 8 pixels (a long) for each, the tile being that row 8 times) and the background's two Enigma maps ($DCBCA, $DCA9C)
const uint8_t *SS_TrackTiles(size_t *count);
const uint8_t *SS_BackgroundMap(int which, size_t *size);

#endif //_SPECIALSTAGETABLES_H
