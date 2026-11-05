#include "Pylon.h"
#include "Constants.h"
#include "Object.h"

#include "LevelScroll.h"
#include "Resource/Mappings/Pylon.h"

// Object 5C - metal pylons in foreground (SLZ)

static void Pyl_Display(Sst *self) {
    // Pylons scroll twice as fast as the camera, and opposite its direction
    // -- a cheap parallax trick that makes them appear to swing past close
    // to the screen. Doubling+truncating the raw fixed-point camera
    // position (rather than doubling just the integer part) preserves the
    // real hardware's exact sub-pixel wraparound behavior.
    uint32_t doubled_x = (uint32_t)scrpos_x.v * 2;
    self->x_pos.f.u = (int16_t)-(int16_t)(doubled_x >> 16);

    uint32_t doubled_y = (uint32_t)scrpos_y.v * 2;
    int16_t y = (int16_t)((doubled_y >> 16) & 0x3F); // vertically wrap every 64px
    self->y_pos.f.u = (int16_t)(-y + 0x100);

    Sst_Draw(self);
}

void Obj_Pylon(Sst *self) {
    if (self->routine == 0) {
        self->routine = 2; // advance to Pyl_Display
        self->mappings = Mappings_Pylon;
        self->art_tile = TILE_MAP(1, 0, 0, 0, ArtTile_SLZ_Pylon); // | Tile_Prio
        self->width_pixels = 32 / 2;
        self->render_flags = 0; // screen-fixed positioning mode (explicit, matches FixBugs)
    }

    Pyl_Display(self);
}
