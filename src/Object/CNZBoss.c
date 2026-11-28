// Casino Night's boss for Sonic 2, as Beta 4 has it (object 51, Obj_0x51_CNz_Boss; the pre-beta's was a different fight): see CNZBoss.h. Like Dust Hill's it uses the shared boss helpers (BossCommon): the ship's place and speed in a buffer, and the
// animation of its parts, each a pair of nibbles in a small buffer. Its parts are the object's child sprites: the right catcher, the ship, Robotnik and the left catcher (its own sprite is the flame under the ship).
#include "SplitScreen.h"
#include "Object/CNZBoss.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"
#include "Object/BossCommon.h"
#include "Object/Tails.h"
#include "Object/Sonic.h"
#include "Constants.h"

#include "EngineObject.h"
#include "EnginePalette.h"
#include "HTZQuake.h"
#include "Level.h"
#include "LevelData.h"
#include "LevelCollision.h"
#include "MathUtil.h"
#include "Sound.h"

#include <string.h>

#include "Macros.h"

#include "Resource/Animation/CNZBoss.h"
#include "Resource/Mappings/CNZBoss.h"

#define ArtTile_CNZBoss 0x3A7 // (the art word $03A7: the mapping adds its own tile numbers, which reach the ship at $A000 and the boss at $80E0 of list $2D)

enum { Boss_Start = 0, Boss_Run = 2, Boss_Ball = 4 };
// What the boss is doing (Beta 4's $26: loc_2DF46 and on)
enum { Run_Patrol = 0, Run_Drop = 2, Run_Dive = 4, Run_Wreck = 6, Run_Fall = 8, Run_Leave = 0xA };
// Which way it patrols (the byte at $38)
enum { Path_Left = 0, Path_Right = 2 };
enum { Part_RightCatcher = 0, Part_Ship = 1, Part_Robotnik = 2, Part_LeftCatcher = 3 };
enum { Ball_Make = 0, Ball_Follow = 2, Ball_Fall = 4, Ball_Away = 6 };

typedef struct {
    Own_AlphaBoss boss; // flag: the way it patrols (0x38), side: it has been beaten (0x2C), count: it is coming back up from a dive (0x3E), flash: Obj_Inertia
    int16_t left_speed;     // 0x2E: how fast the left catcher falls when the wreck lets go of it
    int16_t right_speed;    // 0x30
    int32_t right_y;        // 0x34 (Control_Var_08)
    int32_t left_y;         // 0x3A (Control_Var_0E)
    uint8_t dives;          // 0x2D: how many times it has let the ball fall since it turned round
    uint8_t cooldown;       // 0x3F: frames before it looks for Sonic again
} Own_CNZBoss;

// The ball's own: it shares the object's scratch memory with the ship's
typedef struct {
    int16_t drop;       // 0x28: how far it has fallen from the catcher, a pixel a frame up to $2E
    int16_t x_speed;    // 0x10
    int16_t y_speed;    // 0x12
    uint8_t parent;     // 0x34: the slot of the boss it fell from
} Own_CNZBall;

static uint8_t catcher_state; // $F73F: 0 the catchers do not hurt, 1 they are shut, 2 open (the box under the ship hurts)
static uint8_t dle_counter;   // $F73A

void CNZBoss_Reset(void) {
    catcher_state = 0;
    dle_counter = 0;
}

