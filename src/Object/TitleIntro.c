#include "Constants.h"
#include "Game.h"
#include "Sound.h"
#include "Level.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"
#include "EnginePalette.h"
#include "EngineMacros.h"

#include <string.h>

#include "Object/TitleIntro.h"
#include "Object/TitleMenu.h"
#include "Resource/Mappings/TitleIntro.h"
#include "Resource/Animation/TitleIntro.h"
#include "Resource/S2TitleF/PalSonic.h"
#include "Resource/S2TitleF/PalBack.h"
#include "Resource/S2TitleF/PalEmblem.h"
#include "Resource/S2TitleF/PalStar.h"

// The final game's title screen intro (object $0E, with the palette changer object $C9 and TitleScreen_SetFinalState): Sonic rises over the emblem and waves, then Tails, the stars twinkle, the background
// flashes in and the menu comes up. One object type, its routine from the subtype: 2 Sonic (the conductor: the others are made by it), 4 Tails, 6 the top of the emblem, 8 the flashing star, $A Sonic's hand,
// $C the falling star, $E the masking sprite, $10 Tails' hand. Its art is at tile $150; positions are screen positions as the final game's spriteScreenPositionXCentered / YCentered.
#define TITLE_SPRITES_ART 0x150
#define TI_X(v) (288 + (v) + PLANE_WIDEADD * 4)
#define TI_Y(v) (240 + (v) + SCREEN_TALLADD2)

typedef struct {
    int16_t counter;      // objoff_2A
    int16_t array_index;  // objoff_2C
    uint8_t intro_complete; // objoff_2F
    uint8_t music_playing;  // objoff_30
    int16_t current_frame;  // objoff_34
} Scratch_TitleIntro;

static Sst *intro_sonic, *intro_tails, *intro_emblem_top, *intro_star, *intro_sonic_hand, *intro_tails_hand, *intro_falling_star, *intro_mask;
static bool intro_done;

// The palette changer (object $C9): fades a line in over its steps, from black (up) or from white (down) toward the target colours. Run by the intro object each frame
typedef struct { bool active; int line, time_left, time, amount; bool up; } PalChanger;
static PalChanger changers[2];

static uint16_t PalWord(const uint8_t *p, int i) { return (uint16_t)((p[i * 2] << 8) | p[i * 2 + 1]); }

static void LoadLine(int line, const uint8_t *pal) {
    for (int i = 0; i < 16; i++)
        dry_palette[line][i] = PalWord(pal, i); // (the palettes hold the colour as the machine's 0x0BGR)
}

// What a changer does to one colour: each of its three parts a step (2) toward the target
static uint16_t StepColour(uint16_t cur, uint16_t target, bool up) {
    uint16_t out = 0;
    for (int shift = 0; shift < 12; shift += 4) {
        int c = (cur >> shift) & 0xE, t = (target >> shift) & 0xE;
        if (up && c < t) c += 2;
        if (!up && c > t) c -= 2;
        out |= (uint16_t)(c << shift);
    }
    return out;
}

static void StartChanger(int which, int line, const uint8_t *pal, int time, int amount, bool up) {
    for (int i = 0; i < 16; i++)
        dry_palette_dup[line][i] = PalWord(pal, i);
    changers[which] = (PalChanger){ true, line, time, time, amount, up };
}

static void RunChangers(void) {
    for (int k = 0; k < 2; k++) {
        PalChanger *c = &changers[k];
        if (!c->active || --c->time_left >= 0)
            continue;
        c->time_left = c->time;
        if (--c->amount < 0) {
            c->active = false;
            continue;
        }
        for (int i = 0; i < 16; i++) {
            const uint16_t cur = dry_palette[c->line][i], tgt = dry_palette_dup[c->line][i];
            dry_palette[c->line][i] = StepColour(cur, tgt, c->up);
        }
    }
}

// A child of the intro: its art, mappings and the routine of its subtype
static Sst *MakeChild(uint8_t subtype) {
    Sst *s = Sst_SpawnCode(Obj_TitleIntro, 0, 0);
    if (s != NULL)
        s->subtype = subtype;
    return s;
}

static void InitSprite(Sst *s) {
    s->mappings = Mappings_TitleIntro;
    s->art_tile = TILE_MAP(0, 0, 0, 0, TITLE_SPRITES_ART);
    s->priority = 4 * 0x80;
    s->render_flags = 0;
}

