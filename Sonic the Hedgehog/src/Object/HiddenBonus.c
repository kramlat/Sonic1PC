#include "HiddenBonus.h"
#include "Resource/Mappings/HiddenBonuses.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"

#include "Level.h"
#include "LevelScroll.h"
#include "Macros.h"
#include "Sound.h"

typedef struct {
    int16_t time_left; // frames left to display the bonus sprite/points
} Own_HiddenBonus;

// Bonus points, indexed by subtype (0 is invalid/unused)
// (in tens, AddPoints' unit: 1000 = 10000 points)
static const uint16_t bonus_points[] = {
    0,
    1000,
    100,
#ifdef SCP_FIX_BUGS
    10, // subtype 03 -- 100 points; the real game's own table has this wrong (see #else)
#else
    1, // matches the real (buggy) shipped table -- 10 points, should be 100
#endif
};

// Object 7D - hidden points at the end of a level
void Obj_HiddenBonus(Sst *self) {
    Own_HiddenBonus *own = (Own_HiddenBonus *)self->own.u8;

    switch (self->routine) {
    case 0: { // ChkTouch
        const int16_t radius = 0x10;
        const int16_t diameter = radius * 2;

        int16_t x_diff = (int16_t)(player->x_pos.f.u - self->x_pos.f.u + radius);
        if ((uint16_t)x_diff < (uint16_t)diameter) {
            int16_t y_diff = (int16_t)(player->y_pos.f.u - self->y_pos.f.u + radius);
            if ((uint16_t)y_diff < (uint16_t)diameter && !debug_use && !big_ring) {
                // Touched -- advance to Display
                self->routine += 2;
                self->mappings = Mappings_HiddenBonuses;
                self->art_tile = TILE_MAP(1, 0, 0, 0, ArtTile_Hidden_Points);
                self->render_flags |= SST_RENDER_LEVEL;
                self->priority = 0 * 0x80;
                self->width_pixels = 32 / 2;

                uint8_t subtype = self->subtype; // must not be 0 -- trusted level data, no bounds check on real hardware either
                self->mapping_frame = subtype; // subtype doubles as the object's frame ID
                own->time_left = 120 - 1;
                PlaySound(sfx_Bonus);

                AddPoints(bonus_points[subtype]);
            }
        }
        SstDeleteIfNotInRange2(self, self->x_pos.f.u);
        break;
    }
    case 2: // Display
        if (--own->time_left < 0) {
            Sst_Delete(self);
            break;
        }
        if (SstOutOfRange(self->x_pos.f.u)) {
            SstLoad_Forget(self);
            Sst_Delete(self);
            break;
        }
        Sst_Draw(self);
        break;
    }
}
