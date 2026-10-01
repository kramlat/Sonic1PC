#include "BossStarLight.h"

#include <string.h>

#include "Level.h"
#include "LevelScroll.h"
#include "MathUtil.h"
#include "Object/Seesaw.h"
#include "Object/Sonic.h"
#include "Palette.h"
#include "Resource/Mappings/BossSpikeball.h"
// Mappings_SeesawBall is defined by Seesaw.c.
extern const uint8_t Mappings_SeesawBall[];
#include "Sound.h"

// Animation_Eggman/Mappings_Eggman are owned by BossGreenHill.c, and
// Mappings_BossItems by BossBall.c -- bare externs, same as BossLabyrinth.c.
extern const uint8_t Animation_Eggman[];
extern const uint8_t Mappings_Eggman[];
extern const uint8_t Mappings_BossItems[];

// Objects 7A/7B - Eggman (SLZ) and his exploding spike balls. Ported from
// s1disasm's "_incObj/7A, 7B Boss - SLZ Main and Spike Balls.asm".
// boss_slz_x = 0x2000, boss_slz_y = 0x210, boss_slz_end = boss_slz_x+0x160
// (real s1disasm _Constants.asm).

#define SLZ_BOSS_X 0x2000
#define SLZ_BOSS_Y 0x210
#define SLZ_BOSS_END (SLZ_BOSS_X + 0x160)

#define SLZ_COL_BOSS 0x0F // col_48x48 | col_boss

static const struct { uint8_t routine, anim, priority; } BSLZ_ObjData[4] = {
    { 2, 0, 4 }, // ship body
    { 4, 1, 4 }, // face
    { 6, 7, 4 }, // thruster
    { 8, 0, 3 }, // wide pipe
};

// Y offset between a falling spike ball and the seesaw side it lands on,
// indexed by (seesaw frame) + (2 if the ball is on the left side).
static const int16_t BSB_SeesawYOffset[5] = { -8, -0x1C, -0x2F, -0x1C, -8 };

static const int16_t BSB_FragSpeed[4][2] = {
    { -0x100, -0x340 }, { -0xA0, -0x240 }, { 0x100, -0x340 }, { 0xA0, -0x240 },
};

static bool BossStarLight_ShipMain(Object *obj, Scratch_BossStarLight *scratch);

// The real ASM scans only part of object RAM for seesaws and the boss, and
// pointers into that range; the port's slots differ, so FixBugs-style full
// scans of the level objects are forced on.
static void BossStarLight_Main(Object *obj, Scratch_BossStarLight *scratch) {
    memset(&obj->scratch, 0, sizeof(obj->scratch));

    obj->pos.l.x.f.u = SLZ_BOSS_X + 0x188;
    obj->pos.l.y.f.u = SLZ_BOSS_Y + 0x18;
    scratch->boss_x.f.u = obj->pos.l.x.f.u;
    scratch->boss_x.f.l = 0;
    scratch->boss_y.f.u = obj->pos.l.y.f.u;
    scratch->boss_y.f.l = 0;
    obj->col_type = SLZ_COL_BOSS;
    obj->col_property = 8; // obBossHits

    uint8_t self_index = (uint8_t)(obj - objects);

    for (int i = 0; i < 4; i++) {
        Object *sub;
        if (i == 0) {
            sub = obj;
        } else {
            sub = FindNextFreeObj(obj);
            if (sub == NULL)
                break;
            memset(&sub->scratch, 0, sizeof(sub->scratch));
            sub->type = ObjId_BossStarLight;
            sub->pos.l.x.f.u = obj->pos.l.x.f.u;
            sub->pos.l.y.f.u = obj->pos.l.y.f.u;
        }

        obj->status.o.f.x_flip = false;
        sub->routine_sec = 0;
        sub->routine = BSLZ_ObjData[i].routine;
        sub->anim = BSLZ_ObjData[i].anim;
        sub->priority = BSLZ_ObjData[i].priority;
        // Force AnimateSprite's reset guard on the first call (see BossSpringYard.c).
        sub->prev_anim = (uint8_t)(BSLZ_ObjData[i].anim + 1);
        sub->anim_frame = 0;
        sub->frame_time.w = 0;
        sub->frame = 0;
        sub->mappings = Mappings_Eggman;
        sub->tile = TILE_MAP(0, 0, 0, 0, ArtTile_Eggman);
        sub->render.b = 0;
        sub->render.f.align_fg = true;
        sub->width_pixels = 64 / 2;
        ((Scratch_BossStarLight *)&sub->scratch)->parent_index = self_index;
    }

    // Collect the seesaws without a ball (subtype != 0): the three drop points.
    scratch->seesaw_count = 0;
    for (int i = 0; i < LEVEL_OBJECTS && scratch->seesaw_count < 3; i++) {
        Object *saw = &level_objects[i];
        if (saw->type == ObjId_Seesaw && saw->scratch.u8[0] != 0)
            scratch->seesaws[scratch->seesaw_count++] = (uint8_t)(saw - objects);
    }

    BossStarLight_ShipMain(obj, scratch);
}

