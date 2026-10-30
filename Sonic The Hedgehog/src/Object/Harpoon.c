#include "Harpoon.h"
#include "Object.h"
#include "ObjectSst.h"
#include "SstLoad.h"

#include "Level.h"
#include "LevelScroll.h"
#include "Macros.h"
#include "Resource/Animation/Harpoon.h"
#include "Resource/Mappings/Harpoon.h"

// Object 16 - harpoon (LZ). Extends and retracts on a 1-second timer,
// sideways or upright depending on subtype; its collision box grows/
// shrinks to match whichever frame is currently showing.

static const uint8_t Harp_ColTypes[6] = {
    0x1B | 0x80, // frame 0, sideways, retracted (col_16x8 | col_hurt)
    0x1C | 0x80, // frame 1, sideways, moving    (col_48x8 | col_hurt)
    0x1D | 0x80, // frame 2, sideways, extended   (col_80x8 | col_hurt)
    0x1E | 0x80, // frame 3, upright,  retracted   (col_8x16 | col_hurt)
    0x1F | 0x80, // frame 4, upright,  moving       (col_8x48 | col_hurt)
    0x20 | 0x80, // frame 5, upright,  extended      (col_8x80 | col_hurt)
};

static void Harp_Move(Sst *self) {
    Sst_Animate(self, Animation_Harpoon); // advances obRoutine to Harp_Wait on animation finish
    self->collision_flags = Harp_ColTypes[self->mapping_frame];
    SstOnScreenTest(self);
}

static void Harp_Wait(Sst *self, Own_Harpoon *own) {
    if (--own->time < 0) {
        own->time = 60;
        self->routine -= 2; // back to Harp_Move
        self->anim ^= 1; // toggle between extending/retracting animation
    }
    SstOnScreenTest(self);
}

void Obj_Harpoon(Sst *self) {
    Own_Harpoon *own = (Own_Harpoon *)self->own.u8;

    switch (self->routine) {
    case 0:
        self->routine += 2;
        self->mappings = Mappings_Harpoon;
        self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_LZ_Harpoon);
        self->render_flags |= SST_RENDER_LEVEL;
        self->priority = 4 * 0x80;
        self->anim = self->subtype; // subtype: 0 = sideways, 2 = upright
        self->width_pixels = 40 / 2;
        own->time = 60;
        __attribute__((fallthrough));
    case 2:
        Harp_Move(self);
        break;
    case 4:
        Harp_Wait(self, own);
        break;
    }
}
