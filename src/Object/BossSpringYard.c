#include "BossSpringYard.h"

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

static void BossSpringYard_ShipMain(Object *obj, Scratch_BossSpringYard *scratch);
static void BSYZ_CalcSine(Object *obj, Scratch_BossSpringYard *scratch);
static void BSYZ_MoveUpdate(Object *obj, Scratch_BossSpringYard *scratch);
static void BSYZ_StatusUpdate(Object *obj, Scratch_BossSpringYard *scratch);
static void BSYZ_Display(Object *obj, Scratch_BossSpringYard *scratch);

static void BossSpringYard_Main(Object *obj, Scratch_BossSpringYard *scratch) {
    obj->pos.l.x.f.u = SYZ_BOSS_X + 0x1B0;
    obj->pos.l.y.f.u = SYZ_BOSS_Y + 0xE;
    obj->status.o.f.x_flip = false;

    uint8_t self_index = (uint8_t)(obj - objects);

    for (int i = 0; i < 4; i++) {
        Object *sub;
        if (i == 0) {
            sub = obj;
        } else {
            sub = FindNextFreeObj(obj);
            if (sub == NULL)
                break;
            sub->pos.l.x.f.u = obj->pos.l.x.f.u;
            sub->pos.l.y.f.u = obj->pos.l.y.f.u;
        }

        // A freshly-FindNextFreeObj()'d slot can carry a stale scratch
        // region from whatever object previously lived there. The spike
        // (routine 8) in particular reuses scratch->generic_timer directly
        // as its own vertical-extension counter (BossSpringYard_SpikeMain)
        // -- a stale garbage value there would offset its Y position off-
        // screen from the very first frame, rendering every call but never
        // visibly. Clearing the whole scratch region up front (parent_index
        // gets (re)written right after) closes that off for every field,
        // not just the ones this file happens to read today.
        memset(&sub->scratch, 0, sizeof(sub->scratch));

        sub->routine_sec = 0;
        sub->routine = BSYZ_ObjData[i].routine;
        sub->anim = BSYZ_ObjData[i].anim;
        // AnimateSprite only resets anim_frame/frame_time when obj->anim !=
        // obj->prev_anim -- setting anim above isn't enough on its own: if
        // a pooled slot's stale prev_anim already happens to equal it, that
        // reset guard never fires, and anim_frame stays stale, indexing
        // Animation_Eggman out of bounds. All four fields the guard depends
        // on need to be reset here (same root cause identified for
        // Electrocuter.c's own obj->anim fix, applied thoroughly this time).
        sub->prev_anim = (uint8_t)(BSYZ_ObjData[i].anim + 1); // deliberately != anim, forces the reset guard on the very first AnimateSprite call
        sub->anim_frame = 0;
        sub->frame_time.w = 0;
        sub->frame = 0;
        sub->priority = BSYZ_ObjData[i].priority;
        sub->type = ObjId_BossSpringYard;
        sub->mappings = Mappings_Eggman;
        sub->tile = TILE_MAP(0, 0, 0, 0, ArtTile_Eggman);
        sub->render.f.align_fg = true;
        sub->width_pixels = 64 / 2;

        ((Scratch_BossSpringYard *)&sub->scratch)->parent_index = self_index;
    }

    scratch->boss_x.f.u = obj->pos.l.x.f.u;
    scratch->boss_x.f.l = 0;
    scratch->boss_y.f.u = obj->pos.l.y.f.u;
    scratch->boss_y.f.l = 0;
    obj->col_type = 0x0F;  // col_48x48 | col_boss
    obj->col_property = 8; // obBossHits

    BossSpringYard_ShipMain(obj, scratch);
}

static void BSYZ_CalcSine(Object *obj, Scratch_BossSpringYard *scratch) {
    uint8_t angle = scratch->sine_counter;
    scratch->sine_counter = (uint8_t)(scratch->sine_counter + 2);

    int16_t sin, cos;
    CalcSine(angle, &sin, &cos);
    obj->ysp = (int16_t)(sin >> 2);

    BSYZ_MoveUpdate(obj, scratch);
}

static void BSYZ_MoveUpdate(Object *obj, Scratch_BossSpringYard *scratch) {
    BossMove(obj, &scratch->boss_x, &scratch->boss_y);
    obj->pos.l.y.f.u = scratch->boss_y.f.u;
    obj->pos.l.x.f.u = scratch->boss_x.f.u;
    BSYZ_StatusUpdate(obj, scratch);
}

