#include "test.h"

#include <stdlib.h>
#include <string.h>

#include "EnginePLC.h"
#include "Nemesis.h"
#include "PLC.h"
#include "SpecialStageTables.h"
#include "SpecialStageTrack.h"
#include "SpecialStageDisplay.h"

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

// --- Stage 3a: the track's decoding and order ---

// Every segment decodes to 4 slices of 7 units of 128 words, each word with the palette's bits the stage puts on it, and the flipped track is each unit turned round
static void SpecialStage_EverySegmentDecodesToItsNameTable(void) {
    static uint16_t normal[SS_TRACK_SEGMENT_WORDS], flipped[SS_TRACK_SEGMENT_WORDS];
    for (int s = 0; s < SS_SEGMENT_COUNT; s++) {
        CHECK(SS_DecodeSegment(s, false, normal));
        CHECK(SS_DecodeSegment(s, true, flipped));
        bool palette = true, turned = true;
        int across = 0;
        for (int i = 0; i < SS_TRACK_SEGMENT_WORDS; i++) {
            palette &= (normal[i] & 0x6000) == 0x6000 && (flipped[i] & 0x6000) == 0x6000;
            const int unit = i / SS_TRACK_UNIT_WORDS, at = i % SS_TRACK_UNIT_WORDS;
            const uint16_t was = normal[unit * SS_TRACK_UNIT_WORDS + SS_TRACK_UNIT_WORDS - 1 - at];
            // (the same words in the other order, a single tile turned the other way across: only the horizontal flip bit differs)
            turned &= flipped[i] == was || flipped[i] == (uint16_t)(was ^ 0x0800);
            across += flipped[i] != was;
        }
        CHECK(palette);
        CHECK(turned);
        CHECK(across > 0); // (every segment has single tiles)
    }
    CHECK(!SS_DecodeSegment(SS_SEGMENT_COUNT, false, normal));
}

// The slices come one at a time (the pre-beta decodes one a frame) and the same words come out as from the whole; a fifth does not
static void SpecialStage_TheSlicesGiveTheWholeSegment(void) {
    static uint16_t whole[SS_TRACK_SEGMENT_WORDS], parts[SS_TRACK_SEGMENT_WORDS];
    CHECK(SS_DecodeSegment(0x26, false, whole));
    SSDecoder d;
    CHECK(SSDecoder_Start(&d, 0x26));
    for (int slice = 0; slice < SS_TRACK_SLICES; slice++)
        CHECK(SSDecoder_Slice(&d, false, parts + slice * SS_TRACK_SLICE_WORDS));
    CHECK(memcmp(whole, parts, sizeof(whole)) == 0);
    CHECK(!SSDecoder_Slice(&d, false, parts));
}

