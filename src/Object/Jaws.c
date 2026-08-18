#include "Jaws.h"

#include "Level.h"
#include "LevelScroll.h"
#include "Macros.h"
#include "Resource/Animation/Jaws.h"
#include "Resource/Mappings/Jaws.h"

// Object 2C - Jaws enemy (LZ). Swims back and forth, turning around after
// a subtype-scaled delay.

static void Jaws_Swim(Object *obj, Scratch_Jaws *scratch) {
    if (--scratch->turndelay_current < 0) {
        scratch->turndelay_current = scratch->turndelay_base;
        obj->xsp = (int16_t)-obj->xsp;
        obj->status.o.f.x_flip ^= 1;
        obj->prev_anim = 1; // force animation to restart
    }

    AnimateSprite(obj, Animation_Jaws);
    SpeedToPos(obj);
    RememberState(obj);
}

void Obj_Jaws(Object *obj) {
    Scratch_Jaws *scratch = (Scratch_Jaws *)&obj->scratch;

    switch (obj->routine) {
    case 0: {
        obj->routine += 2;
        obj->mappings = Mappings_Jaws;
        obj->tile = TILE_MAP(0, 1, 0, 0, ArtTile_Jaws); // | Tile_Pal2
        obj->render.f.align_fg = true;
        obj->col_type = 0x0A; // col_32x24 | col_badnik
        obj->priority = 4;
        obj->width_pixels = 48 / 2; // Bug fix: real hardware's original 32/2 gets culled too early

        uint8_t subtype = obj->scratch.u8[0];
        int16_t delay = (int16_t)((subtype << 6) - 1);
        scratch->turndelay_current = delay;
        scratch->turndelay_base = delay;

        obj->xsp = -0x40;
        if (obj->status.o.f.x_flip)
            obj->xsp = (int16_t)-obj->xsp;
        __attribute__((fallthrough));
    }
    case 2:
        Jaws_Swim(obj, scratch);
        break;
    }
}
