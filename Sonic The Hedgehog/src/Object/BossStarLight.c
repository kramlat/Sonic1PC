#include "BossStarLight.h"
#include "SstLoad.h"
#include "Object/Explosion.h"
#include "ObjectSst.h"
#include "Object.h"
#include "Constants.h"

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

static bool BossStarLight_ShipMain(Sst *self, Own_BossStarLight *own);

// The real ASM scans only part of object RAM for seesaws and the boss, and
// pointers into that range; the port's slots differ, so FixBugs-style full
// scans of the level objects are forced on.
static void BossStarLight_Main(Sst *self, Own_BossStarLight *own) {
    memset(self->own.u8, 0, sizeof(self->own));

    self->x_pos.f.u = SLZ_BOSS_X + 0x188;
    self->y_pos.f.u = SLZ_BOSS_Y + 0x18;
    own->boss_x.f.u = self->x_pos.f.u;
    own->boss_x.f.l = 0;
    own->boss_y.f.u = self->y_pos.f.u;
    own->boss_y.f.l = 0;
    self->collision_flags = SLZ_COL_BOSS;
    self->collision_property = 8; // obBossHits

    uint8_t self_index = (uint8_t)(self - sst);

    for (int i = 0; i < 4; i++) {
        Sst *sub;
        if (i == 0) {
            sub = self;
        } else {
            sub = Sst_AllocateAfter(self);
            if (sub == NULL)
                break;
            memset(sub->own.u8, 0, sizeof(sub->own));
            sub->code = Obj_BossStarLight;
            sub->x_pos.f.u = self->x_pos.f.u;
            sub->y_pos.f.u = self->y_pos.f.u;
        }

        SST_STATUS_SET(self, SST_STATUS_X_FLIP, false);
        sub->routine_secondary = 0;
        sub->routine = BSLZ_ObjData[i].routine;
        sub->anim = BSLZ_ObjData[i].anim;
        sub->priority = (uint16_t)(BSLZ_ObjData[i].priority * 0x80);
        // Force AnimateSprite's reset guard on the first call (see BossSpringYard.c).
        sub->prev_anim = (uint8_t)(BSLZ_ObjData[i].anim + 1);
        sub->anim_frame = 0;
        sub->anim_frame_timer = 0;
        sub->mapping_frame = 0;
        sub->mappings = Mappings_Eggman;
        sub->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_Eggman);
        sub->render_flags = SST_RENDER_LEVEL;
        sub->width_pixels = 64 / 2;
        ((Own_BossStarLight *)sub->own.u8)->parent_index = self_index;
    }

    // Collect the seesaws without a ball (subtype != 0): the three drop points.
    own->seesaw_count = 0;
    for (int i = 0; i < SST_COUNT && own->seesaw_count < 3; i++) {
        Sst *saw = &sst[i];
        if (saw->code == Obj_Seesaw && saw->subtype != 0)
            own->seesaws[own->seesaw_count++] = (uint8_t)(saw - sst);
    }

    BossStarLight_ShipMain(self, own);
}

static void BSLZ_StatusUpdate(Sst *self, Own_BossStarLight *own) {
    if (self->routine_secondary >= 6)
        return; // exploding or beyond
    if (((self->status & 0x80) != 0)) { // defeated flag, set by the spike ball on the last hit
        AddPoints(100); // 1000 points (AddPoints counts in tens, like the score)
        self->routine_secondary = 6;
        own->generic_timer = 120;
        self->x_vel = 0;
        return;
    }
    if (self->collision_flags != 0)
        return; // not currently being hit

    if (own->flash == 0) {
        own->flash = 0x20;
        QueueSound2(sfx_HitBoss);
    }

    dry_palette[1][1] = (dry_palette[1][1] == 0) ? 0x0EEE : 0;
    if (--own->flash != 0)
        return;
    self->collision_flags = SLZ_COL_BOSS; // restore collision
}

