#include "test.h"

#include <stdlib.h>
#include <string.h>

#include "EnginePLC.h"
#include "Nemesis.h"
#include "PLC.h"
#include "SpecialStageTables.h"
#include "SpecialStageTrack.h"
#include "SpecialStageDisplay.h"
#include "SpecialStageWide.h"
#include "SpecialStageCharacter.h"
#include "SpecialStageItems.h"
#include "SpecialStageB4Tables.h"
#include "Object/SpecialStageArt.h"

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
    // (the map has two halves, 16 rows each; every cell names one of the background's 127 tiles from $700)
    for (int row = 0; row < 32; row++)
        for (int column = 0; column < 32; column++) {
            const uint16_t tile = d.plane_b[row * SS_PLANE_WIDTH + column] & 0x07FF;
            CHECK(tile >= 0x700 && tile < 0x700 + 127);
        }
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

// --- Stage 3b: widescreen ---

// A screen of 256 pixels is the pre-beta's own: the plane 128 cells across and the scroll -$100 a stripe; wider screens get a plane of 4 stripes with room for their margins, the track in the middle
static void SpecialStage_TheWideLayoutKeepsTheTrackInTheMiddle(void) {
    SSWide w;
    SSWide_Layout(&w, 256);
    CHECK_EQ(w.plane_width, 128);
    CHECK_EQ(w.x0, 0);
    for (int stripe = 0; stripe < 4; stripe++)
        CHECK_EQ(SSWide_ScrollA(&w, stripe), -0x100 * stripe);
    static const int widths[5] = { 320, 360, 426, 640, 1024 };
    for (int i = 0; i < 5; i++) {
        SSWide_Layout(&w, widths[i]);
        CHECK_EQ(w.x0, (widths[i] - 256) / 2);
        CHECK_EQ(w.plane_width, 4 * w.stride);
        CHECK(w.plane_width == 256 || w.plane_width == 512 || w.plane_width == 1024);
        // the margins hold the screen's sides, and the stripe has room for the track and both
        CHECK(w.left_cells * 8 >= w.x0);
        CHECK((w.stride - 32 - w.left_cells) * 8 >= widths[i] - 256 - w.x0);
        for (int stripe = 0; stripe < 4; stripe++) // (the track's first cell of the stripe is at the screen's x0)
            CHECK_EQ(-SSWide_ScrollA(&w, stripe) + w.x0, (stripe * w.stride + w.left_cells) * 8);
        CHECK_EQ(SSWide_ScrollB(&w, 0x10), 0x10 + w.x0);
    }
}

// A unit's stripes are put in the wide row with the track's own words in the middle; at each side the cell next to the track carries the edge pixel out (a tile of that colour) and the rest is blank (the background shows)
static void SpecialStage_TheWideRowPadsTheWallsAndCropsTheRest(void) {
    SSWide w;
    SSWide_Layout(&w, 360);
    uint16_t unit[128], row[512];
    for (int i = 0; i < 128; i++)
        unit[i] = (uint16_t)(0xE000 | 0x14);
    for (int stripe = 0; stripe < 4; stripe++)
        unit[stripe * 32 + 31] = (uint16_t)(0xE000 | 0x0800 | 0x14);
    SSWide_ExpandUnit(&w, unit, row);
    size_t count;
    const uint8_t *rows = SS_TrackTiles(&count);
    const uint32_t tile = ((uint32_t)rows[0x14 * 4] << 24) | ((uint32_t)rows[0x14 * 4 + 1] << 16) | ((uint32_t)rows[0x14 * 4 + 2] << 8) | rows[0x14 * 4 + 3];
    const int first = (int)(tile >> 28);
    CHECK(first != 0); // (the tile has a colour at its edge, or the test shows nothing)
    for (int stripe = 0; stripe < 4; stripe++) {
        const uint16_t *s = row + stripe * w.stride;
        for (int c = 0; c < w.left_cells - 1; c++)
            CHECK_EQ(s[c], 0x8000 | ArtTile_SpecialStageBlank);
        CHECK_EQ(s[w.left_cells - 1], SSWide_MarginWord(0xE014, first));
        CHECK_EQ(s[w.left_cells], unit[stripe * 32]);
        CHECK_EQ(s[w.left_cells + 31], unit[stripe * 32 + 31]);
        CHECK_EQ(s[w.left_cells + 32], SSWide_MarginWord(0xE014, first)); // (the last cell is turned across: the tile's first pixel is its right edge as drawn)
        for (int c = w.left_cells + 33; c < w.stride; c++)
            CHECK_EQ(s[c], 0x8000 | ArtTile_SpecialStageBlank);
    }
    CHECK_EQ(SSWide_MarginWord(0xE014, 5) & 0xE000, 0xE000);
    CHECK_EQ(SSWide_MarginWord(0xE014, 5) & 0x07FF, ArtTile_SpecialStageSolid + 5);
    CHECK_EQ(SSWide_MarginWord(0xE014, 0), 0x8000 | ArtTile_SpecialStageBlank);
}

