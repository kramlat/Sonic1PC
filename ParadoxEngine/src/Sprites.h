#pragma once

#include <stdint.h>

// The sprite pipeline (Sonic 2's: priority display lists, built into the VDP sprite table once a frame). The functions (DisplaySprite, BuildSprites,
// BuildSpr_Normal) are declared with the object type in EngineObject.h; this is the data they are configured with.
//
// An object that wants drawing this frame calls DisplaySprite (the object code does it itself, or RememberState does). BuildSprites then walks the 8 priority
// queues, the front one (0) first, and writes each visible object's sprite pieces (Mappings.h) to sprite_buffer. It sets the object's render.on_screen flag for
// the objects it drew, and clears it for the others: that is what PlaySoundLocal and the objects' own checks read.

// Where a sprite is placed: against these screen positions (the camera, in pixels)
typedef struct {
	const int16_t *x;
	const int16_t *y;
} SpriteLayer;

// The camera layers sprites are placed against, by an object's render flags (level_bg << 1 | level_fg): [0] none (the object's position already is a screen
// position, so the pointers are not used), [1] the foreground, [2] the background, [3] both (Sonic 1: the third scroll layer). Plain data a game assigns.
// The three scroll layers stay as they are: Sonic 1 uses the third (Sonic 2 has it too, unused). Sonic 3 keeps its scrolling as plain data (PODs) instead of
// code-based layers, which will replace them when its drawing code comes; the Sonic 2 drawing and physics are ported first.
typedef struct {
	SpriteLayer layer[4];
	// Does the on-screen check wrap the Y distance to 11 bits (& 0x7FF), as the original Sonic 1 and Sonic 2 do? With it, an object a level's height away from the
	// camera can show up on screen; this port's Sonic 1 never did that, so it is off there (it keeps its output exactly), and on for Sonic 2.
	int wrap_y;
} SpriteView;
extern SpriteView sprite_view;

// The second player's view in the split screen: its own camera (Camera.h), every level layer on it
extern SpriteView sprite_view_p2;

// The split screen (Sonic 2's Two_player_mode, for the sprites): BuildSprites then builds two tables, the first player's in sprite_buffer and the second's in
// sprite_buffer_p2, from the same queues, one per view; an object's on_screen flag says whether it was in either. Two layouts, matching the VDP's
// (VDP_SetSplitScreen):
//   SPRITE_SPLIT_STACKED: Sonic 2's: one view above the other in the VDP's double-height mode. The pieces' 2-player tile words and half-height sizes are used,
//     the views' tops are at 0x100 and 0x100 + the screen's height, and the first table starts with two masking sprites.
//   SPRITE_SPLIT_SIDE: for wide pictures: the views side by side, the first half the picture's width (rounded down), the second the rest. Ordinary tiles and sizes,
//     both tops at 128, and the cull checks use the view's width, not the picture's.
typedef enum {
	SPRITE_SPLIT_NONE,
	SPRITE_SPLIT_STACKED,
	SPRITE_SPLIT_SIDE,
} SpriteSplit;
extern int sprite_split_screen; // a SpriteSplit

// Sonic 2's Teleport_flag, kept here because the sprite pass reads it (Sonic 2 only: the other games leave it 0). In the stacked split screen the queues are not
// emptied while it is set. The space is reserved; nothing else in the engine sets it.
extern uint8_t sprite_teleport_flag;
