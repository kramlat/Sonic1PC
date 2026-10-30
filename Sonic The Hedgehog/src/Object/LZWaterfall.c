#include "LZWaterfall.h"

#include "Level.h"
#include "LevelScroll.h"
#include "Macros.h"
#include "Object.h"
#include "SstLoad.h"
#include "Resource/Animation/LZWaterfalls.h"
#include "Resource/Mappings/LZWaterfalls.h"

// Object 65 - decorative waterfall objects (LZ). Most pieces (corners,
// vertical/diagonal sections) are purely static; only the "splash"
// subtype ($x9) actually animates, and has two special variants: one that
// tracks the swaying water surface height, and one (LZ3 only) that's
// hidden behind the level's foreground layer until a specific changing
// chunk gets swapped in by the water slide.

static void WFall_Animate(Sst *self) {
    Sst_Animate(self, Animation_LZWaterfalls);
    SstOnScreenTest(self);
}

static void WFall_OnWater(Sst *self) {
    self->y_pos.f.u = (int16_t)(wtr_pos1 - 16);
    WFall_Animate(self);
}

static void WFall_Priority(Sst *self) {
    self->art_tile &= (uint16_t)~0x8000; // render on low plane (hidden behind level chunk)
    // LZ3 water slide wall: visible only once switch $F has made
    // DLE_LZ3 swap row 5, columns 12-13 to chunks $17/$18 (MJ: P128
    // compares the whole word $1718). $F8/$F9 there is the closed wall.
    if (LevelLayout_Chunk(LEVEL_LAYOUT_PLANE_FG, 12, 5) == 0x17 && LevelLayout_Chunk(LEVEL_LAYOUT_PLANE_FG, 13, 5) == 0x18)
        self->art_tile |= 0x8000; // render on high plane (visible)
    WFall_Animate(self);
}

static void WFall_Main(Sst *self) {
    self->routine += 4; // tentatively WFall_Display

    self->mappings = Mappings_LZWaterfalls;
    self->art_tile = TILE_MAP(0, 2, 0, 0, ArtTile_LZ_Splash); // | Tile_Pal3
    self->render_flags |= SST_RENDER_LEVEL; // real ASM ORs this bit in rather than clearing others first
    self->width_pixels = 48 / 2;
    self->priority = 1 * 0x80;

    uint8_t subtype = self->subtype;
    if ((int8_t)subtype < 0)
        self->art_tile |= 0x8000; // high-priority flag (real ASM bsets bit 7 of obGfx's high byte)

    uint8_t frame = subtype & 0xF;
    self->mapping_frame = frame;

    if (frame != 9) {
        SstOnScreenTest(self); // WFall_Display
        return;
    }

    self->priority = 0;
    self->routine -= 2; // tentatively WFall_Animate

    if (subtype & 0x40) // subtype $49 -- align splash to water surface
        self->routine = 6;
    if (subtype & 0x20) // subtype $A9 -- hidden splash in LZ3's changing chunk (takes priority over the above)
        self->routine = 8;

    // Real ASM falls straight into WFall_Animate here for every splash
    // variant; routines 6/8 only take over from the next frame.
    WFall_Animate(self);
}

void Obj_LZWaterfall(Sst *self) {
    switch (self->routine) {
    case 0: WFall_Main(self); break;
    case 2: WFall_Animate(self); break;
    case 4: SstOnScreenTest(self); break;
    case 6: WFall_OnWater(self); break;
    case 8: WFall_Priority(self); break;
    }
}
