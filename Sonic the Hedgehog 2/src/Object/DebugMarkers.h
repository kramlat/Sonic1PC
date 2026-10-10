#ifndef _DEBUGMARKERS_H
#define _DEBUGMARKERS_H

#include "Object.h"

// What an invisible object (a box that hurts or holds, which has no art of its own) shows when the debug cheat is on (always in a debug build): the "?" of the monitor art in each corner of its box, so that it
// can be seen where it is and how big. Each is a child sprite of the object (as the chains' links are), drawn with the object's own place as the middle of the box.
extern const uint8_t Mappings_DebugUnknown[];

// The "?" at the corners of the box of `half_width` and `half_height` around the object (does nothing without the cheat). Call it where the object would be drawn, after it has set its render flags.
void DebugMarkers_Show(Object *obj, int16_t half_width, int16_t half_height);

// The half sizes of the box a collision type touches (the first six of the touch sizes the hurt boxes use), or false when the type is not one of those
bool DebugMarkers_TouchBox(uint8_t col_type, int16_t *half_width, int16_t *half_height);

#endif //_DEBUGMARKERS_H
