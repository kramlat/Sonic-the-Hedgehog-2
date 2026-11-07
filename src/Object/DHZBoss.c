// Dust Hill's boss for Sonic 2, as the pre-beta has it (object 57, Obj_0x57_DHz_Boss): see DHZBoss.h. It uses the same boss helpers as the other bosses (BossCommon): the ship's place and speed in a buffer, and the
// animation of its parts, each a pair of nibbles in a small buffer. Its parts (the front drill, the ship's body, Robotnik, the back drill) are the object's child sprites.
#include "Object/DHZBoss.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"
#include "Object/BossCommon.h"
#include "Constants.h"

#include "EngineObject.h"
#include "EnginePalette.h"
#include "HTZQuake.h"
#include "Level.h"
#include "MathUtil.h"
#include "Sound.h"
#include "Backend/VDP.h"

#include <string.h>

#include "Macros.h"

#include "Resource/Animation/DHZBoss.h"
#include "Resource/Art/DHZBossRock.h"
#include "Resource/Mappings/DHZBoss.h"

#define ArtTile_DHZBoss     0x3C0 // ($7800: list $2C)
#define ArtTile_DHZBossRock 0x560 // (where the falling rocks' mappings point; the eight tiles of their art go to VRAM $AC00, over the animals' first art)

enum { Boss_Start = 0, Boss_Run = 2, Boss_Rock = 4 };
enum { Run_Rise = 0, Run_Down = 2, Run_Charge = 4, Run_Across = 6, Run_Wreck = 8, Run_Fall = 0xA, Run_Leave = 0xC };
enum { Part_FrontDrill = 0, Part_Body = 1, Part_Robotnik = 2, Part_BackDrill = 3 };

typedef struct {
    Own_AlphaBoss boss; // flag: a character has been hurt by it (var_0C: it stops its charge), side: it has been beaten (var_00), flash: Obj_Inertia
    int16_t back_speed;  // 0x2E: how fast the back drill falls when the wreck lets go of it
    int16_t front_speed; // 0x30: and the front
    int32_t front_y;     // 0x34 (Control_Var_08): the front drill's place as 16.16
    int32_t back_y;      // 0x3A (Control_Var_0E)
} Own_DHZBoss;

// The palette the boss's list $19 sets over the second line (PalLoad2)
static const uint16_t boss_palette[16] = {
    0x0002, 0x0000, 0x006C, 0x008E, 0x00CE, 0x0044, 0x0EEE, 0x0AAA, 0x0888, 0x0444, 0x0666, 0x0006, 0x00EE, 0x0088, 0x002E, 0x002A,
};

static uint8_t drill_out;   // $F73F: the front drill is out (the box in front of the ship hurts instead of the two on top)
static uint8_t dle_counter; // $F73A

void DHZBoss_Reset(void) {
    drill_out = 0;
    dle_counter = 0;
}

// DynResize_DHz of act 2: X $2080 fixes the left edge and the lower limit ($5D0); X $20F0 shuts the arena, fades the music, puts the rocks' eight tiles at VRAM $AC00, sets Boss_Flag 5, the boss's list and its palette;
// then the camera's top is held at Y $5C8 and, $5A frames on, the boss is made and its music starts; while it digs the screen shakes with the sound of the ground; when it is beaten, the results card
void DHZBoss_Events(void) {
    switch (dle_routine) {
    case 0:
        if (scrpos_x.f.u < (0x2080 - SCREEN_WIDEADD2))
            break;
        limit_left2 = scrpos_x.f.u;
        limit_btm1 = 0x5D0 - SCREEN_TALLADD;
        dle_routine += 2;
        break;
    case 2:
        if (scrpos_x.f.u < (0x20F0 - SCREEN_WIDEADD2))
            break;
        limit_right1 = limit_right2 = 0x20F0 + SCREEN_WIDEADD2;
        limit_left2 = 0x20F0 - SCREEN_WIDEADD2;
        dle_routine += 2;
        QueueSound1(bgm_Fade);
        dle_counter = 0;
        VDP_SeekVRAM(0xAC00); // (the rocks' eight tiles)
        VDP_WriteVRAM(Art_DHZBossRock, sizeof(Art_DHZBossRock));
        boss_flag = 5;
        AddPLC(PlcId_DHZBoss);
        for (int i = 0; i < 16; i++)
            dry_palette[1][i] = boss_palette[i];
        break;
    case 4:
        if (scrpos_y.f.u >= (0x5C8 - SCREEN_TALLADD))
            limit_top2 = 0x5C8 - SCREEN_TALLADD;
        if (++dle_counter >= 0x5A) {
            Sst *boss = Sst_Allocate();
            if (boss != NULL)
                boss->code = sst_object_codes[ObjId_DHZBoss];
            dle_routine += 2;
            QueueSound1(bgm_Boss);
        }
        break;
    case 6:
        if (dhz_shake && (frame_count & 0x1F) == 0)
            QueueSound2((enum SoundID)(sfx_Jump + (0xE1 - 0xA0))); // (the ground rumbles: $E1)
        if (boss_status)
            GotThroughAct();
        break;
    }
}

