#include "BossLabyrinth.h"
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
static bool BossLabyrinth_ShipMain(Object *obj, Scratch_BossLabyrinth *scratch);

static void BossLabyrinth_Main(Object *obj, Scratch_BossLabyrinth *scratch) {
    // Spawned from DLE_LZ3 into a FindFreeObj() slot -- start from a clean
    // scratch region so early_defeat/sine_counter/flash can't be stale.
    memset(&obj->scratch, 0, sizeof(obj->scratch));

    obj->pos.l.x.f.u = LZ_BOSS_X + 0x30;
    obj->pos.l.y.f.u = LZ_BOSS_Y + 0x500;
    scratch->boss_x.f.u = obj->pos.l.x.f.u;
    scratch->boss_x.f.l = 0;
    scratch->boss_y.f.u = obj->pos.l.y.f.u;
    scratch->boss_y.f.l = 0;
    obj->col_type = 0x0F;  // col_48x48 | col_boss
    obj->col_property = 8; // obBossHits
    obj->priority = 4;

    uint8_t self_index = (uint8_t)(obj - objects);

    for (int i = 0; i < 3; i++) {
        Object *sub;
        if (i == 0) {
            sub = obj;
        } else {
            sub = FindNextFreeObj(obj);
            if (sub == NULL)
                break;
            // Same stale-slot guard as BossSpringYard.c.
            memset(&sub->scratch, 0, sizeof(sub->scratch));
            sub->type = ObjId_BossLabyrinth;
            sub->pos.l.x.f.u = obj->pos.l.x.f.u;
            sub->pos.l.y.f.u = obj->pos.l.y.f.u;
        }

        obj->status.o.f.x_flip = false;
        sub->routine_sec = 0;
        sub->routine = BLZ_ObjData[i].routine;
        sub->anim = BLZ_ObjData[i].anim;
        // Force AnimateSprite's reset guard on the first call (see
        // BossSpringYard.c for why all four fields are needed).
        sub->prev_anim = (uint8_t)(BLZ_ObjData[i].anim + 1);
        sub->anim_frame = 0;
        sub->frame_time.w = 0;
        sub->frame = 0;
        sub->priority = obj->priority;
        sub->mappings = Mappings_Eggman;
        sub->tile = TILE_MAP(0, 0, 0, 0, ArtTile_Eggman);
        sub->render.b = 0;
        sub->render.f.level_fg = true;
        sub->width_pixels = 64 / 2;
        ((Scratch_BossLabyrinth *)&sub->scratch)->parent_index = self_index;
    }

    BossLabyrinth_ShipMain(obj, scratch);
}

