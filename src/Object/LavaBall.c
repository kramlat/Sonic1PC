#include "LavaBall.h"

#include "Level.h"
#include "LevelCollision.h"
#include "LevelScroll.h"
#include "Macros.h"
#include "Resource/Animation/Fireballs.h"
#include "Sound.h"

// Object 14 - lava balls (MZ, SLZ). Launched by LavaMaker (object 13) in
// one of 9 fixed patterns selected by subtype: 0-3 rise and fall back down
// (different launch speeds), 4/5 fly straight up/down until hitting a
// ceiling/floor, 6/7 fly sideways left/right until hitting a wall, and 8
// just sits still (only ever reached by the wall/ceiling/floor-hit types
// switching themselves to it).
//
// Mappings_Fireballs is owned by GrassFire.c (Object/GrassFire.h directly
// #includes the actual array definition there) -- a bare extern here
// avoids a second definition and a link failure, same reasoning as
// InvisibleBarrier.c's own comment on this.
extern const uint8_t Mappings_Fireballs[];

static const int16_t lball_speeds[9] = {
    -0x400, -0x500, -0x600, -0x700, -0x200, 0x200, -0x200, 0x200, 0,
};

static void LBall_RiseAndFall(Object *obj, Scratch_LavaBall *scratch) {
    obj->ysp += 0x18; // fall down faster than it rose

    if (scratch->orig_y < obj->pos.l.y.f.u) // fallen back past its spawn height
        obj->routine += 2;                  // -> LBall_Delete
    obj->status.o.f.y_flip = (obj->ysp < 0); // face up while still rising, down otherwise
}

static void LBall_Up(Object *obj) {
    obj->status.o.f.y_flip = true; // face up

    if (ObjHitCeiling(obj) < 0) {
        obj->scratch.u8[0] = 8; // -> LBall_DoNothing
        obj->anim = 1;          // vertical collide animation
        obj->ysp = 0;
    }
}

static void LBall_Down(Object *obj) {
    obj->status.o.f.y_flip = false; // face down

    if (ObjFloorDist(obj, obj->pos.l.x.f.u) < 0) {
        obj->scratch.u8[0] = 8; // -> LBall_DoNothing
        obj->anim = 1;          // vertical collide animation
        obj->ysp = 0;
    }
}

static void LBall_Left(Object *obj) {
    obj->status.o.f.x_flip = true; // face left

    if (ObjHitWallLeft(obj, -8) < 0) {
        obj->scratch.u8[0] = 8; // -> LBall_DoNothing
        obj->anim = 3;          // horizontal collide animation
        obj->xsp = 0;
    }
}

static void LBall_Right(Object *obj) {
    obj->status.o.f.x_flip = false; // face right

    if (ObjHitWallRight(obj, 8) < 0) {
        obj->scratch.u8[0] = 8; // -> LBall_DoNothing
        obj->anim = 3;          // horizontal collide animation
        obj->xsp = 0;
    }
}

void Obj_LavaBall(Object *obj) {
    Scratch_LavaBall *scratch = (Scratch_LavaBall *)&obj->scratch;

    switch (obj->routine) {
    case 0: { // Main
        obj->routine += 2;
        obj->y_rad = 16 / 2;
        obj->x_rad = 16 / 2;
        obj->mappings = Mappings_Fireballs;
        obj->tile = (LEVEL_ZONE(level_id) == ZoneId_SLZ)
            ? TILE_MAP(0, 0, 0, 0, ArtTile_SLZ_Fireball)
            : TILE_MAP(0, 0, 0, 0, ArtTile_MZ_Fireball);
        obj->render.b = 0;
        obj->render.f.align_fg = true;
        obj->priority = 3;
        obj->col_type = 0x8B; // col_16x16 | col_hurt
        scratch->orig_y = obj->pos.l.y.f.u;

        if (scratch->from_boss)
            obj->priority += 2;

        uint8_t subtype = obj->scratch.u8[0];
        obj->ysp = lball_speeds[subtype];
        obj->width_pixels = 16 / 2;

        if (subtype >= 6) {
            obj->width_pixels = 32 / 2;
            obj->anim = 2; // horizontal animation
            obj->xsp = obj->ysp;
            obj->ysp = 0;
        }

        PlaySound(sfx_Fireball);
        __attribute__((fallthrough));
    }
    case 2: // Action
        switch (obj->scratch.u8[0]) {
        case 0:
        case 1:
        case 2:
        case 3:
            LBall_RiseAndFall(obj, scratch);
            break;
        case 4:
            LBall_Up(obj);
            break;
        case 5:
            LBall_Down(obj);
            break;
        case 6:
            LBall_Left(obj);
            break;
        case 7:
            LBall_Right(obj);
            break;
        case 8:
            break; // do nothing
        }

        SpeedToPos(obj);
        AnimateSprite(obj, Animation_Fireballs); // wall/ceiling/floor-collided balls advance obRoutine to LBall_Delete themselves

        if (IS_OFFSCREEN(obj->pos.l.x.f.u)) {
            ObjectDelete(obj);
            return;
        }
        DisplaySprite(obj);
        break;
    case 4: // Delete
        ObjectDelete(obj);
        break;
    }
}
