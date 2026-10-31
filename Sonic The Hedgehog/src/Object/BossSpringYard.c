#include "BossSpringYard.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"
#include "Constants.h"

#include <string.h>

#include "BossBlock.h"
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

// Object 75 - Eggman (SYZ boss, "Egg Stinger"). Directly parallels
// BossGreenHill.c/BossMarble.c's own structure (one physical object doubling
// as both the "controller" and the visible ship, spawning 3 more of itself
// for the face/flame/grabbing-spike), but with its own unique attack: the
// spike descends onto the arena's floor blocks (object 76, see
// BossBlock.c), grabs one, lifts and shakes it, then drops it to shatter.
// boss_syz_x = 0x2C00, boss_syz_y = 0x4CC, boss_syz_end = boss_syz_x+0x140
// (real s1disasm _Constants.asm).

#define SYZ_BOSS_X 0x2C00
#define SYZ_BOSS_Y 0x4CC
#define SYZ_BOSS_END (SYZ_BOSS_X + 0x140)

static const struct { uint8_t routine, anim, priority; } BSYZ_ObjData[4] = {
    { 2, 0, 5 }, // ship
    { 4, 1, 5 }, // face
    { 6, 7, 5 }, // flame
    { 8, 0, 5 }, // spike
};

static void BossSpringYard_ShipMain(Sst *self, Own_BossSpringYard *own);
static void BSYZ_CalcSine(Sst *self, Own_BossSpringYard *own);
static void BSYZ_MoveUpdate(Sst *self, Own_BossSpringYard *own);
static void BSYZ_StatusUpdate(Sst *self, Own_BossSpringYard *own);
static void BSYZ_Display(Sst *self, Own_BossSpringYard *own);

static void BossSpringYard_Main(Sst *self, Own_BossSpringYard *own) {
    self->x_pos.f.u = SYZ_BOSS_X + 0x1B0;
    self->y_pos.f.u = SYZ_BOSS_Y + 0xE;
    SST_STATUS_SET(self, SST_STATUS_X_FLIP, false);

    uint8_t self_index = (uint8_t)(self - sst);

    for (int i = 0; i < 4; i++) {
        Sst *sub;
        if (i == 0) {
            sub = self;
        } else {
            sub = Sst_AllocateAfter(self);
            if (sub == NULL)
                break;
            sub->x_pos.f.u = self->x_pos.f.u;
            sub->y_pos.f.u = self->y_pos.f.u;
        }

        // A freshly-Sst_AllocateAfter()'d slot can carry a stale scratch
        // region from whatever object previously lived there. The spike
        // (routine 8) in particular reuses own->generic_timer directly
        // as its own vertical-extension counter (BossSpringYard_SpikeMain)
        // -- a stale garbage value there would offset its Y position off-
        // screen from the very first frame, rendering every call but never
        // visibly. Clearing the whole scratch region up front (parent_index
        // gets (re)written right after) closes that off for every field,
        // not just the ones this file happens to read today.
        memset(sub->own.u8, 0, sizeof(sub->own));

        sub->routine_secondary = 0;
        sub->routine = BSYZ_ObjData[i].routine;
        sub->anim = BSYZ_ObjData[i].anim;
        // AnimateSprite only resets anim_frame/frame_time when self->anim !=
        // self->prev_anim -- setting anim above isn't enough on its own: if
        // a pooled slot's stale prev_anim already happens to equal it, that
        // reset guard never fires, and anim_frame stays stale, indexing
        // Animation_Eggman out of bounds. All four fields the guard depends
        // on need to be reset here (same root cause identified for
        // Electrocuter.c's own self->anim fix, applied thoroughly this time).
        sub->prev_anim = (uint8_t)(BSYZ_ObjData[i].anim + 1); // deliberately != anim, forces the reset guard on the very first AnimateSprite call
        sub->anim_frame = 0;
        sub->anim_frame_timer = 0;
        sub->mapping_frame = 0;
        sub->priority = (uint16_t)(BSYZ_ObjData[i].priority * 0x80);
        sub->code = Obj_BossSpringYard;
        sub->mappings = Mappings_Eggman;
        sub->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_Eggman);
        sub->render_flags |= SST_RENDER_LEVEL;
        sub->width_pixels = 64 / 2;

        ((Own_BossSpringYard *)sub->own.u8)->parent_index = self_index;
    }

    own->boss_x.f.u = self->x_pos.f.u;
    own->boss_x.f.l = 0;
    own->boss_y.f.u = self->y_pos.f.u;
    own->boss_y.f.l = 0;
    self->collision_flags = 0x0F;  // col_48x48 | col_boss
    self->collision_property = 8; // obBossHits

    BossSpringYard_ShipMain(self, own);
}

