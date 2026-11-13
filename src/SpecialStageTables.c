// The tables of the pre-beta's special stage: see SpecialStageTables.h. This file is the one that includes the data (a resource header defines it).
#include "SpecialStageTables.h"

#include "Resource/SSData/SegmentLists.h"
#include "Resource/SSData/CharacterArtScript.h"
#include "Resource/SSData/HorizontalCurves.h"
#include "Resource/SSData/VerticalCurves.h"
#include "Resource/SSData/PacePatterns.h"
#include "Resource/SSData/TileWords.h"
#include "Resource/SSData/RunTiles.h"
#include "Resource/SSData/TrackBlocks.h"
#include "Resource/SSData/ObjectLayouts.h"
#include "Resource/SSData/Segment00.h"
#include "Resource/SSData/Segment01.h"
#include "Resource/SSData/Segment02.h"
#include "Resource/SSData/Segment03.h"
#include "Resource/SSData/Segment04.h"
#include "Resource/SSData/Segment05.h"
#include "Resource/SSData/Segment06.h"
#include "Resource/SSData/Segment07.h"
#include "Resource/SSData/Segment08.h"
#include "Resource/SSData/Segment09.h"
#include "Resource/SSData/Segment0A.h"
#include "Resource/SSData/Segment0B.h"
#include "Resource/SSData/Segment0C.h"
#include "Resource/SSData/Segment0D.h"
#include "Resource/SSData/Segment0E.h"
#include "Resource/SSData/Segment0F.h"
#include "Resource/SSData/Segment10.h"
#include "Resource/SSData/Segment11.h"
#include "Resource/SSData/Segment12.h"
#include "Resource/SSData/Segment13.h"
#include "Resource/SSData/Segment14.h"
#include "Resource/SSData/Segment15.h"
#include "Resource/SSData/Segment16.h"
#include "Resource/SSData/Segment17.h"
#include "Resource/SSData/Segment18.h"
#include "Resource/SSData/Segment19.h"
#include "Resource/SSData/Segment1A.h"
#include "Resource/SSData/Segment1B.h"
#include "Resource/SSData/Segment1C.h"
#include "Resource/SSData/Segment1D.h"
#include "Resource/SSData/Segment1E.h"
#include "Resource/SSData/Segment1F.h"
#include "Resource/SSData/Segment20.h"
#include "Resource/SSData/Segment21.h"
#include "Resource/SSData/Segment22.h"
#include "Resource/SSData/Segment23.h"
#include "Resource/SSData/Segment24.h"
#include "Resource/SSData/Segment25.h"
#include "Resource/SSData/Segment26.h"
#include "Resource/SSData/Segment27.h"
#include "Resource/SSData/Segment28.h"
#include "Resource/SSData/Segment29.h"
#include "Resource/SSData/Segment2A.h"
#include "Resource/SSData/Segment2B.h"
#include "Resource/SSData/Segment2C.h"
#include "Resource/SSData/Segment2D.h"
#include "Resource/SSData/Segment2E.h"
#include "Resource/SSData/Segment2F.h"
#include "Resource/SSData/Segment30.h"
#include "Resource/SSData/Segment31.h"
#include "Resource/SSData/Segment32.h"
#include "Resource/SSData/Segment33.h"
#include "Resource/SSData/Segment34.h"
#include "Resource/SSData/Segment35.h"
#include "Resource/SSData/Segment36.h"
#include "Resource/SSData/Segment37.h"
#include "Resource/SSData/TrackTiles.h"
#include "Resource/SSData/BackgroundMapBottom.h"
#include "Resource/SSData/BackgroundMapTop.h"

// (paradoxmake puts 8 zero bytes after every resource: a decoder that reads a little past the end finds zeros)
#define RESOURCE_PADDING 8
#define RES_SIZE(array) (sizeof(array) - RESOURCE_PADDING)

#define SEGMENT(n) { SSData_##n, RES_SIZE(SSData_##n) },
static const SSBlob segments[SS_SEGMENT_COUNT] = {
#include "SpecialStageSegments.inc"
};
#undef SEGMENT

static uint16_t Word(const uint8_t *p) {
    return (uint16_t)((p[0] << 8) | p[1]);
}

const SSBlob *SS_Segment(int segment) {
    return (segment >= 0 && segment < SS_SEGMENT_COUNT) ? &segments[segment] : NULL;
}

bool SS_SegmentSections(int segment, SSBlob sections[3]) {
    const SSBlob *blob = SS_Segment(segment);
    if (blob == NULL)
        return false;
    size_t at = 0;
    for (int i = 0; i < 3; i++) {
        if (at + 4 > blob->size)
            return false;
        const size_t length = Word(blob->data + at + 2);
        if (at + 4 + length > blob->size)
            return false;
        sections[i].data = blob->data + at + 4;
        sections[i].size = length;
        at += 4 + length;
    }
    return true;
}

