#include "Harpoon.h"

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

static void Harp_Move(Object *obj) {
    AnimateSprite(obj, Animation_Harpoon); // advances obRoutine to Harp_Wait on animation finish
    obj->col_type = Harp_ColTypes[obj->frame];
    RememberState(obj);
}

static void Harp_Wait(Object *obj, Scratch_Harpoon *scratch) {
    if (--scratch->time < 0) {
        scratch->time = 60;
        obj->routine -= 2; // back to Harp_Move
        obj->anim ^= 1; // toggle between extending/retracting animation
    }
    RememberState(obj);
}

void Obj_Harpoon(Object *obj) {
    Scratch_Harpoon *scratch = (Scratch_Harpoon *)&obj->scratch;

    switch (obj->routine) {
    case 0:
        obj->routine += 2;
        obj->mappings = Mappings_Harpoon;
        obj->tile = TILE_MAP(0, 0, 0, 0, 0x3CC); // ArtTile_LZ_Harpoon
        obj->render.f.align_fg = true;
        obj->priority = 4;
        obj->anim = obj->scratch.u8[0]; // subtype: 0 = sideways, 2 = upright
        obj->width_pixels = 40 / 2;
        scratch->time = 60;
        __attribute__((fallthrough));
    case 2:
        Harp_Move(obj);
        break;
    case 4:
        Harp_Wait(obj, scratch);
        break;
    }
}