static void BSYZ_CalcSine(Sst *self, Own_BossSpringYard *own) {
    uint8_t angle = own->sine_counter;
    own->sine_counter = (uint8_t)(own->sine_counter + 2);

    int16_t sin, cos;
    CalcSine(angle, &sin, &cos);
    self->y_vel = (int16_t)(sin >> 2);

    BSYZ_MoveUpdate(self, own);
}

static void BSYZ_MoveUpdate(Sst *self, Own_BossSpringYard *own) {
    SstBossMove(self, &own->boss_x, &own->boss_y);
    self->y_pos.f.u = own->boss_y.f.u;
    self->x_pos.f.u = own->boss_x.f.u;
    BSYZ_StatusUpdate(self, own);
}

static void BSYZ_Defeated(Sst *self, Own_BossSpringYard *own) {
    AddPoints(100); // 1000 points (AddPoints counts in tens, like the score)
    self->routine_secondary = 6; // -> BSYZ_Explode
    own->generic_timer = 180;
    self->x_vel = 0;
}

static void BSYZ_StatusUpdate(Sst *self, Own_BossSpringYard *own) {
    // Which 32px-wide floor block column Eggman is currently over --
    // shares parent_index's own byte on the ship instance (real ASM's own
    // BlockIndex/ParentObj offset aliasing, see BossSpringYard.h).
    own->parent_index = (uint8_t)((uint16_t)(int16_t)(self->x_pos.f.u - SYZ_BOSS_X) >> 5);

    if (self->routine_secondary >= 6) // exploding, recovering, or escaping -- skip hit detection
        return;

    if (((self->status & 0x80) != 0)) { // defeated flag
        BSYZ_Defeated(self, own);
        return;
    }
    if (self->collision_flags != 0)
        return; // not currently hittable

    if (own->flash == 0) {
        own->flash = 0x20;
        QueueSound2(sfx_HitBoss);
    }

    dry_palette[1][1] = (dry_palette[1][1] == 0) ? 0x0EEE : 0;
    if (--own->flash != 0)
        return;
    self->collision_flags = 0x0F; // col_48x48 | col_boss -- restore collision
}

static void BossSpringYard_FindBlocks(Own_BossSpringYard *own) {
    own->has_block = false;
    uint8_t target_col = own->parent_index; // block_index alias -- see BossSpringYard.h

    for (int i = 0; i < SST_COUNT; i++) {
        if (sst[i].code != NULL && sst[i].code == Obj_BossBlock && sst[i].subtype == target_col) {
            own->block_index = (uint8_t)i;
            own->has_block = true;
            break;
        }
    }
}

static void BSYZ_DropSetup(Sst *self, Own_BossSpringYard *own) {
    int16_t d0 = (int16_t)(own->boss_x.f.u - (SYZ_BOSS_X + 0x10));
    d0 = (int16_t)(d0 & 0x1F);
    d0 = (int16_t)(d0 - 0x1F);
    if (d0 < 0)
        d0 = (int16_t)-d0;
    d0 -= 1;

    if (d0 <= 0 && !own->swept) {
        int8_t player_col = (int8_t)((player->x_pos.f.u - SYZ_BOSS_X) >> 5);
        if (player_col == (int8_t)own->parent_index) {
            own->boss_x.f.u = (int16_t)(((int16_t)(int8_t)own->parent_index << 5) + SYZ_BOSS_X + 0x10);
            BossSpringYard_FindBlocks(own);
            self->routine_secondary += 2; // -> BSYZ_Attack
            self->subtype = 0;       // subtype -- attack substate (Descend)
            own->spike_disabled = 0;  // cleared by the same word write real ASM does here
            self->x_vel = 0;
        }
    }

    BSYZ_CalcSine(self, own);
}

static void BSYZ_ShipStart(Sst *self, Own_BossSpringYard *own) {
    self->x_vel = (int16_t)-0x100;
    if ((uint16_t)own->boss_x.f.u >= (uint16_t)(SYZ_BOSS_X + 0x138)) {
        BSYZ_CalcSine(self, own);
        return;
    }
    self->routine_secondary += 2;
    BSYZ_CalcSine(self, own);
}

