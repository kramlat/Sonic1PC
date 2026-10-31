#include "Scenery.h"
#include "Object.h"
#include "Resource/Mappings/Scenery.h"
#include "SstLoad.h"
#include "ObjectSst.h"

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

void Obj_Scenery(Sst *self) {
    switch (self->routine) {
    case 0: { // Main
        self->routine += 2;

        uint8_t subtype = self->subtype;
        if (subtype >= 4)
            subtype = 0;
        const SceneryVariant *v = &scenery_variants[subtype];

        self->mappings = (subtype == 3) ? (const void *)Mappings_GHZBridge : (const void *)Mappings_Scenery;
        self->art_tile = v->tile;
        // Original is `ori.b #sprite_cam_field,obRender`: keeps the X/Y flip bits from the
        // object layout, so the scenery draws facing the way it was placed.
        self->render_flags |= SST_RENDER_LEVEL;
        self->mapping_frame = v->frame;
        self->width_pixels = v->width_pixels;
        self->priority = (uint16_t)(v->priority * 0x80);
        self->collision_flags = 0; // col_none
        __attribute__((fallthrough));
    }
    case 2: // ChkDel
        SstOnScreenTest2(self, self->x_pos.f.u);
        break;
    }
}
