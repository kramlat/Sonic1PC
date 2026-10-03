#include "LevelPlane.h"

#include <string.h>

#include "EngineConstants.h"
#include "LevelDrawCore.h"
#include "Camera.h"

LevelPlane level_plane_p2 = { .plane = VRAM_FG_P2, .camera_x = &scrpos_x_p2, .camera_y = &scrpos_y_p2 };

// The right edge of the view, for the column drawn on that side (see LevelDrawCore.h's RIGHT_EDGE_X, which is this for the picture's width)
static int16_t RightEdge(const LevelPlane *p) {
    int16_t w = p->width > 0 ? p->width : SCREEN_WIDTH;
    int16_t rounded = (int16_t)((w + 15) & ~15);
    return (int16_t)(rounded + (SCREEN_WIDTH > 320 ? 16 : 0));
}

void LevelPlane_Init(LevelPlane *p, size_t plane, const dword_s *camera_x, const dword_s *camera_y, int16_t width) {
    memset(p, 0, sizeof(*p));
    p->plane = plane;
    p->camera_x = camera_x;
    p->camera_y = camera_y;
    p->width = width;
    p->snap_x = *camera_x;
    p->snap_y = *camera_y;
}

void LevelPlane_DrawAll(const LevelPlane *p, const uint8_t *layout) {
    DrawChunks(p->camera_x->f.u, p->camera_y->f.u, layout, p->plane);
}

void LevelPlane_CameraMovedX(LevelPlane *p, int16_t old_x) {
    int16_t x = p->camera_x->f.u;

    // Horizontally: crossing a 16-pixel line (the block state says which line is next)
    if (((x & 0x10) ^ p->xblock) == 0) {
        p->xblock ^= 0x10;
        p->flags |= (x < old_x) ? LEVEL_SCROLL_LEFT : LEVEL_SCROLL_RIGHT;
    }
}

void LevelPlane_CameraMovedY(LevelPlane *p, int16_t old_y) {
    int16_t y = p->camera_y->f.u;

    // Vertically
    if ((y & 0x10) != (p->yblock & 0x10)) {
        p->yblock ^= 0x10;
        p->flags |= (y < old_y) ? LEVEL_SCROLL_UP : LEVEL_SCROLL_DOWN;
    }
}

void LevelPlane_CameraMoved(LevelPlane *p, int16_t old_x, int16_t old_y) {
    LevelPlane_CameraMovedX(p, old_x);
    LevelPlane_CameraMovedY(p, old_y);
}

void LevelPlane_ClearFlags(LevelPlane *p) {
    p->flags = 0;
}

void LevelPlane_Snapshot(LevelPlane *p) {
    p->snap_x = *p->camera_x;
    p->snap_y = *p->camera_y;
    p->flags_snap = p->flags;
}

void LevelPlane_DrawPending(LevelPlane *p, const uint8_t *layout) {
    if (p->flags_snap == 0)
        return;
    int16_t sx = p->snap_x.f.u;
    int16_t sy = p->snap_y.f.u;
    if (p->flags_snap & LEVEL_SCROLL_UP) {
        size_t pos = CalcVRAMPos(sx, sy, -16, -16);
        DrawBlocks_LR(p->plane, pos, sx, sy, -16, -16, layout);
        p->flags_snap &= ~LEVEL_SCROLL_UP;
    }
    if (p->flags_snap & LEVEL_SCROLL_DOWN) {
        size_t pos = CalcVRAMPos(sx, sy, -16, SCREEN_HEIGHT);
        DrawBlocks_LR(p->plane, pos, sx, sy, -16, SCREEN_HEIGHT, layout);
        p->flags_snap &= ~LEVEL_SCROLL_DOWN;
    }
    if (p->flags_snap & LEVEL_SCROLL_LEFT) {
        size_t pos = CalcVRAMPos(sx, sy, -16, -16);
        DrawBlocks_TB(p->plane, pos, sx, sy, -16, -16, layout);
        p->flags_snap &= ~LEVEL_SCROLL_LEFT;
    }
    if (p->flags_snap & LEVEL_SCROLL_RIGHT) {
        int16_t edge = RightEdge(p);
        size_t pos = CalcVRAMPos(sx, sy, edge, -16);
        DrawBlocks_TB(p->plane, pos, sx, sy, edge, -16, layout);
        p->flags_snap &= ~LEVEL_SCROLL_RIGHT;
    }
}
