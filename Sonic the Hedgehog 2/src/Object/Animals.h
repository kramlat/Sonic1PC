#ifndef _ANIMALS_H
#define _ANIMALS_H

#include "Object.h"
#include "Macros.h"

typedef struct {
    uint8_t subtype;     // 0x28: 0 for one from a badnik; 10 and up for the end of a level (the kind of movement)
    uint8_t flip_flag;   // 0x29: turns round every other time it lands
    uint8_t pad0[6];
    uint8_t kind;        // 0x30: the animal's kind (0 to 11)
    uint8_t pad1;
    int16_t xsp;         // 0x32: its speed when it moves off
    int16_t ysp;         // 0x34: its jump
    uint16_t timer;      // 0x36: frames to wait (the end of a level)
    uint8_t pad2[6];
    uint16_t points;     // 0x3E: the points of the badnik that let it out, twice the frame of the points
} Scratch_Flicky;

STATIC_ASSERT(sizeof(Scratch_Flicky) == 0x18, "Scratch_Flicky must fill the scratch memory");

// Object 28 as the alpha has it: the animals that jump out of a destroyed badnik (and, with a subtype, those of the end of a level). Twelve kinds now (Sonic 1 has seven), two for each zone, whose art
// the zone's pattern load cue (Level_AnimalsPlc, in Level.h) brings in after the title cards, at tiles $580 and $594.
void Obj_FlickyAnimals(Object *obj);

#endif //_ANIMALS_H
