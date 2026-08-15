#include "LavaWall.h"

#include "Level.h"
#include "LevelScroll.h"
#include "Macros.h"
#include "Object/Sonic.h"
#include "Resource/Animation/LavaWall.h"
#include "Resource/Mappings/LavaWall.h"

// Object 4E - advancing wall of lava (MZ act 2). Hardcoded for its single
// real placement in MZ2: waits until Sonic gets within range, then slides
// right until it reaches a specific X-position and stops. A second,
// trailing segment (routine 6) follows 128px behind the front one.

static void LWall_Solid(Object *obj, Scratch_LavaWall *scratch) {
    int16_t x_rad = (int16_t)(64 / 2 + 11 /* sonic_solid_width */);
    uint8_t saved_routine = obj->routine; // SolidObject doesn't change this, but real ASM backs it up anyway
    SolidObject(obj, (uint16_t)x_rad, 48 / 2, 48 / 2 + 1, obj->pos.l.x.f.u, NULL, NULL);
    obj->routine = saved_routine;

    // Object is hardcoded to only ever appear once, in MZ2, expecting this
    // specific X-position to stop at.
    if (obj->pos.l.x.f.u == 0x6A0) {
        obj->xsp = 0;
        scratch->moving = false;
    }

    AnimateSprite(obj, Animation_LavaWall);

    if (player->routine < 4) // temporarily stop moving while Sonic is hurt/dying
        SpeedToPos(obj);

    if (scratch->moving) {
        DisplaySprite(obj);
        return;
    }
    if (IS_OFFSCREEN(obj->pos.l.x.f.u)) {
        if (obj->respawn_index)
            objstate[obj->respawn_index] &= 0x7F;
        obj->routine = 8; // LWall_Delete
        return;
    }
    DisplaySprite(obj);
}

static void LWall_ChkSonic(Object *obj, Scratch_LavaWall *scratch) {
    int16_t d0 = (int16_t)(player->pos.l.x.f.u - obj->pos.l.x.f.u);
    if (d0 < 0)
        d0 = (int16_t)-d0;
    bool in_range = d0 < 192;
    if (in_range) {
        int16_t d1 = (int16_t)(player->pos.l.y.f.u - obj->pos.l.y.f.u);
        if (d1 < 0)
            d1 = (int16_t)-d1;
        in_range = d1 < 96;
    }

    if (in_range) {
        scratch->moving = true;
        LWall_Solid(obj, scratch);
        return;
    }

    if (scratch->moving) {
        obj->xsp = 0x180;
        obj->routine -= 2; // permanently switch to LWall_Solid
    }
    LWall_Solid(obj, scratch);
}

static void LWall_BackChild(Object *obj, Scratch_LavaWall *scratch) {
    Object *parent = &objects[scratch->parent_index];
    if (parent->routine == 8) {
        ObjectDelete(obj);
        return;
    }

    obj->pos.l.x.f.u = (int16_t)(parent->pos.l.x.f.u - 128);
    DisplaySprite(obj);
}

static void LWall_Main(Object *obj, Scratch_LavaWall *scratch) {
    (void)scratch;
    obj->routine += 4; // -> LWall_ChkSonic

    Object *last = obj;
    for (int i = 0; i < 2; i++) {
        Object *seg = obj;
        if (i > 0) {
            seg = FindNextFreeObj(obj);
            if (seg == NULL)
                break;
        }

        seg->type = ObjId_LavaWall;
        seg->mappings = Mappings_LavaWall;
        seg->tile = TILE_MAP(0, 3, 0, 0, 0x3A8); // ArtTile_MZ_Lava | Tile_Pal4
        seg->render.b = 0;
        seg->render.f.align_fg = true;
        seg->width_pixels = 160 / 2;
        seg->pos.l.x.f.u = obj->pos.l.x.f.u;
        seg->pos.l.y.f.u = obj->pos.l.y.f.u;
        seg->priority = 1;
        seg->anim = 0;
        seg->col_type = 0x14 | 0x80; // col_128x64 | col_hurt

        Scratch_LavaWall *sscratch = (Scratch_LavaWall *)&seg->scratch;
        sscratch->parent_index = (uint8_t)(obj - objects);
        last = seg;
    }

    last->routine += 6; // trailing segment -> LWall_BackChild
    last->frame = 4; // ".lava_back"
}

void Obj_LavaWall(Object *obj) {
    Scratch_LavaWall *scratch = (Scratch_LavaWall *)&obj->scratch;

    switch (obj->routine) {
    case 0:
        LWall_Main(obj, scratch);
        __attribute__((fallthrough));
    case 4:
        LWall_ChkSonic(obj, scratch);
        break;
    case 2:
        LWall_Solid(obj, scratch);
        break;
    case 6:
        LWall_BackChild(obj, scratch);
        break;
    case 8:
        ObjectDelete(obj);
        break;
    }
}
