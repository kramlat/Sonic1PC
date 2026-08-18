#include "SpinningLight.h"

#include "Level.h"
#include "LevelScroll.h"
#include "Macros.h"
#include "Resource/Mappings/Light.h"

// Object 12 - spinning light in hexagonal glass prism (SYZ). Purely
// decorative: cycles through 6 frames, no collision.

static void Light_Animate(Object *obj) {
    if (--obj->frame_time.b < 0) {
        obj->frame_time.b = 8 - 1;
        obj->frame++;
        if (obj->frame >= 6)
            obj->frame = 0;
    }

    if (IS_OFFSCREEN(obj->pos.l.x.f.u)) {
        ObjectDelete(obj);
        return;
    }
    DisplaySprite(obj);
}

void Obj_SpinningLight(Object *obj) {
    switch (obj->routine) {
    case 0:
        obj->routine += 2;
        obj->mappings = Mappings_Light;
        obj->tile = TILE_MAP(0, 0, 0, 0, ArtTile_Level); // ArtTile_Level (part of the level's own already-loaded graphics)
        obj->render.b = 0;
        obj->render.f.align_fg = true;
        obj->width_pixels = 32 / 2;
        obj->priority = 6;
        __attribute__((fallthrough));
    case 2:
        Light_Animate(obj);
        break;
    }
}
