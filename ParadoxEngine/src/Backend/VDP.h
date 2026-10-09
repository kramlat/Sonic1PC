#pragma once

#include "MegaDrive.h"
#include "PeekData.h" // Z80PeekData

#include "EngineConstants.h"
#include "EngineMacros.h"
#include <stdbool.h>

//VDP constants
#define VDP_INTERNAL_PAD 32

#define VRAM_SIZE    0x20000 // the 64 KB of tile space, then room for nametables (Constants.h's VRAM_FG/VRAM_BG, ArtTile_SS_Plane_*)
#define PLANE_SIZE   0x2000
#define SPRITES      80
#define SPRITES_SIZE (SPRITES * 8)
#define COLOURS      (4 * 16)

//Tile structure
#define TILE_PRIORITY_AND   0x8000
#define TILE_PRIORITY_SHIFT 15
#define TILE_PALETTE_AND    0x6000
#define TILE_PALETTE_SHIFT  13
#define TILE_Y_FLIP_AND     0x1000
#define TILE_Y_FLIP_SHIFT   12
#define TILE_X_FLIP_AND     0x0800
#define TILE_X_FLIP_SHIFT   11
#define TILE_PATTERN_AND    0x07FF
#define TILE_PATTERN_SHIFT  0

#define TILE_MAP(priority, palette, y_flip, x_flip, pattern) (    \
		((priority << TILE_PRIORITY_SHIFT) & TILE_PRIORITY_AND) | \
		((palette  << TILE_PALETTE_SHIFT)  & TILE_PALETTE_AND)  | \
		((y_flip   << TILE_Y_FLIP_SHIFT)   & TILE_Y_FLIP_AND)   | \
		((x_flip   << TILE_X_FLIP_SHIFT)   & TILE_X_FLIP_AND)   | \
		((pattern  << TILE_PATTERN_SHIFT)  & TILE_PATTERN_AND)    \
	)

//Sprite structure
//word y 000000YYYYYYYYYY
#define SPRITE_Y_AND   0x3FF
#define SPRITE_Y_SHIFT 0
//word sizelink 0000WWHH0LLLLLLL
#define SPRITE_SL_W_AND   0x0C00
#define SPRITE_SL_W_SHIFT 10
#define SPRITE_SL_H_AND   0x0300
#define SPRITE_SL_H_SHIFT 8
#define SPRITE_SL_L_AND   0x007F
#define SPRITE_SL_L_SHIFT 0
//word tile
//word x
#define SPRITE_X_AND   0x1FF
#define SPRITE_X_SHIFT 0

extern int vsync;
//VDP interface
int VDP_Init(const MD_Header *header);
void VDP_Quit(void);

void VDP_SeekVRAM(size_t offset);
void VDP_WriteVRAM(const uint8_t *data, size_t len);
void VDP_WriteLong(uint32_t val);
void VDP_FillVRAM(uint8_t data, size_t len);

void VDP_SeekCRAM(size_t offset);
void VDP_WriteCRAM(const uint16_t *data, size_t len); // Mega Drive colour words (0000bbb0ggg0rrr0), upscaled to true colour as they go in
void VDP_WriteCRAM_RGB(const uint32_t *data, size_t len); // true colour, 0x00RRGGBB
// The conversions between the two: the machine's colour to the true colour its DAC puts out, and a true colour to the machine's nearest
uint32_t VDP_Genesis2RGB(uint16_t cv);
uint16_t VDP_RGB2Genesis(uint32_t rgb);
void VDP_FillCRAM(uint16_t data, size_t len);

void VDP_SetPlaneALocation(size_t loc);
void VDP_SetPlaneBLocation(size_t loc);
void VDP_SetSpriteLocation(size_t loc);
void VDP_SetHScrollLocation(size_t loc);

// PC-only: registers a sprite table living in its own dedicated buffer,
// entirely outside the emulated VRAM address space, instead of one copied
// into vdp_vram at a VDP_SetSpriteLocation offset. Real hardware has no such
// option (the sprite table always lives in VRAM, sharing address space with
// everything else there) -- this exists so the sprite table's own size
// isn't constrained by neighboring VRAM regions (the plane nametables,
// HScroll table, or anything else placed nearby). Call once during setup;
// VDP_Render reads from this buffer instead of vdp_vram when set.
void VDP_SetSpriteBuffer(const uint16_t *buffer);
void VDP_SetPlaneSize(size_t w, size_t h);
void VDP_SetBackgroundColour(uint8_t index);
void VDP_SetVScroll(int16_t scroll_a, int16_t scroll_b);
void VDP_SetHIntCounter(int16_t counter);
void VDP_SetHIntEnable(bool enable);

void VDP_Render(void);

// PC-only overlay drawn directly by the SDL2 backend after the emulated VDP
// frame is blitted, on top of it -- real Genesis hardware has no vector/arc
// drawing (everything is tile-based), so a smooth pie-wipe progress
// indicator isn't something the VDP layer itself can produce; this exists
// purely for GM_Countdown's SDL_RenderGeometry-drawn pie slice. fraction is
// 0.0 (empty) to 1.0 (full circle). No-op on any backend that doesn't
// implement it (declared here, not in a specific backend's own header, so
// callers don't need to know/care which backend is active).
// seconds_left is drawn as a big centered 2-digit number (simple SDL-drawn
// segments, not a VDP tile font) so it composites cleanly on top of the pie
// fill instead of being drawn underneath it and covered up.
void Render_SetCountdownPie(bool active, float fraction, int seconds_left);

