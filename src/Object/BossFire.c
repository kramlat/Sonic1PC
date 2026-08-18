#include "BossFire.h"

#include "LevelCollision.h"
#include "Sound.h"

// Object 74 - lava that Eggman drops (MZ boss). Doubles as the falling
// fireball itself (subtype != 0, routines 0->2) and the "temp fire" floor
// warning marker it leaves behind as it spreads along the lava pool
// (subtype == 0, routines 0->4->6). boss_mz_x = 0x1800, boss_mz_y = 0x210
// (real s1disasm _Constants.asm; see BossMarble.c).
//
// Map_Fire/Ani_Fire are the real ASM's own shared labels for this art --
// this project's Mappings_Fireballs/Animation_Fireballs are already owned
// by GrassFire.c/LavaBall.c respectively (Object 2F/35 and Object 13/14
// both draw from the same resource), so bare externs here avoid a second
// definition and a link failure, same reasoning as LavaBall.c's own
// comment on this.
extern const uint8_t Mappings_Fireballs[];
extern const uint8_t Animation_Fireballs[];

static void BossFire_Action(Object *obj, Scratch_BossFire *scratch);
static void BossFire_TempFire(Object *obj, Scratch_BossFire *scratch);

static void BossFire_Main(Object *obj, Scratch_BossFire *scratch) {
    obj->y_rad = 16 / 2;
    obj->x_rad = 16 / 2;
    obj->mappings = Mappings_Fireballs;
    obj->tile = TILE_MAP(0, 0, 0, 0, ArtTile_MZ_Fireball);
    obj->render.f.align_fg = true;
    obj->priority = 5;
    scratch->boss_y = obj->pos.l.y.f.u;
    obj->width_pixels = 16 / 2;
    obj->routine += 2;

    if (obj->scratch.u8[0] == 0) {
        // Temp fire (floor warning) -- controls its own collision/routine
        // separately, so jump straight into TempFire this same frame.
        obj->col_type = 0x8B; // col_16x16 | col_hurt
        obj->routine += 2;
        BossFire_TempFire(obj, scratch);
        return;
    }

    // Real fire, dropped by Eggman.
    scratch->generic_timer = 30;
    QueueSound2(sfx_Fireball);
    BossFire_Action(obj, scratch);
}

static void BossFire_Drop(Object *obj, Scratch_BossFire *scratch) {
    obj->status.o.f.y_flip = true; // face up while waiting to drop

    if ((int8_t)(--scratch->generic_timer) >= 0)
        return;

    obj->col_type = 0x8B; // col_16x16 | col_hurt
    obj->scratch.u8[0] = 0; // clear subtype
    obj->ysp += 0x18;
    obj->status.o.f.y_flip = false; // now facing down

    if (ObjFloorDist(obj, obj->pos.l.x.f.u) >= 0)
        return;
    obj->routine_sec += 2;
}

static void BossFire_MakeFlame(Object *obj, Scratch_BossFire *scratch) {
    obj->pos.l.y.f.u -= 2;
    obj->tile |= 0x8000; // high priority (foreground)
    obj->xsp = 0xA0;
    obj->ysp = 0;
    scratch->boss_x = obj->pos.l.x.f.u;
    scratch->boss_y = obj->pos.l.y.f.u;
    scratch->generic_timer = 3;

    // Clone the entire object (matches real ASM's literal 64-byte struct
    // copy) so the new flame starts identical, then spread the pair apart.
    Object *clone = FindNextFreeObj(obj);
    if (clone != NULL) {
        *clone = *obj;
        clone->xsp = (int16_t)-clone->xsp;
        clone->routine_sec += 2;
    }

    obj->routine_sec += 2;
}

static void BossFire_Duplicate2(Object *obj) {
    Object *clone = FindNextFreeObj(obj);
    if (clone == NULL)
        return;
    clone->pos.l.x.f.u = obj->pos.l.x.f.u;
    clone->pos.l.y.f.u = obj->pos.l.y.f.u;
    clone->type = ObjId_BossFire;
    clone->scratch.u8[0] = 0;   // subtype -- temp fire
    clone->scratch.u8[1] = 103; // generic_timer
}

static void BossFire_Duplicate(Object *obj, Scratch_BossFire *scratch) {
    if (ObjFloorDist(obj, obj->pos.l.x.f.u) >= 0) {
        obj->routine_sec += 2;
        return;
    }

    int16_t x = obj->pos.l.x.f.u;
    if (x > (int16_t)(0x1800 + 0x140)) { // boss_mz_x+0x140 -- past the right edge (no left-edge check -- there's a gap there)
        obj->routine += 2;
        return;
    }

    if (scratch->boss_x != x && (scratch->boss_x & 0x10) != (x & 0x10)) {
        BossFire_Duplicate2(obj);
        scratch->spread_x = x;
    }

    scratch->boss_x = x;
}

static void BossFire_FallEdge(Object *obj, Scratch_BossFire *scratch) {
    obj->status.o.f.y_flip = false;
    obj->ysp += 0x24;

    int16_t d0 = (int16_t)(obj->pos.l.x.f.u - scratch->spread_x);
    if (d0 < 0)
        d0 = (int16_t)-d0;

    if (d0 == 0x12)
        obj->tile &= ~0x8000; // low priority (falls behind the foreground lip)

    if (ObjFloorDist(obj, obj->pos.l.x.f.u) >= 0)
        return;

    if (--scratch->generic_timer == 0) {
        ObjectDelete(obj);
        return;
    }
    obj->ysp = 0;
    obj->pos.l.x.f.u = scratch->spread_x;
    obj->pos.l.y.f.u = scratch->boss_y;
    obj->tile |= 0x8000; // high priority again
    obj->routine_sec -= 2; // -> BossFire_Duplicate, spawn another on top
}

static void BossFire_Action(Object *obj, Scratch_BossFire *scratch) {
    switch (obj->routine_sec) {
    case 0: BossFire_Drop(obj, scratch); break;
    case 2: BossFire_MakeFlame(obj, scratch); break;
    case 4: BossFire_Duplicate(obj, scratch); break;
    case 6: BossFire_FallEdge(obj, scratch); break;
    }

    SpeedToPos(obj);
    AnimateSprite(obj, Animation_Fireballs);

    if ((uint16_t)obj->pos.l.y.f.u > (uint16_t)(0x210 + 0xD8)) { // boss_mz_y+0xD8 -- fallen into the lava
        ObjectDelete(obj);
        return;
    }
    DisplaySprite(obj);
}

static void BossFire_TempFire(Object *obj, Scratch_BossFire *scratch) {
    obj->tile |= 0x8000; // high priority

    if (--scratch->generic_timer != 0) {
        AnimateSprite(obj, Animation_Fireballs);
        DisplaySprite(obj);
        return;
    }

    obj->anim = 1; // "flame out"
    obj->pos.l.y.f.u -= 4;
    obj->col_type = 0;

    AnimateSprite(obj, Animation_Fireballs);
    DisplaySprite(obj);
}

static void BossFire_TempFireDel(Object *obj) {
    ObjectDelete(obj);
}

void Obj_BossFire(Object *obj) {
    Scratch_BossFire *scratch = (Scratch_BossFire *)&obj->scratch;

    switch (obj->routine) {
    case 0: BossFire_Main(obj, scratch); break;
    case 2: BossFire_Action(obj, scratch); break;
    case 4: BossFire_TempFire(obj, scratch); break;
    case 6: BossFire_TempFireDel(obj); break;
    }
}
