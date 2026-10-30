#pragma once

#include "Camera.h"
#include "LevelPlane.h"

#include "Types.h"

//Level deformation globals
extern uint8_t nobgscroll, bgscrollvert;


extern dword_s bg2_scrpos_x, bg2_scrpos_y; // (the others, scrpos_*, bg_scrpos_* and bg3_scrpos_*, are the engine's: Camera.h)
extern dword_s scrpos_x_dup, scrpos_y_dup, bg_scrpos_x_dup, bg_scrpos_y_dup, bg2_scrpos_x_dup, bg2_scrpos_y_dup, bg3_scrpos_x_dup, bg3_scrpos_y_dup;

extern int16_t scrshift_x, scrshift_y;

extern LevelPlane fg_plane; // the foreground plane and its camera follow-up

extern int16_t look_shift;

extern uint16_t cam_x_delay;
extern uint8_t cam_y_delay;

extern const int8_t Drown_WobbleData[];
extern const int8_t Lz_Scroll_Data[];
//Level scroll functions
void BgScrollSpeed(int16_t x, int16_t y);
void DeformLayers(void);

// Zones whose background plane is kept by the engine's band layers (ScrollBands) instead of the scroll flags
bool LevelScroll_UsesBands(void);
void LevelScroll_BackgroundReset(void);
void LevelScroll_DrawBands(const dword_s *bg1_x, const dword_s *bg2_x, const dword_s *bg3_x, const dword_s *bg_y);
