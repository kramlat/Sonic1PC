#ifndef _CHOPPER_H
#define _CHOPPER_H

#include "Object.h"

// Chopper assets
#include "Resource/Mappings/Chopper.h"
#include "Resource/Animation/Chopper.h"

// Chopper object structure
typedef struct {
    uint8_t pad[8]; // 0x28-0x2F
    int16_t orig_y; // 0x30
} Scratch_Chopper;

// Function prototypes
void Obj_Chopper_Construct(Object* obj, Scratch_Chopper* scratch);
void Obj_Chopper_Move(Object* obj, Scratch_Chopper* scratch);
void Obj_Chopper_Animate(Object* obj, Scratch_Chopper* scratch);
void Obj_Chopper_CheckAndResetPosition(Object* obj, Scratch_Chopper* scratch);
void Obj_Chopper(Object* obj);

#endif //_CHOPPER_H
