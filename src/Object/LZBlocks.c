#include "LZBlocks.h"

#include "Level.h"
#include "LevelCollision.h"
#include "LevelScroll.h"
#include "Macros.h"
#include "MathUtil.h"
#include "Resource/Mappings/LZBlocks.h"

// Object 61 - multi-variant blocks (LZ): a block that sinks once stood on
// (optionally after a half-second delay, or after being touched from the
// side), a platform that rises the same way, a cork that bobs on the
// water surface, and a plain static block -- all sharing one object with
// a small gentle nudge-down animation while Sonic stands on the movable
// ones.

static const struct { uint8_t width, height; } LBlk_Var[4] = {
    { 32 / 2, 32 / 2 }, // block that sinks when stood on
    { 64 / 2, 24 / 2 }, // platform that rises when stood on
    { 32 / 2, 32 / 2 }, // cork block that floats on water
    { 32 / 2, 32 / 2 }, // generic solid block
};

static void LBlk_CheckStand(Object *obj, Scratch_LBlock *scratch) {
    if (scratch->time == 0) {
        if (!obj->status.o.f.player_stand)
            return;
        scratch->time = 30;
        return;
    }
    if (--scratch->time != 0)
        return;
    obj->scratch.u8[0]++; // advance to LBlk_Sink or LBlk_Rise
    scratch->untouched = false;
}

static void LBlk_Sink(Object *obj) {
    SpeedToPos(obj);
    obj->ysp += 8;

    int16_t floor_dist = ObjFloorDist(obj, obj->pos.l.x.f.u);
    if (floor_dist < 0) {
        obj->pos.l.y.f.u += (int16_t)(floor_dist + 1);
        obj->ysp = 0;
        obj->scratch.u8[0] = 0; // stationary
    }
}

static void LBlk_Rise(Object *obj) {
    SpeedToPos(obj);
    obj->ysp -= 8;

    int16_t ceil_dist = ObjHitCeiling(obj);
    if (ceil_dist < 0) {
        obj->pos.l.y.f.u -= ceil_dist;
        obj->ysp = 0;
        obj->scratch.u8[0] = 0; // stationary
    }
}

static void LBlk_SideSink(Object *obj, Scratch_LBlock *scratch) {
    if (scratch->touchtype != 1) // touched from the sides?
        return;
    obj->scratch.u8[0]++; // advance to LBlk_Sink
    scratch->untouched = false;
}

static void LBlk_OnWater(Object *obj) {
    int16_t d0 = (int16_t)(wtr_pos1 - obj->pos.l.y.f.u);
    if (d0 == 0)
        return;

    if (d0 >= 0) {
        // Block is above water -- sink down to meet it
        if (d0 > 2)
            d0 = 2;
        obj->pos.l.y.f.u += d0;

        int16_t floor_dist = ObjFloorDist(obj, obj->pos.l.x.f.u);
        if (floor_dist < 0)
            obj->pos.l.y.f.u += (int16_t)(floor_dist + 1);
        return;
    }

    // Block is below water -- rise up to meet it
    if (d0 < -2)
        d0 = -2;
    obj->pos.l.y.f.u += d0;

    int16_t ceil_dist = ObjHitCeiling(obj);
    if (ceil_dist < 0)
        obj->pos.l.y.f.u -= ceil_dist;
}

static void LBlk_Nudge(Object *obj, Scratch_LBlock *scratch) {
    if (!scratch->untouched)
        return;

    if (obj->status.o.f.player_stand) {
        if (scratch->nudge == 0x40)
            return;
        scratch->nudge += 4;
    } else {
        if (scratch->nudge == 0)
            return;
        scratch->nudge -= 4;
    }

    int16_t sin, cos;
    CalcSine(scratch->nudge, &sin, &cos);
    int16_t nudge_off = (int16_t)(((int32_t)sin * 0x400) >> 16);
    obj->pos.l.y.f.u = (int16_t)(scratch->orig_y + nudge_off);
}

static void LBlk_Main(Object *obj, Scratch_LBlock *scratch) {
    obj->routine += 2;
    obj->mappings = Mappings_LZBlocks;
    obj->tile = TILE_MAP(0, 2, 0, 0, ArtTile_LZ_Blocks); // | Tile_Pal3
    obj->render.b = 0;
    obj->render.f.align_fg = true;
    obj->priority = 3;

    uint8_t subtype = obj->scratch.u8[0];
    uint8_t index = (subtype >> 4) & 7;
    obj->width_pixels = LBlk_Var[index].width;
    obj->y_rad = (int8_t)LBlk_Var[index].height;
    obj->frame = index;

    scratch->orig_x = obj->pos.l.x.f.u;
    scratch->orig_y = obj->pos.l.y.f.u;

    uint8_t type = subtype & 0xF;
    if (type != 0 && type != 7) // not stationary, and not the small secret platform in LZ1
        scratch->untouched = true;
}

static void LBlk_Action(Object *obj, Scratch_LBlock *scratch) {
    int16_t prev_x = obj->pos.l.x.f.u;

    switch (obj->scratch.u8[0] & 0xF) {
    case 0: break; // stationary
    case 1: LBlk_CheckStand(obj, scratch); break;
    case 2: LBlk_Sink(obj); break;
    case 3: LBlk_CheckStand(obj, scratch); break;
    case 4: LBlk_Rise(obj); break;
    case 5: LBlk_SideSink(obj, scratch); break;
    case 6: LBlk_Sink(obj); break;
    case 7: LBlk_OnWater(obj); break;
    }

    if (obj->render.f.on_screen) {
        int16_t x_rad = (int16_t)(obj->width_pixels + 11 /* sonic_solid_width */);
        int32_t touch = SolidObject(obj, (uint16_t)x_rad, (uint16_t)obj->y_rad, (uint16_t)(obj->y_rad + 1), prev_x, NULL, NULL);
        scratch->touchtype = (int8_t)touch;
        LBlk_Nudge(obj, scratch);
    }

    if (IS_OFFSCREEN(scratch->orig_x)) {
        ObjectDelete(obj);
        return;
    }
    DisplaySprite(obj);
}

void Obj_LabyrinthBlock(Object *obj) {
    Scratch_LBlock *scratch = (Scratch_LBlock *)&obj->scratch;

    switch (obj->routine) {
    case 0:
        LBlk_Main(obj, scratch);
        __attribute__((fallthrough));
    case 2:
        LBlk_Action(obj, scratch);
        break;
    }
}
