// Hill Top's boss for Sonic 2, as the alpha has it (object 52): see HTZBoss.h. It uses the same boss helpers as the Metropolis boss (BossCommon).
#include "Object/HTZBoss.h"
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
static const Object *cannon_boss;

static void Boss_SavePlace(Object *obj) {
    obj->children[0].x = obj->pos.l.x.f.u;
    obj->children[0].y = obj->pos.l.y.f.u;
}

// The ship is bobbing on the spot (Offset_0x0259CC)
static void Ship_Bob(Object *obj, Scratch_AlphaBoss *scratch) {
    int16_t sin, cos;
    CalcSine(scratch->angle, &sin, &cos);
    obj->pos.l.y.f.u = (int16_t)((sin >> 7) + boss_move.y.f.u);
    scratch->angle = (uint8_t)(scratch->angle + 4);
}

// What the ship does in each frame of its waiting, the same for each of the paths into it (Offset_0x0259B6)
static void Ship_Show(Object *obj, Scratch_AlphaBoss *scratch) {
    Boss_Hit(obj, scratch);
    Boss_SavePlace(obj);
    Boss_AnimateSprite(obj, scratch, Animation_HTZBoss);
    DisplaySprite(obj);
}

static void Make_Smoke(Object *obj) {
    Object *smoke = FindFreeObj();
    if (smoke == NULL)
        return;
    smoke->type = ObjId_HTZBoss;
    ((Scratch_AlphaBoss *)&smoke->scratch)->boss_routine = Boss_Smoke;
    smoke->mappings = Mappings_HTZBossSmoke;
    smoke->tile = TILE_MAP(0, 0, 0, 0, ArtTile_HTZBossSmoke);
    smoke->render.b |= SPRITE_CAM_FIELD;
    smoke->priority = 1;
    smoke->pos.l.x.f.u = obj->pos.l.x.f.u;
    smoke->pos.l.y.f.u = (int16_t)(obj->pos.l.y.f.u - 0x28);
    smoke->xsp = -0x60;
    smoke->ysp = -0xC0;
    smoke->frame = 0;
    smoke->frame_time.b = 0x11;
}

