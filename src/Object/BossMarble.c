#include "BossMarble.h"

#include "BossFire.h"
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

static void BossMarble_ShipMain(Object *obj, Scratch_BossMarble *scratch);
static void BMZ_ShipUpdate(Object *obj, Scratch_BossMarble *scratch);
static void BMZ_Display(Object *obj, Scratch_BossMarble *scratch);

static void BossMarble_Main(Object *obj, Scratch_BossMarble *scratch) {
    obj->status.o.f.x_flip = false; // bclr #0,obStatus(a0) -- start facing left

    uint8_t self_index = (uint8_t)(obj - objects);

    for (int i = 0; i < 4; i++) {
        Object *sub;
        if (i == 0) {
            sub = obj;
        } else {
            sub = FindNextFreeObj(obj);
            if (sub == NULL)
                break;
        }

        sub->routine_sec = 0;
        sub->routine = BMZ_ObjData[i].routine;
        sub->anim = BMZ_ObjData[i].anim;
        sub->priority = BMZ_ObjData[i].priority;
        sub->type = ObjId_BossMarble;
        sub->pos.l.x.f.u = obj->pos.l.x.f.u;
        sub->pos.l.y.f.u = obj->pos.l.y.f.u;
        sub->mappings = Mappings_Eggman;
        sub->tile = TILE_MAP(0, 0, 0, 0, ArtTile_Eggman);
        sub->render.f.align_fg = true;
        sub->width_pixels = 64 / 2;

        ((Scratch_BossMarble *)&sub->scratch)->parent_index = self_index;
    }

    scratch->boss_x.f.u = obj->pos.l.x.f.u;
    scratch->boss_x.f.l = 0;
    scratch->boss_y.f.u = obj->pos.l.y.f.u;
    scratch->boss_y.f.l = 0;
    obj->col_type = 0x0F;     // col_48x48 | col_boss
    obj->col_property = 8;    // obBossHits

    BossMarble_ShipMain(obj, scratch);
}

static void BMZ_ShipStart(Object *obj, Scratch_BossMarble *scratch) {
    uint8_t angle = scratch->sine_counter;
    scratch->sine_counter = (uint8_t)(scratch->sine_counter + 2);

    int16_t sin, cos;
    CalcSine(angle, &sin, &cos);
    obj->ysp = (int16_t)(sin >> 2);
    obj->xsp = (int16_t)-0x100;
    BossMove(obj, &scratch->boss_x, &scratch->boss_y);

    if (scratch->boss_x.f.u == (int16_t)(0x1800 + 0x110)) { // boss_mz_x+0x110
        obj->routine_sec += 2;
        obj->scratch.u8[0] = 0; // clear subtype
        obj->xsp = 0;
    }

    // Unlike GHZ's own bobbing (an overlay applied continuously in
    // ShipUpdate), MZ only uses CalcSine here during entry -- the lava
    // timer below is re-randomized every frame regardless, matching real
    // hardware exactly even though it looks redundant while still moving.
    scratch->lava_timer = (uint8_t)RandomNumber();

    BMZ_ShipUpdate(obj, scratch);
}

static void BMZ_Defeated(Object *obj, Scratch_BossMarble *scratch) {
    AddPoints(1000); // real ASM passes literal 100, but its own AddPoints takes points/10 -- this project's AddPoints takes the full displayed value
    // Real ASM's own comment here claims this sets "BMZ_Recover", but the
    // value actually written (4) selects BMZ_Explode -- BMZ_Explode is what
    // advances to Recover (6) once its own countdown finishes.
    obj->routine_sec = 4; // BMZ_Explode
    scratch->generic_timer = 180;
    obj->xsp = 0;
}

static void BMZ_ShipUpdate(Object *obj, Scratch_BossMarble *scratch) {
    obj->pos.l.y.f.u = scratch->boss_y.f.u;
    obj->pos.l.x.f.u = scratch->boss_x.f.u;

    if (obj->routine_sec >= 4)
        return; // exploding, recovering, or escaping -- skip hit detection

    if (obj->status.o.f.flag7) { // defeated flag
        BMZ_Defeated(obj, scratch);
        return;
    }
    if (obj->col_type != 0)
        return; // not currently hittable

    if (scratch->flash == 0) {
        scratch->flash = 0x28;
        QueueSound2(sfx_HitBoss);
    }

    dry_palette[1][1] = (dry_palette[1][1] == 0) ? 0x0EEE : 0;
    if (--scratch->flash != 0)
        return;
    obj->col_type = 0x0F; // col_48x48 | col_boss -- restore collision
}

