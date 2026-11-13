#ifndef _SPECIALSTAGEOBJECTS_H
#define _SPECIALSTAGEOBJECTS_H

#include "Sst.h"
#include "SpecialStageCharacter.h"

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
void SpecialStageObjects_SetTrack(uint16_t speed, uint8_t period_less);
SSPlayer *SpecialStageObjects_Sonic(void);
SSPlayer *SpecialStageObjects_Miles(void);

#endif //_SPECIALSTAGEOBJECTS_H