// BSLZ_ShipUpdate: move, bob up and down, then status.
static void BSLZ_ShipUpdate(Sst *self, Own_BossStarLight *own) {
    SstBossMove(self, &own->boss_x, &own->boss_y);
    int16_t sin, cos;
    CalcSine(own->sine_counter, &sin, &cos);
    own->sine_counter = (uint8_t)(own->sine_counter + 2);
    self->y_pos.f.u = (int16_t)((sin >> 6) + own->boss_y.f.u);
    self->x_pos.f.u = own->boss_x.f.u;
    BSLZ_StatusUpdate(self, own);
}

// BSLZ_MoveUpdate: move without bobbing, then status.
static void BSLZ_MoveUpdate(Sst *self, Own_BossStarLight *own) {
    SstBossMove(self, &own->boss_x, &own->boss_y);
    self->y_pos.f.u = own->boss_y.f.u;
    self->x_pos.f.u = own->boss_x.f.u;
    BSLZ_StatusUpdate(self, own);
}

static void BSLZ_ShipStart(Sst *self, Own_BossStarLight *own) {
    self->x_vel = -0x100;
    if ((uint16_t)own->boss_x.f.u < SLZ_BOSS_X + 0x120)
        self->routine_secondary += 2;
    BSLZ_ShipUpdate(self, own);
}

static void BSLZ_ShipMove(Sst *self, Own_BossStarLight *own) {
    int16_t bx = own->boss_x.f.u;
    self->x_vel = 0x200;
    int16_t offset = 0x28;
    if (!((self->status & SST_STATUS_X_FLIP) != 0)) {
        self->x_vel = -0x200;
        offset = -0x28;
        if (bx <= SLZ_BOSS_X + 8)
            SST_STATUS_SET(self, SST_STATUS_X_FLIP, !((self->status & SST_STATUS_X_FLIP) != 0));
    } else if (bx >= SLZ_BOSS_X + 0x138) {
        SST_STATUS_SET(self, SST_STATUS_X_FLIP, !((self->status & SST_STATUS_X_FLIP) != 0));
    }

    // Is a seesaw's drop point (its X +/- 0x28 on the side we're heading
    // for) directly under us, with Sonic not standing on it?
    own->pick = -1;
    for (int i = 0; i < own->seesaw_count; i++) {
        Sst *saw = &sst[own->seesaws[i]];
        if (((saw->status & SST_STATUS_PLAYER_STAND) != 0))
            continue;
        if ((int16_t)(saw->x_pos.f.u + offset) == self->x_pos.f.u) {
            own->pick = (int8_t)i;
            self->routine_secondary += 2;
            own->generic_timer = 40;
            break;
        }
    }
    BSLZ_ShipUpdate(self, own);
}

static void BSLZ_MakeBall(Sst *self, Own_BossStarLight *own) {
    if (own->generic_timer == 40) {
        bool abort_drop = own->pick < 0;
        Sst *saw = NULL;
        uint8_t saw_index = 0;
        if (!abort_drop) {
            saw_index = own->seesaws[own->pick];
            saw = &sst[saw_index];
            // Is there already a ball linked to this seesaw?
            for (int i = 0; i < SST_COUNT; i++) {
                Sst *ball = &sst[i];
                if (ball->code == Obj_BossSpikeball && ball->routine != 0xA &&
                    ((Own_BossSpikeball *)ball->own.u8)->seesaw_index == saw_index) {
                    abort_drop = true;
                    break;
                }
            }
        }
        if (!abort_drop) {
            Sst *ball = Sst_Allocate();
            if (ball == NULL) {
                abort_drop = true;
            } else {
                ball->code = Obj_BossSpikeball;
                ball->x_pos.f.u = self->x_pos.f.u;
                ball->y_pos.f.u = (int16_t)(self->y_pos.f.u + 0x20); // out of the ball launcher
                ball->status = saw->status;
                ((Own_BossSpikeball *)ball->own.u8)->seesaw_index = saw_index;
            }
        }
        if (abort_drop) {
            self->routine_secondary -= 2;
            BSLZ_ShipUpdate(self, own);
            return;
        }
    }

    if (--own->generic_timer == 0) {
        self->routine_secondary -= 2; // time to start moving again
        BSLZ_ShipUpdate(self, own);
        return;
    }
    BSLZ_StatusUpdate(self, own);
}

