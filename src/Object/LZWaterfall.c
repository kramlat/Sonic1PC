#include "LZWaterfall.h"

#include "Level.h"
#include "LevelScroll.h"
#include "Macros.h"
#include "Resource/Animation/LZWaterfalls.h"
#include "Resource/Mappings/LZWaterfalls.h"

// Object 65 - decorative waterfall objects (LZ). Most pieces (corners,
// vertical/diagonal sections) are purely static; only the "splash"
// subtype ($x9) actually animates, and has two special variants: one that
// tracks the swaying water surface height, and one (LZ3 only) that's
// hidden behind the level's foreground layer until a specific changing
// chunk gets swapped in by the water slide.

static void WFall_Animate(Object *obj) {
    AnimateSprite(obj, Animation_LZWaterfalls);
    RememberState(obj);
}

static void WFall_OnWater(Object *obj) {
    obj->pos.l.y.f.u = (int16_t)(wtr_pos1 - 16);
    WFall_Animate(obj);
}

static void WFall_Priority(Object *obj) {
    obj->tile &= (uint16_t)~0x80; // render on low plane (hidden behind level chunk)
    // LZ3 water slide wall: opened once LEVEL_LAYOUT_FG(5)[12] gets
    // swapped to chunk $F8 by DynWater_LZ3 (see LZWaterFeatures.c).
    if (LEVEL_LAYOUT_FG(5)[12] == 0xF8)
        obj->tile |= 0x80; // render on high plane (visible)
    WFall_Animate(obj);
}

static void WFall_Main(Object *obj) {
    obj->routine += 4; // tentatively WFall_Display

    obj->mappings = Mappings_LZWaterfalls;
    obj->tile = TILE_MAP(0, 2, 0, 0, 0x259); // ArtTile_LZ_Splash | Tile_Pal3
    obj->render.f.align_fg = true; // real ASM ORs this bit in rather than clearing others first
    obj->width_pixels = 48 / 2;
    obj->priority = 1;

    uint8_t subtype = obj->scratch.u8[0];
    if ((int8_t)subtype < 0)
        obj->tile |= 0x80; // high-priority VRAM tile half

    uint8_t frame = subtype & 0xF;
    obj->frame = frame;

    if (frame != 9) {
        RememberState(obj); // WFall_Display
        return;
    }

    obj->priority = 0;
    obj->routine -= 2; // tentatively WFall_Animate

    if (subtype & 0x40) // subtype $49 -- align splash to water surface
        obj->routine = 6;
    if (subtype & 0x20) // subtype $A9 -- hidden splash in LZ3's changing chunk (takes priority over the above)
        obj->routine = 8;

    switch (obj->routine) {
    case 6: WFall_OnWater(obj); break;
    case 8: WFall_Priority(obj); break;
    default: WFall_Animate(obj); break;
    }
}

void Obj_LZWaterfall(Object *obj) {
    switch (obj->routine) {
    case 0: WFall_Main(obj); break;
    case 2: WFall_Animate(obj); break;
    case 4: RememberState(obj); break;
    case 6: WFall_OnWater(obj); break;
    case 8: WFall_Priority(obj); break;
    }
}
