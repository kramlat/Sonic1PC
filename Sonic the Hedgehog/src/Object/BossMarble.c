#include "BossMarble.h"
#include "ObjectSst.h"
#include "Object.h"
#include "Constants.h"

#include "BossFire.h"
#include "SstLoad.h"
#include "LavaBall.h"
#include "Level.h"
#include "LevelScroll.h"
#include "MathUtil.h"
#include "Object/Sonic.h"
#include "Palette.h"
#include "Sound.h"

// Animation_Eggman/Mappings_Eggman are owned by BossGreenHill.c, and
// Mappings_BossItems by BossBall.c -- bare externs here avoid a second
// definition and a link failure, same reasoning as LavaBall.c's own
// comment on Mappings_Fireballs.
extern const uint8_t Animation_Eggman[];
extern const uint8_t Mappings_Eggman[];
extern const uint8_t Mappings_BossItems[];

// Object 73 - Eggman (MZ boss, "Egg Scorcher"). Directly parallels
// BossGreenHill.c's own structure (one physical object doubling as both the
// "controller" and the visible ship, spawning 3 more of itself for the
// face/flame/tube), just with an extra sub-object (the tube decoration) and
// its own movement/lava-drop phases. boss_mz_x = 0x1800, boss_mz_y = 0x210,
// boss_mz_end = boss_mz_x+0x160 (real s1disasm _Constants.asm).

static const struct { uint8_t routine, anim, priority; } BMZ_ObjData[4] = {
    { 2, 0, 4 }, { 4, 1, 4 }, { 6, 7, 4 }, { 8, 0, 3 },
};

static void BossMarble_ShipMain(Sst *self, Own_BossMarble *own);
static void BMZ_ShipUpdate(Sst *self, Own_BossMarble *own);
static void BMZ_Display(Sst *self, Own_BossMarble *own);

static void BossMarble_Main(Sst *self, Own_BossMarble *own) {
    SST_STATUS_SET(self, SST_STATUS_X_FLIP, false); // bclr #0,obStatus(a0) -- start facing left

    uint8_t self_index = (uint8_t)(self - sst);

    for (int i = 0; i < 4; i++) {
        Sst *sub;
        if (i == 0) {
            sub = self;
        } else {
            sub = Sst_AllocateAfter(self);
            if (sub == NULL)
                break;
        }

        sub->routine_secondary = 0;
        sub->routine = BMZ_ObjData[i].routine;
        sub->anim = BMZ_ObjData[i].anim;
        sub->priority = (uint16_t)(BMZ_ObjData[i].priority * 0x80);
        sub->code = Obj_BossMarble;
        sub->x_pos.f.u = self->x_pos.f.u;
        sub->y_pos.f.u = self->y_pos.f.u;
        sub->mappings = Mappings_Eggman;
        sub->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_Eggman);
        sub->render_flags |= SST_RENDER_LEVEL;
        sub->width_pixels = 64 / 2;

        ((Own_BossMarble *)sub->own.u8)->parent_index = self_index;
    }

    own->boss_x.f.u = self->x_pos.f.u;
    own->boss_x.f.l = 0;
    own->boss_y.f.u = self->y_pos.f.u;
    own->boss_y.f.l = 0;
    self->collision_flags = 0x0F;     // col_48x48 | col_boss
    self->collision_property = 8;    // obBossHits

    BossMarble_ShipMain(self, own);
}

static void BMZ_ShipStart(Sst *self, Own_BossMarble *own) {
    uint8_t angle = own->sine_counter;
    own->sine_counter = (uint8_t)(own->sine_counter + 2);

    int16_t sin, cos;
    CalcSine(angle, &sin, &cos);
    self->y_vel = (int16_t)(sin >> 2);
    self->x_vel = (int16_t)-0x100;
    SstBossMove(self, &own->boss_x, &own->boss_y);

    if (own->boss_x.f.u == (int16_t)(0x1800 + 0x110)) { // boss_mz_x+0x110
        self->routine_secondary += 2;
        self->subtype = 0; // clear subtype
        self->x_vel = 0;
    }

    // Unlike GHZ's own bobbing (an overlay applied continuously in
    // ShipUpdate), MZ only uses CalcSine here during entry -- the lava
    // timer below is re-randomized every frame regardless, matching real
    // hardware exactly even though it looks redundant while still moving.
    own->lava_timer = (uint8_t)RandomNumber();

    BMZ_ShipUpdate(self, own);
}

