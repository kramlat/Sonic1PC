#pragma once

#include "Backend/VDP.h"

#include <stdbool.h>

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
extern uint16_t sprite_buffer_p2[BUFFER_SPRITES][4]; //The split screen's second player's sprite table (Sonic 2's Sprite_Table_P2)
extern int16_t hscroll_buffer[SCREEN_MAX_HEIGHT][2];

//The split screen's second view (see VDP_SetSplitScreen): its horizontal scroll (a foreground and a background X for each line, as hscroll_buffer's), a
//ready-made VDPView for it (its foreground plane at VRAM_FG_P2, its background plane at VRAM_BG_P2, its scroll table at VRAM_HSCROLL_P2, its sprite table in
//sprite_buffer_p2, the first view's palette), and the copy of the scroll table to VRAM that goes with the first's. The game fills hscroll_buffer_p2 from the
//second camera as it does the first's, calls Video_UploadHScrollP2 where it uploads hscroll_buffer, and sets video_second_view's scroll values each frame.
extern int16_t hscroll_buffer_p2[SCREEN_MAX_HEIGHT][2];
extern VDPView video_second_view;
void Video_UploadHScrollP2(void);

//Picture size (Video > Resolution): the aspect ratio picks the width or the height of the picture
typedef enum {
	RESOLUTION_ORIGINAL, //320x224 (10:7)
	RESOLUTION_16_9,
	RESOLUTION_8_5,
	RESOLUTION_5_4,
	RESOLUTION_4_3,
	RESOLUTION_COUNT,
} ResolutionMode;

const char *Video_ResolutionName(int mode);
int Video_GetResolution(void);
void Video_SelectResolution(int mode);   //Just picks it (start-up, before there is a window)
void Video_RequestResolution(int mode);  //Menu: pick it; it takes hold at the next safe point
bool Video_ResolutionPending(void);
bool Video_ApplyPendingResolution(void); //Called as each game mode starts, and by the level's loop; true if the size changed

//The water palette split (Labyrinth Zone, and any other zone that has a water line: the VDP swaps the palette at a scanline)
extern uint8_t wtr_state;         //Nonzero while the whole screen is under water
extern bool hblank_pal;           //Set every VBlank; tells HBlank() to swap CRAM to the water palette
extern bool doupdatesinhblank;    //Set when VBlank ran out of time; defers standard transfers to HBlank

//Video interface
void VDPSetupGame(void);
void VDPDisableWaterSplit(void);
void WaitForVBla(void);
void ClearScreen(void);
void CopyTilemap(const uint8_t *tilemap, size_t offset, size_t width, size_t height);