// DynResize_CNz of act 2: X $27C0 fixes the left edge and the lower limit and closes the arena's first wall (the layout's chunk $F9 at ($54,12)); X $2890 closes the second, fixes the edges ($2860 to $28E0), fades
// the music, sets Boss_Flag 6 and loads the boss's list; once the camera is at Y $4E0 its top is held, and $5A frames on the boss is made and its music starts; when it is beaten the camera is let go (Beta 4: the capsule
// in the layout ends the act, not the events as in the pre-beta)
void CNZBoss_Events(void) {
    switch (dle_routine) {
    case 0:
        if (Boss_Behind((0x27C0 - SCREEN_WIDEADD2)))
            break;
        limit_left2 = scrpos_x.f.u;
        limit_btm1 = 0x062E - SCREEN_TALLADD;
        LevelLayout_SetChunk(LEVEL_LAYOUT_PLANE_FG, 0x54, 0x0C, 0xF9);
        dle_routine += 2;
        break;
    case 2:
        if (Boss_Behind((0x2890 - SCREEN_WIDEADD2)))
            break;
        LevelLayout_SetChunk(LEVEL_LAYOUT_PLANE_FG, 0x50, 0x0C, 0xF9);
        limit_left2 = 0x2860 - SCREEN_WIDEADD2;
        limit_right2 = 0x28E0 + SCREEN_WIDEADD2;
        dle_routine += 2;
        QueueSound1(bgm_Fade);
        dle_counter = 0;
        boss_flag = 6;
        AddPLC(PlcId_CNZBoss);
        break;
    case 4:
        if (scrpos_y.f.u >= (0x4E0 - SCREEN_TALLADD))
            limit_top2 = 0x4E0 - SCREEN_TALLADD;
        if (++dle_counter >= 0x5A) {
            Sst *boss = Sst_Allocate();
            if (boss != NULL)
                boss->code = sst_object_codes[ObjId_CNZBoss];
            dle_routine += 2;
            QueueSound1(bgm_Boss);
        }
        break;
    case 6: // once the camera is past $2A00 its lower limit is let down to $5D0 and the left edge follows it, the right limit stays where it is (loc_D092)
        if (!Boss_Behind((0x2A00 - SCREEN_WIDEADD2))) {
            limit_btm1 = 0x5D0 - SCREEN_TALLADD;
            limit_left2 = scrpos_x.f.u;
            limit_right1 = limit_right2;
        }
        break;
    }
}

// The catchers' box (the touch code of Boss_Flag 6, Offset_0x038684): under the ship, $10 across and 8 high each way while they are shut (Beta 4: the pre-beta's was 4 high), $20 across and 8 high, a little to the right, while they are open: it hurts
bool CNZBoss_CatcherBox(const Sst *hit, int16_t *centre_x, int16_t *centre_y, int16_t *half_width, int16_t *half_height) {
    if (hit->code != sst_object_codes[ObjId_CNZBoss] || hit->routine != Boss_Run || boss_flag != 6 || catcher_state == 0)
        return false;
    *centre_x = hit->x_pos.f.u;
    *centre_y = (int16_t)(hit->y_pos.f.u + 0x28);
    *half_width = 0x10;
    *half_height = 8;
    if (catcher_state != 1) {
        *half_width = 0x20;
        *centre_y = (int16_t)(*centre_y - 8);
        *centre_x = (int16_t)(*centre_x + 4);
    }
    return true;
}

static void SetPartAnim(int part, uint8_t anim) {
    boss_anim[(part + 1) * 2] = (uint8_t)((boss_anim[(part + 1) * 2] & 0xF0) | anim);
}

// Offset_0x02D23A: where the parts are, from the ship's place; once it has been beaten the catchers fall (the left first)
static void Boss_Parts(Sst *self, Own_CNZBoss *own) {
    const int16_t x = self->x_pos.f.u, y = self->y_pos.f.u;
    self->children[Part_Ship].x = x;
    self->children[Part_Ship].y = y;
    self->children[Part_Robotnik].x = x;
    self->children[Part_Robotnik].y = y;
    if (!own->boss.side) {
        self->children[Part_LeftCatcher].x = x;
        self->children[Part_LeftCatcher].y = y;
        self->children[Part_RightCatcher].x = x;
        self->children[Part_RightCatcher].y = y;
        own->left_y = (int32_t)((uint32_t)(uint16_t)y << 16) | (own->left_y & 0xFFFF);
        own->right_y = (int32_t)((uint32_t)(uint16_t)y << 16) | (own->right_y & 0xFFFF);
        return;
    }
    if (boss_move.timer > 0x78)
        return;
    self->children[Part_LeftCatcher].x--;
    own->left_y += (int32_t)own->left_speed << 8;
    own->left_speed += 0x38;
    self->children[Part_LeftCatcher].y = (int16_t)(own->left_y >> 16);
    if (self->children[Part_LeftCatcher].y >= 0x6F0)
        own->left_speed = 0;
    if (boss_move.timer > 0x3C)
        return;
    self->children[Part_RightCatcher].x++;
    own->right_y += (int32_t)own->right_speed << 8;
    own->right_speed += 0x38;
    self->children[Part_RightCatcher].y = (int16_t)(own->right_y >> 16);
    if (self->children[Part_RightCatcher].y >= 0x6F0)
        own->right_speed = 0;
}