// The stage's order: 226 segments, and the pre-beta's slip (the second time round it begins with the first list, not the first step's)
static void SpecialStage_TheSegmentsComeInTheStagesOrder(void) {
    SSTrack track;
    SSTrack_Reset(&track);
    int total = 0;
    static const int sizes[13] = { 16, 16, 16, 16, 7, 24, 24, 24, 12, 16, 7, 24, 24 }; // (the lists of the 13 steps)
    int at_step[13] = { 0 };
    int first[13];
    bool flipped_at[256]; // (by the number of the call)
    int segment_at[256];
    int step_at[256];
    for (int step = 0; step < 13; step++) {
        first[step] = total;
        for (int i = 0; i < sizes[step]; i++) {
            const int step_before = track.step;
            const int segment = SSTrack_NextSegment(&track);
            CHECK_EQ(step_before, step);
            segment_at[total] = segment;
            step_at[total] = step;
            flipped_at[total] = track.flipped;
            at_step[step]++;
            total++;
        }
    }
    CHECK_EQ(total, 226);
    CHECK_EQ(track.step, 13);
    CHECK_EQ(segment_at[0], 0x11);                 // (list 3: the four segments $11 to $14 and again)
    CHECK_EQ(segment_at[3], 0x14);
    CHECK_EQ(segment_at[4], 0x11);
    CHECK_EQ(segment_at[first[4]], 0x31);          // (list 4)
    CHECK_EQ(segment_at[first[5]], 0x26);          // (list 0)
    CHECK_EQ(segment_at[first[5] + 7], 0x00);
    CHECK_EQ(segment_at[first[6]], 0x26);          // (list 1: the start, then $15 on)
    CHECK_EQ(segment_at[first[6] + 7], 0x15);
    CHECK(flipped_at[0]);                          // ($11 turns the track over)
    // ($0D turns it over where the step's flag is set and back where it is not; $1A the same)
    CHECK(!flipped_at[first[5] + 20]);             // (step 5: list 0 without the flag)
    CHECK_EQ(segment_at[first[5] + 20], 0x0D);
    CHECK(flipped_at[first[7] + 20]);              // (step 7: list 0 with it)
    CHECK_EQ(segment_at[first[7] + 20], 0x0D);
    CHECK_EQ(segment_at[first[6] + 12], 0x1A);
    CHECK(!flipped_at[first[6] + 12]);              // (step 6: list 1 without)
    CHECK_EQ(segment_at[first[12] + 12], 0x1A);
    CHECK(flipped_at[first[12] + 12]);              // (step 12: list 1 with)
    (void)step_at;
    // the sequence is over: it starts again with list 0
    CHECK_EQ(SSTrack_NextSegment(&track), 0x26);
    CHECK_EQ(track.list, 0);
    CHECK_EQ(track.step, 0);
    for (int i = 1; i < 24; i++)
        SSTrack_NextSegment(&track);
    CHECK_EQ(track.step, 1);                       // (and goes on from the second step)
    CHECK_EQ(SSTrack_NextSegment(&track), 0x11);
}

// --- Stage 3b: the track as it is shown ---

// The pace: speed $0C is five frames a segment all the way round; $08 is eight and seven by turns (and the pattern goes round after twice the speed)
static void SpecialStage_ThePaceFollowsTheSpeed(void) {
    static SSDisplay d;
    SSDisplay_Reset(&d);
    int sum = 0;
    for (int i = 0; i < 24; i++) {
        d.counter = 0; // (each call is the first frame of a segment)
        const int p = SSDisplay_Pace(&d);
        CHECK_EQ(p, 5);
        sum += p;
    }
    CHECK_EQ(sum, 120);
    SSDisplay_Reset(&d);
    d.speed = 8;
    int periods[16];
    for (int i = 0; i < 16; i++) {
        d.counter = 0;
        periods[i] = SSDisplay_Pace(&d);
    }
    CHECK_EQ(periods[0], 8);
    CHECK_EQ(periods[1], 7);
    CHECK_EQ(periods[14], 8);
    CHECK_EQ(periods[15], 7);
    // (a frame that is not the first of a segment gives the same period, and the segment ends when the frames are out)
    SSDisplay_Reset(&d);
    CHECK_EQ(SSDisplay_Pace(&d), 5);
    for (int i = 0; i < 4; i++)
        CHECK_EQ(SSDisplay_Pace(&d), 5);
}

// The pre-roll decodes the first segment ($11 of list 3, which turns the track over) into the second buffer over its first period (the first period passes empty), shows it, and has begun the next into the first;
// every segment after is decoded into the buffer that is not shown and shown when its time is out
static void SpecialStage_TheTrackIsDecodedIntoOneBufferAndShownFromTheOther(void) {
    static SSDisplay d;
    static uint16_t expected[SS_TRACK_SEGMENT_WORDS];
    SSDisplay_Reset(&d);
    SSDisplay_Prime(&d);
    CHECK_EQ(d.segment, 0x12);                             // (the second has begun)
    CHECK_EQ(d.shown, 1);                                  // (the first is shown, from the second buffer)
    CHECK_EQ(d.bank, 0);
    CHECK(d.track.flipped);                                // ($11 turned the decoding at once)
    CHECK(d.display_flipped);                              // (and the turned track's scroll shown by the end of its time)
    CHECK(SS_DecodeSegment(0x11, true, expected));
    CHECK(memcmp(d.plane_a[1], expected, sizeof(expected)) == 0);
    // the second is decoded into the first buffer over its next frames
    for (int i = 0; i < 3; i++)
        SSDisplay_Frame(&d);
    CHECK(SS_DecodeSegment(0x12, true, expected));
    CHECK(memcmp(d.plane_a[0], expected, sizeof(expected)) == 0);
    // and shown when its time is out
    for (int i = 0; i < 2 && d.shown != 0; i++)
        SSDisplay_Frame(&d);
    CHECK_EQ(d.shown, 0);
    CHECK_EQ(d.bank, 1);
}

