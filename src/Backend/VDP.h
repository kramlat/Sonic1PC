#pragma once

#include "MegaDrive.h"

#include "Constants.h"
#include "Macros.h"
#include <stdbool.h>

//VDP constants
#define VDP_INTERNAL_PAD 32

#define VRAM_SIZE    0x10000
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
void VDP_WriteCRAM(const uint16_t *data, size_t len);
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
typedef struct {
    uint8_t fm_alg_fb[2][3];  // [port][chan]: raw $B0+ch byte (algorithm low 3 bits, feedback next 3)
    uint8_t fm_tl[2][3][4];   // [port][chan][operator 0=op1..3=op4]: raw $40+op*4+ch byte
    uint8_t fm_keyon;         // last $28 write -- bit layout matches the real key-on register
    uint16_t psg_tone_period[3];
    uint8_t psg_tone_atten[3];
    uint8_t psg_noise_atten;
    uint8_t psg_noise_shift_rate;
    uint8_t psg_noise_fb_white;
} Z80PeekData;
void Render_SetZ80Peek(bool active, const Z80PeekData *data);
