#include "Bumper.h"

#include "Level.h"
#include "LevelScroll.h"
#include "Macros.h"
#include "MathUtil.h"
#include "Object/Sonic.h"
#include "Resource/Animation/Bumper.h"
#include "Resource/Mappings/Bumper.h"
#include "Sound.h"

// Object 47 - pinball bumper (SYZ). Collision is handled through
// ReactToItem (see Sonic.c's col_16x16_alt case), which just increments
// col_property each time Sonic touches it; this object's own routine
// reads/clears that flag each frame to trigger the actual bounce.

static void Bump_Hit(Object *obj) {
    if (obj->col_property) {
        obj->col_property = 0;

        int16_t dx = (int16_t)(obj->pos.l.x.f.u - player->pos.l.x.f.u);
        int16_t dy = (int16_t)(obj->pos.l.y.f.u - player->pos.l.y.f.u);
        uint16_t angle = CalcAngle(dx, dy);
        int16_t sin, cos;
        CalcSine((uint8_t)angle, &sin, &cos);

        player->xsp = (int16_t)(((int32_t)cos * -0x700) >> 8);
        player->ysp = (int16_t)(((int32_t)sin * -0x700) >> 8);

        player->status.p.f.in_air = true;
        player->status.p.f.roll_jump = false;
        player->status.p.f.pushing = false;
        Scratch_Sonic *pscratch = (Scratch_Sonic *)&player->scratch;
        pscratch->jumping = false;

        obj->anim = 1; // bumper "hit" animation
        QueueSound2(sfx_Bumper);

        bool award_points = true;
        if (obj->respawn_index) {
            if (objstate[obj->respawn_index] >= (10 + 0x80))
                award_points = false;
            else
                objstate[obj->respawn_index]++;
        }
        if (award_points) {
            AddPoints(10);

            Object *points = FindFreeObj();
            if (points != NULL) {
                points->type = ObjId_Points;
                points->pos.l.x.f.u = obj->pos.l.x.f.u;
                points->pos.l.y.f.u = obj->pos.l.y.f.u;
                points->frame = 4; // "10" frame
            }
        }
    }

    AnimateSprite(obj, Animation_Bumper);
    RememberState(obj);
}

void Obj_Bumper(Object *obj) {
    switch (obj->routine) {
    case 0:
        obj->routine += 2;
        obj->mappings = Mappings_Bumper;
        obj->tile = TILE_MAP(0, 0, 0, 0, 0x380); // ArtTile_SYZ_Bumper
        obj->render.b = 0;
        obj->render.f.align_fg = true;
        obj->width_pixels = 32 / 2;
        obj->priority = 1;
        obj->col_type = 0x17 | 0xC0; // col_16x16_alt | col_special
        __attribute__((fallthrough));
    case 2:
        Bump_Hit(obj);
        break;
    }
}
