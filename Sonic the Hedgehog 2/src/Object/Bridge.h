#ifndef _BRIDGE_H
#define _BRIDGE_H

#include "Sst.h"

// The bridge's mappings (defined in Bridge.c; the scenery's stakes are frames of them)
extern const uint8_t Mappings_BridgeEHZ[];
extern const uint8_t Mappings_BridgeGHZ[];
extern const uint8_t Mappings_BridgeHPZ[];

void Obj_Bridge(Sst *self);

#endif //_BRIDGE_H
