#include "BossGreenHill.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"
#include "Constants.h"

#include "BossBall.h"
#include "Level.h"
#include "LevelScroll.h"
#include "MathUtil.h"
#include "Object/Sonic.h"
#include "Palette.h"
#include "Resource/Animation/Eggman.h"
#include "Resource/Mappings/Eggman.h"
#include "Sound.h"

// Object 3D - Eggman (GHZ boss). One physical object doubles as both the
// "controller" (boss_x/boss_y, generic timer, hit-flash) and the visible
// ship (routine 2) -- it spawns 2 more objects of this same type for the
// face (routine 4) and thruster flame (routine 6), which read the
// controller's state back via parent_index.

static const struct { uint8_t routine; uint8_t anim; } BGHZ_ObjData[3] = {
    { 2, 0 }, { 4, 1 }, { 6, 7 },
};

static void BGHZ_ShipMain(Sst *self, Own_BossGreenHill *own);
static void BGHZ_ShipUpdate(Sst *self, Own_BossGreenHill *own);

static void BGHZ_Main(Sst *self, Own_BossGreenHill *own) {
    uint8_t self_index = (uint8_t)(self - sst);

    for (int i = 0; i < 3; i++) {
        Sst *sub;
        if (i == 0) {
            sub = self;
        } else {
            sub = Sst_AllocateAfter(self);
            if (sub == NULL)
                break;
        }

        sub->routine = BGHZ_ObjData[i].routine;
        sub->code = Obj_BossGreenHill;
        sub->x_pos.f.u = self->x_pos.f.u;
        sub->y_pos.f.u = self->y_pos.f.u;
        sub->mappings = Mappings_Eggman;
        sub->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_Eggman);
        sub->render_flags |= SST_RENDER_LEVEL;
        sub->width_pixels = 64 / 2;
        sub->priority = 3 * 0x80;
        sub->anim = BGHZ_ObjData[i].anim;

        ((Own_BossGreenHill *)sub->own.u8)->parent_index = self_index;
    }

    own->boss_x.f.u = self->x_pos.f.u;
    own->boss_x.f.l = 0;
    own->boss_y.f.u = self->y_pos.f.u;
    own->boss_y.f.l = 0;
    self->collision_flags = 0x0F; // col_48x48 | col_boss
    self->collision_property = 8; // obBossHits

    BGHZ_ShipMain(self, own);
}

static void BGHZ_ShipStart(Sst *self, Own_BossGreenHill *own) {
    self->y_vel = 0x100; // move ship down
    SstBossMove(self, &own->boss_x, &own->boss_y);
    if (own->boss_y.f.u != (int16_t)(0x300 + 0x38)) { // boss_ghz_y+0x38
        BGHZ_ShipUpdate(self, own);
        return;
    }
    self->y_vel = 0;
    self->routine_secondary += 2;
    BGHZ_ShipUpdate(self, own);
}

static void BGHZ_Defeated(Sst *self, Own_BossGreenHill *own) {
    AddPoints(100); // 1000 points (AddPoints counts in tens, like the score)
    self->routine_secondary = 8; // BGHZ_Explode
    own->generic_timer = 0xB3;
}

static void BGHZ_ShipUpdate(Sst *self, Own_BossGreenHill *own) {
    int16_t sin, cos;
    CalcSine(own->sine_counter, &sin, &cos);
    int16_t bob = (int16_t)(sin >> 6);
    self->y_pos.f.u = (int16_t)(own->boss_y.f.u + bob);
    self->x_pos.f.u = own->boss_x.f.u;
    own->sine_counter = (uint8_t)(own->sine_counter + 2);

    if (self->routine_secondary >= 8)
        return; // exploding -- skip hit detection

    if (((self->status & 0x80) != 0)) { // defeated flag
        BGHZ_Defeated(self, own);
        return;
    }
    if (self->collision_flags != 0)
        return; // not currently hittable

    if (own->flash == 0) {
        own->flash = 0x20;
        QueueSound2(sfx_HitBoss);
    }

    // BGHZ_ShipFlash
    dry_palette[1][1] = (dry_palette[1][1] == 0) ? 0x0EEE : 0;
    if (--own->flash != 0)
        return;
    self->collision_flags = 0x0F; // col_48x48 | col_boss -- restore collision
}