static void BSLZ_StatusUpdate(Object *obj, Scratch_BossStarLight *scratch) {
    if (obj->routine_sec >= 6)
        return; // exploding or beyond
    if (obj->status.o.f.flag7) { // defeated flag, set by the spike ball on the last hit
        AddPoints(1000); // real ASM passes 100 -- its AddPoints takes points/10, this project's takes the full value
        obj->routine_sec = 6;
        scratch->generic_timer = 120;
        obj->xsp = 0;
        return;
    }
    if (obj->col_type != 0)
        return; // not currently being hit

    if (scratch->flash == 0) {
        scratch->flash = 0x20;
        QueueSound2(sfx_HitBoss);
    }

    dry_palette[1][1] = (dry_palette[1][1] == 0) ? 0x0EEE : 0;
    if (--scratch->flash != 0)
        return;
    obj->col_type = SLZ_COL_BOSS; // restore collision
}

// BSLZ_ShipUpdate: move, bob up and down, then status.
static void BSLZ_ShipUpdate(Object *obj, Scratch_BossStarLight *scratch) {
    BossMove(obj, &scratch->boss_x, &scratch->boss_y);
    int16_t sin, cos;
    CalcSine(scratch->sine_counter, &sin, &cos);
    scratch->sine_counter = (uint8_t)(scratch->sine_counter + 2);
    obj->pos.l.y.f.u = (int16_t)((sin >> 6) + scratch->boss_y.f.u);
    obj->pos.l.x.f.u = scratch->boss_x.f.u;
    BSLZ_StatusUpdate(obj, scratch);
}

// BSLZ_MoveUpdate: move without bobbing, then status.
static void BSLZ_MoveUpdate(Object *obj, Scratch_BossStarLight *scratch) {
    BossMove(obj, &scratch->boss_x, &scratch->boss_y);
    obj->pos.l.y.f.u = scratch->boss_y.f.u;
    obj->pos.l.x.f.u = scratch->boss_x.f.u;
    BSLZ_StatusUpdate(obj, scratch);
}

static void BSLZ_ShipStart(Object *obj, Scratch_BossStarLight *scratch) {
    obj->xsp = -0x100;
    if ((uint16_t)scratch->boss_x.f.u < SLZ_BOSS_X + 0x120)
        obj->routine_sec += 2;
    BSLZ_ShipUpdate(obj, scratch);
}

