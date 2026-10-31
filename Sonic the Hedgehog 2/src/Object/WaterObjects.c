// The water's own object for Sonic 2 (Nick Arcade's 04): the surface (three strips across the screen, each as wide as the screen was, that follow the camera and bob through a table of frames). The splash when
// a player enters or leaves the water is DustSplash.c's.
#include "Object/WaterObjects.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"
#include "Constants.h"

#include "Level.h"
#include "LevelScroll.h"
#include "Object/Sonic.h"

#include "Macros.h"

#include "Resource/Mappings/NAWaterSurface.h"

// The frame of the surface on each frame of its cycle (Ani_WaterSurface)
static const uint8_t surface_frames[64] = {
    0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1,
    1, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2,
    2, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0,
};

void Obj_NAWaterSurface(Sst *self) {
    uint8_t which = self->subtype; // 0 and 1 are Nick Arcade's two, 2 the extra one

    if (self->routine == 0) {
        self->routine += 2;
        self->mappings = Mappings_NAWaterSurface;
        self->art_tile = TILE_MAP(1, 0, 0, 0, 0x400);
        self->render_flags = SST_RENDER_LEVEL;
        self->width_pixels = 0x80;
        self->priority = 0 * 0x80;
    }

    // (3 to 5 are the second view's of a split screen: the same places along its camera)
    int16_t base = (int16_t)((which >= 3 ? scrpos_x_p2.f.u : scrpos_x.f.u) + ((frame_count & 1) ? 0x20 : 0));
    int kind = which >= 3 ? which - 3 : which;
    self->x_pos.f.u = (int16_t)(base + (kind == 0 ? 0x60 : kind == 1 ? 0x120 : 0x1E0));
    self->y_pos.f.u = wtr_pos1;
    self->mapping_frame = surface_frames[self->anim_frame & 0x3F];
    self->anim_frame = (uint8_t)((self->anim_frame + 1) & 0x3F);
    Sst_Draw(self);
}