// --- Stage 4a: the character ---

// The square roots of the pre-beta's Newton's iterations
static void SpecialStage_ThePreBetasSquareRoots(void) {
    CHECK_EQ(SS_Sqrt16(10000, 15), 100);
    CHECK_EQ(SS_Sqrt16(16384, 15), 128);
    CHECK_EQ(SS_Sqrt16(1, 15), 1);
    CHECK_EQ(SS_Sqrt16(0, 15), 0);
    CHECK_EQ(SS_Sqrt16(400, 10), 20);
    CHECK_EQ(SS_Sqrt16(40000, 10), 0); // (ten iterations from 1 do not get to it: the pre-beta's second distance is 0 for the far ones)
}

// It starts at the bottom of the pipe ($40), at ($7F,$A8) and stays there with no pad; left turns it up the left side (the angle goes up), right the other way, and it settles back
static void SpecialStage_TheCharacterRunsRoundThePipeAndSettlesBack(void) {
    SSPlayer p;
    SSPlayer_Init(&p);
    const SSTrackState track = { 12, 4 };
    SSPlayer_Step(&p, &track, 0, 0);
    CHECK_EQ(p.angle, 0x40);
    CHECK_EQ(p.x >> 16, 0x7F);
    CHECK_EQ(p.y >> 16, 0xA8);
    for (int i = 0; i < 20; i++)
        SSPlayer_Step(&p, &track, 0x04, 0);                 // (left)
    CHECK(p.angle > 0x40);
    CHECK((p.x >> 16) < 0x7F);                              // (up the left side: further left, the angle on from $40: sine and cosine of the byte angle)
    const uint8_t high = p.angle;
    for (int i = 0; i < 12; i++)
        SSPlayer_Step(&p, &track, 0, 0);
    CHECK(p.angle < high);                                  // (the slope pulls it back down: it swings, the pre-beta's pipe having hardly any friction)
    SSPlayer_Init(&p);
    for (int i = 0; i < 20; i++)
        SSPlayer_Step(&p, &track, 0x08, 0);                 // (right)
    CHECK(p.angle < 0x40);
    CHECK((p.x >> 16) > 0x7F);
}

// A button jumps (the character goes up from the pipe's side it is on: along the angle's opposite), is in the air with the third animation, and is back on the pipe when it has gone as far as the ellipse
static void SpecialStage_TheCharacterJumpsAndComesBackToThePipe(void) {
    SSPlayer p;
    SSPlayer_Init(&p);
    const SSTrackState track = { 12, 4 };
    SSPlayer_Step(&p, &track, 0, 0x40);                     // (A)
    CHECK_EQ(p.routine, 4);
    CHECK(p.status & SS_STATUS_AIR);
    CHECK_EQ(p.anim, 3);
    CHECK(p.y_speed < 0);                                    // (up from the bottom)
    int16_t top = 0x7FFF;
    int frames = 0;
    while (p.routine == 4 && frames++ < 200) {
        SSPlayer_Step(&p, &track, 0, 0);
        if ((int16_t)(p.y >> 16) < top)
            top = (int16_t)(p.y >> 16);
    }
    CHECK_EQ(p.routine, 2);
    CHECK(!(p.status & SS_STATUS_AIR));
    CHECK_EQ(p.inertia, 0);
    CHECK_EQ(p.x_speed, 0);
    CHECK(top < 0xA8 - 0x10);                                // (it went well up before it came down)
    CHECK(frames > 8 && frames < 120);
}

