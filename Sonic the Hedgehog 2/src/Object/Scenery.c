// Decorative sprites for Sonic 2 (Nick Arcade's object 1C, "bgspr"): the stakes at the ends of the bridges, the glowing orbs in Hidden Palace and the poles of Hill Top's lifts. One object, its look from a table
// by the low nibble of its subtype; a high nibble asks for an animation (that number less one).
#include "Object/Scenery.h"
#include "Constants.h"

#include "Level.h"
#include "LevelScroll.h"
#include "Object/Bridge.h"

#include "Macros.h"

#include "Resource/Animation/Scenery.h"
#include "Resource/Mappings/HPZOrb.h"
#include "Resource/Mappings/HTZLift.h"
#include "Resource/Mappings/Scenery.h" // (Sonic 1's, kept for the debug list that names it)

typedef struct {
    uint8_t subtype; // 0x28
} Scratch_SceneryNA;

typedef struct {
    const uint8_t *mappings;
    uint16_t tile;
    uint8_t frame, width, priority;
} SceneryLook;

static const SceneryLook looks[] = {
    { Mappings_BridgeHPZ, TILE_MAP(0, 3, 0, 0, 0x300), 3, 4, 1 },   // Hidden Palace's bridge stake
    { Mappings_HPZOrb, TILE_MAP(1, 3, 0, 0, 0x35A), 0, 0x10, 1 },   // its glowing orb
    { Mappings_BridgeEHZ, TILE_MAP(0, 2, 0, 0, 0x3C6), 1, 4, 1 },   // Emerald Hill's bridge stake
    { Mappings_BridgeGHZ, TILE_MAP(0, 2, 0, 0, ArtTile_GHZ_Bridge), 1, 0x10, 1 }, // Green Hill's
    { Mappings_HTZLift, TILE_MAP(0, 2, 0, 0, 0x3E6), 1, 8, 4 },     // Hill Top's lift poles
    { Mappings_HTZLift, TILE_MAP(0, 2, 0, 0, 0x3E6), 2, 8, 4 },
};

void Obj_Scenery(Object *obj) {
    Scratch_SceneryNA *scratch = (Scratch_SceneryNA *)&obj->scratch;

    switch (obj->routine) {
    case 0: { // Initialization
        obj->routine += 2;
        const SceneryLook *look = &looks[(scratch->subtype & 0xF) % (sizeof(looks) / sizeof(looks[0]))];
        obj->mappings = look->mappings;
        obj->tile = look->tile;
        obj->render.b = 0;
        obj->render.f.level_fg = true;
        obj->frame = look->frame;
        obj->width_pixels = look->width;
        obj->priority = look->priority;

        uint8_t animated = scratch->subtype & 0xF0;
        if (animated) {
            obj->routine += 2;
            obj->anim = (animated >> 4) - 1;
            AnimateSprite(obj, Animation_Scenery);
            DisplaySprite(obj);
            if (IS_OFFSCREEN(obj->pos.l.x.f.u))
                ObjectDelete(obj);
            break;
        }
    }
        // Fallthrough
    case 2: // Standing
        DisplaySprite(obj);
        if (IS_OFFSCREEN(obj->pos.l.x.f.u))
            ObjectDelete(obj);
        break;
    case 4: // Animated
        AnimateSprite(obj, Animation_Scenery);
        DisplaySprite(obj);
        if (IS_OFFSCREEN(obj->pos.l.x.f.u))
            ObjectDelete(obj);
        break;
    }
}
