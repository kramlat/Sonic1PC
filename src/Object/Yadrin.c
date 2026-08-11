#include "Yadrin.h"

#include "Level.h"
#include "LevelCollision.h"
#include "LevelScroll.h"
#include "Macros.h"
#include "Resource/Animation/Yadrin.h"
#include "Resource/Mappings/Yadrin.h"

// Object 50 - Yadrin enemy (MZ [unused in real placement data, but present
// in the debug menu], SYZ). Walks back and forth, pausing and turning
// around whenever it meets a wall, steep upward slope, or a drop ahead.
// Its own special collision (col_special, see React_Special in Sonic.c)
// exposes a narrow, 24x8px "face" hitbox right at its top edge -- only
// touching within that thin band hurts Sonic; touching it anywhere else
// (deeper, or outside that band) is a normal stompable badnik touch.

static bool Yad_ChkWall(Object *obj) {
    // Only checked every 4th frame on real hardware too (offset by this
    // object's own RAM index so multiple Yadrins don't all check on the
    // same frame) -- presumably a performance shortcut, not gameplay-critical.
    if (((uint8_t)frame_count + (uint8_t)(obj - objects)) & 3)
        return false;

    int16_t hit = (obj->xsp < 0) ? ObjHitWallLeft(obj, (int16_t)(~(uint16_t)obj->width_pixels))
                                  : ObjHitWallRight(obj, obj->width_pixels);
    return hit < 0;
}

static void Yad_Action_Wait(Object *obj, Scratch_Yadrin *scratch) {
    if (--scratch->timedelay >= 0)
        return;

    obj->routine_sec += 2;
    obj->xsp = (int16_t)-0x100;
    obj->anim = 1;

    bool was_left = obj->status.o.f.x_flip;
    obj->status.o.f.x_flip ^= 1;
    if (!was_left)
        obj->xsp = (int16_t)-obj->xsp;
}

static void Yad_Action_Move(Object *obj, Scratch_Yadrin *scratch) {
    SpeedToPos(obj);

    int16_t floor_dist = ObjFloorDist(obj, obj->pos.l.x.f.u);
    bool pause = (floor_dist < -8) || (floor_dist >= 0xC);
    if (!pause) {
        obj->pos.l.y.f.u += floor_dist;
        pause = Yad_ChkWall(obj);
    }
    if (!pause)
        return;

    obj->routine_sec -= 2;
    scratch->timedelay = 60 - 1;
    obj->xsp = 0;
    obj->anim = 0;
}

void Obj_Yadrin(Object *obj) {
    Scratch_Yadrin *scratch = (Scratch_Yadrin *)&obj->scratch;

    switch (obj->routine) {
    case 0: { // Main -- falls until it lands, invisible the whole time
        obj->mappings = Mappings_Yadrin;
        obj->tile = TILE_MAP(0, 1, 0, 0, 0x47B); // ArtTile_Yadrin | Tile_Pal2
        obj->render.b = 0;
        obj->render.f.align_fg = true;
        obj->priority = 4;
        obj->width_pixels = 40 / 2;
        obj->y_rad = 34 / 2;
        obj->x_rad = 16 / 2;
        obj->col_type = 0xCC; // col_40x32 | col_special

        ObjectFall(obj);
        int16_t floor_dist = ObjFloorDist(obj, obj->pos.l.x.f.u);
        if (floor_dist >= 0) {
            if ((uint16_t)obj->pos.l.y.f.u > 0x7FF) // fell below max level height
                ObjectDelete(obj);
            return;
        }
        obj->pos.l.y.f.u += floor_dist;
        obj->ysp = 0;
        obj->routine += 2;
        obj->status.o.f.x_flip ^= 1; // face left on spawn
        return;
    }
    case 2: // Action
        if (obj->routine_sec == 0)
            Yad_Action_Wait(obj, scratch);
        else
            Yad_Action_Move(obj, scratch);

        AnimateSprite(obj, Animation_Yadrin);
        RememberState(obj);
        break;
    }
}
