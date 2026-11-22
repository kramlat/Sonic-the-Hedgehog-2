#ifndef _SPECIALSTAGEB4CHARACTER_H
#define _SPECIALSTAGEB4CHARACTER_H

#include <stdbool.h>
#include <stdint.h>

// Beta 4's character of the special stage (Obj $09 Sonic and $10 Miles: objects/obj_0x09.asm and obj_0x10.asm, stage 4 of docs/Sonic2SpecialStageB4.md): what the object does each frame, with no drawing. It is
// not the pre-beta's character (SpecialStageCharacter.h): here it runs on a circle of a radius ($6E, or $80 for the one in front) round the middle of the pipe, by an angle (a byte: $40 is the bottom, y down),
// steered by the pad (the inertia, $14), pulled back to the bottom by the slope; a jump throws it across the circle, it falls by a gravity of $A8 and lands where it meets the circle again; at the top with no speed
// it falls. The numbers in the comments are Beta 4's addresses.

typedef struct {
    int32_t x, y;           // $2A and $2E, 16.16: the word is the place from the middle of the pipe (the 3D shrinks it across: $CC of 256)
    int16_t x_vel, y_vel;   // $10, $12
    int16_t inertia;        // $14: how fast it goes round the pipe
    int16_t radius;         // $34
    uint8_t angle;          // $26
    uint8_t status;         // $22: bits 0 and 1 the flips, 2 in the air from a jump, 6 the pad is let go
    uint8_t routine;        // $24: 2 on the pipe, 4 in the air from a jump, 8 falling from the top
    uint8_t hurt;           // $25: tumbling
    uint8_t hit;            // $21: a bomb has touched it (the items set it)
    uint8_t hurt_time;      // $36
    uint8_t let_go_time;    // $37: after the pad is let go, the slope does not pull for a time
    uint8_t flip_period;
    int8_t flip_count; // $32, $33: the steps of the running animation before it turns round
    uint8_t anim, prev_anim, anim_frame; // $1C, $1D, $1B
    int8_t anim_time;       // $1E
    uint8_t frame;          // $1A
    uint8_t flips;          // the render flags' low bits
    uint8_t direction;      // $3F: the animation's direction by the angle
    uint8_t priority;       // $18: 2 in front, 3 behind
    int16_t screen_x, screen_y; // 8 and $C
    bool miles;             // the second character ($FFFFB040) or the first ($FFFFB000)
    // What the step asks the stage to do (the caller takes and clears them): the jump's and the ring loss's sounds, the rings that spill out of a hit character ($5B), the hit that makes the other one the larger
    bool sound_jump, sound_ring_loss, spill;
    bool other_in_front;    // $FFFFF742: set when the first is hit and cleared when the second is
} SS4Player;

#define SS4_ORIGIN_X 0x80 // $FFFFF73E, $FFFFF740: where the middle of the pipe is on the screen
#define SS4_ORIGIN_Y 0x36
#define SS4_STATUS_AIR 0x04
#define SS4_STATUS_RELEASED 0x40

typedef struct {
    uint8_t period_less; // $FFFFDB21: how many frames a segment lasts, less one
    bool front;          // $FFFFF742
} SS4World;

void SS4Player_Init(SS4Player *p, bool miles);
// A frame: `held` the pad's buttons, `pressed` the ones just pressed (up 0, down 1, left 2, right 3, B 4, C 5, A 6), `rings` the character's ring count. `script` is the animation's table
void SS4Player_Step(SS4Player *p, SS4World *world, uint8_t held, uint8_t pressed, uint16_t rings, const uint8_t *script);

// The shadow (Obj $63): where it is, its art and frame and flips, from its character's angle (loc_3058E). Sonic's is the first, Miles' the second
typedef struct {
    int16_t x, y;
    uint8_t frame;
    uint16_t art_tile;
    uint8_t flips;
} SS4Shadow;
void SS4Player_Shadow(const SS4Player *p, SS4Shadow *shadow);

// The frame's art: where the tiles come from in the one picture the stage keeps (SoncMils) and go to in the character's place of VRAM: pairs of the picture's offset and the length, from the art script (loc_30AE4)
typedef struct {
    uint32_t source;  // a byte offset in the picture
    uint32_t bytes;   // how many bytes
} SS4ArtPiece;
// The pieces of a frame of Sonic (who 0: script entries from 0, the picture at $0, $B00, $2480 and $2E20 by the frame), Miles (1: from $12, at $3060, $3800, $4C80, $53C0) or his tails (2: from $24, at $55C0, $5C60, $63C0 by the frame): the tiles to copy, in order, to the character's place of VRAM
int SS4_FramePieces(int who, int frame, SS4ArtPiece *out, int max);

#endif //_SPECIALSTAGEB4CHARACTER_H
