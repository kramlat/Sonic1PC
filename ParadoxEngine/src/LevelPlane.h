#pragma once

#include <stdint.h>
#include <stddef.h>

#include "Types.h"

// A foreground plane that follows a camera: what keeps one view's foreground drawn as its camera moves. The game keeps one LevelPlane per view (two in a split
// screen); each is plain data it may assign or reset, pointing at its own plane in VRAM and its own camera.
//
// How it works is what Sonic 1 and 2 do: the plane holds the level around the camera with a margin of a block; each time the camera crosses a 16-pixel line it
// flags the row or column that has become needed (LevelPlane_CameraMoved), the flags are copied at the vertical blank together with the camera (the "dup"
// state, LevelPlane_Snapshot), and the blank's drawing (LevelPlane_DrawPending) writes the flagged rows and columns for that snapshot.

// The flags (the same bits as Sonic 1's SCROLL_FLAG_*)
#define LEVEL_SCROLL_UP    (1 << 0) // the row above the view is needed
#define LEVEL_SCROLL_DOWN  (1 << 1) // the row below
#define LEVEL_SCROLL_LEFT  (1 << 2) // the column to the left
#define LEVEL_SCROLL_RIGHT (1 << 3) // the column to the right

typedef struct {
    size_t plane;                       // the foreground plane's address in VRAM
    const dword_s *camera_x, *camera_y; // the camera it follows (live)
    dword_s snap_x, snap_y;             // the camera as of the last snapshot
    uint16_t flags;                     // rows and columns that became needed since the last snapshot
    uint16_t flags_snap;                // ... and the copy the drawing works from
    uint8_t xblock, yblock;             // the 16-pixel line state of the camera: bit 4 of the position when it last crossed
    int16_t width;                      // how wide the view is (0: the picture's width): the column on the right is drawn that far out
} LevelPlane;

// Sets a plane up (everything cleared, the camera snapshot taken)
void LevelPlane_Init(LevelPlane *p, size_t plane, const dword_s *camera_x, const dword_s *camera_y, int16_t width);

// Draws the whole view at the camera: the rows of blocks that fill the plane, from one block above the view
void LevelPlane_DrawAll(const LevelPlane *p, const uint8_t *layout);

// The camera moved from `old_x` / `old_y` to where it is now: flags the column / row the new position needs. Call after each move of the camera (X and Y have
// their own, as the games move the camera one axis at a time; CameraMoved does both).
void LevelPlane_CameraMovedX(LevelPlane *p, int16_t old_x);
void LevelPlane_CameraMovedY(LevelPlane *p, int16_t old_y);
void LevelPlane_CameraMoved(LevelPlane *p, int16_t old_x, int16_t old_y);

// Start of a frame's camera movement: forgets the flags the last frame left (they were copied at the blank)
void LevelPlane_ClearFlags(LevelPlane *p);

// Vertical blank: copies the camera and the flags for the drawing (the live flags stay, until ClearFlags)
void LevelPlane_Snapshot(LevelPlane *p);

// Draws what the snapshot's flags ask for, against the snapshot's camera, and clears them
void LevelPlane_DrawPending(LevelPlane *p, const uint8_t *layout);

// The second view's foreground plane in a split screen, on VRAM_FG_P2 and the second camera (Camera.h). Set up with LevelPlane_Init when a split level starts;
// valid (and idle) before that.
extern LevelPlane level_plane_p2;
