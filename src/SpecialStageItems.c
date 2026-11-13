// The pre-beta's rings and bombs of the special stage: see SpecialStageItems.h
#include "SpecialStageItems.h"
#include "SpecialStageTables.h"

#include "MathUtil.h"

#include <string.h>

// How near an item is by its distance (Offset_0x0303D6: the size, 9 the nearest, 0 for the far ones)
static const uint8_t anim_of_distance[0x60] = {
    9, 9, 9, 8, 8, 7, 7, 6, 6, 5, 5, 4, 4, 3, 3, 3, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0,
};

uint8_t SSItem_AnimationOfDistance(uint16_t distance) {
    return distance < sizeof(anim_of_distance) ? anim_of_distance[distance] : 0;
}

void SSItems_Reset(SSItems *items) {
    memset(items, 0, sizeof(*items));
    items->loaded_step = -1;
}

// What an item is made as (Offset_0x03032A and Offset_0x03022E, and the shadow's look from the angle at Offset_0x0304F4)
static void Make(SSItem *item, SSItemKind kind, uint8_t position, uint8_t angle) {
    memset(item, 0, sizeof(*item));
    item->kind = kind;
    item->distance = (uint32_t)position << 16;
    item->angle = angle;
    item->collision = kind == SS_ITEM_RING ? 1 : 2;
    if (angle <= 0x10) {
        item->shadow_flipped = true;
        item->shadow_look = 2;
    } else if (angle <= 0x30) {
        item->shadow_flipped = true;
        item->shadow_look = 1;
    } else if (angle <= 0x50) {
        item->shadow_look = 0;
    } else if (angle <= 0x70) {
        item->shadow_look = 1;
    } else {
        item->shadow_look = 2;
    }
}

void SSItems_Load(SSItems *items, int step) {
    if (step == items->loaded_step)
        return;
    items->loaded_step = step;
    const int count = SS_ObjectCount(step);
    if (count == 0)
        return;
    // (the pre-beta puts them in the first slots after the characters' in the order of the layout; the ones that were there before are written over)
    for (int i = 0; i < count && i < SS_ITEM_SLOTS; i++) {
        int position;
        bool bomb;
        uint8_t lane;
        SS_ObjectEntry(step, i, &position, &bomb, &lane);
        Make(&items->item[i], bomb ? SS_ITEM_BOMB : SS_ITEM_RING, (uint8_t)position, lane);
    }
}

// Offset_0x030436: where the item is on the screen from the segment's block
static void Place(SSItem *item, const uint8_t *block, size_t block_size, bool flipped) {
    const uint16_t word = (uint16_t)(item->distance >> 16);
    item->visible = false;
    if (block == NULL || block_size < 2 || word == 0)
        return;
    const uint16_t count = (uint16_t)((block[0] << 8) | block[1]);
    if ((int16_t)word > (int16_t)count)
        return;
    const size_t at = 2 + (size_t)(word - 1) * 6;
    if (at + 6 > block_size)
        return;
    const uint8_t *e = block + at;
    int d2 = e[0], d3 = e[1];
    const int d4 = e[2], d5 = e[3];
    int d6 = e[4], d7 = e[5];
    if (!flipped) {
        if (d7 != 0 && item->angle >= d6 && item->angle < d7)
            return; // (behind the pipe's far side)
    } else {
        if (d7 != 0) {
            d6 = (int16_t)(0x80 - d6);
            d7 = (int16_t)(0x80 - d7);
            if ((uint8_t)item->angle >= (uint8_t)d7 && (uint8_t)item->angle < (uint8_t)d6)
                return;
        }
        d2 = (int16_t)(0x100 - d2);
    }
    int16_t sin, cos;
    CalcSine(item->angle, &sin, &cos);
    item->x = (int16_t)(((cos * d4) >> 8) + d2);
    item->y = (int16_t)(((sin * d5) >> 8) + d3);
    item->visible = true;
}

static void Animate(SSItem *item) { // Offset_0x0305E8, with the scripts of Offset_0x03073A (a ring: its size, the spin's two other sizes' frames) and Offset_0x0308F2 (a bomb: its size, or blowing up)
    if ((int8_t)--item->anim_time >= 0)
        return;
    if (item->kind == SS_ITEM_RING) {
        static const uint8_t spin[4] = { 0, 10, 20, 10 };
        item->anim_time = 5;
        item->frame = (uint8_t)(item->anim + spin[item->anim_frame & 3]);
        item->anim_frame = (uint8_t)((item->anim_frame + 1) & 3);
        return;
    }
    if (item->anim == 0x0A) { // (the explosion: three frames round and round)
        static const uint8_t blow[3] = { 0x0A, 0x0B, 0x0C };
        item->anim_time = 2;
        item->frame = blow[item->anim_frame % 3];
        item->anim_frame = (uint8_t)((item->anim_frame + 1) % 3);
        return;
    }
    item->anim_time = 0x0B;
    item->frame = item->anim;
}

void SSItems_Update(SSItems *items, const uint8_t *block, size_t block_size, bool flipped) {
    for (int i = 0; i < SS_ITEM_SLOTS; i++) {
        SSItem *item = &items->item[i];
        if (item->kind == SS_ITEM_NONE)
            continue;
        // Offset_0x0303A2: a ring that was taken goes; the others come on 0.8 a frame, and go when they have come to the character
        if (item->touched) {
            item->kind = SS_ITEM_NONE;
            continue;
        }
        const int32_t distance = (int32_t)item->distance - 0xCD00;
        if (distance <= 0) {
            item->kind = SS_ITEM_NONE;
            continue;
        }
        item->distance = (uint32_t)distance;
        if (item->anim != 0x0A)
            item->anim = SSItem_AnimationOfDistance((uint16_t)(item->distance >> 16));
        Place(item, block, block_size, flipped);
        Animate(item);
        if (item->kind == SS_ITEM_BOMB && item->collision == 0)
            item->anim = 0x0A; // (Offset_0x030290: a bomb that has been hit blows up)
    }
}

// The size of an item's box, by the collision byte (Offset_0x02F0DC: half the width and height)
static const uint8_t box[8][2] = { { 0, 0 }, { 0x0C, 0x0C }, { 0x0A, 0x0A }, { 0x18, 0x10 }, { 0, 0 }, { 0, 0 }, { 0, 0 }, { 0, 0 } };

int SSItems_Touch(SSItems *items, SSPlayer *player) { // Offset_0x02F0E4
    int taken = 0;
    const int d2 = (int16_t)(player->x >> 16) + 0x0E, d3 = d2 - 0x1C;
    const int d4 = (int16_t)(player->y >> 16) + 0x1C, d5 = d4 - 0x38;
    for (int i = 0; i < SS_ITEM_SLOTS; i++) {
        SSItem *item = &items->item[i];
        if (item->kind == SS_ITEM_NONE || item->anim != 8 || item->collision == 0 || item->collision > 3)
            continue;
        const int bx = box[item->collision][0], by = box[item->collision][1];
        if (item->x - bx > d2 || item->x + bx < d3 || item->y - by > d4 || item->y + by < d5)
            continue;
        if (item->kind == SS_ITEM_BOMB) {
            item->collision = 0;
            player->hit = 2;
            player->tumble = player->angle;
            continue;
        }
        if (!item->touched) {
            item->touched = true;
            player->touched++;
            taken++;
        }
    }
    return taken;
}
