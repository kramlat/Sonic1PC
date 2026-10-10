#pragma once

//Screen dimensions
#define SCREEN_SCALE 2 //TODO: make screen scale a variable

// The size of the picture is chosen at run time (Video > Resolution): the aspect ratio picks the width or the height.
// These are the largest it can be, for buffers.
#define SCREEN_MAX_WIDTH  400
#define SCREEN_MAX_HEIGHT 256
extern int screen_width, screen_height;
#define SCREEN_WIDTH  screen_width
#define SCREEN_HEIGHT screen_height

#define SCREEN_WIDEADD  (SCREEN_WIDTH - 320)
#define SCREEN_WIDEADD2 (SCREEN_WIDEADD / 2)

#define SCREEN_TALLADD  (SCREEN_HEIGHT - 224)
#define SCREEN_TALLADD2 (SCREEN_TALLADD / 2)

#define PLANE_WIDEADD ((SCREEN_WIDEADD / 8 + 1) & ~1)
#define PLANE_TALLADD (((SCREEN_TALLADD + 8) / 16) * (PLANE_WIDTH << 1))

//VRAM data: the tile space holds patterns only (VRAM_SONIC, where a game keeps Sonic's art in it); the planes, scroll tables and sprite tables are each viewport's own (Viewport.h)
#define VRAM_SONIC   0xF000 //Sonic graphics

#define PLANE_WIDTH  64
extern int plane_height; //32 tiles, 64 for the taller pictures (Video_SelectResolution): the loaders keep a margin of one block row at each edge
#define PLANE_HEIGHT plane_height
#define PLANE_ROW_BYTES  (PLANE_WIDTH * 2)
#define PLANE_BYTES (PLANE_HEIGHT * PLANE_ROW_BYTES)

#define TILE_SIZE 32

// Converts an ArtTile_* tile index into the VRAM byte address PLC entries
// and VDP_SeekVRAM calls expect.
#define ART_VRAM(tile) ((tile) * TILE_SIZE)
