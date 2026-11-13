#include "test.h"

#include <stdlib.h>
#include <string.h>

#include "EnginePLC.h"
#include "Nemesis.h"
#include "PLC.h"
#include "SpecialStageTables.h"

// The pre-beta's special stage (built in stages, see docs/Sonic2SpecialStage.md). Stage 1: its art, imported as it is: every picture of the two lists the stage loads ($3C and $3D) decodes to the tiles its header
// counts, to the last byte and not one more, and goes to the places the pre-beta's lists give.

static void CheckList(PlcId id, size_t entries, const size_t *expected_vram) {
    const PLCList *list = plcs[id];
    CHECK_EQ(list->plcs, entries);
    for (size_t i = 0; i < list->plcs && i < entries; i++) {
        const uint8_t *art = list->plc[i].art;
        const size_t tiles = (size_t)(((art[0] & 0x7F) << 8) | art[1]);
        CHECK(tiles > 0);
        CHECK_EQ(list->plc[i].off, expected_vram[i]);
        uint8_t *out = malloc(tiles * 32 + 64);
        memset(out, 0xA5, tiles * 32 + 64);
        NemDecToRAM(art, out);
        for (size_t b = tiles * 32; b < tiles * 32 + 64; b++)
            CHECK_EQ(out[b], 0xA5); // (not a byte more than the header says)
        free(out);
    }
}

static void SpecialStage_TheMainListsArtDecodesAndGoesWherePreBetaPutsIt(void) {
    static const size_t main_vram[9] = { 0x2E80, 0x3440, 0xE000, 0x46E0, 0x4F20, 0x53E0, 0x5B20, 0x61A0, 0x7A00 };
    CheckList(PlcId_SSMain, 9, main_vram);
}

static void SpecialStage_TheRingsListGoesOverTheTrafficLights(void) {
    static const size_t rings_vram[1] = { 0x61A0 };
    CheckList(PlcId_SSRings, 1, rings_vram);
    CHECK_EQ(plcs[PlcId_SSRings]->plc[0].off, plcs[PlcId_SSMain]->plc[7].off);
}

// --- Stage 2: the tables ---

// Every segment is three sections whose headers' lengths add up to the file (the stage's code at $5F62 finds the second and third by them)
static void SpecialStage_EverySegmentIsThreeSectionsThatFillItsFile(void) {
    for (int s = 0; s < SS_SEGMENT_COUNT; s++) {
        SSBlob sections[3];
        CHECK(SS_SegmentSections(s, sections));
        const SSBlob *blob = SS_Segment(s);
        size_t total = 0;
        for (int i = 0; i < 3; i++)
            total += 4 + sections[i].size;
        CHECK(total <= blob->size && blob->size - total <= 1); // (a segment whose file has an odd length has a byte of padding)
    }
    CHECK(SS_Segment(SS_SEGMENT_COUNT) == NULL);
    CHECK_EQ(SS_Segment(0)->size, 1188);            // (the file as it is: the generator's zero padding is not counted)
}

// The lists of segments: their sizes are the table's offsets' differences, the numbers are segments, and the stage's sequence names lists
static void SpecialStage_TheListsAndTheSequenceAreConsistent(void) {
    static const size_t sizes[SS_LIST_COUNT] = { 24, 24, 12, 16, 8 };
    for (int l = 0; l < SS_LIST_COUNT; l++) {
        size_t size;
        const uint8_t *list = SS_List(l, &size);
        CHECK_EQ(size, sizes[l]);
        CHECK(SS_ListLength(l) <= (int)size);
        for (int i = 0; i < SS_ListLength(l); i++)
            CHECK(list[i] < SS_SEGMENT_COUNT);
    }
    CHECK_EQ(SS_SequenceLength(), 13);
    for (size_t step = 0; step < SS_SequenceLength(); step++)
        CHECK((SS_SequenceStep(step) & 0x7F) < SS_LIST_COUNT);
    CHECK_EQ(SS_SequenceStep(13), 0xFF);
    // (the first list begins with the six segments of the start: $26 to $2B and $26 again, then the track's own)
    size_t size;
    const uint8_t *first = SS_List(0, &size);
    CHECK_EQ(first[0], 0x26);
    CHECK_EQ(first[5], 0x2B);
    CHECK_EQ(first[7], 0x00);
}

static void SpecialStage_TheTileTablesHaveTheirEntries(void) {
    CHECK_EQ(SS_TileWordCount(), 612);
    CHECK_EQ(SS_RunCount(), 134);
    CHECK_EQ(SS_TileWord(0), 0x8001);
    CHECK_EQ(SS_TileWord(1), 0x8007);
    CHECK_EQ(SS_RunTile(0), 0x0007);
    CHECK_EQ(SS_RunRepeat(0), 0x0001);
    CHECK_EQ(SS_RunTile(133), 0x002C);
    CHECK_EQ(SS_RunRepeat(133), 0x0010);
}

// The tracks' blocks: a word for each segment (some segments share a block), all inside the region, each block at least a byte
static void SpecialStage_EverySegmentHasATrackBlock(void) {
    for (int s = 0; s < SS_SEGMENT_COUNT; s++) {
        size_t size;
        const uint8_t *block = SS_TrackBlock(s, &size);
        CHECK(block != NULL);
        CHECK(size > 0);
    }
    size_t a, b;
    SS_TrackBlock(0, &a);
    CHECK_EQ(a, 0x75F8 - 0x758A); // (the first block runs to the second's start)
    SS_TrackBlock(0x0D, &a);
    SS_TrackBlock(0x0C, &b);
    CHECK_EQ(a, b);               // (the two entries that name the same block: $0C and $0D of the table)
}

static void SpecialStage_ThePartsHaveTheirRingsAndBombs(void) {
    static const int counts[SS_PART_COUNT] = { 0, 0, 0, 0, 0, 8, 2, 0, 0, 9, 0, 0, 0 };
    for (int p = 0; p < SS_PART_COUNT; p++)
        CHECK_EQ(SS_ObjectCount(p), counts[p]);
    int position, ok;
    bool bomb;
    uint8_t lane;
    ok = SS_ObjectEntry(5, 7, &position, &bomb, &lane);        // (the last of the sixth part: a bomb at $4E, lane $40)
    CHECK(ok);
    CHECK(bomb);
    CHECK_EQ(position, 0x4E);
    CHECK_EQ(lane, 0x40);
    ok = SS_ObjectEntry(5, 0, &position, &bomb, &lane);
    CHECK(ok && !bomb && position == 0x46 && lane == 0x10);
    CHECK(!SS_ObjectEntry(5, 8, &position, &bomb, &lane));
    ok = SS_ObjectEntry(9, 8, &position, &bomb, &lane);        // (the tenth: nine rings, the last at $36, lane $7F)
    CHECK(ok && !bomb && position == 0x36 && lane == 0x7F);
}

void RegisterSpecialStageTests(void) {
    RUN_TEST(SpecialStage_TheMainListsArtDecodesAndGoesWherePreBetaPutsIt);
    RUN_TEST(SpecialStage_TheRingsListGoesOverTheTrafficLights);
    RUN_TEST(SpecialStage_EverySegmentIsThreeSectionsThatFillItsFile);
    RUN_TEST(SpecialStage_TheListsAndTheSequenceAreConsistent);
    RUN_TEST(SpecialStage_TheTileTablesHaveTheirEntries);
    RUN_TEST(SpecialStage_EverySegmentHasATrackBlock);
    RUN_TEST(SpecialStage_ThePartsHaveTheirRingsAndBombs);
}
