#ifndef _SAW_H
#define _SAW_H

#include "Sst.h"

typedef struct {
    int16_t orig_y;      // 0x38
    int16_t orig_x;         // 0x3A
    uint8_t shot;                // 0x3D -- flag set once the speeding saw has been launched
} Own_Saw;

void Obj_Saw(Sst *self);

#endif //_SAW_H
