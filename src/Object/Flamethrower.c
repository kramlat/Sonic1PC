#include "Flamethrower.h"

#include "Level.h"
#include "LevelScroll.h"
#include "Resource/Animation/Flamethrower.h"
#include "Resource/Mappings/Flamethrower.h"
#include "Sound.h"

// Object 6D - flame thrower (SBZ)

static bool Flame_OutOfRange(int16_t x) {
    uint16_t obj_pos = (uint16_t)x & 0xFF80;
    uint16_t cam_pos = (uint16_t)(scrpos_x.f.u - 128) & 0xFF80;
    return (uint16_t)(obj_pos - cam_pos) > (128 + 320 + 192);
}

static void Flame_Animate(Object *obj, Scratch_Flamethrower *scratch) {
    AnimateSprite(obj, Animation_Flamethrower); // stays on last 1/2 frames per animation indefinitely

    obj->col_type = 0; // col_none -- harmless by default
    if (obj->frame == scratch->hurtframe)
        obj->col_type = 0x23 | 0x80; // col_24x48 | col_hurt

    if (Flame_OutOfRange(obj->pos.l.x.f.u))
        ObjectDelete(obj);
    else
        DisplaySprite(obj);
}

static void Flame_Action(Object *obj, Scratch_Flamethrower *scratch) {
    if (--scratch->timer >= 0) {
        Flame_Animate(obj, scratch);
        return;
    }

    scratch->timer = scratch->pausetime; // begin pause time
    uint8_t before = obj->anim & 1;
    obj->anim ^= 1; // toggle between expanding/retracting animations
    if (before == 0) {
        Flame_Animate(obj, scratch); // now retracting -- pause
        return;
    }

    scratch->timer = scratch->firetime; // now expanding again -- begin flaming time
    QueueSound2(sfx_Flamethrower);
    Flame_Animate(obj, scratch);
}

void Obj_Flamethrower(Object *obj) {
    Scratch_Flamethrower *scratch = (Scratch_Flamethrower *)&obj->scratch;

    if (obj->routine == 0) {
        obj->routine = 2; // advance to Flame_Action
        obj->mappings = Mappings_Flamethrower;
        obj->tile = TILE_MAP(1, 0, 0, 0, ArtTile_SBZ_Flamethrower); // | Tile_Prio
        obj->render.f.align_fg = true;
        obj->priority = 1;
        obj->width_pixels = 24 / 2;

        uint16_t d0 = (uint16_t)((obj->scratch.u8[0] & 0xF0) * 2);
        scratch->timer = (int16_t)d0;
        scratch->firetime = (int16_t)d0;

        scratch->pausetime = (int16_t)((obj->scratch.u8[0] & 0x0F) << 5);

        scratch->hurtframe = 0xA; // broken-pipe flamethrower
        if (obj->status.o.f.y_flip) {
            obj->anim = 2; // "valve" animations
            scratch->hurtframe = 0x15; // valve flamethrower
        }
    }

    Flame_Action(obj, scratch);
}