// The boxes of the drills (the touch code of Boss_Flag 5, Offset_0x0385F8): it clears the boss's "a character is hurt" flag at each touch check, and sets it again where the character has just been hurt ($78 frames
// of safety left): with the drill out, one box $30 in front of the ship, 4 across and 4 high each way; with it in, two on top, $14 to either side and $20 up, 4 across and $10 high each way. They hurt.
bool DHZBoss_Box(Sst *hit, int index, int16_t *centre_x, int16_t *centre_y, int16_t *half_width, int16_t *half_height) {
    if (hit->code != sst_object_codes[ObjId_DHZBoss] || hit->routine != Boss_Run || boss_flag != 5)
        return false;
    if (index == 0)
        ((Own_DHZBoss *)hit->own.u8)->boss.flag = 0; // (sf $38(A1))
    if (drill_out) {
        if (index != 0)
            return false;
        *centre_x = (int16_t)(hit->x_pos.f.u + (((hit->render_flags & SST_RENDER_X_FLIP) != 0) ? 0x30 : -0x30));
        *centre_y = (int16_t)(hit->y_pos.f.u + 4);
        *half_width = 4;
        *half_height = 4;
        return true;
    }
    if (index > 1)
        return false;
    *centre_x = (int16_t)(hit->x_pos.f.u + (index == 0 ? 0x14 : -0x14));
    *centre_y = (int16_t)(hit->y_pos.f.u - 0x20);
    *half_width = 4;
    *half_height = 0x10;
    return true;
}

void DHZBoss_AfterTouch(Sst *hit, uint16_t invulnerable_time) {
    if (hit->code == sst_object_codes[ObjId_DHZBoss] && hit->routine == Boss_Run && boss_flag == 5 && invulnerable_time == 0x78)
        ((Own_DHZBoss *)hit->own.u8)->boss.flag = 0xFF; // (st $38(A1))
}

// The animation buffer's nibble of a part, set to another animation (the buffer holds a pair of bytes for the object's own sprite and then each child's: the low nibble of the first is the animation)
static void SetPartAnim(int part, uint8_t anim) {
    boss_anim[(part + 1) * 2] = (uint8_t)((boss_anim[(part + 1) * 2] & 0xF0) | anim);
}