// The shadow follows: under the character in the middle, to its side on the pipe's sides, and mirrored about the pipe's middle line below it
static void SpecialStage_TheShadowFollowsTheCharacter(void) {
    SSPlayer p;
    SSPlayer_Init(&p);
    SSShadow s;
    SSPlayer_Shadow(&p, &s);
    CHECK_EQ(s.art_tile, 0x6279);
    CHECK_EQ(s.frame, 0);
    CHECK_EQ(s.x, 0x7F);
    CHECK_EQ(s.y, 0xA8 + 0x18);
    p.angle = 0x20;                                          // (on the right side: the diagonal shadow)
    SSPlayer_Shadow(&p, &s);
    CHECK_EQ(s.art_tile, 0x629F);
    p.angle = 0xC0;                                          // (past the middle line: mirrored about it, the frame past)
    SSPlayer_Shadow(&p, &s);
    CHECK(s.y != (int16_t)((p.y >> 16) + 0x18));
}

// Miles is lower and in front, on a bigger pipe: ($7F,$B8) with the middle $80 above, $66 across and $80 down, and his ellipse for the landing is the bigger ($4D and $100); he runs and jumps as Sonic does
static void SpecialStage_MilesRunsOnABiggerPipe(void) {
    SSPlayer p;
    SSPlayer_InitMiles(&p);
    CHECK_EQ(p.x >> 16, 0x7F);
    CHECK_EQ(p.y >> 16, 0xB8);
    CHECK_EQ(p.centre_y, 0x38);
    CHECK_EQ(p.radius_x, 0x66);
    CHECK_EQ(p.radius_y, 0x80);
    CHECK_EQ(p.landing_offset, 0x4D);
    CHECK_EQ(p.landing_sum, 0x100);
    const SSTrackState track = { 12, 4 };
    for (int i = 0; i < 20; i++)
        SSPlayer_Step(&p, &track, 0x04, 0);
    CHECK(p.angle > 0x40);
    CHECK((p.x >> 16) < 0x7F - 0x10);         // (further across than Sonic goes: $66 against $50)
    SSPlayer_InitMiles(&p);
    SSPlayer_Step(&p, &track, 0, 0x20);       // (C)
    CHECK_EQ(p.routine, 4);
    int frames = 0;
    while (p.routine == 4 && frames++ < 300)
        SSPlayer_Step(&p, &track, 0, 0);
    CHECK_EQ(p.routine, 2);
    CHECK(frames > 8 && frames < 200);
}

// --- Stage 5a: rings and bombs ---

// The size by distance: 9 at the character, 0 from $1E on
static void SpecialStage_AnItemsSizeFollowsItsDistance(void) {
    CHECK_EQ(SSItem_AnimationOfDistance(0), 9);
    CHECK_EQ(SSItem_AnimationOfDistance(3), 8);
    CHECK_EQ(SSItem_AnimationOfDistance(0x13), 1);
    CHECK_EQ(SSItem_AnimationOfDistance(0x1E), 0);
    CHECK_EQ(SSItem_AnimationOfDistance(0x7F), 0);
}

// The step's layout is put in the slots once: step 5 has seven rings and a bomb, step 6 a ring and a bomb, the others nothing
static void SpecialStage_TheLayoutOfAStepFillsTheSlots(void) {
    SSItems items;
    SSItems_Reset(&items);
    SSItems_Load(&items, 0);
    int n = 0;
    for (int i = 0; i < SS_ITEM_SLOTS; i++)
        n += items.item[i].kind != SS_ITEM_NONE;
    CHECK_EQ(n, 0);
    SSItems_Load(&items, 5);
    int rings = 0, bombs = 0;
    for (int i = 0; i < SS_ITEM_SLOTS; i++) {
        rings += items.item[i].kind == SS_ITEM_RING;
        bombs += items.item[i].kind == SS_ITEM_BOMB;
    }
    CHECK_EQ(rings, 7);
    CHECK_EQ(bombs, 1);
    CHECK_EQ(items.item[0].distance, 0x46u << 16);
    CHECK_EQ(items.item[0].angle, 0x10);
    CHECK_EQ(items.item[7].kind, SS_ITEM_BOMB);
    CHECK_EQ(items.item[7].distance, 0x4Eu << 16);
    items.item[0].distance = 1;
    SSItems_Load(&items, 5);           // (once for a step: the same step again does not put them back)
    CHECK_EQ(items.item[0].distance, 1);
}

