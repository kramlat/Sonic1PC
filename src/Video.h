#pragma once

#include "Backend/VDP.h"

//Video constants
// Real Genesis hardware caps this at 0x50 (80, the true VDP sprite-table
// size) -- deliberately raised here as a conscious accuracy trade-off, not
// a bug fix: sprite dropout at the real hardware limit doesn't affect
// gameplay physics/logic, only rendering, and this is a PC port with no
// actual VDP to constrain it.
//
// sprite_buffer no longer shares address space with anything else in VRAM
// (VDP_SetSpriteBuffer registers it as its own dedicated buffer -- see
// Backend/VDP.h's own comment on that function, and Video.c's
// VDPSetupGame), so the old VRAM_SPRITES/HUD_Lives collision that capped
// this at 116 no longer applies. The real remaining ceiling is the sprite
// entry's own link field (SPRITE_SL_L_AND, Backend/VDP.h), which is only 7
// bits wide (matching real hardware's actual sprite attribute table
// format) -- values at or past 128 there wrap around and get misread by
// VDP_Render as the list terminator or the wrong sprite entirely. 0x78
// (120) stays comfortably under that wall, and is already far more than
// any real level scene needs (a busy scene was measured well under 50
// total this session).
#define BUFFER_SPRITES 0x78

//Video globals
extern uint8_t vbla_routine;

extern uint8_t sprite_count;

extern uint8_t hbla_pal;
extern int16_t hbla_pos;    //Last scanline actually drawn by VDP_Render (rendering progress)
extern int16_t hbla_counter; //v_hblank_line: the scanline the HBlank H-int is currently set to trigger on.
                              //Only meaningfully driven by LZ's water surface tracking (not yet
                              //implemented -- see GM_Level_Branch); stays at the dormant position
                              //(223) for every other zone.

extern int16_t vid_scrpos_y_dup, vid_bg_scrpos_y_dup, vid_scrpos_x_dup, vid_bg_scrpos_x_dup, vid_bg3_scrpos_y_dup, vid_bg3_scrpos_x_dup;

extern uint16_t sprite_buffer[BUFFER_SPRITES][4];
extern int16_t hscroll_buffer[SCREEN_HEIGHT][2];

//Video interface
void VDPSetupGame(void);
void WaitForVBla(void);
void ClearScreen(void);
void CopyTilemap(const uint8_t *tilemap, size_t offset, size_t width, size_t height);
