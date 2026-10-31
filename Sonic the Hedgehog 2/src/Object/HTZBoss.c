// Hill Top's boss for Sonic 2, as the alpha has it (object 52): see HTZBoss.h. It uses the same boss helpers as the Metropolis boss (BossCommon).
#include "Object/HTZBoss.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"
#include "Object/BossCommon.h"
#include "Object/HTZObjects.h"
#include "Constants.h"

#include "EngineObject.h"
#include "Level.h"
#include "LevelCollision.h"
#include "MathUtil.h"
#include "Sound.h"

#include "Macros.h"

#include "Resource/Animation/HTZBoss.h"
#include "Resource/Mappings/HTZBoss.h"
#include "Resource/Mappings/HTZBossSmoke.h"

#define ArtTile_HTZBoss      0x3C1
#define ArtTile_HTZBossParts 0x421 // (the jet and the lava balls)
#define ArtTile_HTZBossSmoke 0x5E4

// The animation table and mappings are in this file only (a resource header can only be included once)
enum { Ship_Rise = 0, Ship_Wait = 2, Ship_Close = 4, Ship_Descend = 6, Ship_Wreck = 8 };
enum { Part_Make = 0, Part_Run = 2 };
enum { Boss_Start = 0, Boss_Ship = 2, Boss_Jet = 4, Boss_Ball = 6, Boss_Smoke = 8 };
enum { Anim_Jet = 5, Anim_Ball = 7 };

// The alpha's $FFFFF73F: the cannon is open (the second hitbox is there)
static uint8_t cannon_open;
static const Sst *cannon_boss;

static void Boss_SavePlace(Sst *self) {
    self->children[0].x = self->x_pos.f.u;
    self->children[0].y = self->y_pos.f.u;
}

// The ship is bobbing on the spot (Offset_0x0259CC)
static void Ship_Bob(Sst *self, Own_AlphaBoss *own) {
    int16_t sin, cos;
    CalcSine(own->angle, &sin, &cos);
    self->y_pos.f.u = (int16_t)((sin >> 7) + boss_move.y.f.u);
    own->angle = (uint8_t)(own->angle + 4);
}

// What the ship does in each frame of its waiting, the same for each of the paths into it (Offset_0x0259B6)
static void Ship_Show(Sst *self, Own_AlphaBoss *own) {
    Boss_Hit(self, own);
    Boss_SavePlace(self);
    Boss_AnimateSprite(self, own, Animation_HTZBoss);
    Sst_Draw(self);
}

static void Make_Smoke(Sst *self) {
    Sst *smoke = Sst_Allocate();
    if (smoke == NULL)
        return;
    smoke->code = sst_object_codes[ObjId_HTZBoss];
    ((Own_AlphaBoss *)smoke->own.u8)->boss_routine = Boss_Smoke;
    smoke->mappings = Mappings_HTZBossSmoke;
    smoke->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_HTZBossSmoke);
    smoke->render_flags |= SST_RENDER_LEVEL;
    smoke->priority = 1 * 0x80;
    smoke->x_pos.f.u = self->x_pos.f.u;
    smoke->y_pos.f.u = (int16_t)(self->y_pos.f.u - 0x28);
    smoke->x_vel = -0x60;
    smoke->y_vel = -0xC0;
    smoke->mapping_frame = 0;
    smoke->anim_frame_timer = 0x11;
}