static void BSYZ_Defeated(Object *obj, Scratch_BossSpringYard *scratch) {
    AddPoints(1000); // real ASM passes literal 100, but its own AddPoints takes points/10 -- this project's AddPoints takes the full displayed value
    obj->routine_sec = 6; // -> BSYZ_Explode
    scratch->generic_timer = 180;
    obj->xsp = 0;
}

static void BSYZ_StatusUpdate(Object *obj, Scratch_BossSpringYard *scratch) {
    // Which 32px-wide floor block column Eggman is currently over --
    // shares parent_index's own byte on the ship instance (real ASM's own
    // BlockIndex/ParentObj offset aliasing, see BossSpringYard.h).
    scratch->parent_index = (uint8_t)((uint16_t)(int16_t)(obj->pos.l.x.f.u - SYZ_BOSS_X) >> 5);

    if (obj->routine_sec >= 6) // exploding, recovering, or escaping -- skip hit detection
        return;

    if (obj->status.o.f.flag7) { // defeated flag
        BSYZ_Defeated(obj, scratch);
        return;
    }
    if (obj->col_type != 0)
        return; // not currently hittable

    if (scratch->flash == 0) {
        scratch->flash = 0x20;
        QueueSound2(sfx_HitBoss);
    }

    dry_palette[1][1] = (dry_palette[1][1] == 0) ? 0x0EEE : 0;
    if (--scratch->flash != 0)
        return;
    obj->col_type = 0x0F; // col_48x48 | col_boss -- restore collision
}

static void BossSpringYard_FindBlocks(Scratch_BossSpringYard *scratch) {
    scratch->has_block = false;
    uint8_t target_col = scratch->parent_index; // block_index alias -- see BossSpringYard.h

    for (int i = RESERVED_OBJECTS; i < OBJECTS; i++) {
        if (objects[i].type == ObjId_BossBlock && objects[i].scratch.u8[0] == target_col) {
            scratch->block_index = (uint8_t)i;
            scratch->has_block = true;
            break;
        }
    }
}

static void BSYZ_DropSetup(Object *obj, Scratch_BossSpringYard *scratch) {
    int16_t d0 = (int16_t)(scratch->boss_x.f.u - (SYZ_BOSS_X + 0x10));
    d0 = (int16_t)(d0 & 0x1F);
    d0 = (int16_t)(d0 - 0x1F);
    if (d0 < 0)
        d0 = (int16_t)-d0;
    d0 -= 1;

    if (d0 <= 0 && !scratch->swept) {
        int8_t player_col = (int8_t)((player->pos.l.x.f.u - SYZ_BOSS_X) >> 5);
        if (player_col == (int8_t)scratch->parent_index) {
            scratch->boss_x.f.u = (int16_t)(((int16_t)(int8_t)scratch->parent_index << 5) + SYZ_BOSS_X + 0x10);
            BossSpringYard_FindBlocks(scratch);
            obj->routine_sec += 2; // -> BSYZ_Attack
            obj->scratch.u8[0] = 0;       // subtype -- attack substate (Descend)
            scratch->spike_disabled = 0;  // cleared by the same word write real ASM does here
            obj->xsp = 0;
        }
    }

    BSYZ_CalcSine(obj, scratch);
}

static void BSYZ_ShipStart(Object *obj, Scratch_BossSpringYard *scratch) {
    obj->xsp = (int16_t)-0x100;
    if ((uint16_t)scratch->boss_x.f.u >= (uint16_t)(SYZ_BOSS_X + 0x138)) {
        BSYZ_CalcSine(obj, scratch);
        return;
    }
    obj->routine_sec += 2;
    BSYZ_CalcSine(obj, scratch);
}

static void BSYZ_ShipMove(Object *obj, Scratch_BossSpringYard *scratch) {
    int16_t bx = scratch->boss_x.f.u;
    obj->xsp = 0x140;

    if (!obj->status.o.f.x_flip) {
        obj->xsp = (int16_t)-obj->xsp;
        if (bx > (int16_t)(SYZ_BOSS_X + 8)) {
            BSYZ_DropSetup(obj, scratch);
            return;
        }
    } else {
        if (bx < (int16_t)(SYZ_BOSS_X + 0x138)) {
            BSYZ_DropSetup(obj, scratch);
            return;
        }
    }

    // Reached a bound -- flip direction and allow another attack sweep,
    // then fall straight into DropSetup (real ASM's own fallthrough).
    obj->status.o.f.x_flip = !obj->status.o.f.x_flip;
    scratch->swept = false;
    BSYZ_DropSetup(obj, scratch);
}

