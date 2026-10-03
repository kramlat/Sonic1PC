#pragma once

#include <stdint.h>

#include "EngineMacros.h"

// The level's terrain data: the layout of 128x128 chunks, the chunk and block tables, and the collision tables. The game loads them (Level.c); the engine's
// collision code (LevelCollision.c) reads them. The layout and the chunk tables are Sonic 2's format, which Sonic 1's second branch uses as well.

//Level bitfield structures
//Dual collision-path block word format (matches s1disasm's ProjectSonic1TwoEight
//branch / Sonic 2 & 3K convention): bits 0-9 = tile index (0-1023), bit10 =
//x-flip, bit11 = y-flip, bit12 = path-1 top-solid, bit13 = path-1 LRB-solid,
//bit14 = path-2 top-solid, bit15 = path-2 LRB-solid. Path selection (which
//pair of solid bits to test, and which of the two heightmap arrays below to
//consult) is runtime state set by the path-swapper object (Obj03), not baked
//into the block data itself.
#define META_SOLID_TOP_1 0x1000
#define META_SOLID_LRB_1 0x2000
#define META_SOLID_TOP_2 0x4000
#define META_SOLID_LRB_2 0x8000
#define META_Y_FLIP       0x0800
#define META_X_FLIP       0x0400
#define META_TILE         0x03FF
//Bare names used by every existing FindFloor/FindWall call site: alias to
//path 1 so those callers keep testing the exact same bit they always have.
//The actual bit tested at runtime is shifted by collision_path*2 inside
//LevelCollision.c, which is what makes path 2 apply without touching every
//call site.
#define META_SOLID_TOP META_SOLID_TOP_1
#define META_SOLID_LRB META_SOLID_LRB_1

extern uint8_t *const level_map128;
extern uint8_t level_map16[0x1800];
//Interleaved single-buffer level layout (matches s1disasm's ProjectSonic1TwoEight
//branch / Sonic 2 convention): each row is a fixed 0x100-byte stride, with
//foreground chunk IDs in the first 0x80 bytes and background chunk IDs in the
//second 0x80 bytes of that same row. Use level_layout[row]/level_layout[row]+0x80
//(or the LEVEL_LAYOUT_FG/LEVEL_LAYOUT_BG helpers) rather than indexing a
//separate plane dimension.
#define LEVEL_LAYOUT_ROWS 16
#define LEVEL_LAYOUT_ROW_STRIDE 0x100
#define LEVEL_LAYOUT_COLS 0x80
#define LEVEL_LAYOUT_FG(row) (&level_layout[(row) * LEVEL_LAYOUT_ROW_STRIDE])
#define LEVEL_LAYOUT_BG(row) (&level_layout[(row) * LEVEL_LAYOUT_ROW_STRIDE + LEVEL_LAYOUT_COLS])
extern uint8_t level_layout[LEVEL_LAYOUT_ROWS * LEVEL_LAYOUT_ROW_STRIDE];
//Dual collision heightmap arrays (collision curve ID -> 16 height bytes),
//selected at runtime by the active path (see META_SOLID_TOP_1/2,
//META_SOLID_LRB_1/2). coll_index[0] = path 1 (primary), coll_index[1] = path
//2 (secondary). 0x400 bytes/path is headroom over the largest existing zone's
//real data (SBZ at 608 bytes) -- these are decompressed via KosDec at level
//load time, not indexed directly against ROM like the old single pointer was.
extern uint8_t coll_index[2][0x400];
//Active collision path (0 = path 1/primary, 1 = path 2/secondary). Set by
//the path-swapper object (Obj03) once it's ported; defaults to 0, which
//reproduces Sonic 1's original single-path behaviour exactly.
extern uint8_t collision_path;

//The work buffer the level's chunk table lives in (it is shared with other screens' scratch use)
extern ALIGNED4 uint8_t buffer0000[0xA400];

//The game's collision maps (they are data: Sonic 1's and Sonic 2's differ): the angle of each collision curve, and the height of every column / the width of
//every row of each one (16 bytes a curve). Provided by the game (GameInterface.h).
extern const uint8_t Collision_Angle[];
extern const uint8_t Collision_HeightMap[];
extern const uint8_t Collision_WidthMap[];
