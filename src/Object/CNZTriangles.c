// Casino Night's large triangle bumpers: see CNZTriangles.h
#include "Object/CNZTriangles.h"
#include "Object.h"
#include "Constants.h"
#include "Game.h"
#include "Level.h"
#include "MathUtil.h"
#include "Object/Sonic.h"
#include "Sound.h"
#include "Resource/S2Triangles/CNZ1.h"
#include "Resource/S2Triangles/CNZ2.h"

// (paradoxmake puts 8 zero bytes after every resource)
#define RES_SIZE(array) (sizeof(array) - 8)

typedef struct {
    int type; // the word's value, 0 to 10 in steps of 2
    int x, y;
} Triangle;

static uint16_t Word(const uint8_t *p) {
    return (uint16_t)((p[0] << 8) | p[1]);
}

// The half width and height of a triangle's box by its type (Offset_0x013154)
static const uint8_t box[6][2] = { { 0x20, 0x20 }, { 0x20, 0x20 }, { 0x40, 0x08 }, { 0x40, 0x08 }, { 0x08, 0x40 }, { 0x08, 0x40 } };

static int16_t S(int v) {
    return (int16_t)v;
}

// Offset_0x0131E6: the character's speed is turned about the triangle's slope: mirrored over the slope's normal (`normal` is that angle, byte), or straight along the normal when it comes in too far from it
static void Reflect(Sst *chr, int normal) {
    int d0 = (int16_t)(CalcAngle(chr->x_vel, chr->y_vel) & 0xFF); // (the angle of the speed)
    d0 = S(d0 - normal);
    int d1 = d0 < 0 ? -d0 : d0;
    d0 = S(-d0 + normal);
    if ((uint8_t)d1 >= 0x38)
        d0 = normal;
    int16_t sine, cosine;
    CalcSine((uint8_t)d0, &sine, &cosine);
    chr->x_vel = (int16_t)((cosine * (int16_t)0xF600) >> 8);
    chr->y_vel = (int16_t)((sine * (int16_t)0xF600) >> 8);
}

// Offset_0x0133F6: it is in the air, no longer jumping or pushing, and the sound
static void Bounce(Sst *chr) {
    chr->status |= SST_PSTATUS_IN_AIR;
    chr->status &= (uint8_t) ~(SST_PSTATUS_ROLL_JUMP | SST_PSTATUS_PUSHING);
    ((Scratch_Sonic *)chr->own.u8)->jumping = false;
    PlaySound(sfx_LargeBumper);
}

// The six types, as the ROM's handlers do them (Offset_0x013182 to Offset_0x0133A0): `tx` and `ty` the triangle's place, the character's `cx`, `cy`
static void Handle(Sst *chr, int type, int tx, int ty) {
    const int cx = chr->x_pos.f.u, cy = chr->y_pos.f.u;
    int d0;
    switch (type) {
    case 0: // Offset_0x013182
        if (cy - ty >= 0x20) {
            chr->y_vel = 0x0A00;
            break;
        }
        if (cx - tx >= 0x20) {
            chr->x_vel = 0x0A00;
            break;
        }
        d0 = tx - cx;
        if (d0 >= 0x20)
            d0 = 0x20;
        if ((uint16_t)(d0 + ty - 8) >= (uint16_t)(cy + 0x0E))
            return;
        Reflect(chr, 0x20);
        break;
    case 2: // Offset_0x013234
        if (cy - ty >= 0x20) {
            chr->y_vel = 0x0A00;
            break;
        }
        if (tx - cx >= 0x20) {
            chr->x_vel = (int16_t)0xF600;
            break;
        }
        d0 = cx - tx;
        if (d0 >= 0x20)
            d0 = 0x20;
        if ((uint16_t)(d0 + ty - 8) >= (uint16_t)(cy + 0x0E))
            return;
        Reflect(chr, 0x60);
        break;
    case 4: // Offset_0x01329A
        if (cy - ty >= 8) {
            chr->y_vel = 0x0A00;
            break;
        }
        if (tx - cx >= 0x40) {
            chr->x_vel = (int16_t)0xF600;
            break;
        }
        d0 = cx - tx;
        if (d0 >= 0x40) {
            chr->x_vel = 0x0A00;
            break;
        }
        Reflect(chr, d0 < 0 ? 0x38 : 0x48);
        break;
    case 6: // Offset_0x0132F2
        if (ty - cy >= 8) {
            chr->y_vel = (int16_t)0xF600;
            break;
        }
        if (tx - cx >= 0x40) {
            chr->x_vel = (int16_t)0xF600;
            break;
        }
        d0 = cx - tx;
        if (d0 >= 0x40) {
            chr->x_vel = 0x0A00;
            break;
        }
        Reflect(chr, d0 < 0 ? 0xC8 : 0xB8);
        break;
    case 8: // Offset_0x013348
        if (cx - tx >= 8) {
            chr->x_vel = 0x0A00;
            break;
        }
        if (ty - cy >= 0x40) {
            chr->y_vel = (int16_t)0xF600;
            break;
        }
        d0 = cy - ty;
        if (d0 >= 0x40) {
            chr->x_vel = 0x0A00; // (sic: the ROM sets the sideways speed here)
            break;
        }
        Reflect(chr, d0 < 0 ? 0x08 : 0xF8);
        break;
    default: // 10: Offset_0x0133A0
        if (tx - cx >= 8) {
            chr->x_vel = 0x0A00;
            break;
        }
        if (ty - cy >= 0x40) {
            chr->y_vel = (int16_t)0xF600;
            break;
        }
        d0 = cy - ty;
        if (d0 >= 0x40) {
            chr->x_vel = 0x0A00;
            break;
        }
        Reflect(chr, d0 < 0 ? 0x78 : 0x88);
        break;
    }
    Bounce(chr);
}

void CNZTriangles_Touch(Sst *chr) {
    const uint8_t *list = LEVEL_ACT(level_id) == 0 ? S2Triangles_CNZ1 : S2Triangles_CNZ2;
    const size_t count = (LEVEL_ACT(level_id) == 0 ? RES_SIZE(S2Triangles_CNZ1) : RES_SIZE(S2Triangles_CNZ2)) / 6;

    // The entries about the camera (Load_Triangle_Pos: from the camera less 8 to $150 further on, a window that the wide screen widens)
    int first = (int)scrpos_x.f.u - 8 - SCREEN_WIDEADD2;
    if (first <= 0)
        first = 1;
    const int last = first + 0x150 + 2 * SCREEN_WIDEADD2;

    // The character's box (Offset_0x0130E4): 0x12 wide from 9 left of it, and tall by its radius (less when it is ducking)
    const int d2 = chr->x_pos.f.u - 9;
    int height = (uint8_t)(chr->y_radius - 3);
    int d3 = chr->y_pos.f.u - height;
    if (chr->mapping_frame == 0x39) { // (ducking)
        d3 += 12;
        height = 10;
    }
    height <<= 1;

    for (size_t i = 0; i < count; i++) {
        const uint8_t *e = list + i * 6;
        const int x = Word(e + 2), y = Word(e + 4);
        if (x < first || x >= last)
            continue;
        const int type = Word(e);
        const int hx = box[(type & 0xE) >> 1][0], hy = box[(type & 0xE) >> 1][1];
        if (x - hx > d2 + 0x12 || d2 > x + hx || y - hy > d3 + height || d3 > y + hy)
            continue;
        Handle(chr, type, x, y);
        return; // (the ROM's handler jumps out: one triangle a frame)
    }
}
