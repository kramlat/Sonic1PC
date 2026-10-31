#include "MagicSwitch.h"
#include "Object.h"
#include "ObjectSst.h"
#include "SstLoad.h"
#include "Constants.h"

#include "Level.h"
#include "LevelScroll.h"
#include "Object/Sonic.h"
#include "Resource/Mappings/UnusedSwitch.h"

static bool Swi_ChkTouch(Sst *self, int16_t trig_w) {
    int16_t d0 = (int16_t)(player->x_pos.f.u - self->x_pos.f.u + trig_w);
    if (d0 < 0)
        return false; // Sonic is left of switch
    if ((uint16_t)d0 >= (uint16_t)(trig_w * 2))
        return false; // Sonic is right of switch

    int16_t feet = (int16_t)(player->y_radius + player->y_pos.f.u);
    int16_t d0b = (int16_t)((self->y_pos.f.u - 16) - feet);
    if (d0b > 0)
        return false; // Sonic is above switch
    if (d0b < -16)
        return false; // Sonic is below switch

    return true;
}

static void Swi_Action(Sst *self, Own_MagicSwitch *own) {
    self->y_pos.f.u = own->orig_y;

    if (Swi_ChkTouch(self, 16)) {
        self->y_pos.f.u = (int16_t)(self->y_pos.f.u + 2); // move down while touched
        f_switch[0] = 1;
    }

    SstOnScreenTest(self);
}

void Obj_MagicSwitch(Sst *self) {
    Own_MagicSwitch *own = (Own_MagicSwitch *)self->own.u8;

    if (self->routine == 0) {
        self->routine = 2; // advance to Swi_Action
        self->mappings = Mappings_UnusedSwitch;
        self->art_tile = TILE_MAP(0, 2, 0, 0, ArtTile_Level); // ArtTile_Level | Tile_Pal3
        self->render_flags = SST_RENDER_LEVEL;
        own->orig_y = self->y_pos.f.u;
        self->width_pixels = 32 / 2;
        self->priority = 5 * 0x80;
    }

    Swi_Action(self, own);
}