static void BGHZ_MakeBall(Sst *self, Own_BossGreenHill *own) {
    self->x_vel = (int16_t)-0x100;
    self->y_vel = (int16_t)-0x40;
    SstBossMove(self, &own->boss_x, &own->boss_y);

    if (own->boss_x.f.u != (int16_t)(0x2960 + 0xA0)) { // boss_ghz_x+0xA0
        BGHZ_ShipUpdate(self, own);
        return;
    }

    self->x_vel = 0;
    self->y_vel = 0;
    self->routine_secondary += 2; // -> BGHZ_ShipMove

    Sst *ball = Sst_Allocate();
    if (ball != NULL) {
        ball->code = Obj_BossBall;
        ball->x_pos.f.u = own->boss_x.f.u;
        ball->y_pos.f.u = own->boss_y.f.u;
        ((Own_BossBall *)ball->own.u8)->parent_index = (uint8_t)(self - sst);
    }
    own->generic_timer = 120 - 1;

    BGHZ_ShipUpdate(self, own);
}

static void BGHZ_Reverse(Sst *self, Own_BossGreenHill *own) {
    if (!((self->status & SST_STATUS_X_FLIP) != 0))
        self->x_vel = (int16_t)-self->x_vel;
    BGHZ_ShipUpdate(self, own);
}

static void BGHZ_ShipMove(Sst *self, Own_BossGreenHill *own) {
    if (--own->generic_timer >= 0) {
        BGHZ_Reverse(self, own);
        return;
    }
    self->routine_secondary += 2; // -> BGHZ_ChgDir
    own->generic_timer = 0x40 - 1;
    self->x_vel = 0x100;
    if (own->boss_x.f.u != (int16_t)(0x2960 + 0xA0)) {
        BGHZ_Reverse(self, own);
        return;
    }
    own->generic_timer = (0x40 * 2) - 1;
    self->x_vel = 0x40;
    BGHZ_Reverse(self, own);
}

static void BGHZ_ChgDir(Sst *self, Own_BossGreenHill *own) {
    if (--own->generic_timer >= 0) {
        SstBossMove(self, &own->boss_x, &own->boss_y);
        BGHZ_ShipUpdate(self, own);
        return;
    }
    SST_STATUS_SET(self, SST_STATUS_X_FLIP, !((self->status & SST_STATUS_X_FLIP) != 0));
    own->generic_timer = 64 - 1;
    self->routine_secondary -= 2; // -> BGHZ_ShipMove
    self->x_vel = 0;
    BGHZ_ShipUpdate(self, own);
}

static void BGHZ_Explode(Sst *self, Own_BossGreenHill *own) {
    if (--own->generic_timer >= 0) {
        SstBossDefeated(self);
        return;
    }

    SST_STATUS_SET(self, SST_STATUS_X_FLIP, true);
    self->status &= (uint8_t)~0x80; // clear defeated flag (set in ReactToItem)
    self->x_vel = 0;
    self->routine_secondary += 2; // -> BGHZ_Recover
    own->generic_timer = -38;

    if (!boss_status)
        boss_status = 1;
}

static void BGHZ_Recover(Sst *self, Own_BossGreenHill *own) {
    own->generic_timer++;

    if (own->generic_timer == 0) {
        self->y_vel = 0; // done falling
    } else if (own->generic_timer < 0) {
        self->y_vel = (int16_t)(self->y_vel + 0x18); // fall a little
    } else if (own->generic_timer < 0x30) {
        self->y_vel = (int16_t)(self->y_vel - 8); // rise
    } else if (own->generic_timer == 0x30) {
        self->y_vel = 0;
        QueueSound1(bgm_GHZ);
    } else if (own->generic_timer < 0x38) {
        // wait
    } else {
        self->routine_secondary += 2; // -> BGHZ_Escape
    }

    SstBossMove(self, &own->boss_x, &own->boss_y);
    BGHZ_ShipUpdate(self, own);
}

static void BGHZ_Escape(Sst *self, Own_BossGreenHill *own) {
    self->x_vel = 0x400; // move right quickly
    self->y_vel = (int16_t)-0x40; // move up a little

    if (limit_right2 != (uint16_t)(0x2960 + 0x160)) { // boss_ghz_end
        limit_right2 += 2; // keep unlocking the screen bounds
    } else if (SstOutOfRange(self->x_pos.f.u)) {
        SstLoad_Forget(self);
        Sst_Delete(self); // has Eggman left the screen?
        return;
    }

    SstBossMove(self, &own->boss_x, &own->boss_y);
    BGHZ_ShipUpdate(self, own);
}