static void Ship_Run(Object *obj, Scratch_AlphaBoss *scratch) {
    const bool left_side = scratch->side == 0; // (the side that it is on)

    switch (scratch->anim_routine) {
    case Ship_Rise:
        cannon_open = 0;
        Boss_Move();
        if (boss_move.y.f.u <= (left_side ? 0x520 : 0x4D8)) {
            boss_move.y.f.u = left_side ? 0x520 : 0x4D8;
            boss_move.ysp = 0;
            scratch->anim_routine += 2;
            scratch->count = 0x3C;
        }
        obj->pos.l.y.f.u = boss_move.y.f.u;
        Boss_Hit(obj, scratch);
        DisplaySprite(obj);
        break;

    case Ship_Wait: // bobs, and then the cannon opens (its animation moves the routine on) and fires its jet
        if ((int8_t)--scratch->count >= 0) {
            Ship_Bob(obj, scratch);
        } else {
            cannon_open = 1;
            obj->child_count = 1;
            if (scratch->count != 0xE8) {
                Ship_Bob(obj, scratch);
            } else {
                Object *jet = FindFreeObj();
                if (jet != NULL) {
                    jet->type = ObjId_HTZBoss;
                    ((Scratch_AlphaBoss *)&jet->scratch)->boss_routine = Boss_Jet;
                    jet->render.f.x_flip = obj->render.f.x_flip;
                    jet->pos.l.x.f.u = obj->pos.l.x.f.u;
                    jet->pos.l.y.f.u = obj->pos.l.y.f.u;
                    scratch->count = 0x2F;
                }
            }
        }
        Ship_Show(obj, scratch);
        break;

    case Ship_Close:
        cannon_open = 0;
        obj->child_count = 0;
        boss_anim[2] = 0x10;
        boss_anim[3] = 0x00;
        if (--scratch->count != 0) {
            Ship_Bob(obj, scratch);
            Ship_Show(obj, scratch);
        } else {
            boss_move.ysp = 0xE0;
            scratch->anim_routine += 2;
            Boss_SavePlace(obj);
            DisplaySprite(obj);
        }
        break;

    case Ship_Descend: {
        Boss_Move();
        if (boss_move.y.f.u >= (left_side ? 0x540 : 0x4F0) && !scratch->flag) {
            scratch->flag = 0xFF;
            Object *balls = FindFreeObj(); // the lava
            if (balls != NULL) {
                balls->type = ObjId_HTZBoss;
                ((Scratch_AlphaBoss *)&balls->scratch)->boss_routine = Boss_Ball;
                balls->pos.l.x.f.u = obj->pos.l.x.f.u;
                balls->pos.l.y.f.u = obj->pos.l.y.f.u;
            }
        }
        if (boss_move.y.f.u >= (left_side ? 0x5D0 : 0x580)) { // at the bottom: up again, on the side that Sonic is not on
            boss_move.y.f.u = left_side ? 0x5D0 : 0x580;
            boss_move.ysp = (int16_t)0xFF20;
            scratch->anim_routine = Ship_Rise;
            scratch->flag = 0;
            if ((int16_t)(player->pos.l.x.f.u - 0x2FEC) >= 0) {
                boss_move.y.f.u = 0x580;
                obj->pos.l.x.f.u = 0x30A0;
                scratch->side = 0xFF;
            } else {
                obj->pos.l.x.f.u = 0x2F38;
                boss_move.y.f.u = 0x5D0;
                scratch->side = 0;
            }
            obj->render.f.x_flip = obj->pos.l.x.f.u <= (int16_t)player->pos.l.x.f.u; // (faces Sonic)
        }
        obj->pos.l.y.f.u = boss_move.y.f.u;
        Boss_Hit(obj, scratch);
        DisplaySprite(obj);
        break;
    }

    case Ship_Wreck:
        obj->child_count = 0;
        if (--boss_move.timer >= 0) {
            if (boss_move.timer <= 0x1E) {
                obj->frame = 0x10; // the broken ship
                Boss_Defeated(obj);
                if ((frame_count & 0x1F) == 0)
                    Make_Smoke(obj);
            } else {
                Boss_Defeated(obj);
            }
            DisplaySprite(obj);
            break;
        }
        if ((frame_count & 0x1F) == 0)
            Make_Smoke(obj);
        if (boss_move.timer > (int16_t)0xFFC4) { // (burning)
            DisplaySprite(obj);
            break;
        }
        obj->pos.l.y.f.u += 2; // (sinking)
        if (obj->pos.l.y.f.u <= (left_side ? 0x540 : 0x4F0)) {
            DisplaySprite(obj);
            break;
        }
        if (!boss_status) {
            boss_status = 1; // the boss is beaten
            limit_right2 = limit_right1;
            ObjectDelete(obj);
        } else {
            DisplaySprite(obj);
        }
        break;
    }
}

