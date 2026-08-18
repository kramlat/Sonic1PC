#include "Burrobot.h"

#include "Level.h"
#include "LevelCollision.h"
#include "LevelScroll.h"
#include "Macros.h"
#include "Object/Sonic.h"
#include "Resource/Animation/Burrobot.h"
#include "Resource/Mappings/Burrobot.h"

// Object 2D - Burrobot enemy (LZ). Waits underground until Sonic walks
// into range, then jumps out, walks back and forth checking ledges/floor
// alignment, and periodically either turns around or jumps again.

// Checks how far Sonic is horizontally and faces the object towards him;
// returns true if Sonic is within `zone` px. Sets *out_vel to the speed
// (and direction) to move towards Sonic, when non-NULL.
static bool Burro_CheckDistanceAndFaceSonic(Object *obj, int16_t zone, int16_t *out_vel) {
    int16_t vel = 0x80;
    obj->status.o.f.x_flip = true;

    int16_t d0 = (int16_t)(player->pos.l.x.f.u - obj->pos.l.x.f.u);
    if (d0 < 0) {
        d0 = (int16_t)-d0;
        vel = (int16_t)-vel;
        obj->status.o.f.x_flip = false;
    }

    if (out_vel)
        *out_vel = vel;
    return d0 < zone;
}

static void Burro_Action_Move_NextAction(Object *obj, Scratch_Burrobot *scratch) {
    // 50/50 chance to jump again or turn around, based on the current VBlank counter
    if (!((uint8_t)frame_count & 4)) {
        obj->routine_sec += 2; // -> Burro_Action_Jump
        obj->ysp = -0x400;
        obj->anim = 2;
        return;
    }

    obj->routine_sec -= 2; // -> Burro_Action_TurnAround
    scratch->timedelay = 60 - 1;
    obj->xsp = 0;
    obj->anim = 0;
}

static void Burro_Action_TurnAround(Object *obj, Scratch_Burrobot *scratch) {
    if (--scratch->timedelay >= 0)
        return;

    obj->routine_sec += 2; // -> Burro_Action_Move
    scratch->timedelay = 255;
    obj->xsp = 0x80;
    obj->anim = 1;
    obj->status.o.f.x_flip ^= 1;
    if (!obj->status.o.f.x_flip)
        return;
    obj->xsp = (int16_t)-obj->xsp;
}

static void Burro_Action_Move(Object *obj, Scratch_Burrobot *scratch) {
    if (--scratch->timedelay < 0) {
        Burro_Action_Move_NextAction(obj, scratch);
        return;
    }

    SpeedToPos(obj);

    scratch->checktype ^= 1;
    if (scratch->checktype) {
        int16_t floor_dist = ObjFloorDist(obj, obj->pos.l.x.f.u);
        obj->pos.l.y.f.u += floor_dist;
        return;
    }

    int16_t x = (int16_t)(obj->pos.l.x.f.u + 12);
    if (!obj->status.o.f.x_flip)
        x = (int16_t)(obj->pos.l.x.f.u - 12);
    int16_t floor_dist = ObjFloorDist(obj, x);
    if (floor_dist >= 0xC)
        Burro_Action_Move_NextAction(obj, scratch);
}

static void Burro_Action_Jump(Object *obj, Scratch_Burrobot *scratch) {
    SpeedToPos(obj);
    obj->ysp += 0x18;
    if (obj->ysp < 0)
        return;
    obj->anim = 3;

    int16_t floor_dist = ObjFloorDist(obj, obj->pos.l.x.f.u);
    if (floor_dist >= 0)
        return;
    obj->pos.l.y.f.u += floor_dist;

    obj->ysp = 0;
    obj->anim = 1;
    scratch->timedelay = 255;
    obj->routine_sec -= 2; // -> Burro_Action_Move
    Burro_CheckDistanceAndFaceSonic(obj, 0, NULL); // face Sonic one last time (distance check ignored)
}

static void Burro_Action_ChkSonic(Object *obj, Scratch_Burrobot *scratch) {
    (void)scratch;
    int16_t vel;
    if (!Burro_CheckDistanceAndFaceSonic(obj, 96, &vel))
        return;

    int16_t dy = (int16_t)(player->pos.l.y.f.u - obj->pos.l.y.f.u);
    if (dy >= 0 || dy < -128 || debug_use)
        return;

    obj->routine_sec -= 2; // -> Burro_Action_Jump
    obj->xsp = vel;
    obj->ysp = -0x400;
}

void Obj_Burrobot(Object *obj) {
    Scratch_Burrobot *scratch = (Scratch_Burrobot *)&obj->scratch;

    switch (obj->routine) {
    case 0:
        obj->routine += 2;
        obj->y_rad = 38 / 2;
        obj->x_rad = 16 / 2;
        obj->mappings = Mappings_Burrobot;
        obj->tile = TILE_MAP(0, 0, 0, 0, ArtTile_Burrobot);
        obj->render.f.align_fg = true;
        obj->priority = 4;
        obj->col_type = 0x05; // col_24x36 | col_badnik
        obj->width_pixels = 24 / 2;
        obj->routine_sec += 6; // run Burro_Action_ChkSonic first
        obj->anim = 2;
        __attribute__((fallthrough));
    case 2:
        switch (obj->routine_sec) {
        case 0: Burro_Action_TurnAround(obj, scratch); break;
        case 2: Burro_Action_Move(obj, scratch); break;
        case 4: Burro_Action_Jump(obj, scratch); break;
        case 6: Burro_Action_ChkSonic(obj, scratch); break;
        }
        AnimateSprite(obj, Animation_Burrobot);
        RememberState(obj);
        break;
    }
}
