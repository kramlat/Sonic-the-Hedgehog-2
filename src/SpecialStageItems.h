#ifndef _SPECIALSTAGEITEMS_H
#define _SPECIALSTAGEITEMS_H

#include "SpecialStageCharacter.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// The pre-beta's rings and bombs of the special stage, stage 5a of docs/Sonic2SpecialStage.md (Obj $60 and $61 and what they do with the stage's layouts; the characters' touching them is Offset_0x02F0E4 of Obj $09). An
// item comes down the pipe: its distance (a 16.16 number, set from the layout's byte) goes down 0.8 a frame, its picture is a size by that distance and its place on the screen is from the track's block of the segment now
// shown ($751A: for each distance, where the pipe's middle is and how far across and down it goes, and which angles are hidden behind its far side).

#define SS_ITEM_SLOTS 12

typedef enum { SS_ITEM_NONE, SS_ITEM_RING, SS_ITEM_BOMB } SSItemKind;

typedef struct {
    SSItemKind kind;
    uint32_t distance;   // Obj_Control_Var_04 (16.16): the layout's byte in its top half
    uint8_t angle;       // Obj_Angle: where round the pipe it is
    uint8_t anim;        // Obj_Ani_Number: its size (9 is the nearest, 0 the farthest; a bomb's 10 is its blowing up)
    uint8_t anim_frame, anim_time;
    uint8_t frame;       // Obj_Map_Id
    bool touched;        // Obj_Status bit 7: a ring that has been taken
    bool visible;        // Obj_Flags bit 7: it is on the screen (inside the range, in front of the far side of the pipe)
    uint8_t collision;   // Obj_Col_Flags: a ring 1, a bomb 2 (0 once hit)
    int16_t x, y;
    uint8_t shadow_look; // the shadow's looks by the angle (Offset_0x0304F4): 0 the player's, 1 the diagonal, 2 the vertical; and whether it is flipped
    bool shadow_flipped;
} SSItem;

typedef struct {
    SSItem item[SS_ITEM_SLOTS];
    int loaded_step;     // $FFFF9B4C: the step the layout in the slots is of (-1: none)
} SSItems;

void SSItems_Reset(SSItems *items);
// SS_Load_Objects (Offset_0x005E7E): the layout of a step (ObjectLayouts) put in the slots, once for each step
void SSItems_Load(SSItems *items, int step);
// A frame for every item: the distance, size and place (`block` is the shown segment's track block, `flipped` the turned track's decoding)
void SSItems_Update(SSItems *items, const uint8_t *block, size_t block_size, bool flipped);

// Offset_0x02F0E4: the characters touching the items: a ring in the nearest size (anim 8) that the character's box overlaps is taken (his ring count, `touched` on the player, goes up), a bomb hurts him
// (the item is spent and he tumbles). Returns how many rings were taken.
int SSItems_Touch(SSItems *items, SSPlayer *player);

// The drawing's numbers: a ring's frame is its size and its spin (the animation: Offset_0x03073A), a bomb's its size or explosion
uint8_t SSItem_AnimationOfDistance(uint16_t distance); // Offset_0x0303D6

#endif //_SPECIALSTAGEITEMS_H