// loc_2E1EC: the bob (the sine of its angle, a byte that goes on by 2 a frame, about the ship's place), and then its hits: when they are out it is beaten, and a hit flashes it
static void Boss_Bob(Sst *self, Own_CNZBoss *own) {
    int16_t sin, cos;
    CalcSine(own->boss.angle, &sin, &cos);
    self->y_pos.f.u = (int16_t)((sin >> 6) + boss_move.y.f.u);
    self->x_pos.f.u = boss_move.x.f.u;
    own->boss.angle = (uint8_t)(own->boss.angle + 2);
    if (own->boss.anim_routine >= Run_Wreck)
        return;
    if (self->collision_property == 0) { // the hits are out: it is beaten
        AddPoints(100);
        boss_move.timer = 0xB3;
        own->boss.anim_routine = Run_Wreck;
        return;
    }
    if (self->collision_flags != 0)
        return;
    if (own->boss.flash == 0) {
        own->boss.flash = 0x30;
        QueueSound2(sfx_HitBoss);
    }
    dry_palette[1][1] = (dry_palette[1][1] == 0) ? 0x0EEE : 0;
    if (--own->boss.flash == 0)
        self->collision_flags = 0x0F;
}

// loc_2E1A2: Eggman's face: a hit shows it (the frame after the hit), and so does a hurt character, unless it is showing the hit
static void Boss_Face(Own_CNZBoss *own) {
    if (own->boss.flash == 0x2F) {
        SetPartAnim(Part_Robotnik, 6);
        return;
    }
    if (player->routine == 4 || (TAILS_OBJ->code != NULL && TAILS_OBJ->routine == 4)) {
        if ((boss_anim[6] & 0xF) != 6)
            SetPartAnim(Part_Robotnik, 5);
    }
}

// loc_2E118: what is done each frame it is moving, hanging or diving: its place and hits, the catchers, the face, the animation and the drawing
static void Boss_Moving(Sst *self, Own_CNZBoss *own) {
    Boss_Bob(self, own);
    Boss_Parts(self, own);
    Boss_Face(own);
    Boss_AnimateSprite(self, &own->boss, Animation_CNZBoss);
    Sst_Draw(self);
}

// The catchers open (the box under the ship hurts) or shut; the animation of the box under the ship is the main sprite's (anim 9 open, 4 shut)
static void Boss_Catchers(uint8_t state, uint8_t main_anim, uint8_t parts) {
    catcher_state = state;
    boss_anim[3] = parts;
    boss_anim[9] = parts;
    boss_anim[0] = main_anim;
}

static void Boss_DropBall(Sst *self) { // loc_2E102
    Sst *ball = Sst_Allocate();
    if (ball == NULL)
        return;
    ball->code = sst_object_codes[ObjId_CNZBoss];
    ball->routine = Boss_Ball;
    ((Own_CNZBall *)ball->own.u8)->parent = (uint8_t)(self - sst);
}

// loc_2DFB4: the ship moves, and looks for Sonic under it: in the pit (below $6B0) it lets the ball fall, on the ledge (from $67C) it dives with the catchers open; every $40 frames it opens or shuts the catchers
static void Boss_Hunt(Sst *self, Own_CNZBoss *own) {
    Boss_Move();
    if (own->cooldown) {
        own->cooldown--;
    } else {
        const uint16_t under = (uint16_t)(player->x_pos.f.u - self->x_pos.f.u + 0x10);
        if (under < 0x20) {
            const uint16_t y = (uint16_t)player->y_pos.f.u;
            if (y >= 0x6B0) {
                if (own->dives < 3) {
                    own->dives++;
                    own->boss.anim_routine += 2;
                    Boss_Catchers(0, 8, 0);
                    Boss_DropBall(self);
                    boss_move.timer = 0x50;
                    Boss_Moving(self, own);
                    return;
                }
            } else if (y >= 0x67C) {
                self->mapping_frame = 0x0F;
                Boss_Catchers(2, 9, 0x20);
                own->boss.anim_routine += 4;
                boss_move.xsp = 0;
                boss_move.ysp = 0x180;
                own->boss.count = 0;
                Boss_Moving(self, own);
                return;
            }
        }
    }
    // loc_2E056
    boss_move.timer++;
    if (!(boss_move.timer & 0x3F)) {
        if (boss_move.timer & 0x40) {
            self->mapping_frame = 0x0F;
            Boss_Catchers(2, 9, 0x20);
        } else {
            self->mapping_frame = 0x0C;
            Boss_Catchers(1, 4, 0);
        }
    }
    Boss_Moving(self, own);
}