static void BMZ_Defeated(Sst *self, Own_BossMarble *own) {
    AddPoints(100); // 1000 points (AddPoints counts in tens, like the score)
    // Real ASM's own comment here claims this sets "BMZ_Recover", but the
    // value actually written (4) selects BMZ_Explode -- BMZ_Explode is what
    // advances to Recover (6) once its own countdown finishes.
    self->routine_secondary = 4; // BMZ_Explode
    own->generic_timer = 180;
    self->x_vel = 0;
}

static void BMZ_ShipUpdate(Sst *self, Own_BossMarble *own) {
    self->y_pos.f.u = own->boss_y.f.u;
    self->x_pos.f.u = own->boss_x.f.u;

    if (self->routine_secondary >= 4)
        return; // exploding, recovering, or escaping -- skip hit detection

    if (((self->status & 0x80) != 0)) { // defeated flag
        BMZ_Defeated(self, own);
        return;
    }
    if (self->collision_flags != 0)
        return; // not currently hittable

    if (own->flash == 0) {
        own->flash = 0x28;
        QueueSound2(sfx_HitBoss);
    }

    dry_palette[1][1] = (dry_palette[1][1] == 0) ? 0x0EEE : 0;
    if (--own->flash != 0)
        return;
    self->collision_flags = 0x0F; // col_48x48 | col_boss -- restore collision
}

static void BossMarble_MakeLava(Sst *self, Own_BossMarble *own) {
    if (own->lava_timer-- == 0) {
        uint32_t rand = RandomNumber();
        Sst *ball = Sst_SpawnCode(Obj_LavaBall, (int16_t)(0x1800 + 0x78 + ((uint16_t)rand % 0x50)), (int16_t)(0x210 + 0xD8)); // boss_mz_x+0x78, clamped to a 0x50-wide range; boss_mz_y+0xD8
        if (ball != NULL) {
            ball->subtype = 0; // subtype -- LBall_RiseAndFall
            ((Own_LavaBall *)ball->own.u8)->from_boss = 0xFF;
        }
        own->lava_timer = (uint8_t)((RandomNumber() & 0x1F) + 0x40);
    }

    if (((self->status & SST_STATUS_X_FLIP) != 0)) {
        if (own->boss_x.f.u < (int16_t)(0x1800 + 0x110)) // boss_mz_x+0x110
            return;
        own->boss_x.f.u = 0x1800 + 0x110;
    } else {
        if (own->boss_x.f.u > (int16_t)(0x1800 + 0x30)) // boss_mz_x+0x30
            return;
        own->boss_x.f.u = 0x1800 + 0x30;
    }

    self->x_vel = 0;
    self->y_vel = (int16_t)-0x180;
    if ((uint16_t)own->boss_y.f.u < (uint16_t)(0x210 + 0x1C)) // boss_mz_y+0x1C
        self->y_vel = 0x180;
    self->subtype += 2;
}

static void BMZ_ChgDir(Sst *self, Own_BossMarble *own) {
    if (self->x_vel == 0) {
        uint16_t target = (uint16_t)(0x210 + 0x1C); // boss_mz_y+0x1C
        uint16_t by = (uint16_t)own->boss_y.f.u;
        if (by != target) {
            self->y_vel = (by < target) ? 0x40 : (int16_t)-0x40;
            SstBossMove(self, &own->boss_x, &own->boss_y);
            return;
        }
        // Swoop -- start a horizontal+vertical dash in whichever direction
        // Eggman's currently facing.
        self->x_vel = 0x200;
        self->y_vel = 0x100;
        if (!((self->status & SST_STATUS_X_FLIP) != 0))
            self->x_vel = (int16_t)-self->x_vel;
    }

    if (own->flash >= 0x18) {
        // Recently hit -- freeze movement, but still keep dropping lava.
        BossMarble_MakeLava(self, own);
        return;
    }
    SstBossMove(self, &own->boss_x, &own->boss_y);
    self->y_vel -= 4; // curve the swoop into a U shape
    BossMarble_MakeLava(self, own);
}

