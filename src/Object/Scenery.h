#ifndef _SCENERY_H
#define _SCENERY_H

#include "Object.h"

// Scenery assets -- subtype 3 (GHZ bridge stump) reuses the Bridge
// object's own mappings (real hardware's Map_Bri IS GHZBridge's own
// mapping table, just displayed at a different frame) -- declared
// `extern` here rather than re-including GHZBridge's own resource header,
// since that header is a real definition (only safe to #include from
// GHZBridge.c itself). Subtypes 0-2 (SLZ lava/fireball thrower, three
// identical setup entries -- only the first is ever used) get their own
// mapping asset.
extern const uint8_t Mappings_GHZBridge[];
#include "Resource/Mappings/Scenery.h"

void Obj_Scenery(Object *obj);

#endif //_SCENERY_H