static void Ship_Run(Sst *self, Own_AlphaBoss *own) {
    const bool left_side = own->side == 0; // (the side that it is on)

    switch (own->anim_routine) {
    case Ship_Rise:
        cannon_open = 0;
        Boss_Move();
        if (boss_move.y.f.u <= (left_side ? 0x520 : 0x4D8)) {
            boss_move.y.f.u = left_side ? 0x520 : 0x4D8;
            boss_move.ysp = 0;
            own->anim_routine += 2;
            own->count = 0x3C;
        }
        self->y_pos.f.u = boss_move.y.f.u;
        Boss_Hit(self, own);
        Sst_Draw(self);
        break;

    case Ship_Wait: // bobs, and then the cannon opens (its animation moves the routine on) and fires its jet
        if ((int8_t)--own->count >= 0) {
            Ship_Bob(self, own);
        } else {
            cannon_open = 1;
            self->child_count = 1;
            if (own->count != 0xE8) {
                Ship_Bob(self, own);
            } else {
                Sst *jet = Sst_Allocate();
                if (jet != NULL) {
                    jet->code = sst_object_codes[ObjId_HTZBoss];
                    ((Own_AlphaBoss *)jet->own.u8)->boss_routine = Boss_Jet;
                    SST_RENDER_SET(jet, SST_RENDER_X_FLIP, ((self->render_flags & SST_RENDER_X_FLIP) != 0));
                    jet->x_pos.f.u = self->x_pos.f.u;
                    jet->y_pos.f.u = self->y_pos.f.u;
                    own->count = 0x2F;
                }
            }
        }
        Ship_Show(self, own);
        break;

    case Ship_Close:
        cannon_open = 0;
        self->child_count = 0;
        boss_anim[2] = 0x10;
        boss_anim[3] = 0x00;
        if (--own->count != 0) {
            Ship_Bob(self, own);
            Ship_Show(self, own);
        } else {
            boss_move.ysp = 0xE0;
            own->anim_routine += 2;
            Boss_SavePlace(self);
            Sst_Draw(self);
        }
        break;

    case Ship_Descend: {
        Boss_Move();
        if (boss_move.y.f.u >= (left_side ? 0x540 : 0x4F0) && !own->flag) {
            own->flag = 0xFF;
            Sst *balls = Sst_Allocate(); // the lava
            if (balls != NULL) {
                balls->code = sst_object_codes[ObjId_HTZBoss];
                ((Own_AlphaBoss *)balls->own.u8)->boss_routine = Boss_Ball;
                balls->x_pos.f.u = self->x_pos.f.u;
                balls->y_pos.f.u = self->y_pos.f.u;
            }
        }
        if (boss_move.y.f.u >= (left_side ? 0x5D0 : 0x580)) { // at the bottom: up again, on the side that Sonic is not on
            boss_move.y.f.u = left_side ? 0x5D0 : 0x580;
            boss_move.ysp = (int16_t)0xFF20;
            own->anim_routine = Ship_Rise;
            own->flag = 0;
            if ((int16_t)(player->x_pos.f.u - 0x2FEC) >= 0) {
                boss_move.y.f.u = 0x580;
                self->x_pos.f.u = 0x30A0;
                own->side = 0xFF;
            } else {
                self->x_pos.f.u = 0x2F38;
                boss_move.y.f.u = 0x5D0;
                own->side = 0;
            }
            SST_RENDER_SET(self, SST_RENDER_X_FLIP, self->x_pos.f.u <= (int16_t)player->x_pos.f.u); // (faces Sonic)
        }
        self->y_pos.f.u = boss_move.y.f.u;
        Boss_Hit(self, own);
        Sst_Draw(self);
        break;
    }

    case Ship_Wreck:
        self->child_count = 0;
        if (--boss_move.timer >= 0) {
            if (boss_move.timer <= 0x1E) {
                self->mapping_frame = 0x10; // the broken ship
                Boss_Defeated(self);
                if ((frame_count & 0x1F) == 0)
                    Make_Smoke(self);
            } else {
                Boss_Defeated(self);
            }
            Sst_Draw(self);
            break;
        }
        if ((frame_count & 0x1F) == 0)
            Make_Smoke(self);
        if (boss_move.timer > (int16_t)0xFFC4) { // (burning)
            Sst_Draw(self);
            break;
        }
        self->y_pos.f.u += 2; // (sinking)
        if (self->y_pos.f.u <= (left_side ? 0x540 : 0x4F0)) {
            Sst_Draw(self);
            break;
        }
        if (!boss_status) {
            boss_status = 1; // the boss is beaten
            limit_right2 = limit_right1;
            Sst_Delete(self);
        } else {
            Sst_Draw(self);
        }
        break;
    }
}

