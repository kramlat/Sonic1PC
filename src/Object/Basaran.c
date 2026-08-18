#include "Basaran.h"

#include "Level.h"
#include "LevelScroll.h"
#include "Macros.h"
#include "Object/Sonic.h"
#include "Resource/Animation/Basaran.h"
#include "Resource/Mappings/Basaran.h"
#include "Sound.h"

// Object 55 - Basaran enemy (MZ) -- hangs from the ceiling until Sonic
// walks underneath, drops down to about his height, then flies alongside
// him until he wanders out of range, at which point it flies back up to
// the nearest ceiling and waits again.

// Sets *move_speed to +/-0x100 (and faces the object toward Sonic) either
// way, returning true if Sonic is currently within `zone` px horizontally.
static bool Bas_CheckDistanceAndFaceSonic(Object *obj, int16_t zone, int16_t *move_speed) {
    int16_t speed = 0x100;
    obj->status.o.f.x_flip = true;
    int16_t d0 = (int16_t)(player->pos.l.x.f.u - obj->pos.l.x.f.u);
    if (d0 < 0) {
        d0 = (int16_t)-d0;
        speed = (int16_t)-speed;
        obj->status.o.f.x_flip = false;
    }
    *move_speed = speed;
    return d0 < zone;
}

// Real hardware restricts dropping down (and flying back up) to once every
// 8 frames, offset by this object's own RAM index -- likely just to keep
// multiple Basarans from all triggering in sync with each other.
static bool Bas_StaggeredTick(Object *obj) {
    uint8_t d0 = (uint8_t)((uint8_t)frame_count + (uint8_t)(obj - objects));
    return (d0 & 7) == 0;
}

static void Bas_Action_ChkSonic(Object *obj, Scratch_Basaran *scratch) {
    int16_t move_speed;
    if (!Bas_CheckDistanceAndFaceSonic(obj, 128, &move_speed))
        return;

    int16_t sonic_y = player->pos.l.y.f.u;
    scratch->sonic_y = sonic_y;
    int16_t d0 = (int16_t)(sonic_y - obj->pos.l.y.f.u);
    if (d0 < 0 || d0 >= 128)
        return;
    if (debug_use)
        return;
    if (!Bas_StaggeredTick(obj))
        return;

    obj->anim = 1;
    obj->routine_sec += 2;
}

// Returns true if this object deleted itself (real hardware's own
// Bas_Action_DropDown_Delete -- unreachable per the real disasm's own
// comment, since Bas_Action_ChkSonic already guarantees Sonic is at or
// below the Basaran before it ever starts dropping, but implemented
// faithfully regardless).
static bool Bas_Action_DropDown(Object *obj, Scratch_Basaran *scratch) {
    SpeedToPos(obj);
    obj->ysp += 0x18;

    int16_t move_speed;
    Bas_CheckDistanceAndFaceSonic(obj, 128, &move_speed); // distance result unused here, only the facing/speed output matters

    int16_t d0 = (int16_t)(scratch->sonic_y - obj->pos.l.y.f.u);
    if (d0 < 0) {
        if (obj->render.f.on_screen)
            return false;
        ObjectDelete(obj);
        return true;
    }
    if (d0 >= 16)
        return false;

    obj->xsp = move_speed;
    obj->ysp = 0;
    obj->anim = 2;
    obj->routine_sec += 2;
    return false;
}

static bool Bas_Action_Fly(Object *obj) {
    if (((uint8_t)frame_count & 0xF) == 0)
        QueueSound2(sfx_Basaran);

    SpeedToPos(obj);

    int16_t d0 = (int16_t)(player->pos.l.x.f.u - obj->pos.l.x.f.u);
    if (d0 < 0)
        d0 = (int16_t)-d0;
    if (d0 < 128)
        return false;
    if (!Bas_StaggeredTick(obj))
        return false;

    obj->routine_sec += 2;
    return false;
}

static bool Bas_Action_BackToCeiling(Object *obj) {
    SpeedToPos(obj);
    obj->ysp -= 0x18;

    int16_t dist = ObjHitCeiling(obj);
    if (dist >= 0)
        return false;
    obj->pos.l.y.f.u -= dist;

    obj->pos.l.x.f.u &= ~7; // snap to the nearest 8px
    obj->xsp = 0;
    obj->ysp = 0;
    obj->anim = 0;
    obj->routine_sec = 0;
    return false;
}

static bool Bas_Action(Object *obj, Scratch_Basaran *scratch) {
    switch (obj->routine_sec) {
    case 0:
        Bas_Action_ChkSonic(obj, scratch);
        return false;
    case 2:
        return Bas_Action_DropDown(obj, scratch);
    case 4:
        return Bas_Action_Fly(obj);
    case 6:
        return Bas_Action_BackToCeiling(obj);
    }
    return false;
}

void Obj_Basaran(Object *obj) {
    Scratch_Basaran *scratch = (Scratch_Basaran *)&obj->scratch;

    switch (obj->routine) {
    case 0: // Main
        obj->routine += 2;
        obj->mappings = Mappings_Basaran;
        obj->tile = TILE_MAP(1, 0, 0, 0, ArtTile_Basaran); // | Tile_Prio
        obj->render.b = 0;
        obj->render.f.align_fg = true;
        obj->y_rad = 24 / 2;
        obj->priority = 2;
        obj->col_type = 0x0B; // col_16x16 | col_badnik
        obj->width_pixels = 32 / 2;
        __attribute__((fallthrough));
    case 2: // Action
        if (Bas_Action(obj, scratch))
            return;
        AnimateSprite(obj, Animation_Basaran);
        RememberState(obj);
        break;
    }
}
