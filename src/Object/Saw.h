#ifndef _SAW_H
#define _SAW_H

#include "Object.h"

typedef struct {
    uint8_t pad0[16]; // 0x28-0x37 (0x28 = subtype)
    int16_t orig_y;      // 0x38
    int16_t orig_x;         // 0x3A
    uint8_t pad1;              // 0x3C
    uint8_t shot;                // 0x3D -- flag set once the speeding saw has been launched
} Scratch_Saw;

void Obj_Saw(Object *obj);

#endif //_SAW_H
