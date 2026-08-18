#include "Roller.h"

#include "Level.h"
#include "LevelCollision.h"
#include "LevelScroll.h"
#include "Macros.h"
#include "Object/Sonic.h"
#include "Resource/Animation/Roller.h"
#include "Resource/Mappings/Roller.h"

// Object 43 - Roller enemy (SYZ). Falls in hidden/curled state until it
// lands, then waits until Sonic passes 256px to its left, at which point
// it unfolds, waits briefly (destroyable while unfolded), then rolls
// rightward at high speed (invincible and damaging while rolling),
// jumping over any ledge/drop it encounters.

static void Roll_Action_FromLeft(Object *obj, Scratch_Roller *scratch) {
    (void)scratch;
    int16_t d0 = (int16_t)(player->pos.l.x.f.u - 256);
    if (d0 < 0)
        return;
    d0 = (int16_t)(d0 - obj->pos.l.x.f.u);
    if (d0 < 0)
        return;

    obj->routine_sec += 4;
    obj->anim = 2;
    obj->xsp = 0x700;
    obj->col_type = 0x0E | 0x80; // col_28x28 | col_hurt
}

static void Roll_Action_Unfolded(Object *obj, Scratch_Roller *scratch) {
    if (obj->anim == 2) { // advanced to rolling animation again (set by the animation script itself)
        obj->routine_sec += 2;
        return;
    }
    if (--scratch->wait_unfolded >= 0)
        return;

    obj->anim = 1; // re-folding animation (sets itself to rolling animation once finished)
    obj->xsp = 0x700;
    obj->col_type = 0x0E | 0x80; // col_28x28 | col_hurt
}

// Only once: stop and unfold Roller 48px to the left of Sonic, making it
// destroyable and starting its wait timer.
static void Roll_Action_StopAndUnfold(Object *obj, Scratch_Roller *scratch) {
    if ((int8_t)scratch->stateflags < 0)
        return;

    int16_t d0 = (int16_t)(player->pos.l.x.f.u - 48);
    d0 = (int16_t)(d0 - obj->pos.l.x.f.u);
    if (d0 >= 0)
        return;

    obj->anim = 0; // unfolding animation
    obj->col_type = 0x0E; // col_28x28 | col_badnik
    obj->xsp = 0;
    scratch->wait_unfolded = 2 * 60;
    obj->routine_sec = 2;
    scratch->stateflags |= 0x80;
}

static void Roll_Action_Rolling(Object *obj, Scratch_Roller *scratch) {
    Roll_Action_StopAndUnfold(obj, scratch);
    SpeedToPos(obj);

    int16_t floor_dist = ObjFloorDist(obj, obj->pos.l.x.f.u);
    if (floor_dist < -8 || floor_dist >= 0xC) {
        obj->routine_sec += 2;
        bool hit_before = (scratch->stateflags & 1) != 0;
        scratch->stateflags |= 1;
        if (hit_before)
            obj->ysp = -0x600;
        return;
    }
    obj->pos.l.y.f.u += floor_dist;
}

static void Roll_Action_Jumping(Object *obj, Scratch_Roller *scratch) {
    (void)scratch;
    ObjectFall(obj);
    if (obj->ysp < 0)
        return;

    int16_t floor_dist = ObjFloorDist(obj, obj->pos.l.x.f.u);
    if (floor_dist < 0)
        return;

    obj->pos.l.y.f.u += floor_dist;
    obj->routine_sec -= 2;
    obj->ysp = 0;
}

void Obj_Roller(Object *obj) {
    Scratch_Roller *scratch = (Scratch_Roller *)&obj->scratch;

    switch (obj->routine) {
    case 0: {
        obj->y_rad = 28 / 2;
        obj->x_rad = 16 / 2;

        // Fall until landing on the floor (while invisible)
        ObjectFall(obj);
        int16_t floor_dist = ObjFloorDist(obj, obj->pos.l.x.f.u);
        if (floor_dist < 0) {
            obj->pos.l.y.f.u += floor_dist;
            obj->ysp = 0;
            obj->routine += 2;
            obj->mappings = Mappings_Roller;
            obj->tile = TILE_MAP(0, 0, 0, 0, ArtTile_Roller);
            obj->render.b = 0;
            obj->render.f.align_fg = true;
            obj->priority = 4;
            obj->width_pixels = 32 / 2;
        }

        // Bug fix: prevents invisibly falling forever if there's no floor beneath it
        if ((uint16_t)obj->pos.l.y.f.u > 0x7FF)
            ObjectDelete(obj);
        return; // do NOT display sprite yet
    }
    case 2:
        switch (obj->routine_sec) {
        case 0:
            Roll_Action_FromLeft(obj, scratch);
            return; // stays invisible while waiting -- no animate/display this frame
        case 2:
            Roll_Action_Unfolded(obj, scratch);
            break;
        case 4:
            Roll_Action_Rolling(obj, scratch);
            break;
        case 6:
            Roll_Action_Jumping(obj, scratch);
            break;
        }

        AnimateSprite(obj, Animation_Roller);
        RememberState(obj);
        break;
    }
}