static void BMZ_DropFire(Sst *self, Own_BossMarble *own) {
    SstBossMove(self, &own->boss_x, &own->boss_y);

    if (own->boss_y.f.u > (int16_t)(0x210 + 0x1C)) // boss_mz_y+0x1C -- still rising
        return;

    if (self->y_vel != 0) {
        self->y_vel = 0;
        own->generic_timer = 80;
        SST_STATUS_SET(self, SST_STATUS_X_FLIP, !((self->status & SST_STATUS_X_FLIP) != 0)); // turn back to the screen bound

        Sst *fire = Sst_SpawnCode(Obj_BossFire, own->boss_x.f.u, (int16_t)(own->boss_y.f.u + 0x18));
        if (fire != NULL) {
            fire->subtype = 1; // subtype -- real fire, not a floor warning
        }
    }

    if (--own->generic_timer != 0)
        return;
    self->subtype += 2;
}

static void BMZ_ShipMove(Sst *self, Own_BossMarble *own) {
    switch (self->subtype & 6) {
    case 0: BMZ_ChgDir(self, own); break;
    case 2: BMZ_DropFire(self, own); break;
    case 4: BMZ_ChgDir(self, own); break;
    case 6: BMZ_DropFire(self, own); break;
    }
    self->subtype &= 6;
    BMZ_ShipUpdate(self, own);
}

static void BMZ_Explode(Sst *self, Own_BossMarble *own) {
    if (--own->generic_timer >= 0) {
        SstBossDefeated(self);
        return;
    }

    SST_STATUS_SET(self, SST_STATUS_X_FLIP, true);
    self->status &= (uint8_t)~0x80; // clear defeated flag (set in ReactToItem)
    self->x_vel = 0;
    self->routine_secondary += 2;
    own->generic_timer = -38;

    if (!boss_status)
        boss_status = 1;
}

static void BMZ_Recover(Sst *self, Own_BossMarble *own) {
    own->generic_timer++;

    if (own->generic_timer == 0) {
        self->y_vel = 0; // done falling
    } else if (own->generic_timer < 0) {
        if ((uint16_t)own->boss_y.f.u >= (uint16_t)(0x210 + 0x60)) { // boss_mz_y+0x60
            self->y_vel = 0;
        } else {
            self->y_vel = (int16_t)(self->y_vel + 0x18); // fall a little
        }
    } else if (own->generic_timer < 48) {
        self->y_vel = (int16_t)(self->y_vel - 8); // rise
    } else if (own->generic_timer == 48) {
        self->y_vel = 0;
        QueueSound1(bgm_MZ);
    } else if (own->generic_timer < 56) {
        // wait
    } else {
        self->routine_secondary += 2; // -> BMZ_Escape
    }

    SstBossMove(self, &own->boss_x, &own->boss_y);
    BMZ_ShipUpdate(self, own);
}

static void BMZ_Escape(Sst *self, Own_BossMarble *own) {
    self->x_vel = 0x500;          // move right quickly
    self->y_vel = (int16_t)-0x40; // move up a little

    if (limit_right2 != (uint16_t)(0x1800 + 0x160)) { // boss_mz_end
        limit_right2 += 2; // keep unlocking the screen bounds
    } else if (SstOutOfRange(self->x_pos.f.u)) {
        SstLoad_Forget(self);
        Sst_Delete(self); // has Eggman left the screen?
        return;
    }

    SstBossMove(self, &own->boss_x, &own->boss_y);
    BMZ_ShipUpdate(self, own);
}

static void BossMarble_ShipMain(Sst *self, Own_BossMarble *own) {
    switch (self->routine_secondary) {
    case 0: BMZ_ShipStart(self, own); break;
    case 2: BMZ_ShipMove(self, own); break;
    case 4: BMZ_Explode(self, own); break;
    case 6: BMZ_Recover(self, own); break;
    case 8: BMZ_Escape(self, own); break;
    }

    Sst_Animate(self, Animation_Eggman);

    uint8_t flip = self->status & 3;
    self->render_flags = (uint8_t)((self->render_flags & ~3) | flip);
    Sst_Draw(self);
}