static void BSYZ_ShipMove(Sst *self, Own_BossSpringYard *own) {
    int16_t bx = own->boss_x.f.u;
    self->x_vel = 0x140;

    if (!((self->status & SST_STATUS_X_FLIP) != 0)) {
        self->x_vel = (int16_t)-self->x_vel;
        if (bx > (int16_t)(SYZ_BOSS_X + 8)) {
            BSYZ_DropSetup(self, own);
            return;
        }
    } else {
        if (bx < (int16_t)(SYZ_BOSS_X + 0x138)) {
            BSYZ_DropSetup(self, own);
            return;
        }
    }

    // Reached a bound -- flip direction and allow another attack sweep,
    // then fall straight into DropSetup (real ASM's own fallthrough).
    SST_STATUS_SET(self, SST_STATUS_X_FLIP, !((self->status & SST_STATUS_X_FLIP) != 0));
    own->swept = false;
    BSYZ_DropSetup(self, own);
}

static void BSYZ_Descend(Sst *self, Own_BossSpringYard *own) {
    self->y_vel = 0x180;
    if ((uint16_t)own->boss_y.f.u < (uint16_t)(SYZ_BOSS_Y + 0x8A)) {
        BSYZ_MoveUpdate(self, own);
        return;
    }

    own->boss_y.f.u = SYZ_BOSS_Y + 0x8A;
    own->generic_timer = 0;

    if (own->has_block) {
        Sst *block = &sst[own->block_index];
        Own_BossBlock *bscratch = (Own_BossBlock *)block->own.u8;
        bscratch->child_cmd = -1; // grabbed
        bscratch->parent_index = (uint8_t)(self - sst);
        own->spike_disabled = 1;
        own->generic_timer = 50;
    }

    self->y_vel = 0;
    self->subtype += 2; // -> Lift
    BSYZ_MoveUpdate(self, own);
}

static void BSYZ_Lift(Sst *self, Own_BossSpringYard *own) {
    own->generic_timer -= 1;
    int16_t d0;

    if (own->generic_timer >= 0) {
        d0 = 0;
        if (own->generic_timer <= 30) {
            d0 = 2;
            if (((uint8_t)own->generic_timer) & 2)
                d0 = -2;
        }
    } else {
        self->subtype += 2; // -> LiftStop
        self->y_vel = (int16_t)-0x800;
        if (!own->has_block)
            self->y_vel = (int16_t)(self->y_vel >> 1);
        d0 = 0;
    }

    self->y_pos.f.u = (int16_t)(own->boss_y.f.u + d0);
    self->x_pos.f.u = own->boss_x.f.u;
    BSYZ_StatusUpdate(self, own);
}

static void BSYZ_LiftStop(Sst *self, Own_BossSpringYard *own) {
    int16_t target = (int16_t)(SYZ_BOSS_Y + 0xE);
    if (own->has_block)
        target = (int16_t)(target - 0x18);

    if (target < own->boss_y.f.u) {
        if (self->y_vel < (int16_t)-0x40)
            self->y_vel = (int16_t)(self->y_vel + 0xC);
        BSYZ_MoveUpdate(self, own);
        return;
    }

    own->generic_timer = 8;
    if (own->has_block)
        own->generic_timer = 45;
    self->subtype += 2; // -> BreakBlock
    self->y_vel = 0;
    BSYZ_MoveUpdate(self, own);
}

static void BSYZ_BreakBlock(Sst *self, Own_BossSpringYard *own) {
    own->generic_timer -= 1;

    if (own->generic_timer == 0) {
        if (own->has_block) {
            Sst *block = &sst[own->block_index];
            ((Own_BossBlock *)block->own.u8)->child_cmd = 0xA; // start breaking
        }
        own->has_block = false;
    } else if (own->generic_timer < 0 && own->generic_timer == -30) {
        own->spike_disabled = 0; // spike dangerous again
        self->routine_secondary -= 2;       // -> BSYZ_ShipMove
        own->swept = true;
        BSYZ_StatusUpdate(self, own);
        return;
    }

    int16_t d0 = own->has_block ? 2 : 1;
    int16_t rest = (int16_t)(SYZ_BOSS_Y + 0xE);
    if (own->boss_y.f.u != rest) {
        if (own->boss_y.f.u > rest)
            d0 = (int16_t)-d0;
        own->boss_y.f.u = (int16_t)(own->boss_y.f.u + d0);
    }

    int16_t shake = 0;
    if (own->has_block) {
        shake = 2;
        if (((uint8_t)own->generic_timer) & 1)
            shake = -2;
    }
    self->y_pos.f.u = (int16_t)(own->boss_y.f.u + shake);
    self->x_pos.f.u = own->boss_x.f.u;
    BSYZ_StatusUpdate(self, own);
}

static void BSYZ_Attack(Sst *self, Own_BossSpringYard *own) {
    switch (self->subtype) {
    case 0: BSYZ_Descend(self, own); break;
    case 2: BSYZ_Lift(self, own); break;
    case 4: BSYZ_LiftStop(self, own); break;
    case 6: BSYZ_BreakBlock(self, own); break;
    }
}