// BLZ_ShipUpdate: early-defeat explosions, defeat detection and hit flash.
static void BLZ_ShipUpdate(Object *obj, Scratch_BossLabyrinth *scratch) {
    if (scratch->early_defeat) {
        BossDefeated(obj); // keep trailing explosions while fleeing
        return;
    }
    if (obj->status.o.f.flag7) { // defeated flag, set by the touch response on the last hit
        AddPoints(100); // 1000 points (AddPoints counts in tens, like the score)
        scratch->early_defeat = 0xFF;
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
    obj->col_type = 0x0F; // col_48x48 | col_boss -- restore collision
}

static void BLZ_MoveBoss(Object *obj, Scratch_BossLabyrinth *scratch) {
    BossMove(obj, &scratch->boss_x, &scratch->boss_y);
    obj->pos.l.y.f.u = scratch->boss_y.f.u;
    obj->pos.l.x.f.u = scratch->boss_x.f.u;
    BLZ_ShipUpdate(obj, scratch);
}

// Clamps boss_x/boss_y at a target corner; returns true once both reached.
static bool BLZ_ReachTarget(Object *obj, Scratch_BossLabyrinth *scratch, uint16_t tx, int16_t ty) {
    int d0 = -2;
    if ((uint16_t)scratch->boss_x.f.u >= tx) {
        scratch->boss_x.f.u = (int16_t)tx;
        obj->xsp = 0;
        d0++;
    }
    if (scratch->boss_y.f.u <= ty) {
        scratch->boss_y.f.u = ty;
        obj->ysp = 0;
        d0++;
    }
    return d0 == 0;
}

static void BLZ_ShipStart(Object *obj, Scratch_BossLabyrinth *scratch) {
    // Wait for Sonic to enter the shaft.
    if ((uint16_t)player->pos.l.x.f.u >= LZ_BOSS_X - 0x40) {
        obj->ysp = -0x180; // start rising up
        obj->xsp = 0x60;   // move a little to the right
        obj->routine_sec += 2;
    }
    BLZ_MoveBoss(obj, scratch);
}

static void BLZ_ShipMove1(Object *obj, Scratch_BossLabyrinth *scratch) {
    if (BLZ_ReachTarget(obj, scratch, LZ_BOSS_X + 0x68, LZ_BOSS_Y + 0x440)) {
        obj->xsp = 0x140;
        obj->ysp = -0x200;
        obj->routine_sec += 2;
    }
    BLZ_MoveBoss(obj, scratch);
}

static void BLZ_ShipMove2(Object *obj, Scratch_BossLabyrinth *scratch) {
    if (BLZ_ReachTarget(obj, scratch, LZ_BOSS_X + 0x90, LZ_BOSS_Y + 0x400)) {
        obj->ysp = -0x180;
        obj->routine_sec += 2;
        scratch->sine_counter = 0;
    }
    BLZ_MoveBoss(obj, scratch);
}

// The climb: sway side to side, and slow down the further below him Sonic
// falls (full speed within 72px, half up to 112px, quarter up to 152px,
// stopped beyond that).
static void BLZ_ShipMove3(Object *obj, Scratch_BossLabyrinth *scratch) {
    if (scratch->boss_y.f.u <= LZ_BOSS_Y + 0x40) {
        scratch->boss_y.f.u = LZ_BOSS_Y + 0x40;
        obj->xsp = 0x140; // head right and up towards the top
        obj->ysp = -0x80;
        if (scratch->early_defeat) {
            obj->xsp = (int16_t)(obj->xsp << 1);
            obj->ysp = (int16_t)(obj->ysp << 1);
        }
        obj->routine_sec += 2;
        BLZ_MoveBoss(obj, scratch);
        return;
    }

    obj->status.o.f.x_flip = true; // face right
    scratch->sine_counter = (uint8_t)(scratch->sine_counter + 2);
    int16_t sin, cos;
    CalcSine(scratch->sine_counter, &sin, &cos);
    if (cos < 0)
        obj->status.o.f.x_flip = false; // swaying left -- face left

    // X sways around boss_x without disturbing its fractional part.
    obj->pos.l.x.f.u = (int16_t)(scratch->boss_x.f.u + (sin >> 4));

    int16_t d0 = obj->ysp;
    uint16_t py = (uint16_t)player->pos.l.y.f.u;
    uint16_t oy = (uint16_t)obj->pos.l.y.f.u;
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
    if (scratch->early_defeat)
        step += step; // defeated early -- flee at double speed
    scratch->boss_y.v += step;
    obj->pos.l.y.f.u = scratch->boss_y.f.u;
    BLZ_ShipUpdate(obj, scratch);
}

static void BLZ_ShipAtTop(Object *obj, Scratch_BossLabyrinth *scratch) {
    if (BLZ_ReachTarget(obj, scratch, LZ_BOSS_X + 0x16C, LZ_BOSS_Y)) {
        obj->routine_sec += 2;
        obj->status.o.f.x_flip = false; // face left, waiting for Sonic
    }
    BLZ_MoveBoss(obj, scratch);
}

static void BLZ_ShipWait(Object *obj, Scratch_BossLabyrinth *scratch) {
    bool escape = scratch->early_defeat != 0;
    if (!escape && player->pos.l.x.f.u >= LZ_BOSS_X + 0xE8 && player->pos.l.y.f.u <= LZ_BOSS_Y + 0x30) {
        scratch->generic_timer = 50;
        escape = true;
    }
    if (escape) {
        QueueSound1(bgm_LZ);
        lock_screen = false;
        obj->status.o.f.x_flip = true; // face right
        obj->routine_sec += 2;
    }
    BLZ_MoveBoss(obj, scratch);
}

static void BLZ_Escape1(Object *obj, Scratch_BossLabyrinth *scratch) {
    if (scratch->early_defeat || --scratch->generic_timer == 0) {
        scratch->generic_timer = 0;
        obj->xsp = 0x400; // escape quickly to the right and slightly up
        obj->ysp = -0x40;
        scratch->early_defeat = 0;
        obj->routine_sec += 2;
    }
    BLZ_MoveBoss(obj, scratch);
}

// Returns true if the ship deleted itself.
static bool BLZ_Escape2(Object *obj, Scratch_BossLabyrinth *scratch) {
    if (limit_right2 < LZ_BOSS_END) {
        limit_right2 += 2; // keep unlocking the screen's right boundary
    } else if (!obj->render.f.on_screen) {
#ifdef SCP_FIX_BUGS
        // Don't leave the palette stuck on white if Eggman was hit as he
        // was leaving the screen.
        dry_palette[1][1] = 0;
#endif
        // FixBugs forced on (crash-class bug under C rules): real ASM without
        // FixBugs returns into ShipMain and animates/displays the
        // just-deleted slot. That write-after-delete corrupts whatever
        // reuses the slot, so the early return is unconditional here.
        ObjectDelete(obj);
        return true;
    }
    BLZ_MoveBoss(obj, scratch);
    return false;
}

static void BLZ_Display(Object *obj, Object *parent) {
    AnimateSprite(obj, Animation_Eggman);
    obj->pos.l.x.f.u = parent->pos.l.x.f.u;
    obj->pos.l.y.f.u = parent->pos.l.y.f.u;
    obj->status.b = parent->status.b;
    uint8_t flip = obj->status.b & 3;
    obj->render.b = (uint8_t)((obj->render.b & ~3) | flip);
    DisplaySprite(obj);
}

static bool BossLabyrinth_ShipMain(Object *obj, Scratch_BossLabyrinth *scratch) {
    switch (obj->routine_sec) {
    case 0x0: BLZ_ShipStart(obj, scratch); break;
    case 0x2: BLZ_ShipMove1(obj, scratch); break;
    case 0x4: BLZ_ShipMove2(obj, scratch); break;
    case 0x6: BLZ_ShipMove3(obj, scratch); break;
    case 0x8: BLZ_ShipAtTop(obj, scratch); break;
    case 0xA: BLZ_ShipWait(obj, scratch); break;
    case 0xC: BLZ_Escape1(obj, scratch); break;
    case 0xE:
        if (BLZ_Escape2(obj, scratch))
            return true;
        break;
    }

    AnimateSprite(obj, Animation_Eggman);
    uint8_t flip = obj->status.b & 3;
    obj->render.b = (uint8_t)((obj->render.b & ~3) | flip);
    DisplaySprite(obj);
    return false;
}

static void BossLabyrinth_FaceMain(Object *obj, Scratch_BossLabyrinth *scratch) {
    Object *parent = &objects[scratch->parent_index];
    if (parent->type != obj->type) { // ship already deleted
        ObjectDelete(obj);
        return;
    }
    Scratch_BossLabyrinth *pscratch = (Scratch_BossLabyrinth *)&parent->scratch;

    uint8_t anim = 1; // facenormal1
#ifdef SCP_FIX_BUGS
    bool defeated = pscratch->early_defeat != 0;
#else
    // Real ASM checks the face's own copy of the flag, which is always 0,
    // so the defeated face never shows.
    bool defeated = scratch->early_defeat != 0;
    (void)pscratch;
#endif
    if (defeated)
        anim = 0xA; // facedefeat
    else if (parent->col_type == 0)
        anim = 5; // facehit
    else if (player->routine >= 4)
        anim = 4; // facelaugh

    obj->anim = anim;
    if (parent->routine_sec == 0xE) { // Escape2
        obj->anim = 6; // facepanic
        if (!obj->render.f.on_screen) {
            ObjectDelete(obj);
            return;
        }
    }
    BLZ_Display(obj, parent);
}

static void BossLabyrinth_FlameMain(Object *obj, Scratch_BossLabyrinth *scratch) {
    obj->anim = 7; // blank
    Object *parent = &objects[scratch->parent_index];
    if (parent->type != obj->type) { // ship already deleted
        ObjectDelete(obj);
        return;
    }

    if (parent->routine_sec == 0xE) { // Escape2
        obj->anim = 0xB; // escapeflame
        if (!obj->render.f.on_screen) {
            ObjectDelete(obj);
            return;
        }
    }
#ifdef SCP_FIX_BUGS
    // Real ASM skips this check (missing label), so the flame is invisible
    // outside the escape.
    else if (parent->xsp != 0) {
        obj->anim = 8; // flame
    }
#endif
    BLZ_Display(obj, parent);
}

void Obj_BossLabyrinth(Object *obj) {
    Scratch_BossLabyrinth *scratch = (Scratch_BossLabyrinth *)&obj->scratch;

    switch (obj->routine) {
    case 0: BossLabyrinth_Main(obj, scratch); break;
    case 2: BossLabyrinth_ShipMain(obj, scratch); break;
    case 4: BossLabyrinth_FaceMain(obj, scratch); break;
    case 6: BossLabyrinth_FlameMain(obj, scratch); break;
    }
}
