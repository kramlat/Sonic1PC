#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "EngineObject.h"

// The objects manager (Sonic 2's, which is Sonic 1's with another layout format): keeps the level's objects loaded while they are near the camera, and remembers
// the state of the ones that ask to.
//
// It is plain data. A game keeps an ObjectsManager (a struct it may assign, copy or reset like any other) and gives it a configuration; the functions below
// work on whichever one they are handed, so a game can run several (the second player of a split screen follows its own camera) and a different game can have
// a different load window without touching the engine.
//
// The level's object layout is a list of 6-byte entries sorted by X, ending with an entry whose X is 0xFFFF (big endian):
//     u16 x,   u16 y (bits 0-11) | x flip (bit 13) | y flip (bit 14) | remember state (bit 15),   u8 object id,   u8 subtype
// An object that remembers its state gets a respawn index (1, 2, ... in layout order); once it has been loaded it is not loaded again until it has been forgotten
// (ObjectsManager_Forget: an object that leaves for good, or is destroyed for good, leaves its mark set; one that should come back when the camera returns
// clears it, which RememberState does). The marks live in an array the game provides, indexed by respawn index. The subtype goes into the object's first
// scratch byte (OBJECT_SUBTYPE).

#define OBJECT_SUBTYPE(obj) ((obj)->scratch.u8[0])

// How far from the camera objects are kept loaded: the camera's X is rounded down to 128, and objects load while their X is below that plus `ahead` (moving right)
// or above it minus `behind` (moving left). Objects drop out of the manager's bookkeeping `ahead + behind` pixels away on the other side.
typedef struct {
    int16_t ahead;
    int16_t behind;
} ObjectsManagerConfig;

// Sonic 1 and Sonic 2 both use these (Sonic 1's load range is fixed to its 320-pixel picture, whatever the picture size: see IS_OFFSCREEN)
#define OBJECTS_MANAGER_DEFAULT_CONFIG { 0x280, 0x80 }

typedef struct ObjectsManager {
    ObjectsManagerConfig config;
    uint8_t *marks;          // bit 7 of marks[respawn index]: that object has been loaded (and is remembered)
    uint16_t mark_count;     // how many entries marks has (respawn indexes at or past it are not remembered)
    const uint8_t *layout;   // the start of the object layout
    const uint8_t *load_right; // the next object to load when moving right
    const uint8_t *load_left;  // the next object to load when moving left
    uint8_t respawn_right;   // the respawn index of the next remembering object to the right
    uint8_t respawn_left;    // ... and of the one to the left
    int16_t camera_x_last;   // the 128-pixel column of the camera last time
    int16_t camera_x_coarse; // (camera X - 128) rounded down to 128: the left edge objects behind the camera are kept to (the rings use it too)
    const uint8_t *gap_start; // the entries from gap_start up to gap_end were skipped when the level started (they are behind the start window), not loaded:
    const uint8_t *gap_end;   // inside load_left..load_right, but nobody holds them
    const struct ObjectsManager *partner; // two-player mode: the other camera's manager, whose loaded objects this one must not load again (else NULL)
} ObjectsManager;

// Starts a level: clears `marks`, then loads everything in range of the camera. `layout` may be NULL (a level with no objects).
void ObjectsManager_Init(ObjectsManager *m, const ObjectsManagerConfig *config, uint8_t *marks, uint16_t mark_count, const uint8_t *layout, int16_t camera_x);

// Once a frame, with the camera's X position: loads what came into range, drops the bookkeeping for what left it. Nothing happens until the camera has moved
// into another 128-pixel column.
void ObjectsManager_Update(ObjectsManager *m, int16_t camera_x);

// Lets the object come back: clears its mark (an object with no respawn index is not remembered, and this does nothing for it).
void ObjectsManager_Forget(ObjectsManager *m, const Object *obj);

// Two-player mode: two cameras over one level. Each camera has its own manager (its own load window and its own side pointers) working on the same layout and
// the same marks, so the objects near either camera are there. An object both cameras have in range is loaded once: the entries a manager's window already holds
// are skipped by its partner (a respawn mark alone only covers the objects that remember their state). Nothing here knows who the players are: the game gives
// each camera's X. (Keeping an object loaded while it is near *either* camera is the objects' own business: they check both views.)
typedef struct {
    ObjectsManager view[2]; // [0]: the first player's camera, [1]: the second player's
} ObjectsManager2P;

void ObjectsManager2P_Init(ObjectsManager2P *m, const ObjectsManagerConfig *config, uint8_t *marks, uint16_t mark_count, const uint8_t *layout, int16_t camera_x, int16_t camera_x_p2);
void ObjectsManager2P_Update(ObjectsManager2P *m, int16_t camera_x, int16_t camera_x_p2);
void ObjectsManager2P_Forget(ObjectsManager2P *m, const Object *obj); // the marks are shared: the same as Forget on either view
