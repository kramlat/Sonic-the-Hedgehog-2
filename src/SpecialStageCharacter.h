#ifndef _SPECIALSTAGECHARACTER_H
#define _SPECIALSTAGECHARACTER_H

#include <stdbool.h>
#include <stdint.h>

// The pre-beta's character of the special stage (Obj $09, Sonic; Miles' $10 does the same), stage 4a of docs/Sonic2SpecialStage.md: what the object does each frame, with no drawing: the character runs round the half
// pipe on an angle (a byte: $40 is the bottom, 1 and $7F the sides), steered by the pad, and jumps off it along the line from the angle's opposite. The numbers in the comments are the pre-beta's addresses.

typedef struct {
    int32_t x, y;           // Obj_X, Obj_Y (16.16)
    int16_t x_speed, y_speed; // Obj_Speed, Obj_Speed_Y
    int16_t inertia;        // Obj_Inertia: how fast it goes round the pipe
    uint8_t angle;          // Obj_Angle ($26)
    int16_t centre_x, centre_y; // var_00, var_02: the middle of the pipe's circle
    int16_t radius_x, radius_y; // var_0C, var_0E: how far across and down it runs
    int16_t landing_offset, landing_sum; // the ellipse it comes back to the pipe at: how far above and below the middle its two points are, and the two distances together (Sonic's $3C and $C8, Miles' $4D and $100)
    uint8_t flip_period;    // var_04: how many steps of the running animation before the character turns round (4)
    int8_t flip_count;      // var_05: the steps counted
    uint8_t cannot_steer;   // var_06: it has gone round too fast to steer
    uint8_t status;         // Obj_Status: bits 0 and 1 the flips, bit 2 in the air
    uint8_t routine;        // 2 on the pipe, 4 in the air
    uint8_t hit;            // Obj_Routine_2: tumbling (hit by a bomb)
    uint8_t tumble;         // var_10: the angle the tumble has got to
    uint16_t touched;       // var_12: how many rings are with it
    uint8_t anim, prev_anim, anim_frame, anim_time; // Obj_Ani_Number, Obj_Ani_Flag, Obj_Ani_Frame, Obj_Ani_Time
    uint8_t frame;          // Obj_Map_Id
    uint8_t flips;          // the render flags' low bits
} SSPlayer;

#define SS_STATUS_AIR 0x04

// The stage's own variables the character reads: its speed ($FFFF9B12's word, 8 to $C) and how many frames a segment lasts, less one ($FFFF9B21)
typedef struct {
    uint16_t speed;
    uint8_t period_less;
} SSTrackState;

void SSPlayer_Init(SSPlayer *p);
// Miles (Obj $10): lower and on a bigger pipe, in front: ($7F,$B8), the middle $80 above, $66 across and $80 down
void SSPlayer_InitMiles(SSPlayer *p);
// A frame of the character on the pipe or in the air: `held` the pad's buttons, `pressed` the ones just pressed (Genesis bits: up 0, down 1, left 2, right 3, B 4, C 5, A 6)
void SSPlayer_Step(SSPlayer *p, const SSTrackState *track, uint8_t held, uint8_t pressed);
// The animation of the pre-beta's special stage (Offset_0x02F1E4: its frames last as long as the track's segments do) and the animation chosen by the angle
void SSPlayer_Animate(SSPlayer *p, const SSTrackState *track, const uint8_t *script_table);

// The shadow (Obj $63) of a character: where it is, which of its mappings and art, how it is flipped
typedef struct {
    int16_t x, y;
    uint8_t frame;
    uint16_t art_tile;   // $6279 (the player's shadow), $629F (diagonal), $62D9 (vertical)
    uint8_t flips;
} SSShadow;
void SSPlayer_Shadow(const SSPlayer *p, SSShadow *shadow);

// The distance of a number's square root by the pre-beta's Newton's iterations (Offset_0x02F0A4), in 16 bits as it does it
uint16_t SS_Sqrt16(uint16_t value, int iterations);

#endif //_SPECIALSTAGECHARACTER_H