static void BossMarble_MakeLava(Object *obj, Scratch_BossMarble *scratch) {
    if (scratch->lava_timer-- == 0) {
        Object *ball = FindFreeObj();
        if (ball != NULL) {
            ball->type = ObjId_LavaBall;
            ball->pos.l.y.f.u = (int16_t)(0x210 + 0xD8); // boss_mz_y+0xD8
            uint32_t rand = RandomNumber();
            ball->pos.l.x.f.u = (int16_t)(0x1800 + 0x78 + ((uint16_t)rand % 0x50)); // boss_mz_x+0x78, clamped to a 0x50-wide range
            ball->scratch.u8[0] = 0; // subtype -- LBall_RiseAndFall
            ((Scratch_LavaBall *)&ball->scratch)->from_boss = 0xFF;
        }
        scratch->lava_timer = (uint8_t)((RandomNumber() & 0x1F) + 0x40);
    }

    if (obj->status.o.f.x_flip) {
        if (scratch->boss_x.f.u < (int16_t)(0x1800 + 0x110)) // boss_mz_x+0x110
            return;
        scratch->boss_x.f.u = 0x1800 + 0x110;
    } else {
        if (scratch->boss_x.f.u > (int16_t)(0x1800 + 0x30)) // boss_mz_x+0x30
            return;
        scratch->boss_x.f.u = 0x1800 + 0x30;
    }

    obj->xsp = 0;
    obj->ysp = (int16_t)-0x180;
    if ((uint16_t)scratch->boss_y.f.u < (uint16_t)(0x210 + 0x1C)) // boss_mz_y+0x1C
        obj->ysp = 0x180;
    obj->scratch.u8[0] += 2;
}

static void BMZ_ChgDir(Object *obj, Scratch_BossMarble *scratch) {
    if (obj->xsp == 0) {
        uint16_t target = (uint16_t)(0x210 + 0x1C); // boss_mz_y+0x1C
        uint16_t by = (uint16_t)scratch->boss_y.f.u;
        if (by != target) {
            obj->ysp = (by < target) ? 0x40 : (int16_t)-0x40;
            BossMove(obj, &scratch->boss_x, &scratch->boss_y);
            return;
        }
        // Swoop -- start a horizontal+vertical dash in whichever direction
        // Eggman's currently facing.
        obj->xsp = 0x200;
        obj->ysp = 0x100;
        if (!obj->status.o.f.x_flip)
            obj->xsp = (int16_t)-obj->xsp;
    }

    if (scratch->flash >= 0x18) {
        // Recently hit -- freeze movement, but still keep dropping lava.
        BossMarble_MakeLava(obj, scratch);
        return;
    }
    BossMove(obj, &scratch->boss_x, &scratch->boss_y);
    obj->ysp -= 4; // curve the swoop into a U shape
    BossMarble_MakeLava(obj, scratch);
}

static void BMZ_DropFire(Object *obj, Scratch_BossMarble *scratch) {
    BossMove(obj, &scratch->boss_x, &scratch->boss_y);

    if (scratch->boss_y.f.u > (int16_t)(0x210 + 0x1C)) // boss_mz_y+0x1C -- still rising
        return;

    if (obj->ysp != 0) {
        obj->ysp = 0;
        scratch->generic_timer = 80;
        obj->status.o.f.x_flip = !obj->status.o.f.x_flip; // turn back to the screen bound

        Object *fire = FindFreeObj();
        if (fire != NULL) {
            fire->pos.l.x.f.u = scratch->boss_x.f.u;
            fire->pos.l.y.f.u = (int16_t)(scratch->boss_y.f.u + 0x18);
            fire->type = ObjId_BossFire;
            fire->scratch.u8[0] = 1; // subtype -- real fire, not a floor warning
        }
    }

    if (--scratch->generic_timer != 0)
        return;
    obj->scratch.u8[0] += 2;
}

static void BMZ_ShipMove(Object *obj, Scratch_BossMarble *scratch) {
    switch (obj->scratch.u8[0] & 6) {
    case 0: BMZ_ChgDir(obj, scratch); break;
    case 2: BMZ_DropFire(obj, scratch); break;
    case 4: BMZ_ChgDir(obj, scratch); break;
    case 6: BMZ_DropFire(obj, scratch); break;
    }
    obj->scratch.u8[0] &= 6;
    BMZ_ShipUpdate(obj, scratch);
}

static void BMZ_Explode(Object *obj, Scratch_BossMarble *scratch) {
    if (--scratch->generic_timer >= 0) {
        BossDefeated(obj);
        return;
    }

    obj->status.o.f.x_flip = true;
    obj->status.o.f.flag7 = false; // clear defeated flag (set in ReactToItem)
    obj->xsp = 0;
    obj->routine_sec += 2;
    scratch->generic_timer = -38;

    if (!boss_status)
        boss_status = 1;
}

static void BMZ_Recover(Object *obj, Scratch_BossMarble *scratch) {
    scratch->generic_timer++;

    if (scratch->generic_timer == 0) {
        obj->ysp = 0; // done falling
    } else if (scratch->generic_timer < 0) {
        if ((uint16_t)scratch->boss_y.f.u >= (uint16_t)(0x210 + 0x60)) { // boss_mz_y+0x60
            obj->ysp = 0;
        } else {
            obj->ysp = (int16_t)(obj->ysp + 0x18); // fall a little
        }
    } else if (scratch->generic_timer < 48) {
        obj->ysp = (int16_t)(obj->ysp - 8); // rise
    } else if (scratch->generic_timer == 48) {
        obj->ysp = 0;
        QueueSound1(bgm_MZ);
    } else if (scratch->generic_timer < 56) {
        // wait
    } else {
        obj->routine_sec += 2; // -> BMZ_Escape
    }

    BossMove(obj, &scratch->boss_x, &scratch->boss_y);
    BMZ_ShipUpdate(obj, scratch);
}