// Offset_0x026CF4: where the parts are, from the ship's place; once it has been beaten the drills fall (the back first)
static void Boss_Parts(Sst *self, Own_DHZBoss *own) {
    const int16_t x = self->x_pos.f.u, y = self->y_pos.f.u;
    self->children[Part_Body].x = x;
    self->children[Part_Body].y = y;
    self->children[Part_Robotnik].x = x;
    self->children[Part_Robotnik].y = y;
    if (!own->boss.side) {
        self->children[Part_BackDrill].x = x;
        self->children[Part_BackDrill].y = y;
        self->children[Part_FrontDrill].x = x;
        self->children[Part_FrontDrill].y = y;
        own->back_y = (int32_t)((uint32_t)(uint16_t)y << 16) | (own->back_y & 0xFFFF);
        own->front_y = (int32_t)((uint32_t)(uint16_t)y << 16) | (own->front_y & 0xFFFF);
        self->children[Part_BackDrill].x = (int16_t)(x + (((self->render_flags & SST_RENDER_X_FLIP) != 0) ? 0x28 : -0x28));
        return;
    }
    if (boss_move.timer > 0x78)
        return;
    self->children[Part_BackDrill].x--;
    own->back_y += (int32_t)own->back_speed << 8;
    own->back_speed += 0x38;
    self->children[Part_BackDrill].y = (int16_t)(own->back_y >> 16);
    if (self->children[Part_BackDrill].y >= 0x6F0)
        own->back_speed = 0;
    self->children[Part_FrontDrill].x++;
    own->front_y += (int32_t)own->front_speed << 8;
    own->front_speed += 0x38;
    self->children[Part_FrontDrill].y = (int16_t)(own->front_y >> 16);
    if (self->children[Part_FrontDrill].y >= 0x6F0)
        own->front_speed = 0;
}

// Offset_0x02C902: a rock comes loose at a random place along the arena: every $20 frames a dangerous one (frame $14, collision $AD), and every eighth frame in between a harmless one (frame $D, no collision)
static void Boss_DropRock(void) {
    const uint8_t f = (uint8_t)(frame_count & 0x1F);
    bool harmless = false;
    if (f != 0) {
        if (f & 7)
            return;
        harmless = true;
    }
    int16_t x;
    do {
        x = (int16_t)((RandomNumber() & 0x1FF) + 0x20F0);
    } while (x > 0x2230);
    Sst *rock = Sst_Allocate();
    if (rock == NULL)
        return;
    rock->code = sst_object_codes[ObjId_DHZBoss];
    rock->routine = Boss_Rock;
    rock->x_pos.f.u = x;
    rock->y_pos.f.u = 0x5F0;
    rock->mappings = Mappings_DHZBoss;
    rock->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_DHZBossRock);
    rock->render_flags |= SST_RENDER_LEVEL;
    rock->priority = 3 * 0x80;
    rock->mapping_frame = 0x0D;
    if (!harmless) {
        rock->mapping_frame = 0x14;
        rock->collision_flags = 0xAD;
    }
}

// Offset_0x02C976 (and Offset_0x02C9AC): the hit (a flash of $40 frames, or the wreck when the hits are out) and Robotnik's face at the first frame of a flash
static void Boss_Struck(Sst *self, Own_DHZBoss *own) {
    Own_AlphaBoss *boss = &own->boss;
    if (boss->anim_routine < Run_Wreck) {
        if (self->collision_property == 0) {
            AddPoints(100);
            boss_move.timer = 0xB3;
            boss->anim_routine = Run_Wreck;
        } else if (self->collision_flags == 0) {
            if (boss->flash == 0) {
                boss->flash = 0x40;
                QueueSound2(sfx_HitBoss);
            }
            dry_palette[1][1] = (dry_palette[1][1] == 0) ? 0x0EEE : 0;
            if (--boss->flash == 0)
                self->collision_flags = 0xF;
        }
    }
    if (boss->flash == 0x3F)
        boss_anim[7] = 0xC0;
}

// What every frame of the digging and charging ends with
static void Boss_Show(Sst *self, Own_DHZBoss *own) {
    self->y_pos.f.u = boss_move.y.f.u;
    self->x_pos.f.u = boss_move.x.f.u;
    Boss_Struck(self, own);
    Boss_AnimateSprite(self, &own->boss, Animation_DHZBoss);
    Boss_Parts(self, own);
    Sst_Draw(self);
}

// What the wreck's frames end with (no hit, no animation of its own)
static void Boss_ShowWreck(Sst *self, Own_DHZBoss *own) {
    self->y_pos.f.u = boss_move.y.f.u;
    self->x_pos.f.u = boss_move.x.f.u;
    Boss_Parts(self, own);
    Sst_Draw(self);
}

static void SetNibble(int pair, uint8_t anim) {
    boss_anim[pair] = (uint8_t)((boss_anim[pair] & 0xF0) | anim);
}