static void MaskSprite(void) {
    intro_mask = MakeChild(0xE);
}

// Start skips the intro to its end state (TitleScreen_SetFinalState): the sprites where they end, the three palette lines, the menu
static void SetFinalState(Sst *self) {
    Scratch_TitleIntro *me = (Scratch_TitleIntro *)self->own.u8;
    if (me->intro_complete)
        return;
    if (!(jpad1_press1 & JPAD_START))
        return;
    jpad1_press1 &= (uint8_t)~JPAD_START; // (the press is this skip's: it does not take the menu)
    jpad1_press2 &= (uint8_t)~JPAD_START;
    me->intro_complete = true;
    intro_done = true;
    self->routine_secondary = 0x10;
    self->mapping_frame = 0x12;
    self->x_pos.f.u = TI_X(-24);
    self->y_pos.f.u = TI_Y(-88);

    Sst *older[] = { intro_sonic_hand, intro_tails, intro_tails_hand, intro_emblem_top }; // (the ones made already: the game's own slots are made over, so none is twice)
    for (unsigned i = 0; i < sizeof(older) / sizeof(older[0]); i++)
        if (older[i] != NULL)
            Sst_Delete(older[i]);
    intro_sonic_hand = MakeChild(0xA);
    if (intro_sonic_hand != NULL) {
        InitSprite(intro_sonic_hand);
        intro_sonic_hand->routine = 0xA;
        intro_sonic_hand->priority = 2 * 0x80;
        intro_sonic_hand->mapping_frame = 9;
        intro_sonic_hand->routine_secondary = 4;
        intro_sonic_hand->x_pos.f.u = TI_X(33);
        intro_sonic_hand->y_pos.f.u = TI_Y(-47);
    }
    intro_tails = MakeChild(4);
    if (intro_tails != NULL) {
        InitSprite(intro_tails);
        intro_tails->routine = 4;
        intro_tails->mapping_frame = 4;
        intro_tails->routine_secondary = 8; // (the game's 6 would make his hand again: it is made here)
        intro_tails->priority = 3 * 0x80;
        intro_tails->x_pos.f.u = TI_X(-88);
        intro_tails->y_pos.f.u = TI_Y(-80);
    }
    intro_tails_hand = MakeChild(0x10);
    if (intro_tails_hand != NULL) {
        InitSprite(intro_tails_hand);
        intro_tails_hand->routine = 0x10;
        intro_tails_hand->priority = 2 * 0x80;
        intro_tails_hand->mapping_frame = 0x13;
        intro_tails_hand->routine_secondary = 4;
        intro_tails_hand->x_pos.f.u = TI_X(-19);
        intro_tails_hand->y_pos.f.u = TI_Y(-31);
    }
    intro_emblem_top = MakeChild(6);
    if (intro_mask == NULL)
        MaskSprite();
    Sst_SpawnCode(Obj_TitleMenu, 0, 0);
    changers[0].active = changers[1].active = false;
    LoadLine(3, S2TitleF_PalEmblem);
    LoadLine(2, S2TitleF_PalBack);
    LoadLine(0, S2TitleF_PalSonic);
    for (int l = 0; l < 4; l++) // (the targets are the lines as they stand)
        memcpy(dry_palette_dup[l], dry_palette[l], sizeof(dry_palette[l]));
    if (!me->music_playing)
        PlayMusic(bgm_Title);
}

// The positions the objects move through, four frames to a step (the final game's Obj0E_Move)
static void Move(Sst *self, const int16_t (*positions)[2], int count) {
    Scratch_TitleIntro *me = (Scratch_TitleIntro *)self->own.u8;
    me->counter++;
    if ((me->counter & 3) == 0) {
        me->array_index += 4;
        if (me->array_index >= count * 4 + 4) {
            self->routine_secondary += 2; // (the end of the list)
        } else {
            const int i = me->array_index / 4 - 1;
            self->x_pos.f.u = TI_X(positions[i][0]);
            self->y_pos.f.u = TI_Y(positions[i][1]);
        }
    }
    Sst_Draw(self);
}