static void BSYZ_Explode(Sst *self, Own_BossSpringYard *own) {
    if (--own->generic_timer >= 0) {
        SstBossDefeated(self);
        return;
    }

    SST_STATUS_SET(self, SST_STATUS_X_FLIP, true);
    self->status &= (uint8_t)~0x80; // clear defeated flag (set in ReactToItem)
    self->x_vel = 0;
    self->routine_secondary += 2; // -> BSYZ_Recover
    own->generic_timer = -1;

    if (!boss_status)
        boss_status = 1;

    BSYZ_StatusUpdate(self, own);
}

static void BSYZ_Recover(Sst *self, Own_BossSpringYard *own) {
    own->generic_timer++;

    if (own->generic_timer == 0) {
        self->y_vel = 0; // done falling
    } else if (own->generic_timer < 0) {
        self->y_vel = (int16_t)(self->y_vel + 0x18); // fall a little
    } else if (own->generic_timer < 32) {
        self->y_vel = (int16_t)(self->y_vel - 8); // rise
    } else if (own->generic_timer == 32) {
        self->y_vel = 0;
        QueueSound1(bgm_SYZ);
    } else if (own->generic_timer < 42) {
        // wait
    } else {
        self->routine_secondary += 2; // -> BSYZ_Escape
    }

    BSYZ_MoveUpdate(self, own);
}

static void BSYZ_Escape(Sst *self, Own_BossSpringYard *own) {
    self->x_vel = 0x400;          // move right quickly
    self->y_vel = (int16_t)-0x40; // move up a little

    if (limit_right2 != (uint16_t)SYZ_BOSS_END) {
        limit_right2 += 2; // keep unlocking the screen bounds
    } else if (SstOutOfRange(self->x_pos.f.u)) {
        SstLoad_Forget(self);
        Sst_Delete(self); // has Eggman left the screen?
        return;
    }

    // Real ASM calls BossMove directly here, THEN falls into CalcSine --
    // which itself calls BossMove again via MoveUpdate. Not a mistake to
    // "fix": both calls are real, applying the escape velocity and then an
    // extra sine-bob pass on top of it in the same frame.
    SstBossMove(self, &own->boss_x, &own->boss_y);
    BSYZ_CalcSine(self, own);
}

static void BossSpringYard_ShipMain(Sst *self, Own_BossSpringYard *own) {
    switch (self->routine_secondary) {
    case 0:  BSYZ_ShipStart(self, own); break;
    case 2:  BSYZ_ShipMove(self, own); break;
    case 4:  BSYZ_Attack(self, own); break;
    case 6:  BSYZ_Explode(self, own); break;
    case 8:  BSYZ_Recover(self, own); break;
    case 10: BSYZ_Escape(self, own); break;
    }

    Sst_Animate(self, Animation_Eggman);

    uint8_t flip = self->status & 3;
    self->render_flags = (uint8_t)((self->render_flags & ~3) | flip);
    Sst_Draw(self);
}

// Shared tail for the face/flame sub-objects: syncs position/status
// from the ship and displays, animating first (both are Animation_Eggman
// driven, unlike the spike -- see BossSpringYard_SpikeMain's own comment).
static void BSYZ_SetBits(Sst *self, Own_BossSpringYard *own) {
    Sst *parent = &sst[own->parent_index];
    self->x_pos.f.u = parent->x_pos.f.u;
    self->y_pos.f.u = parent->y_pos.f.u;
    self->status = parent->status;

    uint8_t flip = self->status & 3;
    self->render_flags = (uint8_t)((self->render_flags & ~3) | flip);
    Sst_Draw(self);
}

static void BSYZ_Display(Sst *self, Own_BossSpringYard *own) {
    Sst_Animate(self, Animation_Eggman);
    BSYZ_SetBits(self, own);
}