static void Boss_Dig(Sst *self, Own_DHZBoss *own) {
    Own_AlphaBoss *boss = &own->boss;

    switch (boss->anim_routine) {
    case Run_Rise: // waits, up out of the floor to Y $560, and then down again on the side that Sonic is not on
        if (--boss_move.timer < 0) {
            boss_anim[5] = 0;
            Boss_Move();
            if (boss_move.y.f.u <= 0x560) {
                boss_move.ysp = 0x100;
                boss_move.x.f.u = player->x_pos.f.u >= 0x2190 ? 0x2120 : 0x2200;
                boss->anim_routine += 2;
                SST_RENDER_SET(self, SST_RENDER_X_FLIP, (int16_t)(player->x_pos.f.u - boss_move.x.f.u) >= 0);
            }
        }
        if (boss_move.timer == 0x28)
            drill_out = 0;
        if (boss_move.y.f.u < 0x620) {
            dhz_shake = 1;
            Boss_DropRock();
        }
        Boss_Show(self, own);
        break;

    case Run_Down: // down again, the rocks coming loose
        Boss_Move();
        Boss_DropRock();
        if (boss_move.y.f.u >= 0x620) {
            boss->anim_routine += 2;
            dhz_shake = 0;
        }
        Boss_Show(self, own);
        break;

    case Run_Charge: // on to its floor ($660): the drills spin faster, Robotnik laughs, and it turns to Sonic and sets off
        Boss_Move();
        if (boss_move.y.f.u >= 0x660) {
            boss_move.y.f.u = 0x660;
            boss->anim_routine += 2;
            SetNibble(2, 6);
            SetNibble(8, 6);
            SetNibble(6, 0x0D);
            boss_anim[5] = 0x20;
            boss_move.timer = 0x64;
            boss_anim[1] = 0x30;
            SST_RENDER_SET(self, SST_RENDER_X_FLIP, (int16_t)(player->x_pos.f.u - boss_move.x.f.u) >= 0);
            boss_move.xsp = (self->render_flags & SST_RENDER_X_FLIP) ? 0x0200 : (int16_t)0xFE00;
            boss_move.ysp = 0;
        }
        Boss_Show(self, own);
        break;

    case Run_Across: // a moment, the drill out for a while, and then along the floor to the wall (or until a character has been hurt)
        boss_move.timer--;
        if (boss_move.timer > 0x28 || (drill_out = 1, boss_move.timer >= 0)) {
            Boss_Show(self, own);
            break;
        }
        {
            bool done = false;
            if (boss->flag) {
                boss->flag = 0;
                boss_anim[7] = 0x30;
                done = true;
            } else {
                Boss_Move();
                if (boss_move.x.f.u <= 0x2120) {
                    boss_move.x.f.u = 0x2120;
                    done = true;
                } else if (boss_move.x.f.u >= 0x2200) {
                    boss_move.x.f.u = 0x2200;
                    done = true;
                }
            }
            if (done) {
                boss_move.xsp = 0;
                boss->anim_routine = Run_Rise;
                SetNibble(2, 0x0B);
                SetNibble(8, 0x0B);
                boss_anim[1] = 0;
                SetNibble(6, 0x0D);
                boss_move.timer = 0x64;
                boss_move.ysp = (int16_t)0xFF40;
            }
        }
        Boss_Show(self, own);
        break;

    case Run_Wreck: // blowing up where it is
        boss->side = 0xFF;
        dhz_shake = 0;
        if (--boss_move.timer >= 0) {
            self->children[Part_Robotnik].frame = 0x13;
            self->mapping_frame = 7; // (the wreck's main sprite)
            Boss_Defeated(self);
        } else {
            SST_RENDER_SET(self, SST_RENDER_X_FLIP, true);
            boss_move.xsp = 0;
            boss_move.ysp = 0;
            boss->anim_routine += 2;
            self->children[Part_Robotnik].frame = 0x12;
            boss_move.timer = -0x12;
        }
        Boss_ShowWreck(self, own);
        break;

    case Run_Fall: // the ship drops a little (while it is under Y $620 it sinks until it reaches it) and hovers, and then goes
        boss_move.timer++;
        if (boss_move.timer == 0) {
            boss_move.ysp = 0;
        } else if (boss_move.timer > 0) {
            if (boss_move.timer < 0x18) {
                boss_move.ysp -= 8;
            } else if (boss_move.timer == 0x18) {
                boss_move.ysp = 0;
            } else if (boss_move.timer >= 0x20) {
                boss_anim[7] = 0x0D;
                boss_anim[0] = 2; // (the pre-beta writes this through a register that was left pointing there)
                boss_anim[1] = 0;
                boss->anim_routine += 2;
            }
        } else {
            if ((uint16_t)boss_move.y.f.u < 0x620)
                boss_move.timer--;
            boss_move.ysp += 0x10;
        }
        Boss_Move();
        Boss_ShowWreck(self, own);
        break;

    case Run_Leave: // up and away to the right, the arena opening to $22F0 as it goes; when it is off the screen the act is over
        boss_move.xsp = 0x400;
        boss_move.ysp = (int16_t)0xFFC0;
        if (limit_right2 != 0x22F0 + SCREEN_WIDEADD2) {
            limit_right1 = limit_right2 += 2;
        } else if (!(self->render_flags & SST_RENDER_ON_SCREEN)) {
            if (!boss_status)
                boss_status = 1;
            Sst_Delete(self);
            return;
        }
        Boss_Move();
        self->y_pos.f.u = boss_move.y.f.u;
        self->x_pos.f.u = boss_move.x.f.u;
        Boss_AnimateSprite(self, boss, Animation_DHZBoss);
        Boss_Parts(self, own);
        Sst_Draw(self);
        break;
    }
}

