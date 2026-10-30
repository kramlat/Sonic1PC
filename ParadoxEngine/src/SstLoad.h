#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "Sst.h"

// Load_Sprites of Sonic 3 & Knuckles (sonic3k.asm: Load_Sprites, loc_1B7F2 and the two loaders it uses, Seek_Object_Manager, Clear_SpriteRingMem, Sprite_OnScreen_Test): keeps the level's objects in the status table
// while they are near the camera.
//
// The level's object layout is a list of 6-byte entries sorted by X and ended by an entry whose X is 0xFFFF (big endian):
//     u16 x,   u16 y (bits 0-11) | x flip (bit 13) | y flip (bit 14) | load whatever the camera's Y (bit 15),   u8 object id,   u8 subtype
// Every entry has a byte in the respawn table (its index is the entry's number): bit 7 says the object is out there. It is set when the object loads and cleared by the object when it leaves range
// (SstOnScreenTest, Delete_Sprite_If_Not_In_Range); an object that is destroyed for good deletes itself without clearing it, and is not loaded again.
//
// An object loads while its X is within the window of the camera's X (rounded down to 128): from 0x80 behind to 0x280 ahead; and its Y within the window of the camera's Y: from 0x80 above to 0x200 below
// (bit 15 of the entry's Y word loads it whatever its Y is). A level that wraps vertically (its minimum camera Y is negative) has the Y window wrap too. When the table is full (no free dynamic slot) the loader
// stops where it is and goes on when slots are free.
//
// Which routine an id runs is a table the game gives (Sprite_Listing). An id with no routine (NULL, or past the table) is left alone: the old objects still load through ObjectsManager.c, and the entry
// goes on being skipped by this loader.

#define SST_RESPAWN_COUNT 0x300
extern uint8_t sst_respawn_table[SST_RESPAWN_COUNT];

typedef struct {
    const SstCode *codes;       // routine of each object id (Sprite_Listing)
    int code_count;
} SstObjectIndex;

// The camera as the loader sees it
typedef struct {
    int16_t x, y;
    bool wraps_y;               // Camera_min_Y_pos is negative: the level wraps vertically
    uint16_t y_wrap;            // Screen_Y_wrap_value
} SstLoadCamera;

// Load_Sprites, first call: starts at `layout` (NULL: no objects), clears the respawn table unless `keep_table` (Respawn_table_keep: the level is entered again from a bonus stage or the like), seeks the
// load window to the camera and loads what is in range
void SstLoad_Init(const SstObjectIndex *index, const uint8_t *layout, bool keep_table, const SstLoadCamera *camera);

// Load_Sprites, every frame after (loc_1B7F2): loads what has come into range of the camera, and moves the bookkeeping on. Nothing happens until the camera is in another 128-pixel column or row
void SstLoad_Update(const SstLoadCamera *camera);

// Seek_Object_Manager: the camera has been moved a long way (a scripted jump): the window is seeked to it without loading anything
void SstLoad_Seek(int16_t camera_x);

// Clear_SpriteRingMem, the objects' half: every dynamic object that came from the layout gives its place back (the respawn table's bit 7), so they are loaded again
void SstLoad_ClearLoaded(void);

// Whether this loader owns the objects of this id (the table has a routine for it)
bool SstLoad_Handles(uint8_t id);

// Camera_X_pos_coarse_back / Camera_Y_pos_coarse_back: the camera less 128, rounded down to 128
int16_t SstLoad_CoarseBackX(void);
int16_t SstLoad_CoarseBackY(void);

// Clears the object's place in the respawn table (bclr #7 of respawn_addr): it will be loaded again when the camera comes back
void SstLoad_Forget(const Sst *obj);

// Sprite_OnScreen_Test: the object draws itself, or, when its X (rounded down to 128) is more than 0x280 past the coarse back edge of the camera, it forgets its place and is deleted
void SstOnScreenTest(Sst *obj);
// Sprite_OnScreen_Test2: the same for an X the caller gives (the object's own position is somewhere else)
void SstOnScreenTest2(Sst *obj, int16_t x);
// Delete_Sprite_If_Not_In_Range: the same without the drawing
void SstDeleteIfNotInRange(Sst *obj);

// The game's table of the routine for each object id (Sprite_Listing): a game defines it (an id with NULL has not moved to the status table). The loader above and Sst_Spawn read it.
extern const SstCode sst_object_codes[0x100];

// Creates an object of the id at a position, as the objects that make others do (AllocateObject, then the routine, x_pos, y_pos): the dynamic slot, or NULL when there is none. The object has no place in the
// respawn table, and its routine counter is 0: it sets itself up on its first run.
Sst *Sst_Spawn(uint8_t id, int16_t x, int16_t y);
