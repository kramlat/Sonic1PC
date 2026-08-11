#include "LavaGeyser.h"

#include "Level.h"
#include "LevelScroll.h"
#include "Macros.h"
#include "Resource/Animation/LavaGeyser.h"
#include "Resource/Mappings/LavaGeyser.h"
#include "Sound.h"

// Object 4D - lava geyser / lavafall (MZ). Spawned by GeyserMaker (object
// 4C) as either 2 pieces (geyser: a bubbling top tip + a tall solid lava
// wall trailing beneath it) or 3 (lavafall: same two, plus a second
// bubbling tip at the bottom).

static const int16_t geyser_speeds[2] = { -0x500, 0 }; // geyser, lavafall

static void Geyser_Type00(Object *obj, Scratch_LavaGeyser *scratch) {
    obj->ysp += 0x18;
    if (scratch->orig_y >= obj->pos.l.y.f.u)
        return;
    obj->routine += 4; // -> Delete
    objects[scratch->parent_index].anim = 3; // ".bubble3"
}

static void Geyser_Type01(Object *obj, Scratch_LavaGeyser *scratch) {
    obj->ysp += 0x18;
    if (scratch->orig_y >= obj->pos.l.y.f.u)
        return;
    obj->routine += 4; // -> Delete
    objects[scratch->parent_index].anim = 1; // ".bubble1"
}

static void Geyser_Action(Object *obj, Scratch_LavaGeyser *scratch) {
    if (obj->scratch.u8[0] == 0)
        Geyser_Type00(obj, scratch);
    else
        Geyser_Type01(obj, scratch);

    SpeedToPos(obj);
    AnimateSprite(obj, Animation_LavaGeyser);

    if (IS_OFFSCREEN(obj->pos.l.x.f.u)) {
        ObjectDelete(obj);
        return;
    }
    DisplaySprite(obj);
}

// Routine 4 -- the tall solid lava wall trailing beneath the bubbling top
// tip; picks one of 3 lengths of sprite (each with 2 alternating frames)
// based on how far the tip has currently risen above its own origin.
static void Geyser_BigLavaWall(Object *obj, Scratch_LavaGeyser *scratch) {
    Object *tip = &objects[scratch->parent_index];

    if (tip->routine == 6) { // tip has marked itself for deletion
        ObjectDelete(obj);
        return;
    }

    obj->pos.l.y.f.u = (int16_t)(tip->pos.l.y.f.u + 0x60);

    int16_t risen = (int16_t)(scratch->orig_y - obj->pos.l.y.f.u);
    uint8_t base_frame = 8; // medium
    if (risen < 0x40)
        base_frame = 0xB; // short
    if (risen > 0x80)
        base_frame = 0xE; // long

    if (--obj->frame_time.b < 0) {
        obj->frame_time.b = 8 - 1;
        obj->anim_frame++;
        if (obj->anim_frame >= 2)
            obj->anim_frame = 0;
    }
    obj->frame = (uint8_t)(obj->anim_frame + base_frame);

    if (IS_OFFSCREEN(obj->pos.l.x.f.u)) {
        ObjectDelete(obj);
        return;
    }
    DisplaySprite(obj);
}

static void Geyser_Main(Object *obj, Scratch_LavaGeyser *scratch) {
    obj->routine += 2; // -> Action

    scratch->orig_y = obj->pos.l.y.f.u;
    uint8_t subtype = obj->scratch.u8[0];
    if (subtype != 0)
        obj->pos.l.y.f.u = (int16_t)(obj->pos.l.y.f.u - 0x250);

    obj->ysp = geyser_speeds[subtype & 1];

    // Spawns the bubbling top tip (reusing this very object) and the lava
    // wall beneath it. If object RAM is full for the wall, real hardware
    // would corrupt the tip itself by applying the wall's own adjustments
    // to it instead (a1 retaining the tip's own address on failure) --
    // not reproduced here, matching this session's earlier precedent
    // (see GlassBlock.c's own comment on the same kind of edge case).
    Object *middle = NULL;
    for (int i = 0; i < 2; i++) {
        Object *seg = obj;
        if (i > 0) {
            seg = FindNextFreeObj(obj);
            if (seg == NULL)
                break;
            middle = seg;
        }
        seg->type = ObjId_LavaGeyser;
        seg->mappings = Mappings_LavaGeyser;
        seg->tile = TILE_MAP(0, 3, 0, 0, 0x3A8); // ArtTile_MZ_Lava | Tile_Pal4
        seg->render.b = 0;
        seg->render.f.align_fg = true;
        seg->width_pixels = 112 / 2;
        seg->pos.l.x.f.u = obj->pos.l.x.f.u;
        seg->pos.l.y.f.u = obj->pos.l.y.f.u;
        seg->scratch.u8[0] = subtype;
        seg->priority = 1;
        seg->anim = (subtype == 0) ? 5 : 2; // ".bubble4" / ".end"
    }
    if (middle == NULL)
        return;

    middle->pos.l.y.f.u = (int16_t)(middle->pos.l.y.f.u + 0x60);
    Scratch_LavaGeyser *mscratch = (Scratch_LavaGeyser *)&middle->scratch;
    mscratch->orig_y = (int16_t)(scratch->orig_y + 0x60);
    middle->col_type = 0x93; // col_64x224 | col_hurt
    // 256/2 doesn't fit a signed byte -- real hardware's own obHeight
    // field is sign-extended the same way when read (ext.w), so this
    // wraps to -128 there too; kept bit-for-bit identical rather than
    // "fixed", since it's unclear whether that's visually load-bearing.
    middle->y_rad = (int8_t)(256 / 2);
    middle->render.f.yrad_height = true;
    middle->routine = 6; // BigLavaWall
    mscratch->parent_index = (uint8_t)(obj - objects);

    if (subtype != 0) {
        Object *bottom = FindNextFreeObj(middle);
        if (bottom != NULL) {
            bottom->type = ObjId_LavaGeyser;
            bottom->mappings = Mappings_LavaGeyser;
            bottom->tile = TILE_MAP(0, 3, 0, 0, 0x3B8); // ArtTile_MZ_Lava | Tile_Pal4, +0x10 pattern for the bottom tip's own art
            bottom->render.b = 0;
            bottom->render.f.align_fg = true;
            bottom->width_pixels = 112 / 2;
            bottom->pos.l.x.f.u = obj->pos.l.x.f.u;
            bottom->pos.l.y.f.u = (int16_t)(obj->pos.l.y.f.u + 0x100);
            bottom->scratch.u8[0] = subtype;
            bottom->priority = 0;
            bottom->anim = 2; // ".end"
            bottom->routine = 2; // Action

            Scratch_LavaGeyser *bscratch = (Scratch_LavaGeyser *)&bottom->scratch;
            bscratch->orig_y = scratch->orig_y;
            bscratch->parent_index = scratch->parent_index;
        }
        obj->scratch.u8[0] = 0; // force the top tip to geyser-type behavior henceforth
    }

    QueueSound2(sfx_Burning);
}

void Obj_LavaGeyser(Object *obj) {
    Scratch_LavaGeyser *scratch = (Scratch_LavaGeyser *)&obj->scratch;

    switch (obj->routine) {
    case 0:
        Geyser_Main(obj, scratch);
        __attribute__((fallthrough));
    case 2:
        Geyser_Action(obj, scratch);
        break;
    case 4:
        Geyser_BigLavaWall(obj, scratch);
        break;
    case 6:
        ObjectDelete(obj);
        break;
    }
}
