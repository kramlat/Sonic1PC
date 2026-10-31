#include "FlapDoor.h"
#include "Object.h"
#include "ObjectSst.h"
#include "SstLoad.h"

#include "Level.h"
#include "LevelScroll.h"
#include "Macros.h"
#include "Object/Sonic.h"
#include "Resource/Animation/FlappingDoor.h"
#include "Resource/Mappings/FlappingDoor.h"
#include "Sound.h"

// Object 0C - flapping door before wind tunnels (LZ). Toggles open/closed
// on a timer, playing a sound and going solid (and disallowing the wind
// tunnel just past it) only while shut and Sonic hasn't already passed
// through it.

static void Flap_Animate(Sst *self) {
    Sst_Animate(self, Animation_FlappingDoor);

    f_wtunneldisallow = false;

    if (self->mapping_frame != 0 || player->x_pos.f.u >= self->x_pos.f.u) {
        SstOnScreenTest(self);
        return;
    }

    f_wtunneldisallow = true;

    int16_t x_rad = (int16_t)(16 / 2 + 11 /* sonic_solid_width */);
    SstSolidObject(self, (uint16_t)x_rad, 64 / 2, 64 / 2 + 1, self->x_pos.f.u, NULL, NULL);

    SstOnScreenTest(self);
}

static void Flap_OpenClose(Sst *self, Own_FlapDoor *own) {
    if (--own->wait >= 0) {
        Flap_Animate(self);
        return;
    }

    own->wait = own->time;
    self->anim ^= 1;
    if ((self->render_flags & SST_RENDER_ON_SCREEN))
        QueueSound2(sfx_Door);

    Flap_Animate(self);
}

void Obj_FlapDoor(Sst *self) {
    Own_FlapDoor *own = (Own_FlapDoor *)self->own.u8;

    switch (self->routine) {
    case 0:
        self->routine += 2;
        self->mappings = Mappings_FlappingDoor;
        self->art_tile = TILE_MAP(0, 2, 0, 0, ArtTile_LZ_Flapping_Door); // | Tile_Pal3
        self->render_flags |= SST_RENDER_LEVEL;
        self->width_pixels = 80 / 2;
        own->time = (int16_t)(self->subtype * 60);
        __attribute__((fallthrough));
    case 2:
        Flap_OpenClose(self, own);
        break;
    }
}
