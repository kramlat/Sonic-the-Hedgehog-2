// The zone icons of the menus: see MenuIcons.h
#include "MenuIcons.h"
#include "Enigma.h"
#include "Nemesis.h"
#include "Palette.h"
#include "Backend/VDP.h"

#include "Resource/S2Menu/LevelIcons.h"
#include "Resource/S2Menu/VsMenu2.h"
#include "Resource/S2Palette/LevelIcons.h"

static uint16_t icon_map[200];

static uint16_t Word(const uint8_t *m, size_t i) {
    return (uint16_t)((m[i * 2] << 8) | m[i * 2 + 1]);
}

void MenuIcons_Load(void) {
    static uint8_t icons[200 * 2];
    EniDec(S2Menu_VsMenu2, icons, 0x80);
    for (int i = 0; i < 200; i++)
        icon_map[i] = Word(icons, (size_t)i);
    VDP_SeekVRAM(0x1000);
    NemDec(S2Menu_LevelIcons); // (tile $80 on)
}

void MenuIcons_SetPalette(int icon) {
    for (int i = 0; i < 16; i++) {
        const uint16_t c = Word(S2Palette_LevelIcons, (size_t)icon * 16 + i);
        dry_palette[2][i] = dry_palette_dup[2][i] = c;
    }
}

uint16_t MenuIcons_Word(int word) {
    return icon_map[word];
}
