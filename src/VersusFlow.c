#include "VersusFlow.h"

#include <string.h>

static VsWinner Higher(uint32_t a, uint32_t b) {
    return a == b ? VS_TIE : a > b ? VS_P1 : VS_P2;
}

uint16_t VsFlow_ActEntry(const VsPlayerStats *p1, const VsPlayerStats *p2) {
    const VsWinner w[5] = {
        Higher(p1->score, p2->score),
        Higher(p2->time, p1->time), // (the shorter time wins: loc_90DE with the times the other way round)
        Higher(p1->rings, p2->rings),
        Higher(p1->items, p2->items),
        Higher(p1->total_rings, p2->total_rings),
    };
    unsigned a = 0, b = 0;
    for (int i = 0; i < 5; i++) {
        if (w[i] == VS_P1)
            a++;
        else if (w[i] == VS_P2)
            b++;
    }
    return (uint16_t)((a << 8) | b);
}

uint16_t VsFlow_SpecialEntry(const uint16_t p1[3], const uint16_t p2[3]) {
    unsigned a = 0, b = 0;
    for (int i = 0; i < 3; i++) {
        const VsWinner w = Higher(p1[i], p2[i]);
        if (w == VS_P1)
            a++;
        else if (w == VS_P2)
            b++;
    }
    return (uint16_t)((a << 8) | b);
}

VsWinner VsFlow_EntryWinner(uint16_t entry) {
    if (entry == VS_UNPLAYED || entry == VS_GAME_OVER)
        return VS_TIE;
    return Higher(entry >> 8, entry & 0xFF);
}

void VsFlow_Reset(VsFlow *f) {
    memset(f, 0, sizeof(*f));
    for (int z = 0; z < VS_ZONES; z++)
        for (int p = 0; p < VS_PLAYS; p++)
            f->results[z][p] = VS_UNPLAYED;
}

void VsFlow_Choose(VsFlow *f, int zone) {
    f->zone = zone;
    f->play = 0;
    f->act = 0;
}

void VsFlow_SetEntry(VsFlow *f, uint16_t entry) {
    f->results[f->zone][f->play] = entry;
}

// Whether any entry that the level results page asks about is unplayed (loc_8A9A: the two acts of each of the first three zones, and everything of the special stage box)
static bool AnyUnplayed(const VsFlow *f) {
    for (int z = 0; z < VS_ZONES; z++)
        for (int p = 0; p < (z == 3 ? VS_PLAYS : 2); p++)
            if (f->results[z][p] == VS_UNPLAYED)
                return true;
    return false;
}

bool VsFlow_ZonePlayed(const VsFlow *f, int zone) {
    for (int p = 0; p < (zone == 3 ? VS_PLAYS : 2); p++)
        if (f->results[zone][p] == VS_UNPLAYED)
            return false;
    return true;
}

// loc_8A9A: after the level results (and the special stage's): the zone menu while something is left, the global results when nothing is
static VsNext AfterLevel(const VsFlow *f) {
    if (AnyUnplayed(f))
        return (VsNext){ VSNEXT_ZONE_MENU, 0 };
    return (VsNext){ VSNEXT_PAGE, VS_PAGE_GLOBAL };
}

VsNext VsFlow_Continue(VsFlow *f, VsPage page) {
    switch (page) {
    case VS_PAGE_ACT: {
        if (f->act == 0) { // (the first act: the second)
            f->act = 1;
            f->play = 1;
            return (VsNext){ VSNEXT_ACT2, 0 };
        }
        f->play = 2;
        int a = 0, b = 0;
        for (int p = 0; p < 2; p++) {
            const VsWinner w = VsFlow_EntryWinner(f->results[f->zone][p]);
            if (w == VS_P1)
                a++;
            else if (w == VS_P2)
                b++;
        }
        if (a != b)
            return (VsNext){ VSNEXT_PAGE, VS_PAGE_LEVEL };
        return (VsNext){ VSNEXT_SPECIAL, VS_PAGE_SPECIAL }; // (a tie: the special stage decides)
    }
    case VS_PAGE_LEVEL:
    case VS_PAGE_ALL_SPECIAL:
        return AfterLevel(f);
    case VS_PAGE_GLOBAL:
        return (VsNext){ VSNEXT_SEGA, 0 };
    case VS_PAGE_SPECIAL:
        if (f->zone != 3) // (the tie breaker of a zone: the level results)
            return (VsNext){ VSNEXT_PAGE, VS_PAGE_LEVEL };
        if (f->play == 2) // (the special stage box: after its third round, all of them)
            return (VsNext){ VSNEXT_PAGE, VS_PAGE_ALL_SPECIAL };
        f->play++;
        return (VsNext){ VSNEXT_SPECIAL, VS_PAGE_SPECIAL };
    }
    return (VsNext){ VSNEXT_ZONE_MENU, 0 };
}
