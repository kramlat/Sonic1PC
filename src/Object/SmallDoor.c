#include "SmallDoor.h"

#include "Level.h"
#include "Object/Sonic.h"
#include "Resource/Animation/SmallDoor.h"
#include "Resource/Mappings/SmallDoor.h"

// Object 2A - small vertical door (SBZ)

static void ADoor_OpenShut(Object *obj) {
    obj->anim = 0; // closing, by default

    int16_t sonic_x = player->pos.l.x.f.u;
    int16_t door_x = obj->pos.l.x.f.u;

    if ((uint16_t)(sonic_x + 64) >= (uint16_t)door_x &&
        (uint16_t)(sonic_x - 64) < (uint16_t)door_x) {
        // Sonic is within 64px of the door -- check which side he's on
        bool sonic_past = (uint16_t)sonic_x >= (uint16_t)door_x;
        bool open = sonic_past ? obj->status.o.f.x_flip : !obj->status.o.f.x_flip;
        if (open)
            obj->anim = 1; // opening
    }

    AnimateSprite(obj, Animation_SmallDoor); // animations stay on final frames indefinitely

    if (obj->frame == 0) { // fully closed
        int16_t x_rad = (int16_t)(12 / 2 + 11 /* sonic_solid_width */);
        SolidObject(obj, (uint16_t)x_rad, 64 / 2, 64 / 2 + 1, obj->pos.l.x.f.u, NULL, NULL);
    }

    RememberState(obj);
}

void Obj_SmallDoor(Object *obj) {
    if (obj->routine == 0) {
        obj->routine = 2; // advance to ADoor_OpenShut
        obj->mappings = Mappings_SmallDoor;
        obj->tile = TILE_MAP(0, 2, 0, 0, ArtTile_SBZ_Door); // | Tile_Pal3
        obj->render.f.align_fg = true;
        obj->width_pixels = 16 / 2;
        obj->priority = 4;
    }

    ADoor_OpenShut(obj);
}
