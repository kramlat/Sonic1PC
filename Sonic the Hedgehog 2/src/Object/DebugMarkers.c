// The "?" corners of the invisible objects, for the debug cheat (see DebugMarkers.h).
#include "Object/DebugMarkers.h"
#include "Constants.h"

#include "Game.h"
#include "Level.h"

#include "Macros.h"
#include "ObjectSst.h"

// What an object that has nothing to show (the lava boxes and the leaves, which are never drawn, and the objects not built yet) shows in the debug list, and in the corners of its box: the "?" of the monitor art
// (tile 52 from the monitors' own, the icon of subtype 8), as three identical frames so that the subtype's frame still picks one. The prototype's own mapping for them
// (Obj31_MapUnc_15612) is empty.
#define UNKNOWN_FRAME 0, 1, 0xF8, 5, 0, 52, 0xFF, 0xF8
const uint8_t Mappings_DebugUnknown[] = {
    0, 6, 0, 14, 0, 22,
    UNKNOWN_FRAME,
    UNKNOWN_FRAME,
    UNKNOWN_FRAME,
};

void DebugMarkers_Show(Sst *obj, int16_t half_width, int16_t half_height) {
    if (!debug_cheat)
        return;
    obj->mappings = Mappings_DebugUnknown;
    obj->art_tile = TILE_MAP(1, 0, 0, 0, ArtTile_Monitor);
    obj->render_flags = SST_RENDER_LEVEL | SST_RENDER_MULTI_DRAW;
    obj->width_pixels = (uint8_t)(half_width > 0xFF ? 0xFF : half_width);
    obj->height_pixels = (uint8_t)(half_height > 0xFF ? 0xFF : half_height);
    obj->mapping_frame = 0; // (no main sprite: the corners are all there is)
    obj->child_count = 4;
    for (int i = 0; i < 4; i++) {
        obj->children[i].x = (int16_t)(obj->x_pos.f.u + ((i & 1) ? half_width - 8 : 8 - half_width));
        obj->children[i].y = (int16_t)(obj->y_pos.f.u + ((i & 2) ? half_height - 8 : 8 - half_height));
        obj->children[i].frame = 0;
    }
    Sst_Draw(obj);
}

bool DebugMarkers_TouchBox(uint8_t col_type, int16_t *half_width, int16_t *half_height) {
    switch (col_type & 0x3F) {
    case 0x14: *half_width = 0x40; *half_height = 0x20; return true;
    case 0x15: *half_width = 0x80; *half_height = 0x20; return true;
    case 0x16: *half_width = 0x20; *half_height = 0x20; return true;
    }
    return false;
}
