#include "MagicSwitch.h"

#include "Level.h"
#include "LevelScroll.h"
#include "Object/Sonic.h"
#include "Resource/Mappings/UnusedSwitch.h"

static bool Swi_OutOfRange(int16_t x) {
    uint16_t obj_pos = (uint16_t)x & 0xFF80;
    uint16_t cam_pos = (uint16_t)(scrpos_x.f.u - 128) & 0xFF80;
    return (uint16_t)(obj_pos - cam_pos) > (128 + 320 + 192);
}

static bool Swi_ChkTouch(Object *obj, int16_t trig_w) {
    int16_t d0 = (int16_t)(player->pos.l.x.f.u - obj->pos.l.x.f.u + trig_w);
    if (d0 < 0)
        return false; // Sonic is left of switch
    if ((uint16_t)d0 >= (uint16_t)(trig_w * 2))
        return false; // Sonic is right of switch

    int16_t feet = (int16_t)(player->y_rad + player->pos.l.y.f.u);
    int16_t d0b = (int16_t)((obj->pos.l.y.f.u - 16) - feet);
    if (d0b > 0)
        return false; // Sonic is above switch
    if (d0b < -16)
        return false; // Sonic is below switch

    return true;
}

static void Swi_Action(Object *obj, Scratch_MagicSwitch *scratch) {
    obj->pos.l.y.f.u = scratch->orig_y;

    if (Swi_ChkTouch(obj, 16)) {
        obj->pos.l.y.f.u = (int16_t)(obj->pos.l.y.f.u + 2); // move down while touched
        f_switch[0] = 1;
    }

    if (Swi_OutOfRange(obj->pos.l.x.f.u))
        ObjectDelete(obj);
    else
        DisplaySprite(obj);
}

void Obj_MagicSwitch(Object *obj) {
    Scratch_MagicSwitch *scratch = (Scratch_MagicSwitch *)&obj->scratch;

    if (obj->routine == 0) {
        obj->routine = 2; // advance to Swi_Action
        obj->mappings = Mappings_UnusedSwitch;
        obj->tile = TILE_MAP(0, 2, 0, 0, ArtTile_Level); // ArtTile_Level | Tile_Pal3
        obj->render.f.align_fg = true;
        scratch->orig_y = obj->pos.l.y.f.u;
        obj->width_pixels = 32 / 2;
        obj->priority = 5;
    }

    Swi_Action(obj, scratch);
}