static void BSLZ_Explode(Sst *self, Own_BossStarLight *own) {
    if ((int8_t)--own->generic_timer >= 0) {
        SstBossDefeated(self);
        return;
    }
    self->routine_secondary += 2;
    self->y_vel = 0;
    self->x_vel = 0;
    SST_STATUS_SET(self, SST_STATUS_X_FLIP, true); // face right
    self->status &= (uint8_t)~0x80; // clear the defeated flag
    own->generic_timer = (uint8_t)-24;
    if (boss_status == 0)
        boss_status = 1; // defeated, capsule not opened yet
    BSLZ_StatusUpdate(self, own);
}

static void BSLZ_Recover(Sst *self, Own_BossStarLight *own) {
    int8_t t = (int8_t)(++own->generic_timer);
    if (t == 0) {
        self->y_vel = 0; // done falling
    } else if (t < 0) {
        self->y_vel = (int16_t)(self->y_vel + 0x18); // fall a little faster
    } else if (t < 32) {
        self->y_vel = (int16_t)(self->y_vel - 8); // slow down, then rise
    } else if (t == 32) {
        self->y_vel = 0;
        QueueSound1(bgm_SLZ);
    } else if (t >= 42) {
        self->routine_secondary += 2;
    }
    BSLZ_MoveUpdate(self, own);
}

// Returns true if the ship deleted itself.
static bool BSLZ_Escape(Sst *self, Own_BossStarLight *own) {
    self->x_vel = 0x400;
    self->y_vel = -0x40;
    if (limit_right2 < SLZ_BOSS_END) {
        limit_right2 += 2; // keep unlocking the screen's right boundary
    } else if (!(self->render_flags & SST_RENDER_ON_SCREEN)) {
        // FixBugs forced on (crash-class display-and-delete under C rules).
        Sst_Delete(self);
        return true;
    }
    BSLZ_ShipUpdate(self, own);
    return false;
}

static void BSLZ_Display(Sst *self, Sst *parent) {
    self->x_pos.f.u = parent->x_pos.f.u;
    self->y_pos.f.u = parent->y_pos.f.u;
    self->status = parent->status;
    uint8_t flip = self->status & 3;
    self->render_flags = (uint8_t)((self->render_flags & ~3) | flip);
    Sst_Draw(self);
}

static bool BossStarLight_ShipMain(Sst *self, Own_BossStarLight *own) {
    switch (self->routine_secondary) {
    case 0x0: BSLZ_ShipStart(self, own); break;
    case 0x2: BSLZ_ShipMove(self, own); break;
    case 0x4: BSLZ_MakeBall(self, own); break;
    case 0x6: BSLZ_Explode(self, own); break;
    case 0x8: BSLZ_Recover(self, own); break;
    case 0xA:
        if (BSLZ_Escape(self, own))
            return true;
        break;
    }

    Sst_Animate(self, Animation_Eggman);
    uint8_t flip = self->status & 3;
    self->render_flags = (uint8_t)((self->render_flags & ~3) | flip);
    Sst_Draw(self);
    return false;
}

static void BossStarLight_FaceMain(Sst *self, Own_BossStarLight *own) {
    Sst *parent = &sst[own->parent_index];
    if (parent->code != self->code) { // ship already deleted
        Sst_Delete(self);
        return;
    }

    uint8_t phase = parent->routine_secondary;
    uint8_t anim = 1; // facenormal1
    if (phase >= 6)
        anim = 0xA; // facedefeat
    else if (parent->collision_flags == 0)
        anim = 5; // facehit
    else if (player->routine >= 4)
        anim = 4; // facelaugh

    self->anim = anim;
    if (phase == 0xA) {
        self->anim = 6; // facepanic
        if (!(self->render_flags & SST_RENDER_ON_SCREEN)) {
            Sst_Delete(self);
            return;
        }
    }
    Sst_Animate(self, Animation_Eggman);
    BSLZ_Display(self, parent);
}

