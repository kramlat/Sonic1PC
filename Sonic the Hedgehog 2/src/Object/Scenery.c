// Decorative sprites for Sonic 2 (the Simon Wai prototype's objects 1C and 71): the stakes at the ends of the bridges, the poles of Hill Top's lifts and other scenery of its zones (1C, its look from a table by the subtype),
// and the glowing orbs and bridge stakes of Hidden Palace (71, animated by the subtype's high nibble).
#include "Object/Scenery.h"
#include "Constants.h"

#include "Level.h"
#include "LevelScroll.h"
#include "Object/Bridge.h"

#include "Macros.h"

#include "Resource/Mappings/HPZOrb.h"
#include "Resource/Mappings/HTZLift.h"
#include "Resource/Mappings/SceneryA.h"
#include "Resource/Mappings/SceneryB.h"
#include "Resource/Mappings/SceneryD.h"
#include "Resource/Mappings/SceneryOOZ.h"
#include "Resource/Mappings/Scenery.h" // (Sonic 1's, kept for the debug list that names it)

typedef struct {
    uint8_t subtype; // 0x28
} Scratch_Scenery;

typedef struct {
    const uint8_t *mappings;
    uint16_t tile;
    uint8_t frame, width, priority;
} SceneryLook;

// Obj1C_InitData: each look is a frame of a mapping, its art and palette line, its width and priority
static const SceneryLook looks[] = {
    { Mappings_SceneryD, TILE_MAP(0, 2, 0, 0, 0x3FD), 0, 4, 6 },       // (a Metropolis thing)
    { Mappings_SceneryD, TILE_MAP(0, 2, 0, 0, 0x3FD), 1, 4, 6 },
    { Mappings_BridgeEHZ, TILE_MAP(0, 2, 0, 0, 0x3C6), 1, 4, 1 },      // the stake of a bridge (Emerald Hill's: the same mapping as its logs)
    { Mappings_SceneryD, TILE_MAP(0, 1, 0, 0, 0x3FD), 2, 0x10, 6 },
    { Mappings_HTZLift, TILE_MAP(0, 2, 0, 0, 0x3E6), 3, 8, 4 },        // the poles of Hill Top's lifts
    { Mappings_HTZLift, TILE_MAP(0, 2, 0, 0, 0x3E6), 4, 8, 4 },
    { Mappings_HTZLift, TILE_MAP(0, 2, 0, 0, 0x3E6), 1, 0x20, 1 },
    { Mappings_SceneryA, TILE_MAP(0, 2, 0, 0, 0x000), 0, 8, 1 },       // (level art)
    { Mappings_SceneryA, TILE_MAP(0, 2, 0, 0, 0x000), 1, 8, 1 },
    { Mappings_SceneryB, TILE_MAP(0, 2, 0, 0, 0x428), 0, 4, 4 },       // (Neo Green Hill's waterfall)
    { Mappings_SceneryOOZ, TILE_MAP(0, 2, 0, 0, 0x346), 0, 8, 4 },     // (Oil Ocean's oil)
    { Mappings_SceneryOOZ, TILE_MAP(0, 2, 0, 0, 0x346), 1, 8, 4 },
    { Mappings_SceneryOOZ, TILE_MAP(0, 2, 0, 0, 0x346), 2, 8, 4 },
    { Mappings_SceneryOOZ, TILE_MAP(0, 2, 0, 0, 0x346), 3, 8, 4 },
};

void Obj_Scenery(Object *obj) {
    Scratch_Scenery *scratch = (Scratch_Scenery *)&obj->scratch;

    if (obj->routine == 0) {
        obj->routine += 2;
        const SceneryLook *look = &looks[scratch->subtype % (sizeof(looks) / sizeof(looks[0]))];
        obj->mappings = look->mappings;
        obj->tile = look->tile;
        obj->render.b = 0;
        obj->render.f.level_fg = true;
        obj->frame = look->frame;
        obj->width_pixels = look->width;
        obj->priority = look->priority;
    }
    DisplaySprite(obj);
    if (IS_OFFSCREEN(obj->pos.l.x.f.u))
        ObjectDelete(obj);
}

// Ani_obj71: the animation of each kind
static const uint8_t Animation_HPZDecor[] = {
    0x00, 0x08, 0x00, 0x10, 0x00, 0x1F, 0x00, 0x28,
    0x08, 3, 3, 4, 5, 5, 4, 0xFF,
    0x05, 0, 0, 0, 1, 2, 3, 3, 2, 1, 2, 3, 3, 1, 0xFF,
    0x0B, 0, 1, 2, 3, 4, 5, 0xFD, 3,
    0x7F, 6, 0xFD, 2,
};

// Obj71_InitData: bridge stake, glowing orb (and Metropolis's lava bubble, which is not built yet)
static const SceneryLook decor_looks[] = {
    { Mappings_BridgeHPZ, TILE_MAP(0, 3, 0, 0, 0x300), 3, 4, 1 },
    { Mappings_HPZOrb, TILE_MAP(1, 3, 0, 0, 0x35A), 0, 0x10, 1 },
};

void Obj_HPZDecor(Object *obj) {
    Scratch_Scenery *scratch = (Scratch_Scenery *)&obj->scratch;

    if (obj->routine == 0) {
        obj->routine += 2;
        unsigned kind = scratch->subtype & 0xF;
        if (kind >= sizeof(decor_looks) / sizeof(decor_looks[0])) { // (the lava bubble)
            ObjectDelete(obj);
            return;
        }
        const SceneryLook *look = &decor_looks[kind];
        obj->mappings = look->mappings;
        obj->tile = look->tile;
        obj->render.b = 0;
        obj->render.f.level_fg = true;
        obj->frame = look->frame;
        obj->width_pixels = look->width;
        obj->priority = look->priority;
        obj->anim = scratch->subtype >> 4;
    }
    AnimateSprite(obj, Animation_HPZDecor);
    DisplaySprite(obj);
    if (IS_OFFSCREEN(obj->pos.l.x.f.u))
        ObjectDelete(obj);
}
