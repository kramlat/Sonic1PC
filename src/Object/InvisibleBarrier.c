#include "InvisibleBarrier.h"

#include "Level.h"
#include "LevelScroll.h"
#include "Macros.h"

// Not #include "Object/Monitor.h" -- that header directly #includes its own
// Resource/Mappings/Monitor.h (an array *definition*), so including it here
// too would give this translation unit a duplicate definition and fail to
// link (same reasoning as Object/DebugList.c's own comment on this).
extern const uint8_t Mappings_Monitor[];

// Sonic's own half-width, for solid-object collision purposes -- matches
// real Sonic 1's sonic_solid_width (22/2).
#define INVISBARRIER_SONIC_SOLID_WIDTH 11

typedef struct {
    uint8_t subtype; // 0x28 -- high nibble = width setting, low nibble = height setting (0-based, 16px/unit each)
} Scratch_InvisibleBarrier;

void Obj_InvisibleBarrier(Object *obj) {
    Scratch_InvisibleBarrier *scratch = (Scratch_InvisibleBarrier*)&obj->scratch;

    switch (obj->routine) {
    case 0: // Invis_Main
        obj->routine = 2;
        obj->mappings = Mappings_Monitor;
        obj->tile = TILE_MAP(1, 0, 0, 0, 0x680); // real ASM ORs in Tile_Prio, unlike the real monitor object itself
        obj->render.f.align_fg = true;
        obj->width_pixels = (uint8_t)((((scratch->subtype >> 4) & 0xF) + 1) * 8);
        // Fallthrough
    case 2: { // Invis_Solid
        // Collision height isn't kept in width_pixels the way width is
        // (real ASM writes it to its own obHeight field) -- cheap enough to
        // recompute from the subtype every frame instead of adding a
        // scratch field just to cache one constant.
        uint16_t height = (uint16_t)(((scratch->subtype & 0xF) + 1) * 8);
        SolidObject(obj, (uint16_t)(obj->width_pixels + INVISBARRIER_SONIC_SOLID_WIDTH), height, (uint16_t)(height + 1),
                    obj->pos.l.x.f.u, NULL, NULL);

        if (IS_OFFSCREEN(obj->pos.l.x.f.u)) {
            ObjectDelete(obj);
            return;
        }
        if (debug_use)
            DisplaySprite(obj);
        break;
    }
    }
}
