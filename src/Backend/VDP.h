#pragma once

#include "MegaDrive.h"

#include <Constants.h>
#include <Macros.h>

//VDP constants
#define VDP_INTERNAL_PAD 32

#define VRAM_SIZE    0x10000
#define PLANE_SIZE   0x2000
#define SPRITES      80
#define SPRITES_SIZE (SPRITES * 8)
#define COLOURS      (4 * 16)

//VDP structures
#pragma pack(push)
#pragma pack(1)

//Sprite structure
typedef union
{
	struct
	{
		uint8_t priority : 1;
		uint8_t palette : 2;
		uint8_t y_flip : 1;
		uint8_t x_flip : 1;
		uint16_t pattern : 11;
	} s;
	uint16_t w;
} VDP_Tile;

#define TILE_TO_STRUCT(v)    \
{                            \
	.s = {                   \
		.priority = v >> 15, \
		.palette = v >> 13,  \
		.y_flip = v >> 12,   \
		.x_flip = v >> 11,   \
		.pattern = v,        \
	}                        \
}

typedef union
{
	struct
	{
		uint8_t pad1 : 4;
		uint8_t width : 2;
		uint8_t height : 2;
		uint8_t link;
	} s;
	uint16_t w;
} VDP_SpriteInfo;

typedef struct
{
	//Y coordinate
	uint16_t y;
	VDP_SpriteInfo info;
	VDP_Tile tile;
	uint16_t x;
} VDP_Sprite;

#pragma pack(pop)

//VDP interface
int VDP_Init(const MD_Header *header);
void VDP_Quit();

void VDP_WriteVRAM(size_t offset, const uint8_t *data, size_t len);
void VDP_FillVRAM(size_t offset, uint8_t data, size_t len);

void VDP_WriteCRAM(size_t offset, const uint16_t *data, size_t len);
void VDP_FillCRAM(size_t offset, uint16_t data, size_t len);

void VDP_SetPlaneALocation(size_t loc);
void VDP_SetPlaneBLocation(size_t loc);
void VDP_SetSpriteLocation(size_t loc);
void VDP_SetHScrollLocation(size_t loc);
void VDP_SetPlaneSize(size_t w, size_t h);
void VDP_SetBackgroundColour(uint8_t index);
void VDP_SetVScroll(int16_t scroll_a, int16_t scroll_b);

int VDP_Render();
