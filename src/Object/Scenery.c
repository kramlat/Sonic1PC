#include "Scenery.h"

#include "Level.h"
#include "LevelScroll.h"
#include "Macros.h"

// Object 1C - scenery (GHZ bridge stump, SLZ lava/fireball thrower)

typedef struct {
    const void *mappings;
    uint16_t tile;
    uint8_t frame;
    uint8_t width_pixels;
    uint8_t priority;
} SceneryVariant;

// Indexed by subtype -- subtypes 0-2 are three identical SLZ fireball
// launcher entries in the real data (only the first is ever actually
// used), subtype 3 is GHZ's own bridge stump. Collision type is always
// col_none (0) for every variant, so it's not part of this table.
static const SceneryVariant scenery_variants[4] = {
    { NULL, TILE_MAP(0, 2, 0, 0, ArtTile_SLZ_Fireball_Launcher), 0, 16 / 2, 2 }, // | Tile_Pal3 (mappings filled in below, needs Mappings_Scenery)
    { NULL, TILE_MAP(0, 2, 0, 0, ArtTile_SLZ_Fireball_Launcher), 0, 16 / 2, 2 },
    { NULL, TILE_MAP(0, 2, 0, 0, ArtTile_SLZ_Fireball_Launcher), 0, 16 / 2, 2 },
    { NULL, TILE_MAP(0, 2, 0, 0, ArtTile_GHZ_Bridge), 1, 32 / 2, 1 }, // | Tile_Pal3 -- Mappings_GHZBridge filled in below
};

void Obj_Scenery(Object *obj) {
    switch (obj->routine) {
    case 0: { // Main
        obj->routine += 2;

        uint8_t subtype = obj->scratch.u8[0];
        if (subtype >= 4)
            subtype = 0;
        const SceneryVariant *v = &scenery_variants[subtype];

        obj->mappings = (subtype == 3) ? (const void *)Mappings_GHZBridge : (const void *)Mappings_Scenery;
        obj->tile = v->tile;
        obj->render.b = 0;
        obj->render.f.align_fg = true;
        obj->frame = v->frame;
        obj->width_pixels = v->width_pixels;
        obj->priority = v->priority;
        obj->col_type = 0; // col_none
        __attribute__((fallthrough));
    }
    case 2: // ChkDel
        if (IS_OFFSCREEN(obj->pos.l.x.f.u)) {
            ObjectDelete(obj);
            return;
        }
        DisplaySprite(obj);
        break;
    }
}
