// The water's own objects for Sonic 2 (Nick Arcade's 04 and 08): the surface (three strips across the screen, each as wide as the screen was, that follow the camera and bob through a table of frames)
// and the splash when Sonic enters or leaves the water. The splash sits in the spin dash dust's slot (Sonic 1's level start makes that object), so it waits there, unseen, until it is asked for.
#include "Object/WaterObjects.h"
#include "Constants.h"

#include "Level.h"
#include "LevelScroll.h"
#include "Object/Sonic.h"

#include "Macros.h"

#include "Resource/Animation/WaterSplash.h"
#include "Resource/Mappings/WaterSplash.h"
#include "Resource/Mappings/NAWaterSurface.h"

// The frame of the surface on each frame of its cycle (Ani_WaterSurface)
static const uint8_t surface_frames[64] = {
    0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1,
    1, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2,
    2, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1, 2, 1,
    1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0,
};

void Obj_NAWaterSurface(Object *obj) {
    uint8_t which = obj->scratch.u8[0]; // 0 and 1 are Nick Arcade's two, 2 the extra one

    if (obj->routine == 0) {
        obj->routine += 2;
        obj->mappings = Mappings_NAWaterSurface;
        obj->tile = TILE_MAP(1, 0, 0, 0, 0x400);
        obj->render.b = 0;
        obj->render.f.level_fg = true;
        obj->width_pixels = 0x80;
        obj->priority = 0;
    }

    // (3 to 5 are the second view's of a split screen: the same places along its camera)
    int16_t base = (int16_t)((which >= 3 ? scrpos_x_p2.f.u : scrpos_x.f.u) + ((frame_count & 1) ? 0x20 : 0));
    int kind = which >= 3 ? which - 3 : which;
    obj->pos.l.x.f.u = (int16_t)(base + (kind == 0 ? 0x60 : kind == 1 ? 0x120 : 0x1E0));
    obj->pos.l.y.f.u = wtr_pos1;
    obj->frame = surface_frames[obj->anim_frame & 0x3F];
    obj->anim_frame = (uint8_t)((obj->anim_frame + 1) & 0x3F);
    DisplaySprite(obj);
}

static bool splash_requested;

// Sonic enters or leaves the water: the splash plays once, where he is, at the water's height
void NAWaterSplash_Request(void) {
    splash_requested = true;
    objects[0x1B].routine = 0;
}

void Obj_NAWaterSplash(Object *obj) {
    switch (obj->routine) {
    case 0:
        if (!splash_requested)
            return; // (waiting: the object is made when the level starts)
        splash_requested = false;
        obj->routine += 2;
        obj->mappings = Mappings_WaterSplash;
        obj->tile = TILE_MAP(0, 2, 0, 0, 0x259);
        obj->render.b = 0;
        obj->render.f.level_fg = true;
        obj->priority = 1;
        obj->width_pixels = 0x10;
        obj->anim = 0;
        obj->prev_anim = 0xFF;
        obj->pos.l.x.f.u = player->pos.l.x.f.u;
        // Fallthrough
    case 2:
        obj->pos.l.y.f.u = wtr_pos1;
        AnimateSprite(obj, Animation_WaterSplash);
        DisplaySprite(obj);
        break;
    default: // the animation is over (its last command sent the object on): back to waiting
        obj->routine = 0;
        break;
    }
}