static void BSLZ_ShipMove(Object *obj, Scratch_BossStarLight *scratch) {
    int16_t bx = scratch->boss_x.f.u;
    obj->xsp = 0x200;
    int16_t offset = 0x28;
    if (!obj->status.o.f.x_flip) {
        obj->xsp = -0x200;
        offset = -0x28;
        if (bx <= SLZ_BOSS_X + 8)
            obj->status.o.f.x_flip = !obj->status.o.f.x_flip;
    } else if (bx >= SLZ_BOSS_X + 0x138) {
        obj->status.o.f.x_flip = !obj->status.o.f.x_flip;
    }

    // Is a seesaw's drop point (its X +/- 0x28 on the side we're heading
    // for) directly under us, with Sonic not standing on it?
    scratch->pick = -1;
    for (int i = 0; i < scratch->seesaw_count; i++) {
        Object *saw = &objects[scratch->seesaws[i]];
        if (saw->status.o.f.player_stand)
            continue;
        if ((int16_t)(saw->pos.l.x.f.u + offset) == obj->pos.l.x.f.u) {
            scratch->pick = (int8_t)i;
            obj->routine_sec += 2;
            scratch->generic_timer = 40;
            break;
        }
    }
    BSLZ_ShipUpdate(obj, scratch);
}

static void BSLZ_MakeBall(Object *obj, Scratch_BossStarLight *scratch) {
    if (scratch->generic_timer == 40) {
        bool abort_drop = scratch->pick < 0;
        Object *saw = NULL;
        uint8_t saw_index = 0;
        if (!abort_drop) {
            saw_index = scratch->seesaws[scratch->pick];
            saw = &objects[saw_index];
            // Is there already a ball linked to this seesaw?
            for (int i = 0; i < LEVEL_OBJECTS; i++) {
                Object *ball = &level_objects[i];
                if (ball->type == ObjId_BossSpikeball && ball->routine != 0xA &&
                    ((Scratch_BossSpikeball *)&ball->scratch)->seesaw_index == saw_index) {
                    abort_drop = true;
                    break;
                }
            }
        }
        if (!abort_drop) {
            Object *ball = FindFreeObj();
            if (ball == NULL) {
                abort_drop = true;
            } else {
                ball->type = ObjId_BossSpikeball;
                ball->pos.l.x.f.u = obj->pos.l.x.f.u;
                ball->pos.l.y.f.u = (int16_t)(obj->pos.l.y.f.u + 0x20); // out of the ball launcher
                ball->status.b = saw->status.b;
                ((Scratch_BossSpikeball *)&ball->scratch)->seesaw_index = saw_index;
            }
        }
        if (abort_drop) {
            obj->routine_sec -= 2;
            BSLZ_ShipUpdate(obj, scratch);
            return;
        }
    }

    if (--scratch->generic_timer == 0) {
        obj->routine_sec -= 2; // time to start moving again
        BSLZ_ShipUpdate(obj, scratch);
        return;
    }
    BSLZ_StatusUpdate(obj, scratch);
}

static void BSLZ_Explode(Object *obj, Scratch_BossStarLight *scratch) {
    if ((int8_t)--scratch->generic_timer >= 0) {
        BossDefeated(obj);
        return;
    }
    obj->routine_sec += 2;
    obj->ysp = 0;
    obj->xsp = 0;
    obj->status.o.f.x_flip = true; // face right
    obj->status.o.f.flag7 = false; // clear the defeated flag
    scratch->generic_timer = (uint8_t)-24;
    if (boss_status == 0)
        boss_status = 1; // defeated, capsule not opened yet
    BSLZ_StatusUpdate(obj, scratch);
}

static void BSLZ_Recover(Object *obj, Scratch_BossStarLight *scratch) {
    int8_t t = (int8_t)(++scratch->generic_timer);
    if (t == 0) {
        obj->ysp = 0; // done falling
    } else if (t < 0) {
        obj->ysp = (int16_t)(obj->ysp + 0x18); // fall a little faster
    } else if (t < 32) {
        obj->ysp = (int16_t)(obj->ysp - 8); // slow down, then rise
    } else if (t == 32) {
        obj->ysp = 0;
        QueueSound1(bgm_SLZ);
    } else if (t >= 42) {
        obj->routine_sec += 2;
    }
    BSLZ_MoveUpdate(obj, scratch);
}

