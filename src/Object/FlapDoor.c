#include "FlapDoor.h"

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

static void Flap_Animate(Object *obj) {
    AnimateSprite(obj, Animation_FlappingDoor);

    f_wtunneldisallow = false;

    if (obj->frame != 0 || player->pos.l.x.f.u >= obj->pos.l.x.f.u) {
        RememberState(obj);
        return;
    }

    f_wtunneldisallow = true;

    int16_t x_rad = (int16_t)(16 / 2 + 11 /* sonic_solid_width */);
    SolidObject(obj, (uint16_t)x_rad, 64 / 2, 64 / 2 + 1, obj->pos.l.x.f.u, NULL, NULL);

    RememberState(obj);
}

static void Flap_OpenClose(Object *obj, Scratch_FlapDoor *scratch) {
    if (--scratch->wait >= 0) {
        Flap_Animate(obj);
        return;
    }

    scratch->wait = scratch->time;
    obj->anim ^= 1;
    if (obj->render.f.on_screen)
        QueueSound2(sfx_Door);

    Flap_Animate(obj);
}

void Obj_FlapDoor(Object *obj) {
    Scratch_FlapDoor *scratch = (Scratch_FlapDoor *)&obj->scratch;

    switch (obj->routine) {
    case 0:
        obj->routine += 2;
        obj->mappings = Mappings_FlappingDoor;
        obj->tile = TILE_MAP(0, 2, 0, 0, 0x328); // ArtTile_LZ_Flapping_Door | Tile_Pal3
        obj->render.f.align_fg = true;
        obj->width_pixels = 80 / 2;
        scratch->time = (int16_t)(obj->scratch.u8[0] * 60);
        __attribute__((fallthrough));
    case 2:
        Flap_OpenClose(obj, scratch);
        break;
    }
}
