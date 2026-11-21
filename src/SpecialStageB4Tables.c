// The tables of Beta 4's special stage: see SpecialStageB4Tables.h
#include "SpecialStageB4Tables.h"
#include "Kosinski.h"

#include "Resource/SSData/B4Map0.h"
#include "Resource/SSData/B4Map1.h"
#include "Resource/SSData/B4Map2.h"

static uint8_t map0[0x2000], map1[0x400], map2[0x2400];
static bool decoded;

static void Decode(void) {
    if (decoded)
        return;
    KosDec(SSData_B4Map0, map0);
    KosDec(SSData_B4Map1, map1);
    KosDec(SSData_B4Map2, map2);
    decoded = true;
}

static uint16_t Word(const uint8_t *p) {
    return (uint16_t)((p[0] << 8) | p[1]);
}

int SS4_KindLength(int kind) {
    static const uint8_t lengths[SS4_KIND_COUNT] = { 0x18, 0x18, 0x0C, 0x10, 0x07, 0x00 };
    return kind >= 0 && kind < SS4_KIND_COUNT ? lengths[kind] : 0;
}

const uint8_t *SS4_StageSteps(int stage, size_t *count) {
    Decode();
    const uint16_t start = Word(map1 + stage * 2);
    const uint16_t end = stage + 1 < SS4_STAGE_COUNT ? Word(map1 + (stage + 1) * 2) : 540;
    *count = (size_t)(end - start);
    return map1 + start;
}

const uint8_t *SS4_StageObjects(int stage, size_t *size) {
    Decode();
    const uint16_t start = Word(map2 + stage * 2);
    const uint16_t end = stage + 1 < SS4_STAGE_COUNT ? Word(map2 + (stage + 1) * 2) : 8708;
    *size = (size_t)(end - start);
    return map2 + start;
}

int SS4_PlaceCount(int segment) {
    Decode();
    return Word(map0 + Word(map0 + segment * 2));
}

const uint8_t *SS4_Place(int segment, int index) {
    Decode();
    return map0 + Word(map0 + segment * 2) + 2 + index * 6;
}
