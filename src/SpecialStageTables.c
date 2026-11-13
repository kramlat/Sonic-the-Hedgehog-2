// The tables of the pre-beta's special stage: see SpecialStageTables.h. This file is the one that includes the data (a resource header defines it).
#include "SpecialStageTables.h"

#include "Resource/SSData/006CFC.h"
#include "Resource/SSData/006E3A.h"
#include "Resource/SSData/007302.h"
#include "Resource/SSData/00751A.h"
#include "Resource/SSData/0309C8.h"
#include "Resource/SSData/0C853E.h"
#include "Resource/SSData/0C89E2.h"
#include "Resource/SSData/0C8FB0.h"
#include "Resource/SSData/0C9568.h"
#include "Resource/SSData/0C9BCC.h"
#include "Resource/SSData/0CA1F8.h"
#include "Resource/SSData/0CA8B4.h"
#include "Resource/SSData/0CAEBC.h"
#include "Resource/SSData/0CB3FA.h"
#include "Resource/SSData/0CB97E.h"
#include "Resource/SSData/0CBEF8.h"
#include "Resource/SSData/0CC418.h"
#include "Resource/SSData/0CC88C.h"
#include "Resource/SSData/0CCCF6.h"
#include "Resource/SSData/0CD1BA.h"
#include "Resource/SSData/0CD63A.h"
#include "Resource/SSData/0CDA84.h"
#include "Resource/SSData/0CDEC6.h"
#include "Resource/SSData/0CE544.h"
#include "Resource/SSData/0CEAE0.h"
#include "Resource/SSData/0CF03A.h"
#include "Resource/SSData/0CF636.h"
#include "Resource/SSData/0CFAE6.h"
#include "Resource/SSData/0CFFE8.h"
#include "Resource/SSData/0D0400.h"
#include "Resource/SSData/0D084E.h"
#include "Resource/SSData/0D0CCC.h"
#include "Resource/SSData/0D115C.h"
#include "Resource/SSData/0D1626.h"
#include "Resource/SSData/0D1BB2.h"
#include "Resource/SSData/0D229A.h"
#include "Resource/SSData/0D29E0.h"
#include "Resource/SSData/0D3036.h"
#include "Resource/SSData/0D3592.h"
#include "Resource/SSData/0D3B3C.h"
#include "Resource/SSData/0D41D0.h"
#include "Resource/SSData/0D47E4.h"
#include "Resource/SSData/0D4C68.h"
#include "Resource/SSData/0D50D6.h"
#include "Resource/SSData/0D554C.h"
#include "Resource/SSData/0D59E4.h"
#include "Resource/SSData/0D5E8A.h"
#include "Resource/SSData/0D6232.h"
#include "Resource/SSData/0D6626.h"
#include "Resource/SSData/0D6A5E.h"
#include "Resource/SSData/0D6EF0.h"
#include "Resource/SSData/0D73B2.h"
#include "Resource/SSData/0D77BA.h"
#include "Resource/SSData/0D7C50.h"
#include "Resource/SSData/0D8108.h"
#include "Resource/SSData/0D875A.h"
#include "Resource/SSData/0D8CC0.h"
#include "Resource/SSData/0D91E8.h"
#include "Resource/SSData/0D979C.h"
#include "Resource/SSData/0D9D8E.h"
#include "Resource/SSData/0DA222.h"
#include "Resource/SSData/0DA672.h"
#include "Resource/SSData/0DCA9C.h"
#include "Resource/SSData/0DCBCA.h"

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
    const uint8_t *table = SSData_006CFC;
    const size_t start = Word(table + list * 2);
    const size_t end = (list + 1 < SS_LIST_COUNT) ? Word(table + (list + 1) * 2) : RES_SIZE(SSData_006CFC);
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

size_t SS_TileWordCount(void) { return RES_SIZE(SSData_006E3A) / 2; }
uint16_t SS_TileWord(size_t index) { return Word(SSData_006E3A + index * 2); }
size_t SS_RunCount(void) { return RES_SIZE(SSData_007302) / 4; }
uint16_t SS_RunTile(size_t index) { return Word(SSData_007302 + index * 4); }
uint16_t SS_RunRepeat(size_t index) { return Word(SSData_007302 + index * 4 + 2); }

const uint8_t *SS_TrackBlock(int segment, size_t *size) {
    if (segment < 0 || segment >= SS_SEGMENT_COUNT) {
        *size = 0;
        return NULL;
    }
    const size_t start = Word(SSData_00751A + segment * 2);
    // (the block ends where the nearest block after it begins: blocks are shared by some segments, and the data ends the region)
    size_t end = RES_SIZE(SSData_00751A);
    for (int i = 0; i < SS_SEGMENT_COUNT; i++) {
        const size_t other = Word(SSData_00751A + i * 2);
        if (other > start && other < end)
            end = other;
    }
    *size = end - start;
    return SSData_00751A + start;
}

static const uint8_t *Layout(int part) {
    if (part < 0 || part >= SS_PART_COUNT)
        return NULL;
    return SSData_0309C8 + Word(SSData_0309C8 + part * 2);
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
