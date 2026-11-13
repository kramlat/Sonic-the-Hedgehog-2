#ifndef _CNZTRIANGLES_H
#define _CNZTRIANGLES_H

#include "Sst.h"

// Casino Night's large triangle bumpers of the pre-beta (Touch_Response_Cnz_Triangles, Offset_0x0130C4; the data are Tri_Act1.dat and Tri_Act2.dat). They are not objects: a list of (type, x, y) sorted by x, of which
// the characters' touch routine tests the entries about the camera. Six types (a triangle with its right angle in each of the four corners, two of them for slopes of the other two kinds): a character
// that touches one is thrown off it at $A00 (a straight push at the flat sides, a mirrored bounce along the slope), with the large bumper sound.
#define CNZ_TRIANGLE_TYPES 6

// Whether the character is touching a triangle, and if so the bounce (called with the character's own touch response, in Casino Night only)
void CNZTriangles_Touch(Sst *chr);

#endif //_CNZTRIANGLES_H