// An item comes on at 0.8 a frame, gets its place from the block (the middle, how far across and down), and is gone when it has come
static void SpecialStage_AnItemComesDownThePipeAndGoes(void) {
    // a block of 3 distances: the middle at ($80,$60), $40 across, $30 down, no hidden angles
    static const uint8_t block[2 + 18] = { 0, 3, 0x80, 0x60, 0x40, 0x30, 0, 0, 0x80, 0x60, 0x40, 0x30, 0, 0, 0x80, 0x60, 0x40, 0x30, 0, 0 };
    SSItems items;
    SSItems_Reset(&items);
    items.item[0].kind = SS_ITEM_RING;
    items.item[0].distance = (uint32_t)3 << 16;
    items.item[0].angle = 0x40;       // (straight down: cosine 0, sine 256)
    items.item[0].collision = 1;
    SSItems_Update(&items, block, sizeof(block), false);
    CHECK(items.item[0].visible);
    CHECK_EQ(items.item[0].x, 0x80);
    CHECK_EQ(items.item[0].y, 0x60 + 0x30);
    CHECK_EQ(items.item[0].anim, 9 - (SSItem_AnimationOfDistance(2) == 9 ? 0 : 1)); // (distance 2.2: the nearest size)
    int frames = 1;
    while (items.item[0].kind != SS_ITEM_NONE && frames < 20) {
        SSItems_Update(&items, block, sizeof(block), false);
        frames++;
    }
    CHECK_EQ(frames, 4);              // ($30000 less $CD00 a frame is under 0 on the fourth)
    // (further than the block goes: not shown)
    items.item[1].kind = SS_ITEM_RING;
    items.item[1].distance = (uint32_t)40 << 16;
    SSItems_Update(&items, block, sizeof(block), false);
    CHECK(!items.item[1].visible);
}

// A ring at the nearest size is taken by the character's box; a bomb hurts him and goes off
static void SpecialStage_TheCharacterTakesRingsAndIsHurtByBombs(void) {
    SSPlayer p;
    SSPlayer_Init(&p);
    SSItems items;
    SSItems_Reset(&items);
    const int16_t px = (int16_t)(p.x >> 16), py = (int16_t)(p.y >> 16);
    items.item[0] = (SSItem){ .kind = SS_ITEM_RING, .anim = 8, .collision = 1, .x = px, .y = py };
    items.item[1] = (SSItem){ .kind = SS_ITEM_RING, .anim = 7, .collision = 1, .x = px, .y = py };        // (not near enough yet)
    items.item[2] = (SSItem){ .kind = SS_ITEM_RING, .anim = 8, .collision = 1, .x = (int16_t)(px + 60), .y = py }; // (to the side)
    CHECK_EQ(SSItems_Touch(&items, &p), 1);
    CHECK(items.item[0].touched);
    CHECK(!items.item[1].touched);
    CHECK(!items.item[2].touched);
    CHECK_EQ(p.touched, 1);
    CHECK_EQ(SSItems_Touch(&items, &p), 0);   // (the same ring is not taken twice)
    items.item[3] = (SSItem){ .kind = SS_ITEM_BOMB, .anim = 8, .collision = 2, .x = px, .y = py };
    SSItems_Touch(&items, &p);
    CHECK_EQ(items.item[3].collision, 0);
    CHECK_EQ(p.hit, 2);
    CHECK_EQ(p.tumble, p.angle);
}

static void SpecialStage_ATumbleWithRingsSpillsThemOnItsFirstStepOnly(void) {
    SSPlayer p;
    SSPlayer_Init(&p);
    p.touched = 14;
    p.hit = 2;
    p.tumble = p.angle;
    SSTrackState track = { 12, 4 };
    SSPlayer_Step(&p, &track, 0, 0);
    CHECK(p.spill);
    p.spill = false;
    SSPlayer_Step(&p, &track, 0, 0);
    CHECK(!p.spill);
    SSPlayer q;
    SSPlayer_Init(&q);
    q.hit = 2;
    q.tumble = q.angle;
    SSPlayer_Step(&q, &track, 0, 0);
    CHECK(!q.spill); // (with no rings there is nothing to spill)
}