static void BGHZ_ShipMain(Sst *self, Own_BossGreenHill *own) {
    switch (self->routine_secondary) {
    case 0: BGHZ_ShipStart(self, own); break;
    case 2: BGHZ_MakeBall(self, own); break;
    case 4: BGHZ_ShipMove(self, own); break;
    case 6: BGHZ_ChgDir(self, own); break;
    case 8: BGHZ_Explode(self, own); break;
    case 10: BGHZ_Recover(self, own); break;
    case 12: BGHZ_Escape(self, own); break;
    }

    Sst_Animate(self, Animation_Eggman);

    uint8_t flip = self->status & 3;
    self->render_flags = (uint8_t)((self->render_flags & ~3) | flip);
    Sst_Draw(self);
}

// Shared display tail for the face and flame sub-objects: syncs
// position/status from the ship, animates, and displays.
static void BGHZ_Display(Sst *self, Own_BossGreenHill *own) {
    Sst *parent = &sst[own->parent_index];
    self->x_pos.f.u = parent->x_pos.f.u;
    self->y_pos.f.u = parent->y_pos.f.u;
    self->status = parent->status;

    Sst_Animate(self, Animation_Eggman);

    uint8_t flip = self->status & 3;
    self->render_flags = (uint8_t)((self->render_flags & ~3) | flip);
    Sst_Draw(self);
}

static void BGHZ_FaceMain(Sst *self, Own_BossGreenHill *own) {
    Sst *parent = &sst[own->parent_index];
    if (parent->code != self->code) { // the ship has been deleted: so are its pieces (they would follow whatever takes its slot, such as the prison capsule's animals)
        Sst_Delete(self);
        return;
    }
    Own_BossGreenHill *pscratch = (Own_BossGreenHill *)parent->own.u8;

    int d0 = parent->routine_secondary;
    int d1 = 1; // facenormal1

    d0 -= 4;
    if (d0 != 0)
        goto checkSpecial;
    if (pscratch->boss_x.f.u != (int16_t)(0x2960 + 0xA0))
        goto checkHitState;
    d1 = 4; // facelaugh

checkSpecial:
    d0 -= 6;
    if (d0 < 0)
        goto checkHitState;
    d1 = 0xA; // facedefeat
    goto writeAnim;

checkHitState:
    if (parent->collision_flags == 0) {
        d1 = 5; // facehit
        goto writeAnim;
    }
    if (player->routine < 4)
        goto writeAnim;
    d1 = 4; // facelaugh

writeAnim:
    self->anim = (uint8_t)d1;

    d0 -= 2;
    if (d0 != 0) {
        BGHZ_Display(self, own);
        return;
    }
    self->anim = 6; // facepanic
    if (SstOutOfRange(self->x_pos.f.u)) {
        SstLoad_Forget(self);
        Sst_Delete(self);
        return;
    }
    BGHZ_Display(self, own);
}

static void BGHZ_FlameMain(Sst *self, Own_BossGreenHill *own) {
    self->anim = 7; // blank (default invisible state)

    Sst *parent = &sst[own->parent_index];
    if (parent->code != self->code) { // the ship has been deleted: so are its pieces (they would follow whatever takes its slot, such as the prison capsule's animals)
        Sst_Delete(self);
        return;
    }

    if (parent->routine_secondary == 12) { // Escape
        self->anim = 0xB; // escapeflame -- thruster animation for takeoff
        if (SstOutOfRange(self->x_pos.f.u)) {
            SstLoad_Forget(self);
            Sst_Delete(self);
            return;
        }
        BGHZ_Display(self, own);
        return;
    }

    if (parent->x_vel != 0)
        self->anim = 8; // flame

    BGHZ_Display(self, own);
}

void Obj_BossGreenHill(Sst *self) {
    Own_BossGreenHill *own = (Own_BossGreenHill *)self->own.u8;

    switch (self->routine) {
    case 0: BGHZ_Main(self, own); break;
    case 2: BGHZ_ShipMain(self, own); break;
    case 4: BGHZ_FaceMain(self, own); break;
    case 6: BGHZ_FlameMain(self, own); break;
    }
}
