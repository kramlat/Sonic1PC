#include "Viewport.h"

#include <string.h>

#include "Backend/VDP.h"

// Each plane's name table and each view's scroll table is an array of its own: nothing written in one can reach another
#define PLANE_MEMORY 0x2000
static uint16_t nametable_fg[PLANE_MEMORY / 2];
static uint16_t nametable_bg[PLANE_MEMORY / 2];
static uint16_t nametable_fg_p2[PLANE_MEMORY / 2];
static uint16_t nametable_bg_p2[PLANE_MEMORY / 2];
static int16_t hscroll_p1[SCREEN_MAX_HEIGHT][2];
static int16_t hscroll_p2[SCREEN_MAX_HEIGHT][2];

viewport_t screen1p = {
	.plane_width = 64, .plane_height = 32,
	.plane_a = { nametable_fg, sizeof(nametable_fg) },
	.plane_b = { nametable_bg, sizeof(nametable_bg) },
	.hscroll = &hscroll_p1[0][0], .hscroll_bytes = sizeof(hscroll_p1),
};

viewport_t screen2p = {
	.plane_width = 64, .plane_height = 32,
	.plane_a = { nametable_fg_p2, sizeof(nametable_fg_p2) },
	.plane_b = { nametable_bg_p2, sizeof(nametable_bg_p2) },
	.hscroll = &hscroll_p2[0][0], .hscroll_bytes = sizeof(hscroll_p2),
};

void Viewport_UseOwnPlanes(viewport_t *v) {
	if (v == &screen2p) {
		v->plane_a = (plane_t){ nametable_fg_p2, sizeof(nametable_fg_p2) };
		v->plane_b = (plane_t){ nametable_bg_p2, sizeof(nametable_bg_p2) };
	} else {
		v->plane_a = (plane_t){ nametable_fg, sizeof(nametable_fg) };
		v->plane_b = (plane_t){ nametable_bg, sizeof(nametable_bg) };
	}
}

void Plane_UseTiles(plane_t *plane, size_t tile) {
	uint8_t *space = VDP_TileSpace();
	const size_t at = tile * 32;
	plane->entries = (uint16_t *)(space + at);
	plane->bytes = at < VRAM_SIZE ? ((VRAM_SIZE - at) < PLANE_MEMORY ? (VRAM_SIZE - at) : PLANE_MEMORY) : 0;
}

uint16_t *Plane_At(const plane_t *plane, size_t byte_offset) {
	if (byte_offset + 2 > plane->bytes)
		return NULL;
	return (uint16_t *)((uint8_t *)plane->entries + byte_offset);
}

void Plane_Put(plane_t *plane, size_t byte_offset, uint16_t entry) {
	uint16_t *at = Plane_At(plane, byte_offset);
	if (at != NULL)
		*at = entry;
}

void Plane_Fill(plane_t *plane, size_t byte_offset, size_t bytes, uint8_t value) {
	if (byte_offset >= plane->bytes)
		return;
	if (bytes > plane->bytes - byte_offset)
		bytes = plane->bytes - byte_offset;
	memset((uint8_t *)plane->entries + byte_offset, value, bytes);
}

void Plane_Clear(plane_t *plane) {
	memset(plane->entries, 0, plane->bytes);
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