static const int16_t sonic_positions[][2] = { {-24,-32}, {-32,-48}, {-40,-64}, {-42,-74}, {-38,-82}, {-32,-86}, {-28,-87}, {-24,-88} };
static const int16_t tails_positions[][2] = { {-73,-40}, {-77,-56}, {-82,-68}, {-84,-74}, {-86,-78}, {-87,-79}, {-88,-80} };
static const int16_t star_positions[][2] = { {-70,2}, {80,8}, {18,65}, {126,-78}, {-96,-13}, {96,-16}, {-19,75}, {-96,-69}, {69,23} };
static const int16_t sonic_hand_positions[][2] = { {35,-47}, {32,-46}, {33,-47} };
static const int16_t tails_hand_positions[][2] = { {-20,-32}, {-19,-31} };

static void Obj_Sonic(Sst *self) {
    Scratch_TitleIntro *me = (Scratch_TitleIntro *)self->own.u8;
    me->current_frame++;
    if (me->current_frame < 288)
        SetFinalState(self);
    switch (self->routine_secondary) {
    case 0: // Init: the star and the emblem top are made, and the sparkle
        self->routine_secondary += 2;
        self->mapping_frame = 5;
        self->x_pos.f.u = TI_X(-16);
        self->y_pos.f.u = TI_Y(-16);
        intro_star = MakeChild(8);
        intro_emblem_top = MakeChild(6);
        PlaySound((uint8_t)(sfx_Jump + 7)); // (the final game's Sparkle, $A7)
        break;
    case 2: // fade in and play the music
        if (me->current_frame >= 56) {
            self->routine_secondary += 2;
            StartChanger(0, 3, S2TitleF_PalEmblem, 2, 0x15, true);
            me->music_playing = true;
            PlayMusic(bgm_Title);
        }
        break;
    case 4: // Sonic's palette
        if (me->current_frame >= 128) {
            self->routine_secondary += 2;
            LoadLine(0, S2TitleF_PalSonic);
            memcpy(dry_palette_dup[0], dry_palette[0], sizeof(dry_palette[0]));
            MaskSprite();
        }
        break;
    case 6: // rising
        Move(self, sonic_positions, 8);
        return;
    case 8: // waving
        Sst_Animate(self, Animation_TitleIntro);
        Sst_Draw(self);
        return;
    case 0xA: // the wave is done: his hand
        self->routine_secondary += 2;
        self->mapping_frame = 0x12;
        intro_sonic_hand = MakeChild(0xA);
        break;
    case 0xC: // Tails comes
        if (me->current_frame >= 192) {
            self->routine_secondary += 2;
            intro_tails = MakeChild(4);
        }
        break;
    case 0xE: // the background flashes in, the menu comes up
        if (me->current_frame >= 288) {
            self->routine_secondary += 2;
            me->array_index = 0;
            me->intro_complete = true;
            intro_done = true;
            for (int i = 0; i < 16; i++)
                dry_palette[2][i] = 0x0EEE;
            StartChanger(1, 2, S2TitleF_PalBack, 4, 7, false);
            Sst_SpawnCode(Obj_TitleMenu, 0, 0);
        }
        break;
    case 0x10: // the falling star is made when the music gets there (NTSC)
        if (me->current_frame == 464) {
            intro_falling_star = MakeChild(0xC);
            self->routine_secondary += 2;
            if (intro_mask != NULL) {
                Sst_Delete(intro_mask);
                intro_mask = NULL;
            }
        }
        break;
    case 0x12: // the star sparkles: a colour of its cycle every eighth frame
        if ((frame_count & 7) == 0) {
            me->array_index += 2;
            if (me->array_index >= 6 * 2)
                me->array_index = 0;
            dry_palette[2][5] = PalWord(S2TitleF_PalStar, me->array_index / 2);
        }
        break;
    }
    Sst_Draw(self);
}

void TitleIntro_Start(void) {
    intro_sonic = intro_tails = intro_emblem_top = intro_star = intro_sonic_hand = intro_tails_hand = intro_falling_star = intro_mask = NULL;
    intro_done = false;
    changers[0].active = changers[1].active = false;
    intro_sonic = MakeChild(2);
}

bool TitleIntro_Complete(void) {
    return intro_done;
}

