#include "InvisibleBarrier.h"

#include "Level.h"
#include "LevelScroll.h"
#include "Macros.h"

#include "Resource/Mappings/InvisibleBarrier.h"

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
        // Real hardware's own debug-only preview uses a dedicated mapping
        // (Map_Invis -- 4 tightly-packed Eggman-icon tiles, no monitor box),
        // not Monitor's own box+icon combo frames.
        obj->mappings = Mappings_InvisibleBarrier;
        obj->tile = TILE_MAP(1, 0, 0, 0, ArtTile_Monitor); // real ASM ORs in Tile_Prio, unlike the real monitor object itself
        // Mappings_InvisibleBarrier only has 3 valid frames (0-2), unlike
        // the old Mappings_Monitor reuse (12 frames) -- a pooled slot's
        // stale leftover obj->frame from some earlier, differently-framed
        // occupant is now much more likely to read out of bounds, so it
        // needs an explicit reset here (real ASM never sets obFrame
        // either, but its own Map_Invis is looked up by real hardware's
        // own debug tools differently -- doesn't need this same care).
        obj->frame = 0;
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
