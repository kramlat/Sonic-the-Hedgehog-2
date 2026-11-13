#ifndef _SPECIALSTAGEOBJECTS_H
#define _SPECIALSTAGEOBJECTS_H

#include "Sst.h"
#include "SpecialStageCharacter.h"
#include "SpecialStageItems.h"

// The pre-beta's special stage objects (stage 4 of docs/Sonic2SpecialStage.md): Sonic (object $09) and his shadow ($63). Their movement is in SpecialStageCharacter.c; these are the objects that draw them: the mappings, the
// animation, and the art that is put into VRAM a frame at a time (the stage keeps the characters' pictures decoded in RAM and copies the ones a frame needs: Offset_0x02EED4).
#define ObjId_SpecialStageSonic  0x09
#define ObjId_SpecialStageMiles  0x10
#define ObjId_SpecialStageTail   0x88
#define ObjId_SpecialStageShadow 0x63

void Obj_SpecialStageSonic(Sst *self);
void Obj_SpecialStageMiles(Sst *self);
void Obj_SpecialStageMilesTail(Sst *self);
void Obj_SpecialStageShadow(Sst *self);

// What the stage tells them each frame (its speed and how many frames a segment lasts, less one) and the character they follow
void Obj_SpecialStageItem(Sst *self);
void Obj_SpecialStageItemShadow(Sst *self);

// The rings and bombs: what the stage tells them each frame (the step it is at, the shown segment's block, the turned decoding; `load` on the frame the pre-beta loads a layout, the 4th of a segment), and a frame of them
void SpecialStageObjects_SetItems(int step, const uint8_t *block, size_t block_size, bool flipped, bool load);
void SpecialStageObjects_UpdateItems(void);
SSItems *SpecialStageObjects_Items(void);

// The head up display (stage 5b): the words, the ring counters (subtypes 0 to 2 Sonic, 3 to 5 Miles), the clock (subtypes 0 and 1) and the START banner with its lights. The stage runs once the banner is gone
void Obj_SpecialStageHud(Sst *self);
void Obj_SpecialStageCounter(Sst *self);
void Obj_SpecialStageClock(Sst *self);
void Obj_SpecialStageStart(Sst *self);
void SpecialStageObjects_ResetHud(void);
bool SpecialStageObjects_Started(void);
bool SpecialStageObjects_TimeUp(void);

void SpecialStageObjects_SetTrack(uint16_t speed, uint8_t period_less);
SSPlayer *SpecialStageObjects_Sonic(void);
SSPlayer *SpecialStageObjects_Miles(void);

#endif //_SPECIALSTAGEOBJECTS_H