static void BossSpringYard_FaceMain(Sst *self, Own_BossSpringYard *own) {
    Sst *parent = &sst[own->parent_index];
    uint8_t anim = 1; // facenormal1

    switch (parent->routine_secondary) {
    case 0: // ShipStart -- Face_ChkHit
    case 2: // ShipMove -- Face_ChkHit
        if (parent->collision_flags == 0) {
            anim = 5; // facehit
        } else if (player->routine >= 4) {
            anim = 4; // facelaugh
        }
        break;

    case 4: // Attack -- Face_Attack, sub-dispatched on the ship's own attack substate
        if (parent->subtype == 2) // Lift
            anim = 6; // lifting
        if (parent->collision_flags == 0) {
            anim = 5; // facehit -- overrides the Lift anim above, matching real ASM's own fallthrough
        } else if (player->routine >= 4) {
            anim = 4; // facelaugh
        }
        break;

    case 6: // Explode -- Face_Defeat
    case 8: // Recover -- Face_Defeat
        anim = 0xA; // facedefeat
        break;

    case 10: // Escape -- Face_Escape
        anim = 6; // facepanic
        break;
    }

    self->anim = anim;

    if (parent->routine_secondary == 10) { // Escape -- extra on-screen-delete check once fully escaping
        if (SstOutOfRange(self->x_pos.f.u)) {
            SstLoad_Forget(self);
            Sst_Delete(self);
            return;
        }
    }
    BSYZ_Display(self, own);
}

static void BossSpringYard_FlameMain(Sst *self, Own_BossSpringYard *own) {
    self->anim = 7; // blank (default invisible state)

    Sst *parent = &sst[own->parent_index];

    if (parent->routine_secondary == 10) { // Escape
        self->anim = 0xB; // escapeflame -- thruster animation for takeoff
        if (SstOutOfRange(self->x_pos.f.u)) {
            SstLoad_Forget(self);
            Sst_Delete(self);
            return;
        }
        BSYZ_Display(self, own);
        return;
    }

    if (parent->x_vel != 0)
        self->anim = 8; // flame

    BSYZ_Display(self, own);
}

static void BossSpringYard_SpikeMain(Sst *self, Own_BossSpringYard *own) {
    self->mappings = Mappings_BossItems;
    self->art_tile = TILE_MAP(0, 1, 0, 0, ArtTile_Eggman_Weapons); // | Tile_Pal2
    self->mapping_frame = 5;

    Sst *parent = &sst[own->parent_index];
    Own_BossSpringYard *pscratch = (Own_BossSpringYard *)parent->own.u8;

    if (parent->routine_secondary == 10) { // Escape
        if (SstOutOfRange(self->x_pos.f.u)) {
            SstLoad_Forget(self);
            Sst_Delete(self);
            return;
        }
    }

    self->x_pos.f.u = parent->x_pos.f.u;
    self->y_pos.f.u = parent->y_pos.f.u;

    // Vertical extension amount -- this object's OWN generic_timer,
    // repurposed here purely as the spike's extension counter (real ASM's
    // own BossSpringYard_GenericTimer aliasing, spike-instance-only use).
    int16_t d0 = own->generic_timer;

    bool retract = false;
    if (parent->routine_secondary != 4) {
        retract = true;
    } else if (parent->subtype == 6) { // BreakBlock
        if (pscratch->generic_timer < 0)
            retract = true;
    } else if (parent->subtype == 0) { // Descend
        if (d0 < 0x94)
            d0 = (int16_t)(d0 + 7);
    }
    // else (Lift or LiftStop): d0 unchanged

    if (retract && d0 > 0)
        d0 = (int16_t)(d0 - 5);

    own->generic_timer = d0;
    self->y_pos.f.u = (int16_t)(self->y_pos.f.u + (d0 >> 2));
    self->width_pixels = 16 / 2;
    self->height_pixels = self->y_radius;
    self->y_radius = 24 / 2;
    self->collision_flags = 0;

    if (parent->collision_flags != 0 && !pscratch->spike_disabled)
        self->collision_flags = 0x04 | 0x80; // col_8x32 | col_hurt

    // Display direct -- the spike sets its own frame/mappings/tile above
    // (it is NOT Animation_Eggman-driven, unlike the face/flame), so it
    // must not route through BSYZ_Display/BSYZ_SetBits: AnimateSprite
    // would overwrite self->mapping_frame from Animation_Eggman, and SetBits would
    // reset self->y_pos.f.u back to the parent's own Y, wiping out the
    // d0>>2 extension offset applied just above. Sync the x_flip bits the
    // same way SetBits does, then display with position/frame intact.
    uint8_t flip = parent->status & 3;
    self->status = parent->status;
    self->render_flags = (uint8_t)((self->render_flags & ~3) | flip);
    Sst_Draw(self);
}

void Obj_BossSpringYard(Sst *self) {
    Own_BossSpringYard *own = (Own_BossSpringYard *)self->own.u8;

    switch (self->routine) {
    case 0: BossSpringYard_Main(self, own); break;
    case 2: BossSpringYard_ShipMain(self, own); break;
    case 4: BossSpringYard_FaceMain(self, own); break;
    case 6: BossSpringYard_FlameMain(self, own); break;
    case 8: BossSpringYard_SpikeMain(self, own); break;
    }
}