static void BossStarLight_FlameMain(Sst *self, Own_BossStarLight *own) {
    self->anim = 8;
    Sst *parent = &sst[own->parent_index];
    if (parent->code != self->code) {
        Sst_Delete(self);
        return;
    }

    uint8_t phase = parent->routine_secondary;
    if (phase == 0xA) {
        if (!(self->render_flags & SST_RENDER_ON_SCREEN)) {
            Sst_Delete(self);
            return;
        }
        self->anim = 0xB; // escape flame
    } else if (phase >= 4 && phase <= 8) {
        self->anim = 7; // not moving, hide the flame
    }
    Sst_Animate(self, Animation_Eggman);
    BSLZ_Display(self, parent);
}

static void BossStarLight_PipeMain(Sst *self, Own_BossStarLight *own) {
    Sst *parent = &sst[own->parent_index];
    if (parent->code != self->code) {
        Sst_Delete(self);
        return;
    }
    if (parent->routine_secondary == 0xA && !(self->render_flags & SST_RENDER_ON_SCREEN)) {
        Sst_Delete(self);
        return;
    }
    self->mappings = Mappings_BossItems;
    self->art_tile = TILE_MAP(0, 1, 0, 0, ArtTile_Eggman_Weapons); // | Tile_Pal2
    self->mapping_frame = 3;           // wide pipe
    BSLZ_Display(self, parent);
}

void Obj_BossStarLight(Sst *self) {
    Own_BossStarLight *own = (Own_BossStarLight *)self->own.u8;

    switch (self->routine) {
    case 0: BossStarLight_Main(self, own); break;
    case 2: BossStarLight_ShipMain(self, own); break;
    case 4: BossStarLight_FaceMain(self, own); break;
    case 6: BossStarLight_FlameMain(self, own); break;
    case 8: BossStarLight_PipeMain(self, own); break;
    }
}

// ---------------------------------------------------------------------------
// Object 7B - spike balls
// ---------------------------------------------------------------------------

static void BSB_CheckFlicker(Sst *self, Own_BossSpikeball *own) {
    if (own->timer == 120)
        own->delay = 5;
    if (own->timer == 60)
        own->delay = 2;
    if ((int8_t)--own->time_frame > 0)
        return;
    self->mapping_frame ^= 1;
    own->time_frame = own->delay;
}

static int BSB_YOffsetIndex(Sst *self, Own_BossSpikeball *own, Sst *saw) {
    int idx = saw->mapping_frame;
    if ((uint16_t)self->x_pos.f.u < (uint16_t)own->base_x)
        idx += 2; // left side of the seesaw
    return idx;
}

// Tells the seesaw which side the ball is on and, if Sonic is on the
// opposite end, launches him. Then moves on to the next routine.
static void BSB_LaunchSonic(Sst *self, Own_BossSpikeball *own, Sst *saw, int8_t side) {
    ((Own_Seesaw *)saw->own.u8)->state = side; // tell the seesaw which side the ball is on
    own->side = side;

    if (side != (int8_t)saw->mapping_frame) { // is the seesaw NOT already lowered on this side?
        bool was_standing = ((saw->status & SST_STATUS_PLAYER_STAND) != 0);
        saw->status &= (uint8_t)~SST_STATUS_PLAYER_STAND;
        if (was_standing) {
            saw->routine_secondary = 0;
            saw->routine = 2;
            player->y_vel = (int16_t)-self->y_vel;
            if (saw->mapping_frame == 1)
                player->y_vel >>= 1; // seesaw was flat -- half the launch
            player->status |= SST_PSTATUS_IN_AIR;
            Sonic_CancelSpindash(); // same drop-dash fix as the seesaw's own ball
            player->status &= (uint8_t)~SST_PSTATUS_OBJECT_STAND;
            ((Scratch_Sonic *)player->own.u8)->jumping = 0;
            Sonic_ChkRoll(player);
            player->routine = 2; // Sonic_Control
            QueueSound2(sfx_Spring);
        }
    }

    self->x_vel = 0;
    self->y_vel = 0;
    self->routine += 2;
    BSB_CheckFlicker(self, own);
}

static void BSB_HitBoss(Sst *self, Own_BossSpikeball *own);

