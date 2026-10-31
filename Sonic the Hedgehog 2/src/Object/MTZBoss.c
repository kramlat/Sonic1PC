// Metropolis's boss for Sonic 2, as the alpha has it (objects 54/55 and 53): see MTZBoss.h. It uses the boss helpers of the alpha: Boss_Move (the ship's position as 16.16 numbers in a buffer of its own that the
// boss objects share, with a speed that moves it), Boss_AnimateSprite (the animation of the ship's main sprite and its child sprite, each a pair of nibbles in a small buffer) and Boss_Hit.
#include "Object/MTZBoss.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"
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
} Own_MTZBall;

static void Boss_Bob(Sst *self, Own_AlphaBoss *own) {
    int16_t sin, cos;
    CalcSine(own->angle, &sin, &cos);
    self->y_pos.f.u = (int16_t)((sin >> 6) + boss_move.y.f.u);
    self->x_pos.f.u = boss_move.x.f.u;
    own->angle = (uint8_t)(own->angle + 2);
    Boss_Hit(self, own);
}

// The ship's loop (Offset_0x027B36)
static void Boss_Run(Sst *self, Own_AlphaBoss *own) {
    if (own->anim_routine == 0) {
        Boss_Move();
        Boss_Bob(self, own);
        if (own->flash == 0x1F)
            own->flag = 0xFF; // (hit: the balls may go)
        Boss_AnimateSprite(self, own, Animation_MTZBoss);
        self->children[0].x = self->x_pos.f.u;
        self->children[0].y = self->y_pos.f.u;
        Sst_Draw(self);
    } else if (own->anim_routine == 2) {
        Obj_MTZBossBall(self);
    }
}

void Obj_MTZBoss(Sst *self) {
    Own_AlphaBoss *own = (Own_AlphaBoss *)self->own.u8;

    if (own->boss_routine == 0) {
        self->mappings = Mappings_MTZBoss;
        self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_MTZBossShip);
        self->render_flags |= SST_RENDER_LEVEL;
        self->priority = 3 * 0x80;
        self->x_pos.f.u = 0x1EA0;
        self->y_pos.f.u = 0x178;
        self->mapping_frame = 2;
        own->boss_routine += 2;
        self->render_flags |= SST_RENDER_MULTI_DRAW;
        self->child_count = 1;
        self->collision_flags = 0x0F;
        self->collision_property = 8;
        boss_move.x.v = (int32_t)self->x_pos.f.u << 16;
        boss_move.y.v = (int32_t)self->y_pos.f.u << 16;
        boss_move.xsp = -0x200;
        boss_move.ysp = 0;
        self->children[0].x = self->x_pos.f.u;
        self->children[0].y = self->y_pos.f.u;
        self->children[0].frame = 0;
        Sst *balls = Sst_Allocate();
        if (balls != NULL) {
            balls->code = sst_object_codes[ObjId_MTZBossBall];
            ((Own_MTZBall *)balls->own.u8)->link = (uint8_t)(self - sst);
        }
        boss_anim[0] = 0x10;
        boss_anim[1] = 0x00;
        boss_anim[2] = 0x01;
        boss_anim[3] = 0x00;
    }
    Boss_Run(self, own);
}

// ---------------------------------------------------------------------------------------------------------------------------------------
// The balls (53)
// ---------------------------------------------------------------------------------------------------------------------------------------
enum { BallRoutine_Make = 0, BallRoutine_Orbit = 2, BallRoutine_Fall = 4, BallRoutine_Walk = 6 };
enum { BallAnim_Orbit, BallAnim_Walk, BallAnim_Land };

// Which way the ball is looking from, by depth: its frame and priority (Offset_0x027CD4)
static void Ball_Depth(Sst *self, const Own_MTZBall *own) {
    const int16_t depth = (int16_t)(own->depth_product >> 16);
    if (depth >= 0) {
        if (depth >= 0xC) {
            self->mapping_frame = 3;
            self->priority = 1 * 0x80;
        } else {
            self->mapping_frame = 4;
            self->priority = 2 * 0x80;
        }
    } else if (depth >= -0xC) {
        self->mapping_frame = 4;
        self->priority = 4 * 0x80;
    } else {
        self->mapping_frame = 5;
        self->priority = 5 * 0x80;
    }
}