static void Boss_Patrol(Sst *self, Own_CNZBoss *own) { // loc_2DF46: to and fro between $28C0 and $29C0
    if (own->boss.flag == Path_Left) {
        if (boss_move.x.f.u <= 0x28C0) {
            boss_move.x.f.u = 0x28C0;
            boss_move.xsp = 0x180;
            boss_move.ysp = 0;
            own->boss.flag = Path_Right;
            SST_RENDER_SET(self, SST_RENDER_X_FLIP, true);
            own->dives = 0;
        }
    } else if (boss_move.x.f.u >= 0x29C0) {
        boss_move.x.f.u = 0x29C0;
        boss_move.xsp = (int16_t)0xFE80;
        boss_move.ysp = 0;
        own->boss.flag = Path_Left;
        SST_RENDER_SET(self, SST_RENDER_X_FLIP, false);
        own->dives = 0;
    }
    Boss_Hunt(self, own);
}

// loc_2E0B8: the ball hangs under the catcher for $50 frames, the catchers open as it falls, and $14 frames on they shut and it goes back to its rounds
static void Boss_BallDrop(Sst *self, Own_CNZBoss *own) {
    catcher_state = 0;
    boss_move.timer--;
    if (boss_move.timer == 0) {
        boss_anim[3] = 0x20;
        boss_anim[9] = 0x20;
    } else if (boss_move.timer <= -0x14) {
        boss_anim[3] = 0;
        boss_anim[9] = 0;
        own->boss.anim_routine = Run_Patrol;
        boss_move.timer = -1;
        own->cooldown = 0x40;
    }
    Boss_Moving(self, own);
}

// loc_2E132: the dive: down to $680 with the catchers open, then up to $654 with them shut, and back to its rounds
static void Boss_Dive(Sst *self, Own_CNZBoss *own) {
    Boss_Move();
    if (!own->boss.count) {
        if (self->y_pos.f.u >= 0x680) {
            boss_move.xsp = 0;
            boss_move.ysp = (int16_t)0xFE80;
            own->boss.count = 0xFF;
            Boss_Catchers(1, 4, 0);
        }
    } else if (self->y_pos.f.u < 0x654) {
        own->boss.anim_routine = Run_Patrol;
        boss_move.ysp = 0;
        boss_move.xsp = (int16_t)0xFE80;
        if (self->render_flags & SST_RENDER_X_FLIP) {
            boss_move.xsp = 0x180;
            self->mapping_frame = 0x0C;
        }
    }
    Boss_Moving(self, own);
}

// Where the ship is drawn, as the states below that do not bob it show it (its own place, not the bob's)
static void Boss_Place(Sst *self, Own_CNZBoss *own) {
    Boss_Bob(self, own);
    self->y_pos.f.u = boss_move.y.f.u;
    self->x_pos.f.u = boss_move.x.f.u;
    Boss_Parts(self, own);
}

// loc_2E26C: blowing up where it is; the catchers' balls are gone; at the end the arena's second wall opens
static void Boss_Wreck(Sst *self, Own_CNZBoss *own) {
    own->boss.side = 0xFF;
    if (--boss_move.timer >= 0) {
        catcher_state = 0;
        self->mapping_frame = 0;
        self->children[Part_Robotnik].frame = 0x0B;
        Boss_Defeated(self);
    } else {
        SST_RENDER_SET(self, SST_RENDER_X_FLIP, true);
        boss_move.xsp = 0;
        boss_move.ysp = 0;
        own->boss.anim_routine += 2;
        SetPartAnim(Part_Robotnik, 3);
        boss_anim[0] = 8;
        LevelLayout_SetChunk(LEVEL_LAYOUT_PLANE_FG, 0x54, 0x0C, 0xDD);
        boss_move.timer = -0x12;
    }
    self->y_pos.f.u = boss_move.y.f.u;
    self->x_pos.f.u = boss_move.x.f.u;
    Boss_Parts(self, own);
    Sst_Draw(self);
}

