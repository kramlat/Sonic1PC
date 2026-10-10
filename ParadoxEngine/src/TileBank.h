#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Tile banks: where the patterns (the tile art, 32 bytes a tile of 8x8 pixels at 4 bits) of a plane's tiles come from. A name table entry does not hold a pattern number into one shared 2,048 tile space any more but refers
// to its tile by bank and pattern, so a bank of any size can be made for what needs one (a view's own set of animated tiles, a level's art) and freed when it is not needed, and the 11 bit limit of the Genesis' entry is not the
// limit of what a game can show.
//
// A bank has a generation: freeing a bank changes it, and an entry that remembers the old one no longer finds its tile (it draws blank), so a plane that still names a freed bank cannot read memory that is not there.

#define TILEBANKS 16

typedef struct {
	uint8_t *patterns;   // the tiles' art
	size_t tiles;        // how many tiles
	uint8_t id;          // its place in the table: what an entry names it by
	uint8_t generation;  // changes each time the bank is freed (the main bank's never does)
	bool live;
} tilebank_t;

// A name table entry: the tile it shows (bank, generation of that bank, and pattern in it) and how: the attributes are the Genesis' (priority bit 15, palette lines bits 13-14, flips bits 11 and 12; the other bits are not used)
typedef struct {
	uint32_t pattern;
	uint16_t attrs;
	uint8_t bank;
	uint8_t generation;
} tile_entry_t;

// The main bank: the VDP's tile space, the one the tile art of the games is loaded into and the sprites draw from
tilebank_t *TileBank_Main(void);

// Makes a bank of `tiles` tiles (zeroed), or NULL if the table is full; frees one (its entries go blank)
tilebank_t *TileBank_Create(size_t tiles);
void TileBank_Free(tilebank_t *bank);

// Writes art into a bank at a tile (bytes past its end are dropped)
void TileBank_Write(tilebank_t *bank, size_t tile, const void *data, size_t bytes);

// The art of a tile an entry names, or NULL if its bank is not there, has been freed since the entry was made, or is not that big
const uint8_t *TileBank_Pattern(const tile_entry_t *entry);

// The bank an id and generation name, or NULL if there is none live with that generation (a reference that has gone stale counts in tilebank_stale_count)
const tilebank_t *TileBank_Get(uint8_t id, uint8_t generation);

// How many entries have been drawn that named a tile that is not there (a stale or bad reference): for the tests and the debug displays. With tilebank_stale_fatal set the first one aborts.
extern unsigned tilebank_stale_count;
extern bool tilebank_stale_fatal;

// The entry for a tile word of the Genesis' format (the games' block and tile map data): its 11 bit pattern in a bank, its flips, palette and priority
static inline tile_entry_t TileEntry_FromWord(uint16_t word, const tilebank_t *bank) {
	tile_entry_t e = { (uint32_t)(word & 0x07FF), (uint16_t)(word & 0xF800), bank->id, bank->generation };
	return e;
}

// ... and back (the pattern's low 11 bits)
static inline uint16_t TileEntry_Word(const tile_entry_t *e) {
	return (uint16_t)(e->attrs | (e->pattern & 0x07FF));
}