void Obj_DHZBoss(Sst *self) {
    Own_DHZBoss *own = (Own_DHZBoss *)self->own.u8;

    switch (self->routine) {
    case Boss_Start:
        self->mappings = Mappings_DHZBoss;
        self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_DHZBoss);
        self->render_flags |= SST_RENDER_LEVEL | SST_RENDER_MULTI_DRAW;
        self->priority = 3 * 0x80;
        self->width_pixels = 0x60;
        self->y_radius = 0x40;
        self->x_pos.f.u = 0x21A0;
        self->y_pos.f.u = 0x560;
        self->mapping_frame = 5; // (Obj_Boss_Ani_Map, the main sprite's frame: the ship's flames, animated by the first pair of the animation buffer)
        self->routine = Boss_Run;
        own->boss.anim_routine = Run_Down;
        self->child_count = 4;
        self->collision_flags = 0x0F;
        self->collision_property = 8;
        boss_move.x.f.u = self->x_pos.f.u;
        boss_move.y.f.u = self->y_pos.f.u;
        boss_move.ysp = 0x00C0;
        boss_move.xsp = 0;
        drill_out = 0;
        dhz_shake = 1;
        for (int i = 0; i < 4; i++) {
            self->children[i].x = self->x_pos.f.u;
            self->children[i].y = self->y_pos.f.u;
        }
        self->children[Part_FrontDrill].frame = 2;
        self->children[Part_Body].frame = 1;
        self->children[Part_Robotnik].frame = 0xE;
        self->children[Part_BackDrill].frame = 2;
        self->children[Part_BackDrill].x -= 0x28;
        boss_move.timer = 0x28;
        own->back_speed = (int16_t)0xFC80;
        own->front_speed = (int16_t)0xFC80;
        own->front_y = (int32_t)((uint32_t)(uint16_t)self->y_pos.f.u << 16);
        own->back_y = own->front_y;
        {
            static const uint8_t buffer[10] = { 0x02, 0x00, 0x03, 0x00, 0x10, 0x00, 0x0D, 0x00, 0x03, 0x00 };
            memcpy(boss_anim, buffer, sizeof(buffer));
        }
        break;

    case Boss_Run:
        Boss_Dig(self, own);
        break;

    case Boss_Rock: // a rock that has come loose from the ceiling: it falls, a little less quickly than things usually do ($38 down and $28 back up a frame)
        Sst_MoveSprite(self);
        self->y_vel -= 0x28;
        if (self->y_pos.f.u > 0x6F0)
            Sst_Delete(self);
        else
            Sst_Draw(self);
        break;
    }
}