void Obj_HTZBoss(Object *obj) {
    Scratch_AlphaBoss *scratch = (Scratch_AlphaBoss *)&obj->scratch;

    switch (scratch->boss_routine) {
    case Boss_Start:
        obj->mappings = Mappings_HTZBoss;
        obj->tile = TILE_MAP(0, 0, 0, 0, ArtTile_HTZBoss);
        obj->render.b |= SPRITE_CAM_FIELD;
        obj->width_pixels = 0x90;
        obj->priority = 4;
        obj->pos.l.x.f.u = 0x30A0;
        obj->pos.l.y.f.u = 0x580;
        scratch->side = 1;
        obj->frame = 1;
        scratch->boss_routine += 2;
        obj->render.f.multi_sprite = true;
        obj->col_type = 0x0F;
        scratch->hits = 8;
        boss_flag = 3;
        boss_move.ysp = (int16_t)0xFF20;
        boss_move.x.f.u = obj->pos.l.x.f.u;
        boss_move.y.f.u = obj->pos.l.y.f.u;
        obj->children[0].x = obj->pos.l.x.f.u;
        obj->children[0].y = obj->pos.l.y.f.u;
        obj->children[0].frame = 2;
        boss_anim[0] = 0x06;
        boss_anim[1] = 0x00;
        boss_anim[2] = 0x10;
        boss_anim[3] = 0x00;
        cannon_boss = obj;
        break;

    case Boss_Ship:
        cannon_boss = obj;
        Ship_Run(obj, scratch);
        break;

    case Boss_Jet:
        if (obj->routine_sec == Part_Make) {
            obj->mappings = Mappings_HTZBoss;
            obj->tile = TILE_MAP(0, 0, 0, 0, ArtTile_HTZBossParts);
            obj->render.b |= SPRITE_CAM_FIELD;
            obj->priority = 4;
            obj->routine_sec += 2;
            obj->anim = Anim_Jet;
            obj->col_type = 0x98;
            obj->pos.l.y.f.u -= 0x1C;
            if (obj->render.f.x_flip) {
                obj->pos.l.x.f.u += 0x70;
                obj->xsp = 4;
            } else {
                obj->pos.l.x.f.u -= 0x70;
                obj->xsp = -4;
            }
        } else {
            obj->pos.l.x.f.u += obj->xsp;
            AnimateSprite(obj, Animation_HTZBoss);
            RememberState(obj);
        }
        break;

    case Boss_Ball:
        if (obj->routine_sec == Part_Make) { // the first of the two is this object itself
            Object *ball = obj;
            for (int i = 0; i < 2; i++) {
                if (i > 0) {
                    ball = FindFreeObj();
                    if (ball == NULL)
                        return;
                    ball->pos.l.x.f.u = obj->pos.l.x.f.u;
                    ball->pos.l.y.f.u = obj->pos.l.y.f.u;
                }
                ball->type = ObjId_HTZBoss;
                ((Scratch_AlphaBoss *)&ball->scratch)->boss_routine = Boss_Ball;
                ball->mappings = Mappings_HTZBoss;
                ball->tile = TILE_MAP(0, 0, 0, 0, ArtTile_HTZBossParts);
                ball->render.b |= SPRITE_CAM_FIELD;
                ball->priority = 3;
                ball->routine_sec += 2;
                ball->anim = Anim_Ball;
                ball->col_type = 0x8B;
                ball->y_rad = 8;
                ball->x_rad = 8;
                ball->pos.l.x.v &= ~0xFFFF; // (no fraction)
                ball->xsp = (int16_t)(i == 0 ? -0x1C00 : 0x1C00);
                ball->ysp = (int16_t)(ball->pos.l.x.f.u == 0x2F38 ? 0xAC00 : 0x9C00);
            }
        } else {
            obj->pos.l.x.v += (int32_t)obj->xsp << 4;
            obj->pos.l.y.v += (int32_t)obj->ysp << 4;
            obj->ysp += 0x380;
            if (ObjFloorDist(obj, obj->pos.l.x.f.u) < 0) {
                obj->pos.l.y.f.u += ObjFloorDist(obj, obj->pos.l.x.f.u);
                HTZFireball_BossBallLands(obj);
            } else {
                AnimateSprite(obj, Animation_HTZBoss);
                RememberState(obj);
            }
        }
        break;

    case Boss_Smoke:
        if (--obj->frame_time.b < 0) {
            obj->frame_time.b = 0x11;
            if (++obj->frame == 4) {
                ObjectDelete(obj);
                return;
            }
        }
        obj->pos.l.x.v += (int32_t)obj->xsp << 8;
        obj->pos.l.y.v += (int32_t)obj->ysp << 8;
        DisplaySprite(obj);
        break;
    }
}

bool HTZBoss_CannonBox(const Object *hit, int16_t *centre_x, int16_t *centre_y, int16_t *half_width) {
    static const int16_t reach[8] = { 0x1C, 0x20, 0x28, 0x34, 0x3C, 0x44, 0x60, 0x70 };
    static const uint8_t width[8] = { 0x04, 0x04, 0x08, 0x0C, 0x14, 0x1C, 0x24, 0x08 };

    if (hit->type != ObjId_HTZBoss || !cannon_open || hit != cannon_boss)
        return false;
    const uint8_t i = (uint8_t)(hit->children[0].frame - 2);
    if (i > 7)
        return false;
    *centre_x = (int16_t)(hit->pos.l.x.f.u + (hit->render.f.x_flip ? reach[i] : -reach[i]));
    *centre_y = (int16_t)(hit->pos.l.y.f.u - 0x1C);
    *half_width = width[i];
    return true;
}