static void BSB_Fall(Sst *self, Own_BossSpikeball *own) {
    Sst_MoveSprite(self);
    Sst *saw = &sst[own->seesaw_index];
    int idx = BSB_YOffsetIndex(self, own, saw);
    if ((int16_t)(own->seesaw_y + BSB_SeesawYOffset[idx]) > self->y_pos.f.u)
        return; // hasn't reached the seesaw yet

    int8_t side = ((self->status & SST_STATUS_X_FLIP) != 0) ? 0 : 2;
    own->timer = 240;
    own->delay = 10;
    own->time_frame = 10;
    BSB_LaunchSonic(self, own, saw, side);
}

static void BSB_Main(Sst *self, Own_BossSpikeball *own) {
    self->mappings = Mappings_SeesawBall;
    self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_Eggman_Spikeball);
    self->mapping_frame = 1;
    self->render_flags |= SST_RENDER_LEVEL;
    self->priority = 4 * 0x80;
    self->collision_flags = 0x17 | 0x80; // col_16x16 | col_hurt
    self->width_pixels = 24 / 2;

    Sst *saw = &sst[own->seesaw_index];
    own->base_x = saw->x_pos.f.u;
    own->seesaw_y = saw->y_pos.f.u;
    SST_STATUS_SET(self, SST_STATUS_X_FLIP, true);
    own->side = 0;
    if (self->x_pos.f.u <= saw->x_pos.f.u) {
        SST_STATUS_SET(self, SST_STATUS_X_FLIP, false);
        own->side = 2;
    }
    self->routine += 2;
    BSB_Fall(self, own);
}

static void BSB_Bounce(Sst *self, Own_BossSpikeball *own) {
    Sst *saw = &sst[own->seesaw_index];
    int8_t saw_side = ((Own_Seesaw *)saw->own.u8)->state;
    int diff = own->side - saw_side;

    if (diff != 0) {
        // The seesaw tipped under the ball: launch it.
        if (diff < 0)
            diff = -diff;
        int16_t vy, vx;
        if (diff == 1) { // seesaw flat
            vy = -0x818; vx = -0x114;
        } else {
            vy = -0x960; vx = -0xF4;
            if (((Own_Seesaw *)saw->own.u8)->landspeed >= 0x9C0) {
                vy = -0xA20; vx = -0x80;
            }
        }
        self->y_vel = vy;
        self->x_vel = vx;
        if ((uint16_t)self->x_pos.f.u < (uint16_t)own->base_x)
            self->x_vel = (int16_t)-self->x_vel; // left side: travel right
        self->mapping_frame = 1;
        own->timer = 32;
        self->routine += 2;
        BSB_HitBoss(self, own);
        return;
    }

    // Ball rides the seesaw end.
    int idx = saw->mapping_frame;
    int16_t off = 40;
    if ((uint16_t)self->x_pos.f.u < (uint16_t)own->base_x) {
        off = -40;
        idx += 2;
    }
    self->y_pos.f.u = (int16_t)(own->seesaw_y + BSB_SeesawYOffset[idx]);
    self->x_pos.f.u = (int16_t)(own->base_x + off);
    self->y_pos.f.l = 0;
    self->x_pos.f.l = 0;
    if (--own->timer != 0) {
        BSB_CheckFlicker(self, own);
        return;
    }
    own->timer = 32;
    self->routine = 8; // explode
}

// Is the ball's centre inside Eggman's hitbox? Mirrors the 4 unsigned word
// compares of BossSpikeball_CheckCollision (boss hitbox {-0x18,0x30,-0x18,0x30},
// ball hitbox {8,-16,8,-16}).
static bool BSB_HitsBoss(Sst *self, Sst *boss) {
    uint16_t bl = (uint16_t)(boss->x_pos.f.u - 0x18);
    uint16_t ox = (uint16_t)(self->x_pos.f.u + 8);
    if (ox < bl)
        return false;
    uint16_t br = (uint16_t)(bl + 0x30);
    ox = (uint16_t)(ox - 16);
    if (br < ox)
        return false;
    uint16_t bt = (uint16_t)(boss->y_pos.f.u - 0x18);
    uint16_t oy = (uint16_t)(self->y_pos.f.u + 8);
    if (oy < bt)
        return false;
    uint16_t bb = (uint16_t)(bt + 0x30);
    oy = (uint16_t)(oy - 16);
    return bb >= oy;
}