static void BMZ_Escape(Object *obj, Scratch_BossMarble *scratch) {
    obj->xsp = 0x500;          // move right quickly
    obj->ysp = (int16_t)-0x40; // move up a little

    if (limit_right2 != (uint16_t)(0x1800 + 0x160)) { // boss_mz_end
        limit_right2 += 2; // keep unlocking the screen bounds
    } else if (!obj->render.f.on_screen) {
        ObjectDelete(obj); // has Eggman left the screen?
        return;
    }

    BossMove(obj, &scratch->boss_x, &scratch->boss_y);
    BMZ_ShipUpdate(obj, scratch);
}

static void BossMarble_ShipMain(Object *obj, Scratch_BossMarble *scratch) {
    switch (obj->routine_sec) {
    case 0: BMZ_ShipStart(obj, scratch); break;
    case 2: BMZ_ShipMove(obj, scratch); break;
    case 4: BMZ_Explode(obj, scratch); break;
    case 6: BMZ_Recover(obj, scratch); break;
    case 8: BMZ_Escape(obj, scratch); break;
    }

    AnimateSprite(obj, Animation_Eggman);

    uint8_t flip = obj->status.b & 3;
    obj->render.b = (uint8_t)((obj->render.b & ~3) | flip);
    DisplaySprite(obj);
}

// Shared tail for the face/flame/tube sub-objects: syncs status/render flip
// bits from the ship and displays. Tube uses this directly (it never
// animates); face/flame go through BMZ_Display, which adds the animation
// step first.
static void BMZ_SetBits(Object *obj, Scratch_BossMarble *scratch) {
    Object *parent = &objects[scratch->parent_index];
    obj->pos.l.x.f.u = parent->pos.l.x.f.u;
    obj->pos.l.y.f.u = parent->pos.l.y.f.u;
    obj->status.b = parent->status.b;

    uint8_t flip = obj->status.b & 3;
    obj->render.b = (uint8_t)((obj->render.b & ~3) | flip);
    DisplaySprite(obj);
}

static void BMZ_Display(Object *obj, Scratch_BossMarble *scratch) {
    AnimateSprite(obj, Animation_Eggman);
    BMZ_SetBits(obj, scratch);
}

static void BossMarble_FaceMain(Object *obj, Scratch_BossMarble *scratch) {
    Object *parent = &objects[scratch->parent_index];

    int d0 = parent->routine_sec;
    int d1 = 1; // facenormal1

    d0 -= 2;
    if (d0 != 0)
        goto checkSpecial;
    if (!(parent->scratch.u8[0] & 2))
        goto checkHitState;
    if (parent->ysp != 0)
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
    if (parent->col_type != 0)
        goto checkSonicState;
    d1 = 5; // facehit
    goto writeAnim;

checkSonicState:
    if (player->routine < 4)
        goto writeAnim;
    d1 = 4; // facelaugh

writeAnim:
    obj->anim = (uint8_t)d1;

    d0 -= 4;
    if (d0 != 0) {
        BMZ_Display(obj, scratch);
        return;
    }
    obj->anim = 6; // facepanic
    if (!obj->render.f.on_screen) {
        ObjectDelete(obj);
        return;
    }
    BMZ_Display(obj, scratch);
}

static void BossMarble_FlameMain(Object *obj, Scratch_BossMarble *scratch) {
    obj->anim = 7; // blank (default invisible state)

    Object *parent = &objects[scratch->parent_index];

    if (parent->routine_sec >= 8) { // Escape
        obj->anim = 0xB; // escapeflame -- thruster animation for takeoff
        if (!obj->render.f.on_screen) {
            ObjectDelete(obj);
            return;
        }
        BMZ_Display(obj, scratch);
        return;
    }

    if (parent->xsp != 0)
        obj->anim = 8; // flame

    BMZ_Display(obj, scratch);
}

static void BossMarble_TubeMain(Object *obj, Scratch_BossMarble *scratch) {
    Object *parent = &objects[scratch->parent_index];

    if (parent->routine_sec == 8) { // Escape
        if (!obj->render.f.on_screen) {
            ObjectDelete(obj);
            return;
        }
    }

    obj->mappings = Mappings_BossItems;
    obj->tile = TILE_MAP(0, 1, 0, 0, ArtTile_Eggman_Weapons); // | Tile_Pal2
    obj->frame = 4; // tube frame (Boss Items.asm)
    BMZ_SetBits(obj, scratch);
}

void Obj_BossMarble(Object *obj) {
    Scratch_BossMarble *scratch = (Scratch_BossMarble *)&obj->scratch;

    switch (obj->routine) {
    case 0: BossMarble_Main(obj, scratch); break;
    case 2: BossMarble_ShipMain(obj, scratch); break;
    case 4: BossMarble_FaceMain(obj, scratch); break;
    case 6: BossMarble_FlameMain(obj, scratch); break;
    case 8: BossMarble_TubeMain(obj, scratch); break;
    }
}
