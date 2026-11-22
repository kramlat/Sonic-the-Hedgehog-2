#ifndef _SPECIALSTAGEB4ITEMS_H
#define _SPECIALSTAGEB4ITEMS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "SpecialStageB4Character.h"

// Beta 4's rings and bombs of the special stage (objects $60, $61, their shadows, and the rings a hit character loses, $5B: objects/obj_0x60.asm, obj_0x61.asm and obj_0x5B.asm; stage 5 of
// docs/Sonic2SpecialStageB4.md): what they do each frame, with no drawing. An item is made ahead of the character, with a distance (the long at $30: its high word is how many steps of the segment's table of places
// away it is) that counts down by $CCCC a frame, and it goes by the table of the segment on the screen (SS4_Place): where it is on the screen, for the angle it is at (its lane), and from what angle it is hidden.

#define SS4_ITEM_MAX 0x77 // the slots from $FFFFB200 on (loc_7B8A)

typedef enum { SS4Kind_None, SS4Kind_Ring, SS4Kind_Bomb, SS4Kind_Shadow, SS4Kind_Spilled } SS4Kind;

typedef struct {
    SS4Kind kind;
    uint8_t routine;        // $24: 2 on its way, 6 taken (the ring's sparkle, the bomb's puff), the shadow's 4
    uint8_t lane;           // $26: the angle it is at on the pipe
    int32_t distance;       // $30
    uint8_t anim, anim_frame; // $1C, $1B
    int8_t anim_time;       // $1E
    uint8_t frame;          // $1A
    uint8_t collision;      // $20: 1 for a ring, 2 for a bomb, 0 once it is taken
    bool gone;              // $22 bit 7: its shadow or the stage ends it
    bool visible;           // the render flags' bit 7: there is a place for it on the screen
    uint8_t priority;       // $18
    uint16_t art_tile;
    int16_t x, y;           // 8, $C
    int16_t x_vel, y_vel;   // $10, $12 (the rings that spill)
    uint8_t flips;
    int link;               // $34: an item's shadow, a shadow's item (the pool's index; -1 none)
    uint8_t side;           // a shadow's $2B: which of its three drawings (0 across, 1 and 2 to the sides)
    bool end;               // a shadow's $2A: its item is taken, it goes
    int32_t px, py;         // a spilled ring's place, 16.16
} SS4Item;

// What the items read of the stage, and give back
typedef struct {
    bool frame_four;        // $FFFFDB0D is 4: the segment's slices are all done (the items go $CCCC a frame, and $CCCD when it is not)
    bool flipped;           // $FFFFDB0E: the track is turned over
    int segment;            // the segment on the screen ($FFFFDB10)
    SS4Player *player[2];
    uint16_t rings[2];      // $FFFFFE20, $FFFFFED0
    uint8_t digits[2][3];   // $3C, $3D, $3E of each character: the hundreds, the tens and the ones of the rings it has got
    // events: the sounds the stage plays and the rings taken, which the caller clears
    uint16_t sound[8];
    int sounds;
} SS4World2;

typedef struct {
    SS4Item item[SS4_ITEM_MAX];
    const uint8_t *stream;  // $FFFFDB88: where the stage's objects list has got to
    int stage;
    int step_done;          // $FFFFDB4C: the step whose objects are made (-1: none)
    int total_rings;        // $FFFFDB9A: the rings the stage's lists have made
    bool flash;             // $FFFFDB95: the palette flashes (an end of $FC)
    int check;              // what the end of a step asks for: 0x5A the rings check, 0x59 the emerald's, 0 none
} SS4Items;

void SS4Items_Reset(SS4Items *s, int stage);
// loc_6148: when the stage has come to a new step (`step`, the stage's step index, and `kind_length` the segments of the step's list) the objects of the step are made from the list
void SS4Items_MakeStep(SS4Items *s, int step, int kind_length);
// One frame of every item, in the order of the slots
void SS4Items_Run(SS4Items *s, SS4World2 *world);
// The rings that a hit character spills (loc_31880: the tens, or the hundreds, or the ones are lost), as $5B objects: how many. The ring count is taken from the world's
int SS4Items_Spill(SS4Items *s, SS4World2 *world, int who);

#endif //_SPECIALSTAGEB4ITEMS_H
