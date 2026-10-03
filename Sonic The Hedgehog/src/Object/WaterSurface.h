#pragma once

#include "Object.h"

// Object 0x1B - LZ water surface. Two fixed instances (left/right half of
// the screen), continuously repositioned every frame to track the camera
// and current water height -- see s1disasm's "1B LZ Water Surface.asm".
#define WATERSURFACE_SLOT_LEFT  30 // v_watersurface1
#define WATERSURFACE_SLOT_RIGHT 31 // v_watersurface2
#define WATERSURFACE_SLOT_EXTRA 28 // a third one, for pictures wider than the original: the two cover 352 pixels (off screen, and so unseen, otherwise)

// VRAM tile base for the water surface art -- see PLC.c's PLC_LZ
// ({ Art_Water, 0x6000 }).
#define ArtTile_LZ_Water_Surface 0x300

extern const uint8_t Mappings_WaterSurface[];

void Obj_WaterSurface(Object *obj);
