#include "Constants.h"
#include "Game.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"

#include "Object/SegaScreen.h"
#include "Resource/Mappings/SegaSprites.h"

// The objects of the SEGA screen of the Sonic 2 pre-beta (the final game's), loaded by GM_Sega.c: $B1 here, the two blocks that hide the trademark of the logo (the pre-beta makes it for every
// console but the Japanese one, where the trademark stays); Sonic running across the logo ($B0) is the next stage.

void Obj_SegaTrademarkHider(Sst *self) {
    switch (self->routine) {
    case 0:
        self->routine += 2;
        self->mappings = Mappings_SegaSprites;
        self->mapping_frame = 4;
        self->art_tile = TILE_MAP(0, 0, 0, 0, 3); // (tile 3 of the logo's art: blank)
        self->render_flags = 0;
        self->priority = 2 * 0x80;
        self->width_pixels = 8;
        self->x_pos.f.u = 0x174 + (PLANE_WIDEADD * 4);
        self->y_pos.f.u = 0xD8 + SCREEN_TALLADD2;
        break;
    }
    Sst_Draw(self);
}