static void BSYZ_Descend(Object *obj, Scratch_BossSpringYard *scratch) {
    obj->ysp = 0x180;
    if ((uint16_t)scratch->boss_y.f.u < (uint16_t)(SYZ_BOSS_Y + 0x8A)) {
        BSYZ_MoveUpdate(obj, scratch);
        return;
    }

    scratch->boss_y.f.u = SYZ_BOSS_Y + 0x8A;
    scratch->generic_timer = 0;

    if (scratch->has_block) {
        Object *block = &objects[scratch->block_index];
        Scratch_BossBlock *bscratch = (Scratch_BossBlock *)&block->scratch;
        bscratch->child_cmd = -1; // grabbed
        bscratch->parent_index = (uint8_t)(obj - objects);
        scratch->spike_disabled = 1;
        scratch->generic_timer = 50;
    }

    obj->ysp = 0;
    obj->scratch.u8[0] += 2; // -> Lift
    BSYZ_MoveUpdate(obj, scratch);
}

static void BSYZ_Lift(Object *obj, Scratch_BossSpringYard *scratch) {
    scratch->generic_timer -= 1;
    int16_t d0;

    if (scratch->generic_timer >= 0) {
        d0 = 0;
        if (scratch->generic_timer <= 30) {
            d0 = 2;
            if (((uint8_t)scratch->generic_timer) & 2)
                d0 = -2;
        }
    } else {
        obj->scratch.u8[0] += 2; // -> LiftStop
        obj->ysp = (int16_t)-0x800;
        if (!scratch->has_block)
            obj->ysp = (int16_t)(obj->ysp >> 1);
        d0 = 0;
    }

    obj->pos.l.y.f.u = (int16_t)(scratch->boss_y.f.u + d0);
    obj->pos.l.x.f.u = scratch->boss_x.f.u;
    BSYZ_StatusUpdate(obj, scratch);
}

static void BSYZ_LiftStop(Object *obj, Scratch_BossSpringYard *scratch) {
    int16_t target = (int16_t)(SYZ_BOSS_Y + 0xE);
    if (scratch->has_block)
        target = (int16_t)(target - 0x18);

    if (target < scratch->boss_y.f.u) {
        if (obj->ysp < (int16_t)-0x40)
            obj->ysp = (int16_t)(obj->ysp + 0xC);
        BSYZ_MoveUpdate(obj, scratch);
        return;
    }

    scratch->generic_timer = 8;
    if (scratch->has_block)
        scratch->generic_timer = 45;
    obj->scratch.u8[0] += 2; // -> BreakBlock
    obj->ysp = 0;
    BSYZ_MoveUpdate(obj, scratch);
}

static void BSYZ_BreakBlock(Object *obj, Scratch_BossSpringYard *scratch) {
    scratch->generic_timer -= 1;

    if (scratch->generic_timer == 0) {
        if (scratch->has_block) {
            Object *block = &objects[scratch->block_index];
            ((Scratch_BossBlock *)&block->scratch)->child_cmd = 0xA; // start breaking
        }
        scratch->has_block = false;
    } else if (scratch->generic_timer < 0 && scratch->generic_timer == -30) {
        scratch->spike_disabled = 0; // spike dangerous again
        obj->routine_sec -= 2;       // -> BSYZ_ShipMove
        scratch->swept = true;
        BSYZ_StatusUpdate(obj, scratch);
        return;
    }

    int16_t d0 = scratch->has_block ? 2 : 1;
    int16_t rest = (int16_t)(SYZ_BOSS_Y + 0xE);
    if (scratch->boss_y.f.u != rest) {
        if (scratch->boss_y.f.u > rest)
            d0 = (int16_t)-d0;
        scratch->boss_y.f.u = (int16_t)(scratch->boss_y.f.u + d0);
    }

    int16_t shake = 0;
    if (scratch->has_block) {
        shake = 2;
        if (((uint8_t)scratch->generic_timer) & 1)
            shake = -2;
    }
    obj->pos.l.y.f.u = (int16_t)(scratch->boss_y.f.u + shake);
    obj->pos.l.x.f.u = scratch->boss_x.f.u;
    BSYZ_StatusUpdate(obj, scratch);
}