static void BSB_HitBoss(Sst *self, Own_BossSpikeball *own) {
    Sst *boss = NULL;
    for (int i = 0; i < SST_COUNT; i++) {
        Sst *o = &sst[i];
        if (o->code == Obj_BossStarLight && o->routine == 2) { // the ship, not its face/flame/pipe
            boss = o;
            break;
        }
    }

    if (boss != NULL && BSB_HitsBoss(self, boss)) {
        self->routine += 2;
        own->timer = 0;
        boss->collision_flags = 0;
        if (--boss->collision_property == 0) {
            boss->status |= 0x80; // defeated
            self->x_vel = 0;
            self->y_vel = 0;
        }
    }

    // Check physics.
    if (self->y_vel < 0) { // still rising
        Sst_MoveSprite(self);
        if (own->seesaw_y - 0x2F <= self->y_pos.f.u)
            Sst_MoveSprite(self); // original quirk: extra gravity once below the apex line
        BSB_CheckFlicker(self, own);
        return;
    }

    Sst_MoveSprite(self);
    Sst *saw = &sst[own->seesaw_index];
    int idx = BSB_YOffsetIndex(self, own, saw);
    if ((int16_t)(own->seesaw_y + BSB_SeesawYOffset[idx]) > self->y_pos.f.u) {
        BSB_CheckFlicker(self, own);
        return;
    }
    int8_t side = (self->x_vel < 0) ? 2 : 0;
    own->timer = 0;
    BSB_LaunchSonic(self, own, saw, side);
}

static void BSB_Explode(Sst *self, Own_BossSpikeball *own) {
    self->code = Obj_ExplosionBomb; // real id_Explosion (Object 3F)
    self->routine = 0;
    if (own->timer != 32)
        return;

    self->y_pos.f.u = own->seesaw_y;
    int n = 0;
    for (int i = 0; i < 4; i++) {
        Sst *frag = Sst_Allocate();
        if (frag == NULL)
            continue; // the real loop doesn't consume a table entry here either
        frag->code = Obj_BossSpikeball;
        frag->routine = 0xA;
        frag->mappings = Mappings_BossSpikeball;
        frag->priority = 3 * 0x80;
        frag->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_Eggman_Spikeball);
        frag->x_pos.f.u = self->x_pos.f.u;
        frag->y_pos.f.u = self->y_pos.f.u;
        frag->x_vel = BSB_FragSpeed[n][0];
        frag->y_vel = BSB_FragSpeed[n][1];
        n++;
        frag->collision_flags = 0x18 | 0x80; // col_8x8 | col_hurt
        frag->render_flags |= SST_RENDER_LEVEL;
        frag->render_flags |= SST_RENDER_ON_SCREEN; // display immediately
        frag->width_pixels = 24 / 2;
    }
}

// Returns true if the fragment deleted itself.
static bool BSB_MoveFrag(Sst *self, Own_BossSpikeball *own) {
    Sst_MoveSprite2(self);
    own->base_x = self->x_pos.f.u;
    own->seesaw_y = self->y_pos.f.u;
    self->y_vel = (int16_t)(self->y_vel + 0x18);
    self->mapping_frame = (uint8_t)((frame_count & 4) >> 2);
    if (!(self->render_flags & SST_RENDER_ON_SCREEN)) {
        Sst_Delete(self); // FixBugs forced on: no display-and-delete
        return true;
    }
    return false;
}

void Obj_BossSpikeball(Sst *self) {
    Own_BossSpikeball *own = (Own_BossSpikeball *)self->own.u8;

    switch (self->routine) {
    case 0: BSB_Main(self, own); break;
    case 2: BSB_Fall(self, own); break;
    case 4: BSB_Bounce(self, own); break;
    case 6: BSB_HitBoss(self, own); break;
    case 8: BSB_Explode(self, own); break;
    case 0xA:
        if (BSB_MoveFrag(self, own))
            return;
        break;
    }

    SstOnScreenTest2(self, own->base_x);
}
