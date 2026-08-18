#include "Pylon.h"

#include "LevelScroll.h"
#include "Resource/Mappings/Pylon.h"

// Object 5C - metal pylons in foreground (SLZ)

static void Pyl_Display(Object *obj) {
    // Pylons scroll twice as fast as the camera, and opposite its direction
    // -- a cheap parallax trick that makes them appear to swing past close
    // to the screen. Doubling+truncating the raw fixed-point camera
    // position (rather than doubling just the integer part) preserves the
    // real hardware's exact sub-pixel wraparound behavior.
    uint32_t doubled_x = (uint32_t)scrpos_x.v * 2;
    obj->pos.s.x = (int16_t)-(int16_t)(doubled_x >> 16);

    uint32_t doubled_y = (uint32_t)scrpos_y.v * 2;
    int16_t y = (int16_t)((doubled_y >> 16) & 0x3F); // vertically wrap every 64px
    obj->pos.s.y = (int16_t)(-y + 0x100);

    DisplaySprite(obj);
}

void Obj_Pylon(Object *obj) {
    if (obj->routine == 0) {
        obj->routine = 2; // advance to Pyl_Display
        obj->mappings = Mappings_Pylon;
        obj->tile = TILE_MAP(1, 0, 0, 0, ArtTile_SLZ_Pylon); // | Tile_Prio
        obj->width_pixels = 32 / 2;
        obj->render.b = 0; // screen-fixed positioning mode (explicit, matches FixBugs)
    }

    Pyl_Display(obj);
}
