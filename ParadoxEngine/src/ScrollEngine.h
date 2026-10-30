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

// ---- Deformation by band tables (sonic3k.asm: ApplyDeformation 103695, Draw_BG 103247, Get_DeformDrawPosVert 103322) ----
// A layer that scrolls in bands (a background, or a foreground that is deformed) is described by a table of band heights, top of the layer down, in pixels (a word each). Bit 15
// of a height ($8000) says the band has a position for every line instead of one for the band: the position list then holds that many entries for it. The list holds the
// positions of the bands in the same order.

#define SCROLL_BAND_PER_LINE 0x8000

// Which band holds line y of the layer (the last band for a y below the table, the first for a negative y), and where in it (the line's offset from the band's top)
int ScrollBands_BandAt(const uint16_t *heights, int count, int y, int *offset_in_band);

// Fills `lines` lines of horizontal scroll, as the VDP wants them (a foreground and a background value for each, both negated), for a view that starts at line `layer_y` of the layer
// the bands describe. With `deform_background` (ApplyDeformation) the band positions are the background's and `other_x` is the foreground's camera; without (ApplyFGDeformation) the band
// positions are the foreground's and `other_x` is the background's. The last band's position is held if the table ends before the lines do.
void ScrollEngine_ApplyDeformation(const uint16_t *heights, int count, const int16_t *positions, int16_t layer_y, int16_t other_x, bool deform_background, int lines, int16_t (*out)[2]);

// A band layer drawn into a plane as it scrolls (Draw_BG's work): every band has its own X, so a band that crosses a 16-pixel line draws a column of blocks for its own rows only,
// and a layer that crosses one in Y draws a row whose blocks each use their own band's X. The plane holds the view with a margin of a block, as LevelPlane does.
#include "LevelData.h"
#include "Viewport.h"
#define SCROLL_MAX_BANDS 64

typedef struct {
    plane_t *plane;
    const uint16_t *heights; // (the table: bit 15 is ignored here, a band is drawn as one piece)
    int count;
    int16_t x[SCROLL_MAX_BANDS]; // each band's position now: set these, then call ScrollBands_Draw
    int16_t y;                   // the layer's Y now
    int16_t drawn_x[SCROLL_MAX_BANDS]; // what the plane holds
    int16_t drawn_y;
} ScrollBands;

void ScrollBands_Init(ScrollBands *sb, plane_t *plane, const uint16_t *heights, int count);
void ScrollBands_DrawAll(ScrollBands *sb, const LevelLayoutPlane *layout);  // the whole view, from the positions now
void ScrollBands_Draw(ScrollBands *sb, const LevelLayoutPlane *layout);     // only what the move since the last draw needs
