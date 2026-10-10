// The waterfalls of Emerald Hill for Sonic 2 (Nick Arcade's object 49, "taki"): a tall strip of water, drawn in front of the scenery, that splashes (its next frame) while Sonic or Tails is within 64 pixels of it.
// The subtype is the frame it starts at (which of the strips).
#include "Object/EHZWaterfall.h"
#include "Constants.h"

#include "Level.h"
#include "LevelScroll.h"
#include "Object/Tails.h"

#include "Macros.h"

#include "Resource/Mappings/WaterfallEHZ.h"

typedef struct {
    uint8_t subtype; // 0x28
} Scratch_EHZWaterfall;

void Obj_EHZWaterfall(Object *obj) {
    Scratch_EHZWaterfall *scratch = (Scratch_EHZWaterfall *)&obj->scratch;

    if (obj->routine == 0) {
        obj->routine += 2;
        obj->mappings = Mappings_WaterfallEHZ;
        obj->tile = TILE_MAP(0, 1, 0, 0, 0x39E);
        obj->render.b = 0;
        obj->render.f.level_fg = true;
        obj->width_pixels = 0x20;
        obj->priority = 0;
        obj->y_rad = (int8_t)0x80;
        obj->render.f.explicit_height = true; // (its height is a number of its own, not the usual)
    }

    if (IS_OFFSCREEN(obj->pos.l.x.f.u)) {
        ObjectDelete(obj);
        return;
    }

    uint16_t x0 = (uint16_t)(obj->pos.l.x.f.u - 0x40), x1 = (uint16_t)(obj->pos.l.x.f.u + 0x40);
    uint16_t sonic_x = (uint16_t)player->pos.l.x.f.u;
    obj->frame = 0;
    if (sonic_x >= x0 && sonic_x < x1) {
        obj->frame = 1 + scratch->subtype;
    } else {
        uint16_t tails_x = (uint16_t)TAILS_OBJ->pos.l.x.f.u;
        if (tails_x >= x0 && tails_x < x1)
            obj->frame = 1;
        obj->frame += scratch->subtype;
    }
    DisplaySprite(obj);
}
