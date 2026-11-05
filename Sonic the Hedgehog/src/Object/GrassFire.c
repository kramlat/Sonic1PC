#include "GrassFire.h"
#include "Object.h"
#include "ObjectSst.h"
#include "SstLoad.h"

#include "LargeGrass.h"
#include "Level.h"
#include "Macros.h"
#include "Resource/Animation/GrassFire.h"
#include "Resource/Mappings/Fireballs.h"
#include "Sound.h"

// Object 35 - fireball that sits on the floor (MZ)
// (appears when you walk on a burnable large grass platform for long
// enough, see LargeGrass.c's own LGrass_Burnable)

static void GFire_Animate(Sst *self) {
    Sst_AnimateFlips(self, Animation_GrassFire);
    Sst_Draw(self);
}

// Routine 4 - already spread as far as it's going to; just rides the
// parent platform's own depression up and down.
static void GFire_Move(Sst *self, Own_GrassFire *own) {
    self->y_pos.f.u = (int16_t)(own->orig_y + own->nudge);
    GFire_Animate(self);
}

// Routine 2 - only ever runs for the original (subtype 0) flame: creeps
// 1px/frame across the platform's slope, spawning a new stationary child
// flame (routine 4, see GFire_Move) every 16px until it's crossed the
// whole platform.
static void GFire_Spread(Sst *self, Own_GrassFire *own) {
    const uint8_t *heightmap = LGrass_Heightmap(sst[own->platform].mapping_frame);

    int16_t d1 = (int16_t)(self->x_pos.f.u - own->orig_x + 12);
    uint16_t column = (uint16_t)((uint16_t)d1 >> 1);
    int16_t slope_y = (int16_t)(own->orig_y - heightmap[column]);
    self->y_pos.f.u = (int16_t)(slope_y + own->nudge);

    if ((uint16_t)d1 < 132) {
        self->x_pos.f.u += 1;

        if ((uint16_t)d1 < 128 && (((uint16_t)(self->x_pos.f.u + 8)) & 0xF) == 0) {
            Sst *child = Sst_AllocateAfter(self);
            if (child != NULL) {
                child->code = self->code;
                child->x_pos.f.u = self->x_pos.f.u;

                Own_GrassFire *cscratch = (Own_GrassFire *)child->own.u8;
                cscratch->platform = own->platform;
                cscratch->orig_y = slope_y;
                cscratch->nudge = own->nudge;
                cscratch->platform = own->platform;
                child->subtype = 1; // subtype: skip GFire_Spread, go straight to GFire_Move

                LGrass_AddChildToList(&sst[own->platform], child);
            }
        }
    }

    GFire_Animate(self);
}

void Obj_GrassFire(Sst *self) {
    Own_GrassFire *own = (Own_GrassFire *)self->own.u8;

    switch (self->routine) {
    case 0: // Main
        self->routine += 2;
        self->mappings = Mappings_Fireballs;
        self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_MZ_Fireball);
        own->orig_x = self->x_pos.f.u;
        self->render_flags = SST_RENDER_LEVEL;
        self->priority = 1 * 0x80;
        self->collision_flags = 0x8B; // col_16x16 | col_hurt
        self->width_pixels = 16 / 2;

        QueueSound2(sfx_Burning);

        if (self->subtype != 0) { // child? -- skip straight to Move
            self->routine += 2;
            GFire_Move(self, own);
            return;
        }
        GFire_Spread(self, own);
        break;
    case 2: // Spread
        GFire_Spread(self, own);
        break;
    case 4: // Move
        GFire_Move(self, own);
        break;
    }
}