// loc_2E2DC: the ship drops a little and hovers, the level's music comes back, and then it goes
static void Boss_Fall(Sst *self, Own_CNZBoss *own) {
    boss_move.timer++;
    if (boss_move.timer == 0) {
        boss_move.ysp = 0;
    } else if (boss_move.timer < 0) {
        boss_move.ysp += 0x18;
    } else if ((uint16_t)boss_move.timer < 0x18) {
        boss_move.ysp -= 8;
    } else if (boss_move.timer == 0x18) {
        boss_move.ysp = 0;
        boss_status = 1; // (Boss_Defeated_Music_Restore: the boss is beaten and the level's music is back; the capsule in the layout ends the act)
        QueueSound1(Level_Music(level_id));
                Boss_DefeatedLoadFlickies(); // (the animals' and the explosions' art, as Beta 4's Boss_Defeated_Load_Flickies)
    } else if ((uint16_t)boss_move.timer >= 0x20) {
        own->boss.anim_routine += 2;
    }
    Boss_Move();
    Boss_Place(self, own);
    Sst_Draw(self);
}

// loc_2E33A: up and away to the right, the camera's right limit let out to $2AF0 as it goes; when that is reached and it is off the screen it is gone
static void Boss_Leave(Sst *self, Own_CNZBoss *own) {
    boss_move.xsp = 0x400;
    boss_move.ysp = (int16_t)0xFFC0;
    if (limit_right2 != 0x2AF0 + SCREEN_WIDEADD2) {
        limit_right1 = limit_right2 = (uint16_t)(limit_right2 + 2);
    } else if (!(self->render_flags & SST_RENDER_ON_SCREEN)) {
        Sst_Delete(self);
        return;
    }
    Boss_Move();
    Boss_Place(self, own);
    Boss_AnimateSprite(self, &own->boss, Animation_CNZBoss);
    Sst_Draw(self);
}

static void Boss_Run_Step(Sst *self, Own_CNZBoss *own) {
    // The catchers' sound while they hurt (every 32nd frame)
    if (catcher_state && !(frame_count & 0x1F))
        QueueSound2(sfx_CNZBossZap);
    switch (own->boss.anim_routine) {
    case Run_Patrol:
        Boss_Patrol(self, own);
        break;
    case Run_Drop:
        Boss_BallDrop(self, own);
        break;
    case Run_Dive:
        Boss_Dive(self, own);
        break;
    case Run_Wreck:
        Boss_Wreck(self, own);
        break;
    case Run_Fall:
        Boss_Fall(self, own);
        break;
    case Run_Leave:
        Boss_Leave(self, own);
        break;
    }
}

// The ball's move (loc_2E50E): its X and Y are taken as whole pixels each frame (the speeds are added to the pixels only), and gravity is added
static void Ball_Move(Sst *self, Own_CNZBall *ball) {
    uint32_t x = (uint32_t)(uint16_t)self->x_pos.f.u << 16;
    uint32_t y = (uint32_t)(uint16_t)self->y_pos.f.u << 16;
    x += (uint32_t)((int32_t)ball->x_speed << 8);
    y += (uint32_t)((int32_t)ball->y_speed << 8);
    ball->y_speed += 0x38;
    self->x_pos.f.u = (int16_t)(x >> 16);
    self->y_pos.f.u = (int16_t)(y >> 16);
}

// The ball breaks (loc_2E546): it flies up and away to the left, and a second one, the same, to the right
static void Ball_Break(Sst *self, Own_CNZBall *ball) {
    PlaySound(sfx_Bomb);
    self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_CNZBoss);
    self->anim = 7;
    ball->y_speed = (int16_t)0xFD00;
    ball->x_speed = (int16_t)0xFF00;
    self->routine = Boss_Ball;
    self->routine_secondary = Ball_Away;
    self->collision_flags = 0x98;
    Sst *other = Sst_AllocateAfter(self);
    if (other != NULL) {
        *other = *self;
        ((Own_CNZBall *)other->own.u8)->x_speed = (int16_t)-ball->x_speed;
    }
}

