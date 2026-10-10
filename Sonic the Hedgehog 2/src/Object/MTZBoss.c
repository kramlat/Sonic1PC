// Metropolis's boss for Sonic 2, as the alpha has it (objects 54/55 and 53): see MTZBoss.h. It uses the boss helpers of the alpha: Boss_Move (the ship's position as 16.16 numbers in a buffer of its own that the
// boss objects share, with a speed that moves it), Boss_AnimateSprite (the animation of the ship's main sprite and its child sprite, each a pair of nibbles in a small buffer) and Boss_Hit.
#include "Object/MTZBoss.h"
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

// The boss objects' shared buffers (Boss_Move_Buffer at $FFFFF750 and Boss_Animate_Buffer at $FFFFF740)
static struct {
    dword_s x, y;
    int16_t xsp, ysp;
    int16_t timer;
} boss_move;
static uint8_t boss_anim[8];

typedef struct {
    uint8_t boss_routine;  // the ship's: 0 starts it, 2 runs it
    uint8_t anim_routine;  // 0x26: 0 moves it about, 2 runs the balls' routine, 8 is beaten (nothing in the alpha handles it)
    uint8_t hits;          // 0x32: the hit counter that Boss_Hit looks at (nothing counts it down in the alpha)
    uint8_t flash;         // 0x14: how long its hit flash lasts
    uint8_t angle;         // 0x1A (Obj_Map_Id): the angle of its bob
    uint8_t release;       // 0x38: set when it is hit, for the balls to see
} Scratch_MTZBoss;

STATIC_ASSERT(sizeof(Scratch_MTZBoss) <= 0x18, "Scratch_MTZBoss must fit the scratch memory");

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

// Boss_Move
static void Boss_Move(void) {
    boss_move.x.v += (int32_t)boss_move.xsp << 8;
    boss_move.y.v += (int32_t)boss_move.ysp << 8;
}

// One part's step of Boss_AnimateSprite (Offset_0x026338): `pair` is the part's two bytes of the buffer, `frame` where its frame goes. Returns false where the original leaves without storing (the routine moves on)
static bool BossAnimatePart(Object *obj, Scratch_MTZBoss *scratch, const uint8_t *table, uint8_t *pair, uint8_t *frame) {
    uint8_t d0 = pair[0];
    uint8_t d1 = d0 >> 4;
    d0 &= 0xF;
    uint8_t d2 = d0;
    const bool changed = d0 != d1;
    uint8_t d5 = (uint8_t)((d0 << 4) | d0);
    d0 = pair[1];
    d1 = d0 >> 4;
    if (changed) {
        d0 = 0;
        d1 = 0;
    }
    d0 &= 0xF;
    d0 = (uint8_t)(d0 - 1);
    if ((int8_t)d0 < 0) {
        const uint8_t *script = table + (uint16_t)((table[d2 * 2] << 8) | table[d2 * 2 + 1]);
        d0 = script[0];
        d2 = script[1 + d1];
        while ((int8_t)d2 < 0) {
            if (d2 == 0xFF) { // loops
                d1 = 0;
                d2 = script[1];
            } else if (d2 == 0xFE) { // moves the routine on
                scratch->anim_routine += 2;
                return false;
            } else if (d2 == 0xFD) { // changes the animation
                d5 = (uint8_t)((d5 & 0xF0) | script[2 + d1]);
                goto store;
            } else if (d2 == 0xFC) { // jumps to a frame
                d1 = script[2 + d1];
                d2 = script[1 + d1];
            } else {
                return false;
            }
        }
        *frame = d2 & 0x7F;
        d1++;
    }
store:
    pair[1] = (uint8_t)((d0 & 0xF) | (d1 << 4));
    pair[0] = d5;
    (void)obj;
    return true;
}

static void Boss_AnimateSprite(Object *obj, Scratch_MTZBoss *scratch, const uint8_t *table) {
    uint8_t *pair = boss_anim;
    if (obj->frame != 0) {
        if (!BossAnimatePart(obj, scratch, table, pair, &obj->frame))
            return;
        pair += 2;
    } else {
        pair += 2;
    }
    for (int i = 0; i < obj->child_count; i++, pair += 2) {
        if (!BossAnimatePart(obj, scratch, table, pair, &obj->children[i].frame))
            return;
    }
}

// Offset_0x024580 and Boss_Hit: the ship bobs on its sine, and a hit flashes it (and lets the balls go)
static void Boss_Hit(Object *obj, Scratch_MTZBoss *scratch) {
    if (scratch->anim_routine >= 8)
        return;
    if (scratch->hits == 0) { // (nothing makes it so in the alpha)
        AddPoints(100);
        boss_move.timer = 0xB3;
        scratch->anim_routine = 8;
        return;
    }
    if (obj->col_type != 0)
        return;
    if (scratch->flash == 0) {
        scratch->flash = 0x20;
        QueueSound2(sfx_HitBoss);
    }
    dry_palette[1][1] = (dry_palette[1][1] == 0) ? 0x0EEE : 0;
    if (--scratch->flash == 0)
        obj->col_type = 0xF;
}

static void Boss_Bob(Object *obj, Scratch_MTZBoss *scratch) {
    int16_t sin, cos;
    CalcSine(scratch->angle, &sin, &cos);
    obj->pos.l.y.f.u = (int16_t)((sin >> 6) + boss_move.y.f.u);
    obj->pos.l.x.f.u = boss_move.x.f.u;
    scratch->angle = (uint8_t)(scratch->angle + 2);
    Boss_Hit(obj, scratch);
}

// The ship's loop (Offset_0x027B36)
static void Boss_Run(Object *obj, Scratch_MTZBoss *scratch) {
    if (scratch->anim_routine == 0) {
        Boss_Move();
        Boss_Bob(obj, scratch);
        if (scratch->flash == 0x1F)
            scratch->release = 0xFF; // (hit: the balls may go)
        Boss_AnimateSprite(obj, scratch, Animation_MTZBoss);
        obj->children[0].x = obj->pos.l.x.f.u;
        obj->children[0].y = obj->pos.l.y.f.u;
        DisplaySprite(obj);
    } else if (scratch->anim_routine == 2) {
        Obj_MTZBossBall(obj);
    }
}

void Obj_MTZBoss(Object *obj) {
    Scratch_MTZBoss *scratch = (Scratch_MTZBoss *)&obj->scratch;

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
        Scratch_MTZBoss *ship_scratch = (Scratch_MTZBoss *)&ship->scratch;
        scratch->centre_y = (int16_t)(ship->pos.l.y.f.u - 4);
        scratch->centre_x = ship->pos.l.x.f.u;
        if (ship_scratch->release) { // the ship was hit: the ball breaks away
            ship_scratch->release = 0;
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
