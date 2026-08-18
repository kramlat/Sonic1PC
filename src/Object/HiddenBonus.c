#include "HiddenBonus.h"

#include "Level.h"
#include "LevelScroll.h"
#include "Macros.h"
#include "Sound.h"

typedef struct {
    int16_t time_left; // frames left to display the bonus sprite/points
} Scratch_HiddenBonus;

// Bonus points, indexed by subtype (0 is invalid/unused)
static const uint16_t bonus_points[] = {
    0,
    10000,
    1000,
#ifdef SCP_FIX_BUGS
    100, // subtype 03 -- the real game's own table has this wrong (see #else)
#else
    10, // matches the real (buggy) shipped table -- should be 100
#endif
};

// Object 7D - hidden points at the end of a level
void Obj_HiddenBonus(Object *obj) {
    Scratch_HiddenBonus *scratch = (Scratch_HiddenBonus *)&obj->scratch;

    switch (obj->routine) {
    case 0: { // ChkTouch
        const int16_t radius = 0x10;
        const int16_t diameter = radius * 2;

        int16_t x_diff = (int16_t)(player->pos.l.x.f.u - obj->pos.l.x.f.u + radius);
        if ((uint16_t)x_diff < (uint16_t)diameter) {
            int16_t y_diff = (int16_t)(player->pos.l.y.f.u - obj->pos.l.y.f.u + radius);
            if ((uint16_t)y_diff < (uint16_t)diameter && !debug_use && !big_ring_collected) {
                // Touched -- advance to Display
                obj->routine += 2;
                obj->mappings = Mappings_HiddenBonuses;
                obj->tile = TILE_MAP(1, 0, 0, 0, ArtTile_Hidden_Points);
                obj->render.b = 0;
                obj->render.f.align_fg = true;
                obj->priority = 0;
                obj->width_pixels = 32 / 2;

                uint8_t subtype = obj->scratch.u8[0]; // must not be 0 -- trusted level data, no bounds check on real hardware either
                obj->frame = subtype; // subtype doubles as the object's frame ID
                scratch->time_left = 120 - 1;
                PlaySound(sfx_Bonus);

                AddPoints(bonus_points[subtype]);
            }
        }
        if (IS_OFFSCREEN(obj->pos.l.x.f.u))
            ObjectDelete(obj);
        break;
    }
    case 2: // Display
        if (--scratch->time_left < 0) {
            ObjectDelete(obj);
            break;
        }
        if (IS_OFFSCREEN(obj->pos.l.x.f.u)) {
            ObjectDelete(obj);
            break;
        }
        DisplaySprite(obj);
        break;
    }
}
