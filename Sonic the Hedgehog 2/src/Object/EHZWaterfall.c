// The waterfalls of Emerald Hill for Sonic 2 (Nick Arcade's object 49, "taki"): a tall strip of water, drawn in front of the scenery, that splashes (its next frame) while Sonic or Tails is within 64 pixels of it.
// The subtype is the frame it starts at (which of the strips).
#include "Object/EHZWaterfall.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"
#include "Constants.h"

#include "Level.h"
#include "LevelScroll.h"
#include "Object/Tails.h"

#include "Macros.h"

#include "Resource/Mappings/WaterfallEHZ.h"

typedef struct {
    uint8_t subtype; // 0x28
} Own_EHZWaterfall;

void Obj_EHZWaterfall(Sst *self) {
    Own_EHZWaterfall *own = (Own_EHZWaterfall *)self->own.u8;

    if (self->routine == 0) {
        self->routine += 2;
        self->mappings = Mappings_WaterfallEHZ;
        self->art_tile = TILE_MAP(0, 1, 0, 0, 0x39E);
        self->render_flags = SST_RENDER_LEVEL;
        self->width_pixels = 0x20;
        self->priority = 0 * 0x80;
        self->y_radius = (int8_t)0x80;
        self->height_pixels = self->y_radius; // (its height is a number of its own, not the usual)
    }

    if (SstOutOfRange(self->x_pos.f.u)) {
        SstLoad_Forget(self);
        Sst_Delete(self);
        return;
    }

    uint16_t x0 = (uint16_t)(self->x_pos.f.u - 0x40), x1 = (uint16_t)(self->x_pos.f.u + 0x40);
    uint16_t sonic_x = (uint16_t)player->x_pos.f.u;
    self->mapping_frame = 0;
    if (sonic_x >= x0 && sonic_x < x1) {
        self->mapping_frame = 1 + self->subtype;
    } else {
        uint16_t tails_x = (uint16_t)TAILS_OBJ->x_pos.f.u;
        if (tails_x >= x0 && tails_x < x1)
            self->mapping_frame = 1;
        self->mapping_frame += self->subtype;
    }
    Sst_Draw(self);
}