// Returns true if the ship deleted itself.
static bool BSLZ_Escape(Object *obj, Scratch_BossStarLight *scratch) {
    obj->xsp = 0x400;
    obj->ysp = -0x40;
    if (limit_right2 < SLZ_BOSS_END) {
        limit_right2 += 2; // keep unlocking the screen's right boundary
    } else if (!obj->render.f.on_screen) {
        // FixBugs forced on (crash-class display-and-delete under C rules).
        ObjectDelete(obj);
        return true;
    }
    BSLZ_ShipUpdate(obj, scratch);
    return false;
}

static void BSLZ_Display(Object *obj, Object *parent) {
    obj->pos.l.x.f.u = parent->pos.l.x.f.u;
    obj->pos.l.y.f.u = parent->pos.l.y.f.u;
    obj->status.b = parent->status.b;
    uint8_t flip = obj->status.b & 3;
    obj->render.b = (uint8_t)((obj->render.b & ~3) | flip);
    DisplaySprite(obj);
}

static bool BossStarLight_ShipMain(Object *obj, Scratch_BossStarLight *scratch) {
    switch (obj->routine_sec) {
    case 0x0: BSLZ_ShipStart(obj, scratch); break;
    case 0x2: BSLZ_ShipMove(obj, scratch); break;
    case 0x4: BSLZ_MakeBall(obj, scratch); break;
    case 0x6: BSLZ_Explode(obj, scratch); break;
    case 0x8: BSLZ_Recover(obj, scratch); break;
    case 0xA:
        if (BSLZ_Escape(obj, scratch))
            return true;
        break;
    }

    AnimateSprite(obj, Animation_Eggman);
    uint8_t flip = obj->status.b & 3;
    obj->render.b = (uint8_t)((obj->render.b & ~3) | flip);
    DisplaySprite(obj);
    return false;
}

static void BossStarLight_FaceMain(Object *obj, Scratch_BossStarLight *scratch) {
    Object *parent = &objects[scratch->parent_index];
    if (parent->type != obj->type) { // ship already deleted
        ObjectDelete(obj);
        return;
    }

    uint8_t phase = parent->routine_sec;
    uint8_t anim = 1; // facenormal1
    if (phase >= 6)
        anim = 0xA; // facedefeat
    else if (parent->col_type == 0)
        anim = 5; // facehit
    else if (player->routine >= 4)
        anim = 4; // facelaugh

    obj->anim = anim;
    if (phase == 0xA) {
        obj->anim = 6; // facepanic
        if (!obj->render.f.on_screen) {
            ObjectDelete(obj);
            return;
        }
    }
    AnimateSprite(obj, Animation_Eggman);
    BSLZ_Display(obj, parent);
}

static void BossStarLight_FlameMain(Object *obj, Scratch_BossStarLight *scratch) {
    obj->anim = 8;
    Object *parent = &objects[scratch->parent_index];
    if (parent->type != obj->type) {
        ObjectDelete(obj);
        return;
    }

    uint8_t phase = parent->routine_sec;
    if (phase == 0xA) {
        if (!obj->render.f.on_screen) {
            ObjectDelete(obj);
            return;
        }
        obj->anim = 0xB; // escape flame
    } else if (phase >= 4 && phase <= 8) {
        obj->anim = 7; // not moving, hide the flame
    }
    AnimateSprite(obj, Animation_Eggman);
    BSLZ_Display(obj, parent);
}

static void BossStarLight_PipeMain(Object *obj, Scratch_BossStarLight *scratch) {
    Object *parent = &objects[scratch->parent_index];
    if (parent->type != obj->type) {
        ObjectDelete(obj);
        return;
    }
    if (parent->routine_sec == 0xA && !obj->render.f.on_screen) {
        ObjectDelete(obj);
        return;
    }
    obj->mappings = Mappings_BossItems;
    obj->tile = TILE_MAP(0, 1, 0, 0, ArtTile_Eggman_Weapons); // | Tile_Pal2
    obj->frame = 3;           // wide pipe
    BSLZ_Display(obj, parent);
}

