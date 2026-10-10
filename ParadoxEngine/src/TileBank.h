#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Tile banks: where the patterns (the tile art, 32 bytes a tile of 8x8 pixels at 4 bits) of a plane's tiles come from. A name table entry does not hold a pattern number into one shared 2,048 tile space any more but refers
// to its tile by bank and pattern, so a bank of any size can be made for what needs one (a view's own set of animated tiles, a level's art) and freed when it is not needed, and the 11 bit limit of the Genesis' entry is not the
// limit of what a game can show.
//
// A bank has a generation: freeing a bank changes it, and an entry that remembers the old one no longer finds its tile (it draws blank), so a plane that still names a freed bank cannot read memory that is not there.

#define TILEBANKS 256
#define TILEBANK_NONE 255 // the bank id of an entry that shows nothing (never a live bank: an entry pointing at a freed bank is changed to this)

// What a slot of a bank is (the bank's `depth` table): a pattern number names a slot, and an 8bpp tile named by its head slot takes that and the next
#define TILE_SLOT_4BPP      0
#define TILE_SLOT_8BPP_HEAD 1
#define TILE_SLOT_8BPP_TAIL 2

typedef struct {
	uint8_t *patterns;   // the tiles' art
	uint8_t *depth;      // the allocation table: a byte for each 32 byte slot, TILE_SLOT_4BPP (a 4 bits a pixel tile of its own, or free), TILE_SLOT_8BPP_HEAD (the first half of a 64 byte, 8 bits a pixel tile) or
	                     // TILE_SLOT_8BPP_TAIL (its second half). It is the bank's own, so it goes when the bank does
	size_t tiles;        // how many 32 byte slots (a 4bpp tile takes one, an 8bpp tile two)
	uint8_t id;          // its place in the table: what an entry names it by
	uint16_t generation; // changes each time the bank is freed (the main bank's never does; 1 to 65535 for the others)
	bool live;
} tilebank_t;

// A name table entry (or a sprite's tile) is four words. The first is the pattern: the number of the tile in its bank (the Genesis' 11 bits, in 16 here). The second has in its low half the flags and the palette line the
// Genesis' tile word has (priority, flips, and the palette number in 4 bits, where the Genesis has 2), and in its high half the bank the pattern is in. The third is the bank's generation (see below); the fourth is not used.
//   attr bit 0: priority            bit 1: x flip           bit 2: y flip           bit 3-6: palette line (0-15)           bit 7: not used           bits 8-15: the bank (TILEBANKS of them, 0 the main bank)
// The compat reading of an entry is the Genesis': the palette line is the low two bits of the palette, the pattern its low 11 bits, in the main bank, whatever the bank and generation say (and no 8bpp tiles). The real
// reading uses it all. The games' data stays in the Genesis' tile words: TileEntry_FromWord makes an entry from one and TileEntry_Word a word from an entry.
typedef struct {
	uint16_t pattern;
	uint16_t attr;
	uint16_t generation;
	uint16_t reserved;
} tile_entry_t;

#define TILE_ATTR_PRIORITY      0x0001
#define TILE_ATTR_X_FLIP        0x0002
#define TILE_ATTR_Y_FLIP        0x0004
#define TILE_ATTR_PALETTE_SHIFT 3
#define TILE_ATTR_PALETTE       0x0078
#define TILE_ATTR_BANK_SHIFT    8
#define TILE_ATTR_BANK          0xFF00
#define TILE_BANK_SLOTS         65536 // the most slots in a bank: a pattern is 16 bits

static inline uint8_t TileAttr_Palette(uint16_t attr) { return (uint8_t)((attr & TILE_ATTR_PALETTE) >> TILE_ATTR_PALETTE_SHIFT); }
static inline uint8_t TileAttr_Bank(uint16_t attr) { return (uint8_t)((attr & TILE_ATTR_BANK) >> TILE_ATTR_BANK_SHIFT); }

