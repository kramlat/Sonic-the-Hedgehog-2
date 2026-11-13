// The pre-beta's special stage track: see SpecialStageTrack.h
#include "SpecialStageTrack.h"
#include "SpecialStageTables.h"

#include <string.h>

static void BitsStart(SSBits *bits, const uint8_t *data, size_t size) {
    bits->data = data;
    bits->size = size;
    bits->pos = 0;
    bits->left = 0;
    bits->current = 0;
}

// One bit (most significant first); -1 when the stream has no more
static int Bit(SSBits *bits) {
    if (bits->left == 0) {
        if (bits->pos >= bits->size)
            return -1;
        bits->current = bits->data[bits->pos++];
        bits->left = 8;
    }
    bits->left--;
    return (bits->current >> bits->left) & 1;
}

static int Bits(SSBits *bits, int count) { // -1 when it runs out
    int value = 0;
    for (int i = 0; i < count; i++) {
        const int b = Bit(bits);
        if (b < 0)
            return -1;
        value = (value << 1) | b;
    }
    return value;
}

bool SSDecoder_Start(SSDecoder *decoder, int segment) {
    SSBlob sections[3];
    memset(decoder, 0, sizeof(*decoder));
    if (!SS_SegmentSections(segment, sections))
        return false;
    BitsStart(&decoder->flags, sections[0].data, sections[0].size);
    BitsStart(&decoder->tiles, sections[1].data, sections[1].size);
    BitsStart(&decoder->runs, sections[2].data, sections[2].size);
    decoder->valid = true;
    return true;
}

// One unit: words until the unit's end; the count, or -1 when the data is wrong
static int DecodeUnit(SSDecoder *decoder, uint16_t *out, size_t room, bool flipped) {
    size_t count = 0;
    for (;;) {
        const int flag = Bit(&decoder->flags);
        if (flag < 0)
            return -1;
        if (flag) { // a tile
            const int wide = Bit(&decoder->tiles);
            if (wide < 0)
                return -1;
            const int number = wide ? Bits(&decoder->tiles, 10) : Bits(&decoder->tiles, 6);
            if (number < 0)
                return -1;
            const size_t index = (size_t)number + (wide ? 0x40 : 0);
            if (index >= SS_TileWordCount() || count >= room)
                return -1;
            // (the turned track's decoder, Offset_0x0066AA on, XORs $6800 on a single tile instead of ORing $6000: the palette is put on the same, and the tile is turned the other way across, as the track is)
            out[count++] = flipped ? (uint16_t)(SS_TileWord(index) ^ 0x6800) : (uint16_t)(SS_TileWord(index) | 0x6000);
            continue;
        }
        SSBits *runs = &decoder->runs;
        if (runs->left == 0) { // (a byte of $FF where one begins ends the unit)
            if (runs->pos >= runs->size)
                return -1;
            if (runs->data[runs->pos] == 0xFF) {
                runs->pos++;
                return (int)count;
            }
        }
        const int wide = Bit(runs);
        if (wide < 0)
            return -1;
        int number = wide ? Bits(runs, 7) : Bits(runs, 6);
        if (number < 0)
            return -1;
        if (wide) {
            if (number == 0x7F)
                return (int)count;
            number += 0x40;
        }
        if ((size_t)number >= SS_RunCount())
            return -1;
        const uint16_t tile = (uint16_t)(SS_RunTile((size_t)number) | 0xE000);
        const size_t times = (size_t)SS_RunRepeat((size_t)number) + 1;
        if (count + times > room)
            return -1;
        for (size_t i = 0; i < times; i++)
            out[count++] = tile;
    }
}

bool SSDecoder_Slice(SSDecoder *decoder, bool flipped, uint16_t *out) {
    if (!decoder->valid || decoder->slice >= SS_TRACK_SLICES)
        return false;
    for (int unit = 0; unit < SS_TRACK_SLICE_UNITS; unit++) {
        uint16_t *to = out + unit * SS_TRACK_UNIT_WORDS;
        // (a unit that is not 128 words would have run into the next one's place: the data is not what the stage expects)
        uint16_t scratch[SS_TRACK_UNIT_WORDS + 1];
        const int count = DecodeUnit(decoder, scratch, SS_TRACK_UNIT_WORDS + 1, flipped);
        if (count != SS_TRACK_UNIT_WORDS) {
            decoder->valid = false;
            return false;
        }
        for (int i = 0; i < SS_TRACK_UNIT_WORDS; i++)
            to[i] = flipped ? scratch[SS_TRACK_UNIT_WORDS - 1 - i] : scratch[i];
    }
    decoder->slice++;
    return true;
}

bool SS_DecodeSegment(int segment, bool flipped, uint16_t *out) {
    SSDecoder decoder;
    if (!SSDecoder_Start(&decoder, segment))
        return false;
    for (int slice = 0; slice < SS_TRACK_SLICES; slice++)
        if (!SSDecoder_Slice(&decoder, flipped, out + slice * SS_TRACK_SLICE_WORDS))
            return false;
    return true;
}

void SSTrack_Reset(SSTrack *track) {
    track->step = 0;
    track->index = 0;
    track->list = 0;
    track->segment = -1;
    track->flipped = false;
}

// Offset_0x008B9E: three of the segments turn the track over or back; the others leave it as it is. Segment $11 (the pre-beta's ROM range $0CDEC6 to $0CE544) always turns it over; $0D and $1A turn it
// over when the stage's step (the one the stage is at by now) has its flag set, and back when it has not
static void Turn(SSTrack *track) {
    bool flipped;
    if (SSTrack_Turns(track, &flipped))
        track->flipped = flipped;
}

// Offset_0x005F16's first half. The step's byte gives the list ($FF: the sequence is over and starts again, with the first list); a segment of the list is taken, and the place in the list goes on; at the list's end the
// next step begins
int SSTrack_NextSegment(SSTrack *track) {
    int list = SS_SequenceStep((size_t)track->step);
    if (list == 0xFF) { // (the sequence starts again: from the first list, not the first step's)
        list = 0;
        track->step = 0;
    } else {
        list &= 0x7F;
    }
    size_t size;
    const uint8_t *segments = SS_List(list, &size);
    track->list = list;
    track->segment = segments[track->index];
    track->index++;
    if (track->index == SS_ListLength(list)) {
        track->index = 0;
        track->step++;
    }
    Turn(track);
    return track->segment;
}

bool SSTrack_Turns(const SSTrack *track, bool *flipped) {
    switch (track->segment) {
    case 0x11:
        *flipped = true;
        return true;
    case 0x0D:
    case 0x1A:
        *flipped = (SS_SequenceStep((size_t)track->step) & 0x80) != 0;
        return true;
    default:
        return false;
    }
}