void Obj_BossStarLight(Object *obj) {
    Scratch_BossStarLight *scratch = (Scratch_BossStarLight *)&obj->scratch;

    switch (obj->routine) {
    case 0: BossStarLight_Main(obj, scratch); break;
    case 2: BossStarLight_ShipMain(obj, scratch); break;
    case 4: BossStarLight_FaceMain(obj, scratch); break;
    case 6: BossStarLight_FlameMain(obj, scratch); break;
    case 8: BossStarLight_PipeMain(obj, scratch); break;
    }
}

// ---------------------------------------------------------------------------
// Object 7B - spike balls
// ---------------------------------------------------------------------------

static void BSB_CheckFlicker(Object *obj, Scratch_BossSpikeball *scratch) {
    if (scratch->timer == 120)
        scratch->delay = 5;
    if (scratch->timer == 60)
        scratch->delay = 2;
    if ((int8_t)--scratch->time_frame > 0)
        return;
    obj->frame ^= 1;
    scratch->time_frame = scratch->delay;
}

static int BSB_YOffsetIndex(Object *obj, Scratch_BossSpikeball *scratch, Object *saw) {
    int idx = saw->frame;
    if ((uint16_t)obj->pos.l.x.f.u < (uint16_t)scratch->base_x)
        idx += 2; // left side of the seesaw
    return idx;
}

// Tells the seesaw which side the ball is on and, if Sonic is on the
// opposite end, launches him. Then moves on to the next routine.
static void BSB_LaunchSonic(Object *obj, Scratch_BossSpikeball *scratch, Object *saw, int8_t side) {
    ((Scratch_Seesaw *)&saw->scratch)->state = side; // tell the seesaw which side the ball is on
    scratch->side = side;

    if (side != (int8_t)saw->frame) { // is the seesaw NOT already lowered on this side?
        bool was_standing = saw->status.o.f.player_stand;
        saw->status.o.f.player_stand = false;
        if (was_standing) {
            saw->routine_sec = 0;
            saw->routine = 2;
            player->ysp = (int16_t)-obj->ysp;
            if (saw->frame == 1)
                player->ysp >>= 1; // seesaw was flat -- half the launch
            player->status.p.f.in_air = true;
            Sonic_CancelSpindash(); // same drop-dash fix as the seesaw's own ball
            player->status.p.f.object_stand = false;
            ((Scratch_Sonic *)&player->scratch)->jumping = 0;
            Sonic_ChkRoll(player);
            player->routine = 2; // Sonic_Control
            QueueSound2(sfx_Spring);
        }
    }

    obj->xsp = 0;
    obj->ysp = 0;
    obj->routine += 2;
    BSB_CheckFlicker(obj, scratch);
}

static void BSB_HitBoss(Object *obj, Scratch_BossSpikeball *scratch);

static void BSB_Fall(Object *obj, Scratch_BossSpikeball *scratch) {
    ObjectFall(obj);
    Object *saw = &objects[scratch->seesaw_index];
    int idx = BSB_YOffsetIndex(obj, scratch, saw);
    if ((int16_t)(scratch->seesaw_y + BSB_SeesawYOffset[idx]) > obj->pos.l.y.f.u)
        return; // hasn't reached the seesaw yet

    int8_t side = obj->status.o.f.x_flip ? 0 : 2;
    scratch->timer = 240;
    scratch->delay = 10;
    scratch->time_frame = 10;
    BSB_LaunchSonic(obj, scratch, saw, side);
}

