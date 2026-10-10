// Metropolis's boss for Sonic 2, as the alpha has it (objects 54/55 and 53): see MTZBoss.h. It uses the boss helpers of the alpha: Boss_Move (the ship's position as 16.16 numbers in a buffer of its own that the
// boss objects share, with a speed that moves it), Boss_AnimateSprite (the animation of the ship's main sprite and its child sprite, each a pair of nibbles in a small buffer) and Boss_Hit.
#include "Object/MTZBoss.h"
#include "Object/BossCommon.h"
#include "Constants.h"

#include "EnginePalette.h"
#include "Level.h"
#include "MathUtil.h"
#include "Sound.h"

#include "Macros.h"

#include "Resource/Animation/MTZBoss.h"
#include "Resource/Mappings/MTZBoss.h"

#define ArtTile_MTZBossShip  0x38C // ($7180: the alpha's list $2E)
#define ArtTile_MTZBossBalls 0x3EC // ($7D80)

typedef struct {
    uint8_t subtype;       // 0x28: a ball's second angle (it goes on by 4 each frame)
    uint8_t angle;         // 0x29: a ball's first angle
    int16_t centre_y;      // 0x2A
    int32_t x_product;     // 0x2C
    int32_t depth_product; // 0x30
    uint8_t link;          // 0x34: the slot of the ship
    uint8_t pad;           // 0x35
    int16_t centre_x;      // 0x38
    uint8_t rising;        // 0x3A (cv0E): which way the first angle goes
} Scratch_MTZBall;

static void Boss_Bob(Object *obj, Scratch_AlphaBoss *scratch) {
    int16_t sin, cos;
    CalcSine(scratch->angle, &sin, &cos);
    obj->pos.l.y.f.u = (int16_t)((sin >> 6) + boss_move.y.f.u);
    obj->pos.l.x.f.u = boss_move.x.f.u;
    scratch->angle = (uint8_t)(scratch->angle + 2);
    Boss_Hit(obj, scratch);
}

// The ship's loop (Offset_0x027B36)
static void Boss_Run(Object *obj, Scratch_AlphaBoss *scratch) {
    if (scratch->anim_routine == 0) {
        Boss_Move();
        Boss_Bob(obj, scratch);
        if (scratch->flash == 0x1F)
            scratch->flag = 0xFF; // (hit: the balls may go)
        Boss_AnimateSprite(obj, scratch, Animation_MTZBoss);
        obj->children[0].x = obj->pos.l.x.f.u;
        obj->children[0].y = obj->pos.l.y.f.u;
        DisplaySprite(obj);
    } else if (scratch->anim_routine == 2) {
        Obj_MTZBossBall(obj);
    }
}