// The attribute word for a Genesis tile word's flags and palette line (the palette group, 0-3, adds 4 lines a group) in a bank
static inline uint16_t TileAttr_FromWord(uint16_t word, const tilebank_t *bank, uint8_t palette_group) {
	const unsigned palette = (unsigned)(palette_group & 3) * 4 + ((word >> 13) & 3);
	return (uint16_t)(((word & 0x8000) ? TILE_ATTR_PRIORITY : 0) | ((word & 0x0800) ? TILE_ATTR_X_FLIP : 0) | ((word & 0x1000) ? TILE_ATTR_Y_FLIP : 0) |
	                  (palette << TILE_ATTR_PALETTE_SHIFT) | ((unsigned)bank->id << TILE_ATTR_BANK_SHIFT));
}

// The main bank: the VDP's tile space, the one the tile art of the games is loaded into and the sprites draw from
tilebank_t *TileBank_Main(void);

// Makes a bank of `tiles` tiles (zeroed), or NULL if the table is full; frees one. Freeing it goes through every table that holds entries (the planes, windows and sprite tables, which register with TileBank_OnFree)
// and changes the entries that name the bank to ones that show nothing, so nothing is left pointing at it (the generation, which a stale entry written after the free would carry, is the second check).
tilebank_t *TileBank_Create(size_t tiles);
void TileBank_Free(tilebank_t *bank);

// Marks `slots` slots of a bank from `first` as 8bpp tiles (two slots each: head and tail; `slots` rounded down to whole tiles), or back as 4bpp. This is how an allocator says what is where.
void TileBank_SetDepth(tilebank_t *bank, size_t first, size_t slots, bool eight_bits);

// Is the tile whose first slot is `pattern` an 8bpp one
bool TileBank_IsDeep(const tilebank_t *bank, size_t pattern);

// A table that holds entries asks to be told when a bank is freed: `purge` changes its entries that name bank `id` to TILEBANK_NONE ones
void TileBank_OnFree(void (*purge)(uint8_t id));

// Writes art into a bank at a tile (bytes past its end are dropped)
void TileBank_Write(tilebank_t *bank, size_t tile, const void *data, size_t bytes);

// The art of a pattern in the bank the attribute word `attr` names (and whether it is an 8bpp tile, 64 bytes of colours of the whole palette), or NULL if the bank is not there, has been freed since the entry was made, or is not that big
const uint8_t *TileBank_PatternOf(uint16_t attr, uint16_t generation, size_t pattern, bool *deep);

// The bank an id and generation name, or NULL if there is none live with that generation (a reference that has gone stale counts in tilebank_stale_count)
const tilebank_t *TileBank_Get(uint8_t id, uint16_t generation);

// How many entries have been drawn that named a tile that is not there (a stale or bad reference): for the tests and the debug displays. With tilebank_stale_fatal set the first one aborts.
extern unsigned tilebank_stale_count;
extern bool tilebank_stale_fatal;

// The entry for a tile word of the Genesis' format (the games' block and tile map data) whose pattern is in a bank, in a palette group
static inline tile_entry_t TileEntry_FromWord(uint16_t word, const tilebank_t *bank, uint8_t palette_group) {
	tile_entry_t e = { (uint16_t)(word & 0x07FF), TileAttr_FromWord(word, bank, palette_group), bank->generation, 0 };
	return e;
}

// ... and back: the Genesis' word (the palette's low two bits, the pattern's low 11)
static inline uint16_t TileEntry_Word(const tile_entry_t *e) {
	return (uint16_t)((e->pattern & 0x07FF) | ((e->attr & TILE_ATTR_PRIORITY) ? 0x8000 : 0) | ((e->attr & TILE_ATTR_X_FLIP) ? 0x0800 : 0) | ((e->attr & TILE_ATTR_Y_FLIP) ? 0x1000 : 0) |
	                  ((TileAttr_Palette(e->attr) & 3) << 13));
}