// Shared tail for the face/flame/tube sub-objects: syncs status/render flip
// bits from the ship and displays. Tube uses this directly (it never
// animates); face/flame go through BMZ_Display, which adds the animation
// step first.
static void BMZ_SetBits(Sst *self, Own_BossMarble *own) {
    Sst *parent = &sst[own->parent_index];
    self->x_pos.f.u = parent->x_pos.f.u;
    self->y_pos.f.u = parent->y_pos.f.u;
    self->status = parent->status;

    uint8_t flip = self->status & 3;
    self->render_flags = (uint8_t)((self->render_flags & ~3) | flip);
    Sst_Draw(self);
}

static void BMZ_Display(Sst *self, Own_BossMarble *own) {
    Sst_Animate(self, Animation_Eggman);
    BMZ_SetBits(self, own);
}

static void BossMarble_FaceMain(Sst *self, Own_BossMarble *own) {
    Sst *parent = &sst[own->parent_index];
    if (parent->code != self->code) { // the ship has been deleted: so are its pieces (they would follow whatever takes its slot, such as the prison capsule's animals)
        Sst_Delete(self);
        return;
    }

    int d0 = parent->routine_secondary;
    int d1 = 1; // facenormal1

    d0 -= 2;
    if (d0 != 0)
        goto checkSpecial;
    if (!(parent->subtype & 2))
        goto checkHitState;
    if (parent->y_vel != 0)
        goto checkHitState;
    d1 = 4; // facelaugh
    goto writeAnim;

checkSpecial:
    d0 -= 2;
    if (d0 < 0)
        goto checkHitState;
    d1 = 0xA; // facedefeat
    goto writeAnim;

checkHitState:
    if (parent->collision_flags != 0)
        goto checkSonicState;
    d1 = 5; // facehit
    goto writeAnim;

checkSonicState:
    if (player->routine < 4)
        goto writeAnim;
    d1 = 4; // facelaugh

writeAnim:
    self->anim = (uint8_t)d1;

    d0 -= 4;
    if (d0 != 0) {
        BMZ_Display(self, own);
        return;
    }
    self->anim = 6; // facepanic
    if (SstOutOfRange(self->x_pos.f.u)) {
        SstLoad_Forget(self);
        Sst_Delete(self);
        return;
    }
    BMZ_Display(self, own);
}

static void BossMarble_FlameMain(Sst *self, Own_BossMarble *own) {
    self->anim = 7; // blank (default invisible state)

    Sst *parent = &sst[own->parent_index];
    if (parent->code != self->code) { // the ship has been deleted: so are its pieces (they would follow whatever takes its slot, such as the prison capsule's animals)
        Sst_Delete(self);
        return;
    }

    if (parent->routine_secondary >= 8) { // Escape
        self->anim = 0xB; // escapeflame -- thruster animation for takeoff
        if (SstOutOfRange(self->x_pos.f.u)) {
            SstLoad_Forget(self);
            Sst_Delete(self);
            return;
        }
        BMZ_Display(self, own);
        return;
    }

    if (parent->x_vel != 0)
        self->anim = 8; // flame

    BMZ_Display(self, own);
}

static void BossMarble_TubeMain(Sst *self, Own_BossMarble *own) {
    Sst *parent = &sst[own->parent_index];
    if (parent->code != self->code) { // the ship has been deleted: so are its pieces (they would follow whatever takes its slot, such as the prison capsule's animals)
        Sst_Delete(self);
        return;
    }

    if (parent->routine_secondary == 8) { // Escape
        if (SstOutOfRange(self->x_pos.f.u)) {
            SstLoad_Forget(self);
            Sst_Delete(self);
            return;
        }
    }

    self->mappings = Mappings_BossItems;
    self->art_tile = TILE_MAP(0, 1, 0, 0, ArtTile_Eggman_Weapons); // | Tile_Pal2
    self->mapping_frame = 4; // tube frame (Boss Items.asm)
    BMZ_SetBits(self, own);
}

void Obj_BossMarble(Sst *self) {
    Own_BossMarble *own = (Own_BossMarble *)self->own.u8;

    switch (self->routine) {
    case 0: BossMarble_Main(self, own); break;
    case 2: BossMarble_ShipMain(self, own); break;
    case 4: BossMarble_FaceMain(self, own); break;
    case 6: BossMarble_FlameMain(self, own); break;
    case 8: BossMarble_TubeMain(self, own); break;
    }
}
