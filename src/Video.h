#pragma once

#include "Backend/VDP.h"

//Video constants
#define BUFFER_SPRITES 0x50

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
