#pragma once

#include <stdbool.h>
#include <stdint.h>

// The scrolling engine's frame (sonic3k.asm: LevelSetup, line 102216, and ScreenEvents, line 102264). Each zone and act has four routines, found in a table:
//   screen_init       draws the foreground when the level starts          (S3&K: <Z>_ScreenInit)
//   background_init   the same for the background                         (<Z>_BackgroundInit)
//   screen_event      runs every frame: the foreground's scrolling and drawing  (<Z>_ScreenEvent)
//   background_event  runs every frame: the background's scrolling, deformation and drawing (<Z>_BackgroundEvent)
// A routine that is NULL does nothing. A game registers its zones once, when it starts (ScrollEngine_SetZone), and the engine runs them: ScrollEngine_Init when a level begins,
// ScrollEngine_Events each frame after the camera has moved. The games' own camera code stays where it is (a game's camera is part of how it feels); what the zone routines do is
// what the engine is for. Zones that have not been written for it yet keep their old deformation routine as their background_event.

#define SCROLL_MAX_ZONES 64
#define SCROLL_MAX_ACTS 4

typedef struct {
    void (*screen_init)(void);
    void (*background_init)(void);
    void (*screen_event)(void);
    void (*background_event)(void);
} ScrollZone;

// Registers a zone's routines for one act, or for every act with act = -1. The table is copied.
void ScrollEngine_SetZone(int zone, int act, const ScrollZone *routines);
const ScrollZone *ScrollEngine_GetZone(int zone, int act); // NULL if the zone has none

void ScrollEngine_Init(int zone, int act);   // a level begins: its init routines, foreground then background
void ScrollEngine_Events(int zone, int act); // every frame: its screen event, then its background event

// How a level's layout wraps (S3&K sets these per level in LevelSetup: Layout_row_index_mask, Camera_Y_pos_mask, Screen_Y_wrap_value): chunks across and rows down, as masks, and
// the camera's Y in pixels. The defaults are the engine's old fixed ones (128 across, 16 down: $7F, $F, $7F0, $7FF); a level can set its own.
extern unsigned level_layout_col_mask, level_layout_row_mask;
extern uint16_t level_camera_y_mask, level_screen_y_wrap;
void ScrollEngine_SetLevelMasks(unsigned col_mask, unsigned row_mask); // also sets the two pixel masks from the rows