// $5F02: how many segments each list gives; and the steps of the sequence ($5F08)
static const uint8_t list_lengths[6] = { 0x18, 0x18, 0x0C, 0x10, 0x07, 0x00 };
static const uint8_t sequence[14] = { 3, 3, 3, 3, 4, 0, 1, 0x80, 2, 0x83, 4, 0, 0x81, 0xFF };

int SS_ListLength(int list) {
    return (list >= 0 && list < 6) ? list_lengths[list] : 0;
}

const uint8_t *SS_List(int list, size_t *size) {
    if (list < 0 || list >= SS_LIST_COUNT) {
        *size = 0;
        return NULL;
    }
    const uint8_t *table = SSData_SegmentLists;
    const size_t start = Word(table + list * 2);
    const size_t end = (list + 1 < SS_LIST_COUNT) ? Word(table + (list + 1) * 2) : RES_SIZE(SSData_SegmentLists);
    *size = end - start;
    return table + start;
}

size_t SS_SequenceLength(void) {
    size_t n = 0;
    while (n < sizeof(sequence) && sequence[n] != 0xFF)
        n++;
    return n;
}

uint8_t SS_SequenceStep(size_t step) {
    return step < sizeof(sequence) ? sequence[step] : 0xFF;
}

size_t SS_TileWordCount(void) { return RES_SIZE(SSData_TileWords) / 2; }
uint16_t SS_TileWord(size_t index) { return Word(SSData_TileWords + index * 2); }
size_t SS_RunCount(void) { return RES_SIZE(SSData_RunTiles) / 4; }
uint16_t SS_RunTile(size_t index) { return Word(SSData_RunTiles + index * 4); }
uint16_t SS_RunRepeat(size_t index) { return Word(SSData_RunTiles + index * 4 + 2); }

const uint8_t *SS_TrackBlock(int segment, size_t *size) {
    if (segment < 0 || segment >= SS_SEGMENT_COUNT) {
        *size = 0;
        return NULL;
    }
    const size_t start = Word(SSData_TrackBlocks + segment * 2);
    // (the block ends where the nearest block after it begins: blocks are shared by some segments, and the data ends the region)
    size_t end = RES_SIZE(SSData_TrackBlocks);
    for (int i = 0; i < SS_SEGMENT_COUNT; i++) {
        const size_t other = Word(SSData_TrackBlocks + i * 2);
        if (other > start && other < end)
            end = other;
    }
    *size = end - start;
    return SSData_TrackBlocks + start;
}

static const uint8_t *Layout(int part) {
    if (part < 0 || part >= SS_PART_COUNT)
        return NULL;
    return SSData_ObjectLayouts + Word(SSData_ObjectLayouts + part * 2);
}

int SS_ObjectCount(int part) {
    const uint8_t *layout = Layout(part);
    if (layout == NULL || layout[0] >= 0x80)
        return 0;
    return layout[0] + 1;
}

bool SS_ObjectEntry(int part, int index, int *position, bool *bomb, uint8_t *lane) {
    const uint8_t *layout = Layout(part);
    if (index < 0 || index >= SS_ObjectCount(part))
        return false;
    const uint8_t *entry = layout + 1 + index * 2;
    *position = entry[0] & 0x7F;
    *bomb = (entry[0] & 0x80) != 0;
    *lane = entry[1];
    return true;
}

const uint8_t *SS_PacePattern(int pattern, size_t *size) {
    if (pattern < 0 || pattern >= SS_PACE_PATTERNS) {
        *size = 0;
        return NULL;
    }
    const size_t start = Word(SSData_PacePatterns + pattern * 2);
    const size_t end = (pattern + 1 < SS_PACE_PATTERNS) ? Word(SSData_PacePatterns + (pattern + 1) * 2) : RES_SIZE(SSData_PacePatterns);
    *size = end - start;
    return SSData_PacePatterns + start;
}

uint16_t SS_VCurveEntry(int curve, int entry) {
    if (curve < 0 || curve >= SS_VCURVES || entry < 0 || entry >= 5)
        return 0;
    return Word(SSData_VerticalCurves + Word(SSData_VerticalCurves + curve * 2) + entry * 2);
}

uint16_t SS_HCurveEntry(int curve, int entry) {
    if (curve < 0 || curve >= SS_HCURVES || entry < 0 || entry >= 5)
        return 0;
    return Word(SSData_HorizontalCurves + Word(SSData_HorizontalCurves + curve * 2) + entry * 2);
}

const uint8_t *SS_TrackTiles(size_t *count) {
    *count = Word(SSData_TrackTiles);
    return SSData_TrackTiles + 2;
}

const uint8_t *SS_BackgroundMap(int which, size_t *size) {
    if (which == 0) {
        *size = RES_SIZE(SSData_BackgroundMapTop);
        return SSData_BackgroundMapTop;
    }
    if (which == 1) {
        *size = RES_SIZE(SSData_BackgroundMapBottom);
        return SSData_BackgroundMapBottom;
    }
    *size = 0;
    return NULL;
}

const uint8_t *SS_DynamicScript(size_t *size) {
    *size = RES_SIZE(SSData_CharacterArtScript);
    return SSData_CharacterArtScript;
}
