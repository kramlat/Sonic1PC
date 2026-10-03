#pragma once

#include "Types.h"

// The camera: where the foreground and the two background layers the object drawing aligns to are scrolled to (pixels in .f.u, subpixels below).
// The game's scroll code moves them; the engine reads them to place sprites and to tell what is off screen.
extern dword_s scrpos_x, scrpos_y, bg_scrpos_x, bg_scrpos_y, bg3_scrpos_x, bg3_scrpos_y;

// The second player's camera, in the split screen (Sonic 2's Camera_X_pos_P2 / Camera_Y_pos_P2)
extern dword_s scrpos_x_p2, scrpos_y_p2;

// Placeholders for the second camera's follow-up (Sonic 2's scrolling for player 2): how far it moved this frame, and the look/delay state its follow code will
// need. Nothing reads or sets them yet; the follow code is written when the split screen itself is.
extern int16_t scrshift_x_p2, scrshift_y_p2;
extern int16_t look_shift_p2;
extern uint16_t cam_x_delay_p2;
extern uint8_t cam_y_delay_p2;

// Is this world X position outside the range objects stay loaded in? (The range is the original 320-pixel picture's, whatever the picture size: it is
// rounded to steps of 128 pixels, so a wider picture would move the step where objects wake and go, and attract-mode demos, which replay recorded
// button presses, would go wrong. It is already wide enough to cover the widest picture.)
#define IS_OFFSCREEN(x) (uint16_t)(((x) & ~0x7F) - ((scrpos_x.f.u - 0x80) & ~0x7F)) > (((320 + 0x80) & ~0x7F) + 0x100)