void Obj_HTZBoss(Sst *self) {
    Own_AlphaBoss *own = (Own_AlphaBoss *)self->own.u8;

    switch (own->boss_routine) {
    case Boss_Start:
        self->mappings = Mappings_HTZBoss;
        self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_HTZBoss);
        self->render_flags |= SST_RENDER_LEVEL;
        self->width_pixels = 0x90;
        self->priority = 4 * 0x80;
        self->x_pos.f.u = 0x30A0;
        self->y_pos.f.u = 0x580;
        own->side = 1;
        self->mapping_frame = 1;
        own->boss_routine += 2;
        self->render_flags |= SST_RENDER_MULTI_DRAW;
        self->collision_flags = 0x0F;
        self->collision_property = 8;
        boss_flag = 3;
        boss_move.ysp = (int16_t)0xFF20;
        boss_move.x.f.u = self->x_pos.f.u;
        boss_move.y.f.u = self->y_pos.f.u;
        self->children[0].x = self->x_pos.f.u;
        self->children[0].y = self->y_pos.f.u;
        self->children[0].frame = 2;
        boss_anim[0] = 0x06;
        boss_anim[1] = 0x00;
        boss_anim[2] = 0x10;
        boss_anim[3] = 0x00;
        cannon_boss = self;
        break;

    case Boss_Ship:
        cannon_boss = self;
        Ship_Run(self, own);
        break;

    case Boss_Jet:
        if (self->routine_secondary == Part_Make) {
            self->mappings = Mappings_HTZBoss;
            self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_HTZBossParts);
            self->render_flags |= SST_RENDER_LEVEL;
            self->priority = 4 * 0x80;
            self->routine_secondary += 2;
            self->anim = Anim_Jet;
            self->collision_flags = 0x98;
            self->y_pos.f.u -= 0x1C;
            if (((self->render_flags & SST_RENDER_X_FLIP) != 0)) {
                self->x_pos.f.u += 0x70;
                self->x_vel = 4;
            } else {
                self->x_pos.f.u -= 0x70;
                self->x_vel = -4;
            }
        } else {
            self->x_pos.f.u += self->x_vel;
            Sst_Animate(self, Animation_HTZBoss);
            SstOnScreenTest(self);
        }
        break;

    case Boss_Ball:
        if (self->routine_secondary == Part_Make) { // the first of the two is this object itself
            Sst *ball = self;
            for (int i = 0; i < 2; i++) {
                if (i > 0) {
                    ball = Sst_Allocate();
                    if (ball == NULL)
                        return;
                    ball->x_pos.f.u = self->x_pos.f.u;
                    ball->y_pos.f.u = self->y_pos.f.u;
                }
                ball->code = sst_object_codes[ObjId_HTZBoss];
                ((Own_AlphaBoss *)ball->own.u8)->boss_routine = Boss_Ball;
                ball->mappings = Mappings_HTZBoss;
                ball->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_HTZBossParts);
                ball->render_flags |= SST_RENDER_LEVEL;
                ball->priority = 3 * 0x80;
                ball->routine_secondary += 2;
                ball->anim = Anim_Ball;
                ball->collision_flags = 0x8B;
                ball->y_radius = 8;
                ball->x_radius = 8;
                ball->x_pos.v &= ~0xFFFF; // (no fraction)
                ball->x_vel = (int16_t)(i == 0 ? -0x1C00 : 0x1C00);
                ball->y_vel = (int16_t)(ball->x_pos.f.u == 0x2F38 ? 0xAC00 : 0x9C00);
            }
        } else {
            self->x_pos.v += (int32_t)self->x_vel << 4;
            self->y_pos.v += (int32_t)self->y_vel << 4;
            self->y_vel += 0x380;
            if (SstFloorDist(self, self->x_pos.f.u) < 0) {
                self->y_pos.f.u += SstFloorDist(self, self->x_pos.f.u);
                HTZFireball_BossBallLands(self);
            } else {
                Sst_Animate(self, Animation_HTZBoss);
                SstOnScreenTest(self);
            }
        }
        break;

    case Boss_Smoke:
        if (--self->anim_frame_timer < 0) {
            self->anim_frame_timer = 0x11;
            if (++self->mapping_frame == 4) {
                Sst_Delete(self);
                return;
            }
        }
        self->x_pos.v += (int32_t)self->x_vel << 8;
        self->y_pos.v += (int32_t)self->y_vel << 8;
        Sst_Draw(self);
        break;
    }
}

bool HTZBoss_CannonBox(const Sst *hit, int16_t *centre_x, int16_t *centre_y, int16_t *half_width) {
    static const int16_t reach[8] = { 0x1C, 0x20, 0x28, 0x34, 0x3C, 0x44, 0x60, 0x70 };
    static const uint8_t width[8] = { 0x04, 0x04, 0x08, 0x0C, 0x14, 0x1C, 0x24, 0x08 };

    if (hit->code != sst_object_codes[ObjId_HTZBoss] || !cannon_open || hit != cannon_boss)
        return false;
    const uint8_t i = (uint8_t)(hit->children[0].frame - 2);
    if (i > 7)
        return false;
    *centre_x = (int16_t)(hit->x_pos.f.u + (((hit->render_flags & SST_RENDER_X_FLIP) != 0) ? reach[i] : -reach[i]));
    *centre_y = (int16_t)(hit->y_pos.f.u - 0x1C);
    *half_width = width[i];
    return true;
}