void Obj_MTZBoss(Object *obj) {
    Scratch_AlphaBoss *scratch = (Scratch_AlphaBoss *)&obj->scratch;

    if (scratch->boss_routine == 0) {
        obj->mappings = Mappings_MTZBoss;
        obj->tile = TILE_MAP(0, 0, 0, 0, ArtTile_MTZBossShip);
        obj->render.b |= SPRITE_CAM_FIELD;
        obj->priority = 3;
        obj->pos.l.x.f.u = 0x1EA0;
        obj->pos.l.y.f.u = 0x178;
        obj->frame = 2;
        scratch->boss_routine += 2;
        obj->render.f.multi_sprite = true;
        obj->child_count = 1;
        obj->col_type = 0x0F;
        scratch->hits = 8;
        boss_move.x.v = (int32_t)obj->pos.l.x.f.u << 16;
        boss_move.y.v = (int32_t)obj->pos.l.y.f.u << 16;
        boss_move.xsp = -0x200;
        boss_move.ysp = 0;
        obj->children[0].x = obj->pos.l.x.f.u;
        obj->children[0].y = obj->pos.l.y.f.u;
        obj->children[0].frame = 0;
        Object *balls = FindFreeObj();
        if (balls != NULL) {
            balls->type = ObjId_MTZBossBall;
            ((Scratch_MTZBall *)&balls->scratch)->link = (uint8_t)(obj - objects);
        }
        boss_anim[0] = 0x10;
        boss_anim[1] = 0x00;
        boss_anim[2] = 0x01;
        boss_anim[3] = 0x00;
    }
    Boss_Run(obj, scratch);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// The balls (53)
// ---------------------------------------------------------------------------------------------------------------------------------------
enum { BallRoutine_Make = 0, BallRoutine_Orbit = 2, BallRoutine_Fall = 4, BallRoutine_Walk = 6 };
enum { BallAnim_Orbit, BallAnim_Walk, BallAnim_Land };

// Which way the ball is looking from, by depth: its frame and priority (Offset_0x027CD4)
static void Ball_Depth(Object *obj, const Scratch_MTZBall *scratch) {
    const int16_t depth = (int16_t)(scratch->depth_product >> 16);
    if (depth >= 0) {
        if (depth >= 0xC) {
            obj->frame = 3;
            obj->priority = 1;
        } else {
            obj->frame = 4;
            obj->priority = 2;
        }
    } else if (depth >= -0xC) {
        obj->frame = 4;
        obj->priority = 4;
    } else {
        obj->frame = 5;
        obj->priority = 5;
    }
}

// Where it is on its orbit (Offset_0x027C54)
static void Ball_Orbit(Object *obj, Scratch_MTZBall *scratch) {
    int16_t sin1, cos1, sin2, cos2;
    CalcSine(scratch->angle, &sin1, &cos1);
    obj->pos.l.y.f.u = (int16_t)(((int16_t)(cos1 * 0x24) >> 8) + scratch->centre_y);
    const int32_t ring = (int16_t)(sin1 * 0x24);
    CalcSine(scratch->subtype, &sin2, &cos2);
    scratch->x_product = ring * sin2;
    scratch->depth_product = ring * cos2;
    obj->pos.l.x.f.u = (int16_t)((int16_t)(scratch->x_product >> 16) + scratch->centre_x);
    scratch->subtype = (uint8_t)(scratch->subtype + 4);
    if (scratch->rising) {
        scratch->angle = (uint8_t)(scratch->angle - 2);
        if ((int8_t)scratch->angle < 0) {
            scratch->angle = 2;
            scratch->rising = 0;
        }
    } else {
        scratch->angle = (uint8_t)(scratch->angle + 2);
        if (scratch->angle == 0x82) {
            scratch->angle = 0x7E;
            scratch->rising = 1;
        }
    }
}

void Obj_MTZBossBall(Object *obj) {
    Scratch_MTZBall *scratch = (Scratch_MTZBall *)&obj->scratch;

    switch (obj->routine) {
    case BallRoutine_Make: {
        static const uint8_t angles[8] = { 0x24, 0x48, 0x6C, 0x70, 0x4C, 0x28, 0x04, 0x00 };
        const uint8_t link = scratch->link;
        Object *ball = obj; // (the first is this object itself)
        for (int i = 0; i < 7; i++) {
            if (i > 0) {
                ball = FindFreeObj();
                if (ball == NULL)
                    return;
            }
            Scratch_MTZBall *s = (Scratch_MTZBall *)&ball->scratch;
            s->link = link;
            ball->type = ObjId_MTZBossBall;
            ball->mappings = Mappings_MTZBoss;
            ball->tile = TILE_MAP(0, 0, 0, 0, ArtTile_MTZBossBalls);
            ball->render.b |= SPRITE_CAM_FIELD;
            ball->priority = 3;
            ball->routine += 2;
            ball->frame = 5;
            s->angle = angles[i];
            if (i > 2) {
                s->rising = 1;
                s->subtype = 0x80;
            } else {
                s->rising = 0;
                s->subtype = 0;
            }
        }
        break;
    }

    case BallRoutine_Orbit: {
        Object *ship = &objects[scratch->link];
        Scratch_AlphaBoss *ship_scratch = (Scratch_AlphaBoss *)&ship->scratch;
        scratch->centre_y = (int16_t)(ship->pos.l.y.f.u - 4);
        scratch->centre_x = ship->pos.l.x.f.u;
        if (ship_scratch->flag) { // the ship was hit: the ball breaks away
            ship_scratch->flag = 0;
            obj->routine += 2;
            obj->anim = BallAnim_Land;
            obj->ysp = (int16_t)0xFE80;
            obj->xsp = (int16_t)0xFF80;
            obj->col_type = 0x06;
        }
        Ball_Orbit(obj, scratch);
        Ball_Depth(obj, scratch);
        DisplaySprite(obj);
        break;
    }

    case BallRoutine_Fall:
        ObjectFall(obj);
        obj->ysp -= 0x30;
        if (obj->ysp >= 0x80)
            obj->ysp = 0x80;
        if (obj->pos.l.y.f.u >= 0x1AC) {
            obj->pos.l.y.f.u = 0x1AC;
            obj->routine += 2;
        }
        AnimateSprite(obj, Animation_MTZBoss);
        DisplaySprite(obj);
        break;

    case BallRoutine_Walk: { // the small Robotnik walks towards Sonic
        const int16_t d = (int16_t)(player->pos.l.x.f.u - obj->pos.l.x.f.u);
        obj->render.f.x_flip = d >= 0;
        obj->pos.l.x.f.u += obj->render.f.x_flip ? 1 : -1;
        DisplaySprite(obj);
        break;
    }
    }
}
