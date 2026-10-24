// The Crocobot (object 4E) of Sand Shower: see Crocobot.h
#include "Object/Crocobot.h"
#include "Constants.h"

#include "Level.h"
#include "LevelCollision.h"

#include "Macros.h"

#include "Resource/Mappings/Crocobot.h"

#define ARTTILE_CROCOBOT 0x411 // (the staged ani.asm's, art tile $2411: palette line 1)

typedef struct {
    uint8_t subtype;   // 0x28
    uint8_t pad0;      // 0x29
    uint8_t pad1[6];   // 0x2A-0x2F
    int16_t wait;      // 0x30
} Scratch_Crocobot;

// loc_1F406: the animations: 0 walks (3 ticks a frame), 1 stands (15), 2 has its mouth open (3)
static const uint8_t crocobot_animations[] = {
    0x00, 0x06, 0x00, 0x0E, 0x00, 0x11,
    0x03, 0x00, 0x04, 0x02, 0x03, 0x01, 0x05, 0xFF,
    0x0F, 0x00, 0xFF,
    0x03, 0x06, 0x0A, 0x08, 0x09, 0x07, 0x0B, 0xFF, 0x00,
};

enum { CrocobotRoutine_Init = 0, CrocobotRoutine_Main = 2 };
enum { CrocobotState_Wait = 0, CrocobotState_Walk = 2 };

static void Crocobot_Face(Object *obj, bool flip) {
    obj->status.o.f.x_flip = flip;
    obj->render.f.x_flip = flip;
}

// loc_1F3CE: its mouth opens when Sonic is within $40 pixels in the way it faces
static void Crocobot_Look(Object *obj) {
    const int16_t d = (int16_t)(obj->pos.l.x.f.u - player->pos.l.x.f.u);
    const bool facing_right = obj->status.o.f.x_flip;
    if (d >= 0) { // Sonic is to its left
        if (d > 0x40) {
            obj->anim = 0;
        } else if (!facing_right) {
            obj->anim = 2;
        }
        return;
    }
    if (d < -0x40 || !facing_right) { // to its right, and too far or it faces the other way
        obj->anim = 0;
        return;
    }
    obj->anim = 2;
}

void Obj_Crocobot(Object *obj) {
    Scratch_Crocobot *scratch = (Scratch_Crocobot *)&obj->scratch;

    if (obj->routine == CrocobotRoutine_Init) {
        obj->mappings = Mappings_Crocobot;
        obj->tile = TILE_MAP(0, 1, 0, 0, ARTTILE_CROCOBOT);
        obj->render.b |= 4 | SPRITE_CAM_FIELD;
        obj->col_type = 0x0A;
        obj->priority = 4;
        obj->width_pixels = 0x10;
        obj->y_rad = 0x10;
        obj->x_rad = 8;
        ObjectFall(obj);
        const int16_t dist = ObjFloorDist(obj, obj->pos.l.x.f.u);
        if (dist < 0) { // it has met the floor
            obj->pos.l.y.f.u += dist;
            obj->ysp = 0;
            obj->routine += 2;
        }
        return;
    }

    switch (obj->routine_sec) {
    case CrocobotState_Wait:
        if (--scratch->wait < 0) { // turns round and sets off
            obj->routine_sec += 2;
            obj->xsp = (int16_t)0xFF40;
            obj->anim = 0;
            const bool was_flipped = obj->status.o.f.x_flip;
            Crocobot_Face(obj, !was_flipped);
            if (!was_flipped) // (it faces right now)
                obj->xsp = (int16_t)-obj->xsp;
        }
        break;

    case CrocobotState_Walk: {
        Crocobot_Look(obj);
        SpeedToPos(obj);
        const int16_t dist = ObjFloorDist(obj, obj->pos.l.x.f.u);
        if (dist >= -8 && dist < 0x0C) {
            obj->pos.l.y.f.u += dist;
        } else { // the floor ends: back to waiting, $3B frames
            obj->routine_sec -= 2;
            scratch->wait = 0x3B;
            obj->xsp = 0;
            obj->anim = 1;
        }
        break;
    }
    }

    AnimateSprite(obj, crocobot_animations);
    RememberState(obj);
}