static void BSB_Main(Object *obj, Scratch_BossSpikeball *scratch) {
    obj->mappings = Mappings_SeesawBall;
    obj->tile = TILE_MAP(0, 0, 0, 0, ArtTile_Eggman_Spikeball);
    obj->frame = 1;
    obj->render.f.align_fg = true;
    obj->priority = 4;
    obj->col_type = 0x17 | 0x80; // col_16x16 | col_hurt
    obj->width_pixels = 24 / 2;

    Object *saw = &objects[scratch->seesaw_index];
    scratch->base_x = saw->pos.l.x.f.u;
    scratch->seesaw_y = saw->pos.l.y.f.u;
    obj->status.o.f.x_flip = true;
    scratch->side = 0;
    if (obj->pos.l.x.f.u <= saw->pos.l.x.f.u) {
        obj->status.o.f.x_flip = false;
        scratch->side = 2;
    }
    obj->routine += 2;
    BSB_Fall(obj, scratch);
}

static void BSB_Bounce(Object *obj, Scratch_BossSpikeball *scratch) {
    Object *saw = &objects[scratch->seesaw_index];
    int8_t saw_side = ((Scratch_Seesaw *)&saw->scratch)->state;
    int diff = scratch->side - saw_side;

    if (diff != 0) {
        // The seesaw tipped under the ball: launch it.
        if (diff < 0)
            diff = -diff;
        int16_t vy, vx;
        if (diff == 1) { // seesaw flat
            vy = -0x818; vx = -0x114;
        } else {
            vy = -0x960; vx = -0xF4;
            if (((Scratch_Seesaw *)&saw->scratch)->landspeed >= 0x9C0) {
                vy = -0xA20; vx = -0x80;
            }
        }
        obj->ysp = vy;
        obj->xsp = vx;
        if ((uint16_t)obj->pos.l.x.f.u < (uint16_t)scratch->base_x)
            obj->xsp = (int16_t)-obj->xsp; // left side: travel right
        obj->frame = 1;
        scratch->timer = 32;
        obj->routine += 2;
        BSB_HitBoss(obj, scratch);
        return;
    }

    // Ball rides the seesaw end.
    int idx = saw->frame;
    int16_t off = 40;
    if ((uint16_t)obj->pos.l.x.f.u < (uint16_t)scratch->base_x) {
        off = -40;
        idx += 2;
    }
    obj->pos.l.y.f.u = (int16_t)(scratch->seesaw_y + BSB_SeesawYOffset[idx]);
    obj->pos.l.x.f.u = (int16_t)(scratch->base_x + off);
    obj->pos.l.y.f.l = 0;
    obj->pos.l.x.f.l = 0;
    if (--scratch->timer != 0) {
        BSB_CheckFlicker(obj, scratch);
        return;
    }
    scratch->timer = 32;
    obj->routine = 8; // explode
}

// Is the ball's centre inside Eggman's hitbox? Mirrors the 4 unsigned word
// compares of BossSpikeball_CheckCollision (boss hitbox {-0x18,0x30,-0x18,0x30},
// ball hitbox {8,-16,8,-16}).
static bool BSB_HitsBoss(Object *obj, Object *boss) {
    uint16_t bl = (uint16_t)(boss->pos.l.x.f.u - 0x18);
    uint16_t ox = (uint16_t)(obj->pos.l.x.f.u + 8);
    if (ox < bl)
        return false;
    uint16_t br = (uint16_t)(bl + 0x30);
    ox = (uint16_t)(ox - 16);
    if (br < ox)
        return false;
    uint16_t bt = (uint16_t)(boss->pos.l.y.f.u - 0x18);
    uint16_t oy = (uint16_t)(obj->pos.l.y.f.u + 8);
    if (oy < bt)
        return false;
    uint16_t bb = (uint16_t)(bt + 0x30);
    oy = (uint16_t)(oy - 16);
    return bb >= oy;
}

