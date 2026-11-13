#include "test.h"

#include <stdlib.h>
#include <string.h>

#include "EnginePLC.h"
#include "Nemesis.h"
#include "PLC.h"

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

void RegisterSpecialStageTests(void) {
    RUN_TEST(SpecialStage_TheMainListsArtDecodesAndGoesWherePreBetaPutsIt);
    RUN_TEST(SpecialStage_TheRingsListGoesOverTheTrafficLights);
}
