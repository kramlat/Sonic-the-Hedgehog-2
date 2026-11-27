#include "Constants.h"
#include "Game.h"
#include "Sound.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"

#include "Object/TitleMenu.h"
#include "Resource/Mappings/TitleMenuF.h"

// The menu of the title screen, the final game's object $0F: 1 PLAYER, 2 PLAYER VS and OPTIONS (frames 0 to 2: the lit one is the frame). Up and Down turn it over, round, with the selection blip; Start takes
// the frame (TitleMenu_Choice). Its mappings point at the art by tile number: the menu blocks at $3F2, the words at $402 and the standard font at $680, which the title loads.
static uint8_t title_screen_option;

void Obj_TitleMenu(Sst *self) {
    switch (self->routine) {
    case 0: // Initialization
        self->routine += 2;
        self->x_pos.f.u = 288 + 8 + (PLANE_WIDEADD * 4);
        self->y_pos.f.u = 240 + 92 + SCREEN_TALLADD2;
        self->mappings = Mappings_TitleMenuF;
        self->art_tile = TILE_MAP(0, 0, 0, 0, 0);
        self->render_flags = 0;
        self->priority = 0 * 0x80;
        self->mapping_frame = title_screen_option;
        break;
    case 2: { // Up and Down change the choice
        uint8_t option = title_screen_option;
        const uint8_t press = (uint8_t)(jpad1_press1 | jpad1_press2);
        if ((press & JPAD_UP) && option-- == 0)
            option = 2;
        if ((press & JPAD_DOWN) && ++option >= 3)
            option = 0;
        self->mapping_frame = title_screen_option = option;
        if (press & (JPAD_UP | JPAD_DOWN))
            PlaySound((uint8_t)(sfx_Jump + 8)); // (the final game's Beep, $A8)
        break;
    }
    }
    Sst_Draw(self);
}

void TitleMenu_Reset(void) {
    title_screen_option = 0;
}

int TitleMenu_Choice(void) {
    return title_screen_option;
}

bool TitleMenu_TwoPlayers(void) {
    return title_screen_option == 1;
}
