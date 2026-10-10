#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "EngineConstants.h"
#include "TileBank.h"

// The VDP's memory spaces, each a symbol of its own (Viewport.c, Backend/VDP.c): the tile space (patterns only) and, for each viewport, the name tables of its planes, its horizontal scroll table, its vertical
// scroll memory (vsram_t) and its sprite table. A viewport is what a view of the game is drawn from: the first player's, and in a split screen the second's.

// A plane's name table: rows of entries, each naming the tile it shows (TileBank.h: bank, generation and pattern) and its flips, palette line and priority. It has no size of its own: how many tiles wide and high it is comes
// from the viewport it is in (plane_width, plane_height), so one type serves a plane of any size. `count` is how many entries there is memory for (writes past it are dropped). `bank` is the bank that the tile words of the
// Genesis' format a game writes (the block and tile map data) name their patterns in: the main bank unless the plane is given another.
typedef struct {
	tile_entry_t *entries;
	size_t count;
	tilebank_t *bank;
} plane_t;

// The vertical scroll memory: how far down each plane is scrolled
typedef struct {
	int16_t a, b;
} vsram_t;

// A sprite table entry (the Genesis' four words: y, size and link, tile, x) and the bank its tile's patterns are in (as a name table entry names its tile: TileBank.h)
typedef struct {
	uint16_t y;
	uint16_t size_link;
	uint16_t tile;       // the tile word: priority, palette line, flips and the first pattern
	uint16_t x;
	uint8_t bank;
	uint8_t generation;
} sprite_t;

#define VIEWPORT_SPRITES 0x78 // how many sprites a viewport's table holds (see Video.h's BUFFER_SPRITES for why this is more than the Genesis' 80)

typedef struct {
	uint16_t width, height;      // the viewport's picture size in pixels (the whole picture's, or half of it in a split screen)
	uint16_t plane_width, plane_height; // its planes' size in tiles
	plane_t plane_a, plane_b;    // the foreground and the background
	plane_t window;              // the window (the VDP does not draw it yet: it stays empty)
	int16_t *hscroll;            // its horizontal scroll table: a foreground and a background X for each line
	size_t hscroll_bytes;
	vsram_t vsram;
	sprite_t *sprite_table;      // its sprite table's memory (one more entry than it holds: the list's end)
	const sprite_t *sprites;     // the table the VDP draws it from: its own, or another (the tests), or NULL for none
	const uint16_t *palette;     // its own palette (4 x 16 CRAM words), or NULL for the shared one
	int16_t hint_counter;        // the horizontal interrupt: the line counter (VDP register $0A) and its enable
	bool hint_enable;
} viewport_t;

extern viewport_t screen1p; // the first view (a single view game has only this)
extern viewport_t screen2p; // the second view of a split screen

// A plane's memory as the viewport owns it again (after a plane was pointed at the tile space, as the special stage's are)
void Viewport_UseOwnPlanes(viewport_t *v);

// Points a plane at the scratch plane memory (a space of its own, laid out as the Genesis' $0-$FFFF was: `byte_address` is where the plane starts in it): for the planes a game keeps many of and switches between, such as the
// special stage's backgrounds
void Plane_UseScratchAt(plane_t *plane, size_t byte_address);

// A plane's entry at a byte offset into it (the offset of the Genesis' 16-bit entries: two bytes an entry; NULL when past its memory), and the writes through it. Plane_Put takes a tile word of the Genesis' format,
// whose pattern is in the plane's bank; Plane_PutEntry an entry as it is.
tile_entry_t *Plane_At(const plane_t *plane, size_t byte_offset);
uint16_t Plane_Word(const plane_t *plane, size_t byte_offset); // the entry as a tile word again (0 past the memory)
void Plane_Put(plane_t *plane, size_t byte_offset, uint16_t word);
void Plane_PutEntry(plane_t *plane, size_t byte_offset, tile_entry_t entry);
void Plane_Fill(plane_t *plane, size_t byte_offset, size_t bytes, uint8_t value); // (the byte as both halves of a tile word)
void Plane_Clear(plane_t *plane);

// Sequential writes, for code that fills a row after a row: seek a byte offset in a plane, then write entries one after another (the cursor is one for the whole program)
void Plane_Seek(plane_t *plane, size_t byte_offset);
void Plane_Write(uint16_t entry);

// Copies the rows of a horizontal scroll table in (the games build theirs in a buffer and upload it at the blank)
void Viewport_UploadHScroll(viewport_t *v, const void *table, size_t bytes);

// Sets the size of both viewports: the picture in pixels and the planes in tiles (the second view's picture is set again when a split screen starts)
void Viewport_SetSize(uint16_t width, uint16_t height, uint16_t plane_width, uint16_t plane_height);