// Where it is on its orbit (Offset_0x027C54)
static void Ball_Orbit(Sst *self, Own_MTZBall *own) {
    int16_t sin1, cos1, sin2, cos2;
    CalcSine(own->angle, &sin1, &cos1);
    self->y_pos.f.u = (int16_t)(((int16_t)(cos1 * 0x24) >> 8) + own->centre_y);
    const int32_t ring = (int16_t)(sin1 * 0x24);
    CalcSine(self->subtype, &sin2, &cos2);
    own->x_product = ring * sin2;
    own->depth_product = ring * cos2;
    self->x_pos.f.u = (int16_t)((int16_t)(own->x_product >> 16) + own->centre_x);
    self->subtype = (uint8_t)(self->subtype + 4);
    if (own->rising) {
        own->angle = (uint8_t)(own->angle - 2);
        if ((int8_t)own->angle < 0) {
            own->angle = 2;
            own->rising = 0;
        }
    } else {
        own->angle = (uint8_t)(own->angle + 2);
        if (own->angle == 0x82) {
            own->angle = 0x7E;
            own->rising = 1;
        }
    }
}

void Obj_MTZBossBall(Sst *self) {
    Own_MTZBall *own = (Own_MTZBall *)self->own.u8;

    switch (self->routine) {
    case BallRoutine_Make: {
        static const uint8_t angles[8] = { 0x24, 0x48, 0x6C, 0x70, 0x4C, 0x28, 0x04, 0x00 };
        const uint8_t link = own->link;
        Sst *ball = self; // (the first is this object itself)
        for (int i = 0; i < 7; i++) {
            if (i > 0) {
                ball = Sst_Allocate();
                if (ball == NULL)
                    return;
            }
            Own_MTZBall *s = (Own_MTZBall *)ball->own.u8;
            s->link = link;
            ball->code = sst_object_codes[ObjId_MTZBossBall];
            ball->mappings = Mappings_MTZBoss;
            ball->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_MTZBossBalls);
            ball->render_flags |= SST_RENDER_LEVEL;
            ball->priority = 3 * 0x80;
            ball->routine += 2;
            ball->mapping_frame = 5;
            s->angle = angles[i];
            if (i > 2) {
                s->rising = 1;
                ball->subtype = 0x80;
            } else {
                s->rising = 0;
                ball->subtype = 0;
            }
        }
        break;
    }

    case BallRoutine_Orbit: {
        Sst *ship = &sst[own->link];
        Own_AlphaBoss *ship_scratch = (Own_AlphaBoss *)ship->own.u8;
        own->centre_y = (int16_t)(ship->y_pos.f.u - 4);
        own->centre_x = ship->x_pos.f.u;
        if (ship_scratch->flag) { // the ship was hit: the ball breaks away
            ship_scratch->flag = 0;
            self->routine += 2;
            self->anim = BallAnim_Land;
            self->y_vel = (int16_t)0xFE80;
            self->x_vel = (int16_t)0xFF80;
            self->collision_flags = 0x06;
        }
        Ball_Orbit(self, own);
        Ball_Depth(self, own);
        Sst_Draw(self);
        break;
    }

    case BallRoutine_Fall:
        Sst_MoveSprite(self);
        self->y_vel -= 0x30;
        if (self->y_vel >= 0x80)
            self->y_vel = 0x80;
        if (self->y_pos.f.u >= 0x1AC) {
            self->y_pos.f.u = 0x1AC;
            self->routine += 2;
        }
        Sst_Animate(self, Animation_MTZBoss);
        Sst_Draw(self);
        break;

    case BallRoutine_Walk: { // the small Robotnik walks towards Sonic
        const int16_t d = (int16_t)(player->x_pos.f.u - self->x_pos.f.u);
        SST_RENDER_SET(self, SST_RENDER_X_FLIP, d >= 0);
        self->x_pos.f.u += ((self->render_flags & SST_RENDER_X_FLIP) != 0) ? 1 : -1;
        Sst_Draw(self);
        break;
    }
    }
}
