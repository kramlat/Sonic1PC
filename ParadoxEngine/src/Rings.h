#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "Sprites.h"
#include "Viewport.h"

// The ring handler of Sonic 3 & Knuckles (sonic3k.asm: Load_Rings line 18244, Test_Ring_Collisions line 18470, Render_Rings line 18643). Rings are not objects: the level's ring layout is a list of 4-byte
// entries, one for each ring, sorted by X and ended by an entry whose X is $FFFF (big endian):
//     u16 x,  u16 y
// each ring has a status of its own, and the handler tests the player against the rings near them and draws the rings near each view, from the layout. A collected ring sparkles for a few frames
// and is gone until the level starts again.
//
// Where S3&K keeps the start and end of the rings near the camera in pointers it moves each frame, this finds them when asked (the layout is sorted: a binary search), so that it works for any
// position -- the second view of the split screen, a player away from the camera -- and has no state to keep in step.

#define RINGS_MAX 511           // the status table of S3&K holds 512 words; the last entry is the end mark
#define RINGS_CONSUMING_MAX 63  // Ring_consumption_list
#define RING_SPACING 0x18       // the distance between the rings of a line, for whoever makes the layouts

// A ring's status (a word in S3&K: $0000, $0604 and $FFFF): nothing happened to it, or it was collected and is sparkling -- `timer` frames until the next of the sparkle's frames 4 to 7 -- or it is gone
typedef struct {
    uint8_t timer;
    uint8_t frame;
} RingStatus;

// What a ring looks like: Sonic 3 format mappings (Mappings.h) with the ring's 8 frames (0 to 3 spinning, 4 to 7 the sparkle) and the art tile word. The spinning frame all the uncollected rings show
// is `rings_frame` (Rings_frame: 0 to 3), which the game moves on (ChangeRingFrame) every 8 frames.
extern uint8_t rings_frame;
void Rings_SetLook(const uint8_t *mappings, uint16_t art_tile);

// Starts a level: every ring is there again. `layout` may be NULL (a level with no rings).
void Rings_Init(const uint8_t *layout);

// Once a frame: the collected rings' sparkle goes on (sub_E994)
void Rings_Update(void);

// Test_Ring_Collisions: the rings in the player's box (x and y as the player has them, y_radius their height), which are now collected; returns how many (the game counts them: GiveRing)
int Rings_Collide(int16_t x, int16_t y, uint8_t y_radius);

// Render_Rings: the sprites of the rings in range of a view (the layer pointers of `view`, the same arguments as the objects' queues get: Sonic 3 pieces are put in `sprite`)
void Rings_Render(sprite_t **sprite, uint8_t *sprite_i, const SpriteView *view, int16_t top, int view_w);

// The state, for the tests and the debug viewers
int Rings_Count(void);                         // the layout's rings
RingStatus Rings_StatusOf(int index);
void Rings_Position(int index, int16_t *x, int16_t *y);
int Rings_ConsumingCount(void);                // the rings sparkling now