static void BSB_HitBoss(Object *obj, Scratch_BossSpikeball *scratch) {
    Object *boss = NULL;
    for (int i = 0; i < LEVEL_OBJECTS; i++) {
        Object *o = &level_objects[i];
        if (o->type == ObjId_BossStarLight && o->routine == 2) { // the ship, not its face/flame/pipe
            boss = o;
            break;
        }
    }

    if (boss != NULL && BSB_HitsBoss(obj, boss)) {
        obj->routine += 2;
        scratch->timer = 0;
        boss->col_type = 0;
        if (--boss->col_property == 0) {
            boss->status.o.f.flag7 = true; // defeated
            obj->xsp = 0;
            obj->ysp = 0;
        }
    }

    // Check physics.
    if (obj->ysp < 0) { // still rising
        ObjectFall(obj);
        if (scratch->seesaw_y - 0x2F <= obj->pos.l.y.f.u)
            ObjectFall(obj); // original quirk: extra gravity once below the apex line
        BSB_CheckFlicker(obj, scratch);
        return;
    }

    ObjectFall(obj);
    Object *saw = &objects[scratch->seesaw_index];
    int idx = BSB_YOffsetIndex(obj, scratch, saw);
    if ((int16_t)(scratch->seesaw_y + BSB_SeesawYOffset[idx]) > obj->pos.l.y.f.u) {
        BSB_CheckFlicker(obj, scratch);
        return;
    }
    int8_t side = (obj->xsp < 0) ? 2 : 0;
    scratch->timer = 0;
    BSB_LaunchSonic(obj, scratch, saw, side);
}

static void BSB_Explode(Object *obj, Scratch_BossSpikeball *scratch) {
    obj->type = ObjId_ExplosionBomb; // real id_Explosion (Object 3F)
    obj->routine = 0;
    if (scratch->timer != 32)
        return;

    obj->pos.l.y.f.u = scratch->seesaw_y;
    int n = 0;
    for (int i = 0; i < 4; i++) {
        Object *frag = FindFreeObj();
        if (frag == NULL)
            continue; // the real loop doesn't consume a table entry here either
        frag->type = ObjId_BossSpikeball;
        frag->routine = 0xA;
        frag->mappings = Mappings_BossSpikeball;
        frag->priority = 3;
        frag->tile = TILE_MAP(0, 0, 0, 0, ArtTile_Eggman_Spikeball);
        frag->pos.l.x.f.u = obj->pos.l.x.f.u;
        frag->pos.l.y.f.u = obj->pos.l.y.f.u;
        frag->xsp = BSB_FragSpeed[n][0];
        frag->ysp = BSB_FragSpeed[n][1];
        n++;
        frag->col_type = 0x18 | 0x80; // col_8x8 | col_hurt
        frag->render.f.align_fg = true;
        frag->render.f.on_screen = true; // display immediately
        frag->width_pixels = 24 / 2;
    }
}

// Returns true if the fragment deleted itself.
static bool BSB_MoveFrag(Object *obj, Scratch_BossSpikeball *scratch) {
    SpeedToPos(obj);
    scratch->base_x = obj->pos.l.x.f.u;
    scratch->seesaw_y = obj->pos.l.y.f.u;
    obj->ysp = (int16_t)(obj->ysp + 0x18);
    obj->frame = (uint8_t)((frame_count & 4) >> 2);
    if (!obj->render.f.on_screen) {
        ObjectDelete(obj); // FixBugs forced on: no display-and-delete
        return true;
    }
    return false;
}

void Obj_BossSpikeball(Object *obj) {
    Scratch_BossSpikeball *scratch = (Scratch_BossSpikeball *)&obj->scratch;

    switch (obj->routine) {
    case 0: BSB_Main(obj, scratch); break;
    case 2: BSB_Fall(obj, scratch); break;
    case 4: BSB_Bounce(obj, scratch); break;
    case 6: BSB_HitBoss(obj, scratch); break;
    case 8: BSB_Explode(obj, scratch); break;
    case 0xA:
        if (BSB_MoveFrag(obj, scratch))
            return;
        break;
    }

    if (IS_OFFSCREEN(scratch->base_x)) {
        ObjectDelete(obj);
        return;
    }
    DisplaySprite(obj);
}