// "Z80 Peek" debug overlay -- live YM2612/SN76489 register state drawn
// directly by the SDL2 backend, same PC-only-overlay reasoning as the
// countdown pie above. Left Alt toggles it (see Game.h's Z80_PEEK_DISPLAY),
// same debug-only (#ifndef NDEBUG) gating as the existing VDP_PALETTE_DISPLAY
// VRAM/CRAM peek. Gathered fresh from sound_music each frame by Game.c's
// main loop (the one place that already has direct access to both
// SoundChipSet and the render backend) -- Render.c itself has no business
// knowing about Sound.c's internals, so it only ever sees this plain struct.

void Render_SetZ80Peek(bool active, const Z80PeekData *data);

// Read-only views of VDP memory for the debug viewers. pal is 0-3, index 0-15.
// VDP_PeekColour returns the RGBA8888-packed colour (0xRRGGBBAA, same packing as
// VDP_GetColour); VDP_PeekCRAM the CRAM word as the machine's (the nearest 9 bit colour of what is held).
const uint8_t *VDP_PeekVRAM(void); // VRAM_SIZE bytes
uint16_t VDP_PeekCRAM(int pal, int index);
uint32_t VDP_PeekColour(int pal, int index);
// Walks the sprite table in link order; returns how many entries were written (<= max).
int VDP_PeekSprites(VdpSpritePeek *out, int max);

// Toggles between windowed and borderless fullscreen (F11). Fullscreen uses
// the desktop's current resolution (no display mode change) and scales the
// game image up as far as it fits, preserving aspect ratio -- the leftover
// screen area is black letterbox/pillarbox bars. No-op on backends without a
// window.
void Render_ToggleFullscreen(void);

//Debug overlays (VRAM/CRAM view and the Z80 Peek register dump): toggled from the Qt window, drawn by VDP_Render
extern bool VDP_PALETTE_DISPLAY;
extern uint16_t VRAMADDR;
extern uint8_t CRAMPAL;

// "Z80 Peek" debug overlay (live YM2612/SN76489 register dump) -- Left Alt
// toggles it, same edge-detected pattern as VDP_PALETTE_DISPLAY above, same
// #ifndef NDEBUG gating (see Backend/SDL2/Input.c). Game.c's main loop reads
// this each frame to decide whether to gather register state and push it to
// the render backend (Render_SetZ80Peek, Backend/VDP.h).
extern bool Z80_PEEK_DISPLAY;

//Split screen: two views of the game at once (two players). The first view is the VDP as it is set up (the planes, scroll, sprite table and palette set through the
//functions above); the second view has its own, in a VDPView the game keeps and updates (it is read as the frame is drawn).
//  VDP_SPLIT_STACKED: one view above the other, as Sonic 2's 2-player mode does with the real VDP's double-height (interlace mode 2) display: the picture is
//    twice as many rows, the cells are 8x16 (a name table entry or sprite tile number names a pair of patterns, at twice its number), and sprite coordinates
//    are doubled (the picture starts at Y 256, and the second view's sprites sit a view lower).
//  VDP_SPLIT_SIDE: side by side, each half the picture's width, for the wide pictures (normal 8x8 cells, sprite coordinates as ever, X counted from each view's own
//    left edge).
typedef enum {
	VDP_SPLIT_NONE,
	VDP_SPLIT_STACKED,
	VDP_SPLIT_SIDE,
} VDPSplitMode;

typedef struct {
	size_t plane_a_location, plane_b_location; //where its name tables are in VRAM
	size_t hscroll_location;                   //its horizontal scroll table
	int16_t vscroll_a, vscroll_b;
	const uint16_t *sprite_buffer;             //its sprite table, a buffer as VDP_SetSpriteBuffer takes (4 words a sprite)
	const uint16_t *palette;                   //its palette (4 x 16 CRAM words), or NULL for the first view's
} VDPView;

void VDP_SetSplitScreen(VDPSplitMode mode, const VDPView *second_view);

//Water in a split screen: each view is its own screen with its own water line, as if it had a horizontal interrupt of its own. `dry` and `wet` are the palettes (4 lines of 16 CRAM words, read live
//as each frame is drawn), `line1` and `line2` the row of the first and the second view (counted in the view's own rows) the surface is at: the rows after it are drawn with the wet palette
//(below 0: all of the view is wet; at or beyond its last row: none). While it is set (dry not NULL) the views use these palettes in place of the VDP's colour RAM, and the H interrupt is not run.
void VDP_SetSplitWater(const uint16_t *dry, const uint16_t *wet, int16_t line1, int16_t line2);

//Shadow/highlight mode (the VDP's register $0C bit 3): every pixel not drawn by a high-priority plane or a sprite is shadowed, and the sprites' operator colours (palette line 3, colours 14 and 15) highlight or shadow what is beneath
void VDP_SetShadowHighlight(bool enable);

//How many rows the picture the VDP draws has: the screen's height, twice that when stacked
int VDP_OutputRows(void);

//Draws a frame into the VDP's own picture buffer without handing it to the display (for tests); the pixels (RGBA, `pitch` pixels a row) are read back with
//VDP_GetFrame
void VDP_DrawFrame(void);
const uint32_t *VDP_GetFrame(int *pitch, int *rows);
