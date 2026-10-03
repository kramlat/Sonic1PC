#pragma once

#include <stdint.h>
#include <stdbool.h>

#include "ObjectsManager.h" // ObjectsManagerConfig: the rings use the objects' load window

// The rings manager (Sonic 2's rings keep a layout of their own, apart from the objects'). Plain data like the ObjectsManager: a game keeps a RingsManager, gives it
// a configuration, and the functions work on whichever one they are handed (two cameras, two managers).
//
// The ring layout is a list of 4-byte entries sorted by X, ending with an entry whose X is 0xFFFF (big endian):
//     u16 x,   u16 y (bits 0-11) | rings in the entry - 1 (bits 12-14) | vertical (bit 15)
// An entry is a straight line of 1 to 8 rings, RING_SPACING pixels apart, starting at (x, y), going right or, with bit 15, down.
//
// Which rings have been collected is in a status table the game provides: one byte per layout entry, bit n for the entry's nth ring. Collected rings stay gone
// until the level starts again; the others are made again every time their entry comes back into the load window (the same window and pointers as the objects').
// The manager does not make the rings: it calls the game's spawn function for each uncollected ring of an entry that came into range, and the game decides
// what a ring is (Sonic 1 makes an object, which draws itself and is touched like any other).

#define RING_SPACING 0x18

typedef struct {
    int16_t x, y;      // this ring's position
    int16_t base_x;    // the entry's own X (what the ring is kept loaded by)
    uint16_t entry;    // the entry's index in the layout (its status byte)
    uint8_t bit;       // the ring's place in the entry (its status bit)
} RingSpawn;

typedef void (*RingSpawnFunc)(void *user, const RingSpawn *ring);

typedef struct {
    ObjectsManagerConfig config;
    uint8_t *status;
    uint16_t status_count;   // how many entries status has (entries past it are never remembered)
    const uint8_t *layout;
    const uint8_t *load_right; // the next entry to load when moving right
    const uint8_t *load_left;  // the next entry to load when moving left
    int16_t camera_x_last;     // the 128-pixel column of the camera last time
    int16_t camera_x_coarse;
} RingsManager;

// Starts a level: clears `status`, then loads everything in range of the camera. `layout` may be NULL (a level with no rings).
void RingsManager_Init(RingsManager *m, const ObjectsManagerConfig *config, uint8_t *status, uint16_t status_count, const uint8_t *layout, int16_t camera_x,
                       RingSpawnFunc spawn, void *user);

// Once a frame, with the camera's X position: loads what came into range. Nothing happens until the camera has moved into another 128-pixel column.
void RingsManager_Update(RingsManager *m, int16_t camera_x, RingSpawnFunc spawn, void *user);

// The ring's status: has it been collected? Collect marks it (a ring of an entry the table does not reach is not remembered).
bool RingsManager_Collected(const RingsManager *m, uint16_t entry, uint8_t bit);
void RingsManager_Collect(RingsManager *m, uint16_t entry, uint8_t bit);