// The scroll buffers: a line has the stripe of its line group, and the turned track's the other way round; the background's own are 0
static void SpecialStage_TheScrollBuffersGiveTheLinesTheirStripes(void) {
    static SSDisplay d;
    SSDisplay_Reset(&d);
    for (int line = 0; line < 16; line++) {
        const int group = (line / 2) % 4;
        CHECK_EQ(d.hscroll[0][line][0], -0x100 * group);
        CHECK_EQ(d.hscroll[1][line][0], -0x100 * (3 - group));
        CHECK_EQ(d.hscroll[0][line][1], 0);
    }
    CHECK_EQ(d.hscroll[0][255][0], -0x300);
    // the background: the map, four times across
    CHECK_EQ(d.plane_b[0], d.plane_b[32]);
    CHECK_EQ(d.plane_b[SS_PLANE_WIDTH + 5], d.plane_b[SS_PLANE_WIDTH + 5 + 96]);
    CHECK((d.plane_b[7] & 0x07FF) >= 0x700 || d.plane_b[7] == 0);
}

// A stretch with no curves leaves the background where it is; in the stretches that bend (list 0, the segments up to $0C: the horizontal curve) the background moves, to the one side in the track and the other
// in the turned one
static void SpecialStage_TheBackgroundMovesWithTheCurves(void) {
    static SSDisplay d;
    SSDisplay_Reset(&d);
    SSDisplay_Prime(&d);
    // (the sequence begins with list 3: no curve)
    for (int i = 0; i < 200; i++)
        SSDisplay_Frame(&d);
    CHECK_EQ(d.list, 3);
    CHECK_EQ(d.b_hscroll, 0);
    CHECK_EQ(d.b_vscroll, 0);
    // (on through the lists until list 0 is in: the horizontal curve and then the vertical one move it)
    int guard = 0;
    while (d.list != 0 && guard++ < 20000)
        SSDisplay_Frame(&d);
    CHECK_EQ(d.list, 0);
    int moved_x = 0, moved_y = 0;
    for (int i = 0; i < 5000 && d.list == 0; i++) {
        const int32_t x = d.b_hscroll, y = d.b_vscroll;
        SSDisplay_Frame(&d);
        moved_x += d.b_hscroll != x;
        moved_y += d.b_vscroll != y;
    }
    CHECK(moved_x > 0);
    CHECK(moved_y > 0);
}

void RegisterSpecialStageTests(void) {
    RUN_TEST(SpecialStage_TheMainListsArtDecodesAndGoesWherePreBetaPutsIt);
    RUN_TEST(SpecialStage_TheRingsListGoesOverTheTrafficLights);
    RUN_TEST(SpecialStage_EverySegmentIsThreeSectionsThatFillItsFile);
    RUN_TEST(SpecialStage_TheListsAndTheSequenceAreConsistent);
    RUN_TEST(SpecialStage_TheTileTablesHaveTheirEntries);
    RUN_TEST(SpecialStage_EverySegmentHasATrackBlock);
    RUN_TEST(SpecialStage_ThePartsHaveTheirRingsAndBombs);
    RUN_TEST(SpecialStage_EverySegmentDecodesToItsNameTable);
    RUN_TEST(SpecialStage_TheSlicesGiveTheWholeSegment);
    RUN_TEST(SpecialStage_TheSegmentsComeInTheStagesOrder);
    RUN_TEST(SpecialStage_ThePaceFollowsTheSpeed);
    RUN_TEST(SpecialStage_TheTrackIsDecodedIntoOneBufferAndShownFromTheOther);
    RUN_TEST(SpecialStage_TheScrollBuffersGiveTheLinesTheirStripes);
    RUN_TEST(SpecialStage_TheBackgroundMovesWithTheCurves);
}
