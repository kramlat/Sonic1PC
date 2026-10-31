#pragma once

#include <stddef.h>
#include <stdint.h>

// A level's entry in the load block table (sonic3k.asm: LevelLoadBlock, `levartptrs`, one entry for each zone and act): two sprite art lists, the palette, then the primary and the
// secondary group of the patterns (8 x 8), of the blocks (16 x 16) and of the chunks (128 x 128). Zones share their primaries and give each act its own secondaries; a level that has one group
// names it twice, and the second load is skipped (LoadLevelLoadBlock: `cmp.l d0,d7 / beq`). The games keep it as `level_load_block[zone][act]`.
typedef struct {
	uint8_t plc1, plc2; // the first and the second sprite art list
	uint8_t pal;
	const uint8_t *art[2];    // patterns: [0] primary, [1] secondary
	const uint8_t *map16[2];  // blocks
	const uint8_t *map128[2]; // chunks
} LevelLoadBlock;

// For writing the tables: LLB() is an entry with one group each (named twice), LLB_ACTS() the same entry for the four acts of a zone.
#define LLB(plc1, art, plc2, map16, pal, map128) { plc1, plc2, pal, { art, art }, { map16, map16 }, { map128, map128 } }
#define LLB_ACTS(...) { __VA_ARGS__, __VA_ARGS__, __VA_ARGS__, __VA_ARGS__ }

// A group both ways the original loads it: the primary decompressed to `dest`, then the secondary straight after it (where the primary ended: Kos_Decomp goes on where it stopped), unless
// the two are the same data. Returns where it ended.
uint8_t *LevelLoadBlock_Unpack(const uint8_t *const group[2], uint8_t *dest);
