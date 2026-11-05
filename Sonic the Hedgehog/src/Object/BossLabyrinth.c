#include "BossLabyrinth.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"
#include "Constants.h"

#include <string.h>

#include "Level.h"
#include "LevelScroll.h"
#include "MathUtil.h"
#include "Object/Sonic.h"
#include "Palette.h"
#include "Sound.h"

// Animation_Eggman/Mappings_Eggman are owned by BossGreenHill.c -- bare
// externs here, same as BossSpringYard.c.
extern const uint8_t Animation_Eggman[];
extern const uint8_t Mappings_Eggman[];

// Object 77 - Eggman (LZ boss). Ported from s1disasm's
// "_incObj/77 Boss - LZ Main.asm". Eggman just flies up the shaft at the
// end of LZ3: a short diagonal start, a swaying climb whose speed depends on
// how far below him Sonic is, a pause at the top until Sonic catches up,
// then an escape to the right while the camera's right boundary unlocks.
// Hitting him 8 times before he reaches the top makes him flee early
// (explosions trail him, and the climb runs at double speed).
// boss_lz_x = 0x1DE0, boss_lz_y = 0xC0, boss_lz_end = boss_lz_x+0x250
// (real s1disasm _Constants.asm).

#define LZ_BOSS_X 0x1DE0
#define LZ_BOSS_Y 0xC0
#define LZ_BOSS_END (LZ_BOSS_X + 0x250)

static const struct { uint8_t routine, anim; } BLZ_ObjData[3] = {
    { 2, 0 }, // ship body
    { 4, 1 }, // face
    { 6, 7 }, // thruster
};

// Returns true if the ship deleted itself (caller must not touch it).
static bool BossLabyrinth_ShipMain(Sst *self, Own_BossLabyrinth *own);

static void BossLabyrinth_Main(Sst *self, Own_BossLabyrinth *own) {
    // Spawned from DLE_LZ3 into a Sst_Allocate() slot -- start from a clean
    // scratch region so early_defeat/sine_counter/flash can't be stale.
    memset(self->own.u8, 0, sizeof(self->own));

    self->x_pos.f.u = LZ_BOSS_X + 0x30;
    self->y_pos.f.u = LZ_BOSS_Y + 0x500;
    own->boss_x.f.u = self->x_pos.f.u;
    own->boss_x.f.l = 0;
    own->boss_y.f.u = self->y_pos.f.u;
    own->boss_y.f.l = 0;
    self->collision_flags = 0x0F;  // col_48x48 | col_boss
    self->collision_property = 8; // obBossHits
    self->priority = 4 * 0x80;

    uint8_t self_index = (uint8_t)(self - sst);

    for (int i = 0; i < 3; i++) {
        Sst *sub;
        if (i == 0) {
            sub = self;
        } else {
            sub = Sst_AllocateAfter(self);
            if (sub == NULL)
                break;
            // Same stale-slot guard as BossSpringYard.c.
            memset(sub->own.u8, 0, sizeof(sub->own));
            sub->code = Obj_BossLabyrinth;
            sub->x_pos.f.u = self->x_pos.f.u;
            sub->y_pos.f.u = self->y_pos.f.u;
        }

        SST_STATUS_SET(self, SST_STATUS_X_FLIP, false);
        sub->routine_secondary = 0;
        sub->routine = BLZ_ObjData[i].routine;
        sub->anim = BLZ_ObjData[i].anim;
        // Force AnimateSprite's reset guard on the first call (see
        // BossSpringYard.c for why all four fields are needed).
        sub->prev_anim = (uint8_t)(BLZ_ObjData[i].anim + 1);
        sub->anim_frame = 0;
        sub->anim_frame_timer = 0;
        sub->mapping_frame = 0;
        sub->priority = self->priority;
        sub->mappings = Mappings_Eggman;
        sub->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_Eggman);
        sub->render_flags = SST_RENDER_LEVEL;
        sub->width_pixels = 64 / 2;
        ((Own_BossLabyrinth *)sub->own.u8)->parent_index = self_index;
    }

    BossLabyrinth_ShipMain(self, own);
}

