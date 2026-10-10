#include "Viewport.h"

#include <string.h>

#include "Backend/VDP.h"

// Each plane's name table and each view's scroll table is an array of its own: nothing written in one can reach another
#define PLANE_ENTRIES 0x1000 // 64 x 64
static tile_entry_t nametable_fg[PLANE_ENTRIES];
static tile_entry_t nametable_bg[PLANE_ENTRIES];
static tile_entry_t nametable_fg_p2[PLANE_ENTRIES];
static tile_entry_t nametable_bg_p2[PLANE_ENTRIES];
static int16_t hscroll_p1[SCREEN_MAX_HEIGHT][2];
static int16_t hscroll_p2[SCREEN_MAX_HEIGHT][2];
static sprite_t sprite_table_p1[VIEWPORT_SPRITES + 1];
static sprite_t sprite_table_p2[VIEWPORT_SPRITES + 1];
static tile_entry_t window_p1[PLANE_ENTRIES];
static tile_entry_t window_p2[PLANE_ENTRIES];
static tile_entry_t scratch_planes[0x8000]; // the scratch plane memory (Plane_UseScratchAt)

viewport_t screen1p = {
	.plane_width = 64, .plane_height = 32,
	.plane_a = { nametable_fg, PLANE_ENTRIES },
	.plane_b = { nametable_bg, PLANE_ENTRIES },
	.window = { window_p1, PLANE_ENTRIES },
	.hscroll = &hscroll_p1[0][0], .hscroll_bytes = sizeof(hscroll_p1),
	.sprite_table = sprite_table_p1, .sprites = sprite_table_p1,
};

viewport_t screen2p = {
	.plane_width = 64, .plane_height = 32,
	.plane_a = { nametable_fg_p2, PLANE_ENTRIES },
	.plane_b = { nametable_bg_p2, PLANE_ENTRIES },
	.window = { window_p2, PLANE_ENTRIES },
	.hscroll = &hscroll_p2[0][0], .hscroll_bytes = sizeof(hscroll_p2),
	.sprite_table = sprite_table_p2, .sprites = sprite_table_p2,
};

void Viewport_UseOwnPlanes(viewport_t *v) {
	tilebank_t *main_bank = TileBank_Main();
	if (v == &screen2p) {
		v->plane_a = (plane_t){ nametable_fg_p2, PLANE_ENTRIES, main_bank };
		v->plane_b = (plane_t){ nametable_bg_p2, PLANE_ENTRIES, main_bank };
		v->window = (plane_t){ window_p2, PLANE_ENTRIES, main_bank };
	} else {
		v->plane_a = (plane_t){ nametable_fg, PLANE_ENTRIES, main_bank };
		v->plane_b = (plane_t){ nametable_bg, PLANE_ENTRIES, main_bank };
		v->window = (plane_t){ window_p1, PLANE_ENTRIES, main_bank };
	}
}

void Plane_UseScratchAt(plane_t *plane, size_t byte_address) {
	const size_t first = byte_address / 2;
	const size_t total = sizeof(scratch_planes) / sizeof(scratch_planes[0]);
	plane->entries = scratch_planes + (first < total ? first : total);
	plane->count = first < total ? (total - first < PLANE_ENTRIES ? total - first : PLANE_ENTRIES) : 0;
	plane->bank = TileBank_Main();
}

tile_entry_t *Plane_At(const plane_t *plane, size_t byte_offset) {
	const size_t index = byte_offset / 2;
	if (index >= plane->count)
		return NULL;
	return &plane->entries[index];
}

uint16_t Plane_Word(const plane_t *plane, size_t byte_offset) {
	const tile_entry_t *at = Plane_At(plane, byte_offset);
	return at != NULL ? TileEntry_Word(at) : 0;
}

void Plane_Put(plane_t *plane, size_t byte_offset, uint16_t word) {
	tile_entry_t *at = Plane_At(plane, byte_offset);
	if (at != NULL)
		*at = TileEntry_FromWord(word, plane->bank != NULL ? plane->bank : TileBank_Main());
		at->palette_group = plane->palette_group;
}

void Plane_PutEntry(plane_t *plane, size_t byte_offset, tile_entry_t entry) {
	tile_entry_t *at = Plane_At(plane, byte_offset);
	if (at != NULL)
		*at = entry;
}

void Plane_Fill(plane_t *plane, size_t byte_offset, size_t bytes, uint8_t value) {
	for (size_t at = byte_offset; at + 2 <= byte_offset + bytes; at += 2)
		Plane_Put(plane, at, (uint16_t)((value << 8) | value));
}

void Plane_Clear(plane_t *plane) {
	memset(plane->entries, 0, plane->count * sizeof(tile_entry_t));
}

void Viewport_UploadHScroll(viewport_t *v, const void *table, size_t bytes) {
	if (bytes > v->hscroll_bytes)
		bytes = v->hscroll_bytes;
	memcpy(v->hscroll, table, bytes);
}

void Viewport_SetSize(uint16_t width, uint16_t height, uint16_t plane_width, uint16_t plane_height) {
	viewport_t *both[2] = { &screen1p, &screen2p };
	for (int i = 0; i < 2; i++) {
		both[i]->width = width;
		both[i]->height = height;
		both[i]->plane_width = plane_width;
		both[i]->plane_height = plane_height;
	}
}

static plane_t *cursor_plane;
static size_t cursor_offset;

void Plane_Seek(plane_t *plane, size_t byte_offset) {
	cursor_plane = plane;
	cursor_offset = byte_offset;
}

void Plane_Write(uint16_t entry) {
	if (cursor_plane != NULL)
		Plane_Put(cursor_plane, cursor_offset, entry);
	cursor_offset += 2;
}

void Viewport_SetWindow(viewport_t *v, window_region_t region) {
	region.x &= ~7; // (whole tiles)
	region.y &= ~7;
	v->window_region = region;
}

void Viewport_ClearWindow(viewport_t *v) {
	v->window_region = (window_region_t){ 0 };
}
