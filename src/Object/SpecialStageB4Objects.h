#ifndef _SPECIALSTAGEB4OBJECTS_H
#define _SPECIALSTAGEB4OBJECTS_H

#include "Sst.h"
#include "SpecialStageDisplay.h"

// Beta 4's special stage objects on the screen (stage 6 of docs/Sonic2SpecialStageB4.md): the characters ($09, $10, Miles' tails $88 and the shadows $63), the items (the pool of SpecialStageB4Items.h, one object
// for each that is in it), the HUD ($5E), the ring counters ($87), the START banner ($5F), and the stage's end: the rings' checks at the checkpoints ($5A) and the emerald ($59). The models are in SpecialStageB4Character.c
// and SpecialStageB4Items.c; this file feeds them the stage's variables and draws them.

void SS4Stage_Begin(int stage, bool versus_play);               // the models and the art start (call before the objects are made)
void SS4Stage_Spawn(void);                      // the objects of the stage, in the order of the original's slots
void SS4Stage_Frame(const SSDisplay *d);        // once a frame before the objects run: what the stage tells them
bool SS4Stage_Started(void);                    // the START banner is gone
bool SS4Stage_Over(void);                       // the stage has ended (`$FFFFDB86`)
bool SS4Stage_PadLocked(void);                  // the end has locked the pads ($FFFFDB87)
bool SS4Stage_Versus(void);                     // the versus play
int SS4Stage_RoundsPlayed(void);                // its rounds so far (three end the stage)
uint16_t SS4Stage_RoundRings(int round, int who); // a round's rings of one character
bool SS4Stage_GotEmerald(void);                 // and it was won
int SS4Stage_Rings(void);                       // the rings of both characters
int SS4Stage_RingsOf(int who);                  // one character's rings
int SS4Stage_Required(int stage, int checkpoint); // the rings a checkpoint asks of the two together (loc_8298)
uint16_t SS4Stage_Speed(void);                  // the stage's speed word ($FFFFDB12), which the end slows

void Obj_SS4Character(Sst *self);
void Obj_SS4Tail(Sst *self);
void Obj_SS4Shadow(Sst *self);
void Obj_SS4Item(Sst *self);
void Obj_SS4Controller(Sst *self);
void Obj_SS4Hud(Sst *self);
void Obj_SS4Counter(Sst *self);
void Obj_SS4Start(Sst *self);
void Obj_SS4Letter(Sst *self);
void Obj_SS4Banner(Sst *self);
void Obj_SS4GateRing(Sst *self);
void Obj_SS4Emerald(Sst *self);
void Obj_SS4Emblem(Sst *self);

#endif //_SPECIALSTAGEB4OBJECTS_H