void Obj_TitleIntro(Sst *self) {
    Scratch_TitleIntro *me = (Scratch_TitleIntro *)self->own.u8;
    switch (self->routine) {
    case 0: // Init: the routine is the subtype
        InitSprite(self);
        self->routine = self->subtype;
        me->counter = me->array_index = me->current_frame = 0;
        me->intro_complete = me->music_playing = 0;
        Obj_TitleIntro(self);
        return;
    case 2:
        RunChangers(); // (the conductor runs the palette changers' work)
        Obj_Sonic(self);
        return;
    case 4: // Tails
        switch (self->routine_secondary) {
        case 0:
            self->routine_secondary += 2;
            self->priority = 3 * 0x80;
            self->x_pos.f.u = TI_X(-72);
            self->y_pos.f.u = TI_Y(-24);
            self->anim = 1;
            break;
        case 2:
            Move(self, tails_positions, 7);
            return;
        case 4:
            Sst_Animate(self, Animation_TitleIntro);
            break;
        case 6: // the wave is done: his hand
            self->routine_secondary += 2;
            intro_tails_hand = MakeChild(0x10);
            break;
        }
        Sst_Draw(self);
        return;
    case 6: // the top of the emblem
        if (self->routine_secondary == 0) {
            self->mapping_frame = 0xB; // (with the trademark sign: the overseas one)
            self->priority = 2 * 0x80;
            self->x_pos.f.u = TI_X(0);
            self->y_pos.f.u = TI_Y(-8);
            self->routine_secondary += 2;
        }
        Sst_Draw(self);
        return;
    case 8: // the flashing star
        switch (self->routine_secondary) {
        case 0:
            self->routine_secondary += 2;
            self->mapping_frame = 0xC;
            self->art_tile |= 0x8000;
            self->anim = 2;
            self->priority = 1 * 0x80;
            self->x_pos.f.u = TI_X(-32);
            self->y_pos.f.u = TI_Y(-72);
            me->counter = 4;
            break;
        case 2:
            Sst_Animate(self, Animation_TitleIntro);
            break;
        case 4:
            if (--me->counter < 0)
                self->routine_secondary += 2;
            break;
        case 6: { // the next place, and the animation again
            self->routine_secondary = 2;
            self->anim_frame = 0;
            self->anim_frame_timer = 0;
            me->counter = 6;
            me->array_index += 4;
            if (me->array_index >= 9 * 4 + 4) {
                Sst_Delete(self);
                return;
            }
            self->x_pos.f.u = TI_X(star_positions[me->array_index / 4 - 1][0]);
            self->y_pos.f.u = TI_Y(star_positions[me->array_index / 4 - 1][1]);
            PlaySound((uint8_t)(sfx_Jump + 7));
            break;
        }
        }
        Sst_Draw(self);
        return;
    case 0xA: // Sonic's hand
        if (self->routine_secondary == 0) {
            self->routine_secondary += 2;
            self->mapping_frame = 9;
            self->priority = 2 * 0x80;
            self->x_pos.f.u = TI_X(37);
            self->y_pos.f.u = TI_Y(-49);
        }
        if (self->routine_secondary == 2) {
            Move(self, sonic_hand_positions, 3);
            return;
        }
        Sst_Draw(self);
        return;
    case 0x10: // Tails' hand
        if (self->routine_secondary == 0) {
            self->routine_secondary += 2;
            self->mapping_frame = 0x13;
            self->priority = 2 * 0x80;
            self->x_pos.f.u = TI_X(-17);
            self->y_pos.f.u = TI_Y(-27);
        }
        if (self->routine_secondary == 2) {
            Move(self, tails_hand_positions, 2);
            return;
        }
        Sst_Draw(self);
        return;
    case 0xC: // the falling star
        if (self->routine_secondary == 0) {
            self->routine_secondary += 2;
            self->mapping_frame = 0xC;
            self->priority = 5 * 0x80;
            self->x_pos.f.u = TI_X(80);
            self->y_pos.f.u = TI_Y(-112);
            self->anim = 3;
            me->counter = 140;
            Sst_Draw(self);
            return;
        }
        if (--me->counter < 0) {
            Sst_Delete(self);
            return;
        }
        self->x_pos.f.u -= 2;
        self->y_pos.f.u += 1;
        Sst_Animate(self, Animation_TitleIntro);
        Sst_Draw(self);
        return;
    case 0xE: // the masking sprite
        if (self->routine_secondary == 0) {
            self->routine_secondary += 2;
            self->art_tile = TILE_MAP(0, 0, 0, 0, 0);
            self->mapping_frame = 0x11;
            self->priority = 2 * 0x80;
            self->x_pos.f.u = TI_X(-32);
            self->y_pos.f.u = TI_Y(0);
        }
        Sst_Draw(self);
        return;
    }
    Sst_Draw(self);
}