static void BSYZ_Attack(Object *obj, Scratch_BossSpringYard *scratch) {
    switch (obj->scratch.u8[0]) {
    case 0: BSYZ_Descend(obj, scratch); break;
    case 2: BSYZ_Lift(obj, scratch); break;
    case 4: BSYZ_LiftStop(obj, scratch); break;
    case 6: BSYZ_BreakBlock(obj, scratch); break;
    }
}

static void BSYZ_Explode(Object *obj, Scratch_BossSpringYard *scratch) {
    if (--scratch->generic_timer >= 0) {
        BossDefeated(obj);
        return;
    }

    obj->status.o.f.x_flip = true;
    obj->status.o.f.flag7 = false; // clear defeated flag (set in ReactToItem)
    obj->xsp = 0;
    obj->routine_sec += 2; // -> BSYZ_Recover
    scratch->generic_timer = -1;

    if (!boss_status)
        boss_status = 1;

    BSYZ_StatusUpdate(obj, scratch);
}

static void BSYZ_Recover(Object *obj, Scratch_BossSpringYard *scratch) {
    scratch->generic_timer++;

    if (scratch->generic_timer == 0) {
        obj->ysp = 0; // done falling
    } else if (scratch->generic_timer < 0) {
        obj->ysp = (int16_t)(obj->ysp + 0x18); // fall a little
    } else if (scratch->generic_timer < 32) {
        obj->ysp = (int16_t)(obj->ysp - 8); // rise
    } else if (scratch->generic_timer == 32) {
        obj->ysp = 0;
        QueueSound1(bgm_SYZ);
    } else if (scratch->generic_timer < 42) {
        // wait
    } else {
        obj->routine_sec += 2; // -> BSYZ_Escape
    }

    BSYZ_MoveUpdate(obj, scratch);
}

static void BSYZ_Escape(Object *obj, Scratch_BossSpringYard *scratch) {
    obj->xsp = 0x400;          // move right quickly
    obj->ysp = (int16_t)-0x40; // move up a little

    if (limit_right2 != (uint16_t)SYZ_BOSS_END) {
        limit_right2 += 2; // keep unlocking the screen bounds
    } else if (!obj->render.f.on_screen) {
        ObjectDelete(obj); // has Eggman left the screen?
        return;
    }

    // Real ASM calls BossMove directly here, THEN falls into CalcSine --
    // which itself calls BossMove again via MoveUpdate. Not a mistake to
    // "fix": both calls are real, applying the escape velocity and then an
    // extra sine-bob pass on top of it in the same frame.
    BossMove(obj, &scratch->boss_x, &scratch->boss_y);
    BSYZ_CalcSine(obj, scratch);
}

static void BossSpringYard_ShipMain(Object *obj, Scratch_BossSpringYard *scratch) {
    switch (obj->routine_sec) {
    case 0:  BSYZ_ShipStart(obj, scratch); break;
    case 2:  BSYZ_ShipMove(obj, scratch); break;
    case 4:  BSYZ_Attack(obj, scratch); break;
    case 6:  BSYZ_Explode(obj, scratch); break;
    case 8:  BSYZ_Recover(obj, scratch); break;
    case 10: BSYZ_Escape(obj, scratch); break;
    }

    AnimateSprite(obj, Animation_Eggman);

    uint8_t flip = obj->status.b & 3;
    obj->render.b = (uint8_t)((obj->render.b & ~3) | flip);
    DisplaySprite(obj);
}

// Shared tail for the face/flame/spike sub-objects: syncs position/status
// from the ship and displays. Face/flame use BSYZ_Display, which adds the
// animation step first; the spike sets its own frame directly (it isn't
// Animation_Eggman-driven at all -- real ASM never calls AnimateSprite for
// it either) and must skip animating, same reasoning as BossMarble.c's own
// BMZ_SetBits/TubeMain split.
static void BSYZ_SetBits(Object *obj, Scratch_BossSpringYard *scratch) {
    Object *parent = &objects[scratch->parent_index];
    obj->pos.l.x.f.u = parent->pos.l.x.f.u;
    obj->pos.l.y.f.u = parent->pos.l.y.f.u;
    obj->status.b = parent->status.b;

    uint8_t flip = obj->status.b & 3;
    obj->render.b = (uint8_t)((obj->render.b & ~3) | flip);
    DisplaySprite(obj);
}

static void BSYZ_Display(Object *obj, Scratch_BossSpringYard *scratch) {
    AnimateSprite(obj, Animation_Eggman);
    BSYZ_SetBits(obj, scratch);
}