// What the ball (routine 4) does: it hangs under the catcher, falls to the floor and breaks into two that fall away
static void Ball_Run(Sst *self, Own_CNZBall *ball) {
    const Sst *boss = &sst[ball->parent];
    if (((const Own_CNZBoss *)boss->own.u8)->boss.anim_routine >= Run_Wreck) { // (the boss is beaten: it goes)
        Sst_Delete(self);
        return;
    }
    switch (self->routine_secondary) {
    case Ball_Make:
        self->mappings = Mappings_CNZBoss;
        self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_CNZBoss);
        self->render_flags |= SST_RENDER_LEVEL;
        self->priority = 7 * 0x80;
        self->routine_secondary += 2;
        self->x_pos.f.u = boss->x_pos.f.u;
        self->y_pos.f.u = (int16_t)(boss->y_pos.f.u + 0x30);
        self->y_radius = 8;
        self->x_radius = 8;
        self->mapping_frame = 0x12;
        self->collision_flags = 0x98;
        break;
    case Ball_Follow: // held under the catcher, and goes down a little further each frame until the time is out
        self->x_pos.f.u = boss->x_pos.f.u;
        self->y_pos.f.u = (int16_t)(boss->y_pos.f.u + ball->drop);
        if (++ball->drop >= 0x2E)
            ball->drop = 0x2E;
        if (boss_move.timer == 0) {
            self->routine_secondary += 2;
            ball->x_speed = 0;
            ball->y_speed = 0;
        }
        Sst_Draw(self);
        break;
    case Ball_Fall: {
        Ball_Move(self, ball);
        const int16_t d = SstFloorDist(self, self->x_pos.f.u);
        if (d < 0) {
            self->y_pos.f.u += d;
            Ball_Break(self, ball);
        }
        Sst_Draw(self);
        break;
    }
    case Ball_Away:
        Ball_Move(self, ball);
        Sst_Animate(self, Animation_CNZBoss);
        if ((uint16_t)self->y_pos.f.u >= 0x705) {
            Sst_Delete(self);
            return;
        }
        Sst_Draw(self);
        break;
    }
}

void Obj_CNZBoss(Sst *self) {
    Own_CNZBoss *own = (Own_CNZBoss *)self->own.u8;

    switch (self->routine) {
    case Boss_Start:
        self->mappings = Mappings_CNZBoss;
        self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_CNZBoss);
        self->render_flags |= SST_RENDER_LEVEL | SST_RENDER_MULTI_DRAW;
        self->priority = 3 * 0x80;
        self->width_pixels = 0x60;
        self->y_radius = 0x20;
        self->x_pos.f.u = 0x2A46; // (Beta 4: it comes in from the right; the pre-beta's started at $29D0,$590)
        self->y_pos.f.u = 0x654;
        self->mapping_frame = 0;
        own->boss.flash = 0x80; // (Obj_Inertia: the first flash lasts $80 frames, with no sound)
        self->routine = Boss_Run;
        own->boss.anim_routine = Run_Patrol;
        self->child_count = 4;
        self->collision_flags = 0x0F;
        self->collision_property = Boss_Hits(8);
        boss_move.x.f.u = self->x_pos.f.u;
        boss_move.y.f.u = self->y_pos.f.u;
        for (int i = 0; i < 4; i++) {
            self->children[i].x = self->x_pos.f.u;
            self->children[i].y = self->y_pos.f.u;
        }
        self->children[Part_RightCatcher].frame = 5;
        self->children[Part_Ship].frame = 1;
        self->children[Part_Robotnik].frame = 6;
        self->children[Part_LeftCatcher].frame = 2;
        own->boss.flag = Path_Left;
        own->dives = 0;
        boss_move.xsp = (int16_t)0xFE80;
        boss_move.ysp = 0;
        boss_move.timer = 1;
        own->left_y = own->right_y = (int32_t)((uint32_t)(uint16_t)self->y_pos.f.u << 16);
        {
            static const uint8_t buffer[10] = { 0x08, 0x00, 0x01, 0x00, 0x10, 0x00, 0x03, 0x00, 0x02, 0x00 };
            memcpy(boss_anim, buffer, sizeof(buffer));
        }
        break;

    case Boss_Run:
        Boss_Run_Step(self, own);
        break;

    case Boss_Ball:
        Ball_Run(self, (Own_CNZBall *)self->own.u8);
        break;
    }
}