// BLZ_ShipUpdate: early-defeat explosions, defeat detection and hit flash.
static void BLZ_ShipUpdate(Sst *self, Own_BossLabyrinth *own) {
    if (own->early_defeat) {
        SstBossDefeated(self); // keep trailing explosions while fleeing
        return;
    }
    if (((self->status & 0x80) != 0)) { // defeated flag, set by the touch response on the last hit
        AddPoints(100); // 1000 points (AddPoints counts in tens, like the score)
        own->early_defeat = 0xFF;
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
    self->collision_flags = 0x0F; // col_48x48 | col_boss -- restore collision
}

static void BLZ_MoveBoss(Sst *self, Own_BossLabyrinth *own) {
    SstBossMove(self, &own->boss_x, &own->boss_y);
    self->y_pos.f.u = own->boss_y.f.u;
    self->x_pos.f.u = own->boss_x.f.u;
    BLZ_ShipUpdate(self, own);
}

// Clamps boss_x/boss_y at a target corner; returns true once both reached.
static bool BLZ_ReachTarget(Sst *self, Own_BossLabyrinth *own, uint16_t tx, int16_t ty) {
    int d0 = -2;
    if ((uint16_t)own->boss_x.f.u >= tx) {
        own->boss_x.f.u = (int16_t)tx;
        self->x_vel = 0;
        d0++;
    }
    if (own->boss_y.f.u <= ty) {
        own->boss_y.f.u = ty;
        self->y_vel = 0;
        d0++;
    }
    return d0 == 0;
}

static void BLZ_ShipStart(Sst *self, Own_BossLabyrinth *own) {
    // Wait for Sonic to enter the shaft.
    if ((uint16_t)player->x_pos.f.u >= LZ_BOSS_X - 0x40) {
        self->y_vel = -0x180; // start rising up
        self->x_vel = 0x60;   // move a little to the right
        self->routine_secondary += 2;
    }
    BLZ_MoveBoss(self, own);
}

static void BLZ_ShipMove1(Sst *self, Own_BossLabyrinth *own) {
    if (BLZ_ReachTarget(self, own, LZ_BOSS_X + 0x68, LZ_BOSS_Y + 0x440)) {
        self->x_vel = 0x140;
        self->y_vel = -0x200;
        self->routine_secondary += 2;
    }
    BLZ_MoveBoss(self, own);
}

static void BLZ_ShipMove2(Sst *self, Own_BossLabyrinth *own) {
    if (BLZ_ReachTarget(self, own, LZ_BOSS_X + 0x90, LZ_BOSS_Y + 0x400)) {
        self->y_vel = -0x180;
        self->routine_secondary += 2;
        own->sine_counter = 0;
    }
    BLZ_MoveBoss(self, own);
}

// The climb: sway side to side, and slow down the further below him Sonic
// falls (full speed within 72px, half up to 112px, quarter up to 152px,
// stopped beyond that).
static void BLZ_ShipMove3(Sst *self, Own_BossLabyrinth *own) {
    if (own->boss_y.f.u <= LZ_BOSS_Y + 0x40) {
        own->boss_y.f.u = LZ_BOSS_Y + 0x40;
        self->x_vel = 0x140; // head right and up towards the top
        self->y_vel = -0x80;
        if (own->early_defeat) {
            self->x_vel = (int16_t)(self->x_vel << 1);
            self->y_vel = (int16_t)(self->y_vel << 1);
        }
        self->routine_secondary += 2;
        BLZ_MoveBoss(self, own);
        return;
    }

    SST_STATUS_SET(self, SST_STATUS_X_FLIP, true); // face right
    own->sine_counter = (uint8_t)(own->sine_counter + 2);
    int16_t sin, cos;
    CalcSine(own->sine_counter, &sin, &cos);
    if (cos < 0)
        SST_STATUS_SET(self, SST_STATUS_X_FLIP, false); // swaying left -- face left

    // X sways around boss_x without disturbing its fractional part.
    self->x_pos.f.u = (int16_t)(own->boss_x.f.u + (sin >> 4));

    int16_t d0 = self->y_vel;
    uint16_t py = (uint16_t)player->y_pos.f.u;
    uint16_t oy = (uint16_t)self->y_pos.f.u;
    if (py >= oy) {
        uint16_t d1 = (uint16_t)(py - oy);
        if (d1 >= 72) {
            d0 >>= 1;
            d1 -= 72;
            if (d1 >= 40) {
                d0 >>= 1;
                d1 -= 40;
                if (d1 >= 40)
                    d0 = 0;
            }
        }
    }

    int32_t step = (int32_t)d0 << 8;
    if (own->early_defeat)
        step += step; // defeated early -- flee at double speed
    own->boss_y.v += step;
    self->y_pos.f.u = own->boss_y.f.u;
    BLZ_ShipUpdate(self, own);
}

static void BLZ_ShipAtTop(Sst *self, Own_BossLabyrinth *own) {
    if (BLZ_ReachTarget(self, own, LZ_BOSS_X + 0x16C, LZ_BOSS_Y)) {
        self->routine_secondary += 2;
        SST_STATUS_SET(self, SST_STATUS_X_FLIP, false); // face left, waiting for Sonic
    }
    BLZ_MoveBoss(self, own);
}

static void BLZ_ShipWait(Sst *self, Own_BossLabyrinth *own) {
    bool escape = own->early_defeat != 0;
    if (!escape && player->x_pos.f.u >= LZ_BOSS_X + 0xE8 && player->y_pos.f.u <= LZ_BOSS_Y + 0x30) {
        own->generic_timer = 50;
        escape = true;
    }
    if (escape) {
        QueueSound1(bgm_LZ);
        lock_screen = false;
        SST_STATUS_SET(self, SST_STATUS_X_FLIP, true); // face right
        self->routine_secondary += 2;
    }
    BLZ_MoveBoss(self, own);
}

static void BLZ_Escape1(Sst *self, Own_BossLabyrinth *own) {
    if (own->early_defeat || --own->generic_timer == 0) {
        own->generic_timer = 0;
        self->x_vel = 0x400; // escape quickly to the right and slightly up
        self->y_vel = -0x40;
        own->early_defeat = 0;
        self->routine_secondary += 2;
    }
    BLZ_MoveBoss(self, own);
}

// Returns true if the ship deleted itself.
static bool BLZ_Escape2(Sst *self, Own_BossLabyrinth *own) {
    if (limit_right2 < LZ_BOSS_END) {
        limit_right2 += 2; // keep unlocking the screen's right boundary
    } else if (!(self->render_flags & SST_RENDER_ON_SCREEN)) {
#ifdef SCP_FIX_BUGS
        // Don't leave the palette stuck on white if Eggman was hit as he
        // was leaving the screen.
        dry_palette[1][1] = 0;
#endif
        // FixBugs forced on (crash-class bug under C rules): real ASM without
        // FixBugs returns into ShipMain and animates/displays the
        // just-deleted slot. That write-after-delete corrupts whatever
        // reuses the slot, so the early return is unconditional here.
        Sst_Delete(self);
        return true;
    }
    BLZ_MoveBoss(self, own);
    return false;
}

static void BLZ_Display(Sst *self, Sst *parent) {
    Sst_Animate(self, Animation_Eggman);
    self->x_pos.f.u = parent->x_pos.f.u;
    self->y_pos.f.u = parent->y_pos.f.u;
    self->status = parent->status;
    uint8_t flip = self->status & 3;
    self->render_flags = (uint8_t)((self->render_flags & ~3) | flip);
    Sst_Draw(self);
}

static bool BossLabyrinth_ShipMain(Sst *self, Own_BossLabyrinth *own) {
    switch (self->routine_secondary) {
    case 0x0: BLZ_ShipStart(self, own); break;
    case 0x2: BLZ_ShipMove1(self, own); break;
    case 0x4: BLZ_ShipMove2(self, own); break;
    case 0x6: BLZ_ShipMove3(self, own); break;
    case 0x8: BLZ_ShipAtTop(self, own); break;
    case 0xA: BLZ_ShipWait(self, own); break;
    case 0xC: BLZ_Escape1(self, own); break;
    case 0xE:
        if (BLZ_Escape2(self, own))
            return true;
        break;
    }

    Sst_Animate(self, Animation_Eggman);
    uint8_t flip = self->status & 3;
    self->render_flags = (uint8_t)((self->render_flags & ~3) | flip);
    Sst_Draw(self);
    return false;
}

static void BossLabyrinth_FaceMain(Sst *self, Own_BossLabyrinth *own) {
    Sst *parent = &sst[own->parent_index];
    if (parent->code != self->code) { // ship already deleted
        Sst_Delete(self);
        return;
    }
    Own_BossLabyrinth *pscratch = (Own_BossLabyrinth *)parent->own.u8;

    uint8_t anim = 1; // facenormal1
#ifdef SCP_FIX_BUGS
    bool defeated = pscratch->early_defeat != 0;
#else
    // Real ASM checks the face's own copy of the flag, which is always 0,
    // so the defeated face never shows.
    bool defeated = own->early_defeat != 0;
    (void)pscratch;
#endif
    if (defeated)
        anim = 0xA; // facedefeat
    else if (parent->collision_flags == 0)
        anim = 5; // facehit
    else if (player->routine >= 4)
        anim = 4; // facelaugh

    self->anim = anim;
    if (parent->routine_secondary == 0xE) { // Escape2
        self->anim = 6; // facepanic
        if (!(self->render_flags & SST_RENDER_ON_SCREEN)) {
            Sst_Delete(self);
            return;
        }
    }
    BLZ_Display(self, parent);
}

static void BossLabyrinth_FlameMain(Sst *self, Own_BossLabyrinth *own) {
    self->anim = 7; // blank
    Sst *parent = &sst[own->parent_index];
    if (parent->code != self->code) { // ship already deleted
        Sst_Delete(self);
        return;
    }

    if (parent->routine_secondary == 0xE) { // Escape2
        self->anim = 0xB; // escapeflame
        if (!(self->render_flags & SST_RENDER_ON_SCREEN)) {
            Sst_Delete(self);
            return;
        }
    }
#ifdef SCP_FIX_BUGS
    // Real ASM skips this check (missing label), so the flame is invisible
    // outside the escape.
    else if (parent->x_vel != 0) {
        self->anim = 8; // flame
    }
#endif
    BLZ_Display(self, parent);
}

void Obj_BossLabyrinth(Sst *self) {
    Own_BossLabyrinth *own = (Own_BossLabyrinth *)self->own.u8;

    switch (self->routine) {
    case 0: BossLabyrinth_Main(self, own); break;
    case 2: BossLabyrinth_ShipMain(self, own); break;
    case 4: BossLabyrinth_FaceMain(self, own); break;
    case 6: BossLabyrinth_FlameMain(self, own); break;
    }
}