static void BossSpringYard_FaceMain(Object *obj, Scratch_BossSpringYard *scratch) {
    Object *parent = &objects[scratch->parent_index];
    uint8_t anim = 1; // facenormal1

    switch (parent->routine_sec) {
    case 0: // ShipStart -- Face_ChkHit
    case 2: // ShipMove -- Face_ChkHit
        if (parent->col_type == 0) {
            anim = 5; // facehit
        } else if (player->routine >= 4) {
            anim = 4; // facelaugh
        }
        break;

    case 4: // Attack -- Face_Attack, sub-dispatched on the ship's own attack substate
        if (parent->scratch.u8[0] == 2) // Lift
            anim = 6; // lifting
        if (parent->col_type == 0) {
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

    obj->anim = anim;

    if (parent->routine_sec == 10) { // Escape -- extra on-screen-delete check once fully escaping
        if (!obj->render.f.on_screen) {
            ObjectDelete(obj);
            return;
        }
    }
    BSYZ_Display(obj, scratch);
}

static void BossSpringYard_FlameMain(Object *obj, Scratch_BossSpringYard *scratch) {
    obj->anim = 7; // blank (default invisible state)

    Object *parent = &objects[scratch->parent_index];

    if (parent->routine_sec == 10) { // Escape
        obj->anim = 0xB; // escapeflame -- thruster animation for takeoff
        if (!obj->render.f.on_screen) {
            ObjectDelete(obj);
            return;
        }
        BSYZ_Display(obj, scratch);
        return;
    }

    if (parent->xsp != 0)
        obj->anim = 8; // flame

    BSYZ_Display(obj, scratch);
}

static void BossSpringYard_SpikeMain(Object *obj, Scratch_BossSpringYard *scratch) {
    obj->mappings = Mappings_BossItems;
    obj->tile = TILE_MAP(0, 1, 0, 0, ArtTile_Eggman_Weapons); // | Tile_Pal2
    obj->frame = 5;

    Object *parent = &objects[scratch->parent_index];
    Scratch_BossSpringYard *pscratch = (Scratch_BossSpringYard *)&parent->scratch;

    if (parent->routine_sec == 10) { // Escape
        if (!obj->render.f.on_screen) {
            ObjectDelete(obj);
            return;
        }
    }

    obj->pos.l.x.f.u = parent->pos.l.x.f.u;
    obj->pos.l.y.f.u = parent->pos.l.y.f.u;

    // Vertical extension amount -- this object's OWN generic_timer,
    // repurposed here purely as the spike's extension counter (real ASM's
    // own BossSpringYard_GenericTimer aliasing, spike-instance-only use).
    int16_t d0 = scratch->generic_timer;

    bool retract = false;
    if (parent->routine_sec != 4) {
        retract = true;
    } else if (parent->scratch.u8[0] == 6) { // BreakBlock
        if (pscratch->generic_timer < 0)
            retract = true;
    } else if (parent->scratch.u8[0] == 0) { // Descend
        if (d0 < 0x94)
            d0 = (int16_t)(d0 + 7);
    }
    // else (Lift or LiftStop): d0 unchanged

    if (retract && d0 > 0)
        d0 = (int16_t)(d0 - 5);

    scratch->generic_timer = d0;
    obj->pos.l.y.f.u = (int16_t)(obj->pos.l.y.f.u + (d0 >> 2));
    obj->width_pixels = 16 / 2;
    obj->render.f.yrad_height = true;
    obj->y_rad = 24 / 2;
    obj->col_type = 0;

    if (parent->col_type != 0 && !pscratch->spike_disabled)
        obj->col_type = 0x04 | 0x80; // col_8x32 | col_hurt

    BSYZ_Display(obj, scratch);
    BSYZ_SetBits(obj, scratch);
}

void Obj_BossSpringYard(Object *obj) {
    Scratch_BossSpringYard *scratch = (Scratch_BossSpringYard *)&obj->scratch;

    switch (obj->routine) {
    case 0: BossSpringYard_Main(obj, scratch); break;
    case 2: BossSpringYard_ShipMain(obj, scratch); break;
    case 4: BossSpringYard_FaceMain(obj, scratch); break;
    case 6: BossSpringYard_FlameMain(obj, scratch); break;
    case 8: BossSpringYard_SpikeMain(obj, scratch); break;
    }
}