static void SpecialStage_ATumbleOnlyUsesFramesOfItsTable(void) {
    for (int start = 0; start < 256; start++) {
        SSPlayer p;
        SSPlayer_Init(&p);
        p.angle = (uint8_t)start;
        p.hit = 2;
        p.tumble = p.angle;
        SSTrackState track = { 12, 4 };
        for (int step = 0; step < 40 && p.hit; step++) {
            SSPlayer_Step(&p, &track, 0, 0);
            CHECK(p.frame == 0 || p.frame == 4); // (the table's frames: a byte past it was $FF and flooded VRAM)
        }
    }
}

// Beta 4's tables (stage 2 of docs/Sonic2SpecialStageB4.md): ten stages whose steps are kinds of the pre-beta's five lists, a list of objects for each that parses to its end, and the places of the rings and bombs of
// each of the 56 segments
static void SpecialStageB4_TheStagesTheirObjectsAndTheSegmentsPlacesAreConsistent(void) {
    for (int stage = 0; stage < SS4_STAGE_COUNT; stage++) {
        size_t steps;
        const uint8_t *step = SS4_StageSteps(stage, &steps);
        CHECK(steps >= 40 && steps <= 64);
        for (size_t i = 0; i < steps; i++)
            CHECK((step[i] & 0x7F) <= 4); // (a kind of the five lists)
        size_t size;
        const uint8_t *objects = SS4_StageObjects(stage, &size);
        size_t at = 0;
        int groups = 0, rings = 0;
        while (at < size) {
            if (objects[at] >= 0x80) { // (the end of a step's objects)
                at++;
                groups++;
                continue;
            }
            rings += !(objects[at] & 0x40);
            at += 2;
        }
        CHECK_EQ(at, size);                  // (the pairs and the ends fill the list)
        CHECK(objects[size - 1] >= 0x80);    // (and it ends on an end)
        CHECK(groups >= 40 && groups <= 64);
        CHECK(rings > 0);
    }
    // The three stages of the bonus stars' entry are the stages 4 to 6 again, step for step, but for the last end: the rings' check ($FE) where the emerald's ($FD) was
    for (int stage = 4; stage <= 6; stage++) {
        size_t a, b;
        const uint8_t *x = SS4_StageObjects(stage, &a), *y = SS4_StageObjects(stage + 3, &b);
        CHECK_EQ(a, b);
        int different = 0;
        for (size_t i = 0; i < a; i++)
            if (x[i] != y[i]) {
                different++;
                CHECK_EQ(x[i], 0xFD);
                CHECK_EQ(y[i], 0xFE);
            }
        CHECK_EQ(different, 1);
    }
    CHECK_EQ(SS4_KindLength(0), 0x18);
    CHECK_EQ(SS4_KindLength(2), 0x0C);
    for (int segment = 0; segment < SS_SEGMENT_COUNT; segment++) {
        const int places = SS4_PlaceCount(segment);
        CHECK(places > 0 && places < 200);
        const uint8_t *last = SS4_Place(segment, places - 1);
        CHECK(last[0] != 0 || last[1] != 0 || last[2] != 0 || last[3] != 0 || last[4] != 0 || last[5] != 0);
    }
}

void RegisterSpecialStageTests(void) {
    RUN_TEST(SpecialStageB4_TheStagesTheirObjectsAndTheSegmentsPlacesAreConsistent);
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
    RUN_TEST(SpecialStage_TheWideLayoutKeepsTheTrackInTheMiddle);
    RUN_TEST(SpecialStage_TheWideRowPadsTheWallsAndCropsTheRest);
    RUN_TEST(SpecialStage_ThePreBetasSquareRoots);
    RUN_TEST(SpecialStage_TheCharacterRunsRoundThePipeAndSettlesBack);
    RUN_TEST(SpecialStage_TheCharacterJumpsAndComesBackToThePipe);
    RUN_TEST(SpecialStage_TheShadowFollowsTheCharacter);
    RUN_TEST(SpecialStage_MilesRunsOnABiggerPipe);
    RUN_TEST(SpecialStage_AnItemsSizeFollowsItsDistance);
    RUN_TEST(SpecialStage_TheLayoutOfAStepFillsTheSlots);
    RUN_TEST(SpecialStage_AnItemComesDownThePipeAndGoes);
    RUN_TEST(SpecialStage_TheCharacterTakesRingsAndIsHurtByBombs);
    RUN_TEST(SpecialStage_ATumbleWithRingsSpillsThemOnItsFirstStepOnly);
    RUN_TEST(SpecialStage_ATumbleOnlyUsesFramesOfItsTable);
}
