#include "GrassFire.h"

#include "LargeGrass.h"
#include "Level.h"
#include "Macros.h"
#include "Resource/Animation/GrassFire.h"
#include "Resource/Mappings/Fireballs.h"
#include "Sound.h"

// Object 35 - fireball that sits on the floor (MZ)
// (appears when you walk on a burnable large grass platform for long
// enough, see LargeGrass.c's own LGrass_Burnable)

static void GFire_Animate(Object *obj) {
    AnimateSprite(obj, Animation_GrassFire);
    DisplaySprite(obj);
}

// Routine 4 - already spread as far as it's going to; just rides the
// parent platform's own depression up and down.
static void GFire_Move(Object *obj, Scratch_GrassFire *scratch) {
    obj->pos.l.y.f.u = (int16_t)(scratch->orig_y + scratch->nudge);
    GFire_Animate(obj);
}

// Routine 2 - only ever runs for the original (subtype 0) flame: creeps
// 1px/frame across the platform's slope, spawning a new stationary child
// flame (routine 4, see GFire_Move) every 16px until it's crossed the
// whole platform.
static void GFire_Spread(Object *obj, Scratch_GrassFire *scratch) {
    const uint8_t *heightmap = LGrass_Heightmap(scratch->platform->frame);

    int16_t d1 = (int16_t)(obj->pos.l.x.f.u - scratch->orig_x + 12);
    uint16_t column = (uint16_t)((uint16_t)d1 >> 1);
    int16_t slope_y = (int16_t)(scratch->orig_y - heightmap[column]);
    obj->pos.l.y.f.u = (int16_t)(slope_y + scratch->nudge);

    if ((uint16_t)d1 < 132) {
        obj->pos.l.x.f.u += 1;

        if ((uint16_t)d1 < 128 && (((uint16_t)(obj->pos.l.x.f.u + 8)) & 0xF) == 0) {
            Object *child = FindNextFreeObj(obj);
            if (child != NULL) {
                child->type = ObjId_GrassFire;
                child->pos.l.x.f.u = obj->pos.l.x.f.u;

                Scratch_GrassFire *cscratch = (Scratch_GrassFire *)&child->scratch;
                cscratch->orig_y = slope_y;
                cscratch->nudge = scratch->nudge;
                child->scratch.u8[0] = 1; // subtype: skip GFire_Spread, go straight to GFire_Move

                LGrass_AddChildToList(scratch->platform, child);
            }
        }
    }

    GFire_Animate(obj);
}

void Obj_GrassFire(Object *obj) {
    Scratch_GrassFire *scratch = (Scratch_GrassFire *)&obj->scratch;

    switch (obj->routine) {
    case 0: // Main
        obj->routine += 2;
        obj->mappings = Mappings_Fireballs;
        obj->tile = TILE_MAP(0, 0, 0, 0, ArtTile_MZ_Fireball);
        scratch->orig_x = obj->pos.l.x.f.u;
        obj->render.b = 0;
        obj->render.f.align_fg = true;
        obj->priority = 1;
        obj->col_type = 0x8B; // col_16x16 | col_hurt
        obj->width_pixels = 16 / 2;

        QueueSound2(sfx_Burning);

        if (obj->scratch.u8[0] != 0) { // child? -- skip straight to Move
            obj->routine += 2;
            GFire_Move(obj, scratch);
            return;
        }
        GFire_Spread(obj, scratch);
        break;
    case 2: // Spread
        GFire_Spread(obj, scratch);
        break;
    case 4: // Move
        GFire_Move(obj, scratch);
        break;
    }
}
