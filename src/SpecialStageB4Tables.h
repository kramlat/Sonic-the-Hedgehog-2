#ifndef _SPECIALSTAGEB4TABLES_H
#define _SPECIALSTAGEB4TABLES_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// The tables of Beta 4's special stage (docs/Sonic2SpecialStageB4.md, stage 2): three Kosinski files that the stage decodes into RAM ($FFFF6A60, $FFFF855C, $FFFF8778), here decoded the first time they are asked for. The
// track's segments (SpecialStageTables.h) and the lists of segments of a kind are the pre-beta's, byte for byte.

#define SS4_STAGE_COUNT 10 // (the seven stages, and the three of the bonus stars' entry: the same as stages 4 to 6)

// The kinds of step ($620A): how many segments of its list a step uses (the lists are SS_List 0 to 4 of SpecialStageTables.h)
#define SS4_KIND_COUNT 6
int SS4_KindLength(int kind);

// A stage's steps (ss_map_1: ten offsets, then a list of bytes for each stage): the low 7 bits are the kind of a step, the high bit says the track is turned over for it ($FFFFDB0E is set where a step has it and
// the track has come to the segments that turn it). The list is as long as the file has it (the objects' list says where the stage ends).
const uint8_t *SS4_StageSteps(int stage, size_t *count);

// A stage's objects (ss_map_2: ten offsets, then a list for each stage): for each step, pairs of bytes (a number: the low 6 bits are the place in the segment's list of positions, bit 6 says a bomb, and a byte of its
// own, `$26`) and then one byte that is $80 or more and ends the step: $FF just that, $FE and the rings' check ($5A) is made, $FD and the emerald's ($59), $FC and the palette flashes ($FFFFDB95)
const uint8_t *SS4_StageObjects(int stage, size_t *size);

// The places of the rings and bombs of a segment (ss_map_0: 56 offsets, then for each segment a count and 6 bytes for each place)
int SS4_PlaceCount(int segment);
const uint8_t *SS4_Place(int segment, int index); // the 6 bytes

#endif //_SPECIALSTAGEB4TABLES_H
