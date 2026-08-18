#include "BossGreenHill.h"

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

static void BGHZ_ShipMain(Object *obj, Scratch_BossGreenHill *scratch);
static void BGHZ_ShipUpdate(Object *obj, Scratch_BossGreenHill *scratch);

static void BGHZ_Main(Object *obj, Scratch_BossGreenHill *scratch) {
    uint8_t self_index = (uint8_t)(obj - objects);

    for (int i = 0; i < 3; i++) {
        Object *sub;
        if (i == 0) {
            sub = obj;
        } else {
            sub = FindNextFreeObj(obj);
            if (sub == NULL)
                break;
        }

        sub->routine = BGHZ_ObjData[i].routine;
        sub->type = ObjId_BossGreenHill;
        sub->pos.l.x.f.u = obj->pos.l.x.f.u;
        sub->pos.l.y.f.u = obj->pos.l.y.f.u;
        sub->mappings = Mappings_Eggman;
        sub->tile = TILE_MAP(0, 0, 0, 0, ArtTile_Eggman);
        sub->render.f.align_fg = true;
        sub->width_pixels = 64 / 2;
        sub->priority = 3;
        sub->anim = BGHZ_ObjData[i].anim;

        ((Scratch_BossGreenHill *)&sub->scratch)->parent_index = self_index;
    }

    scratch->boss_x.f.u = obj->pos.l.x.f.u;
    scratch->boss_x.f.l = 0;
    scratch->boss_y.f.u = obj->pos.l.y.f.u;
    scratch->boss_y.f.l = 0;
    obj->col_type = 0x0F; // col_48x48 | col_boss
    obj->col_property = 8; // obBossHits

    BGHZ_ShipMain(obj, scratch);
}

static void BGHZ_ShipStart(Object *obj, Scratch_BossGreenHill *scratch) {
    obj->ysp = 0x100; // move ship down
    BossMove(obj, &scratch->boss_x, &scratch->boss_y);
    if (scratch->boss_y.f.u != (int16_t)(0x300 + 0x38)) { // boss_ghz_y+0x38
        BGHZ_ShipUpdate(obj, scratch);
        return;
    }
    obj->ysp = 0;
    obj->routine_sec += 2;
    BGHZ_ShipUpdate(obj, scratch);
}

static void BGHZ_Defeated(Object *obj, Scratch_BossGreenHill *scratch) {
    AddPoints(1000); // real ASM passes literal 100, but its own AddPoints takes points/10 -- this project's AddPoints takes the full displayed value
    obj->routine_sec = 8; // BGHZ_Explode
    scratch->generic_timer = 0xB3;
}

static void BGHZ_ShipUpdate(Object *obj, Scratch_BossGreenHill *scratch) {
    int16_t sin, cos;
    CalcSine(scratch->sine_counter, &sin, &cos);
    int16_t bob = (int16_t)(sin >> 6);
    obj->pos.l.y.f.u = (int16_t)(scratch->boss_y.f.u + bob);
    obj->pos.l.x.f.u = scratch->boss_x.f.u;
    scratch->sine_counter = (uint8_t)(scratch->sine_counter + 2);

    if (obj->routine_sec >= 8)
        return; // exploding -- skip hit detection

    if (obj->status.o.f.flag7) { // defeated flag
        BGHZ_Defeated(obj, scratch);
        return;
    }
    if (obj->col_type != 0)
        return; // not currently hittable

    if (scratch->flash == 0) {
        scratch->flash = 0x20;
        QueueSound2(sfx_HitBoss);
    }

    // BGHZ_ShipFlash
    dry_palette[1][1] = (dry_palette[1][1] == 0) ? 0x0EEE : 0;
    if (--scratch->flash != 0)
        return;
    obj->col_type = 0x0F; // col_48x48 | col_boss -- restore collision
}

static void BGHZ_MakeBall(Object *obj, Scratch_BossGreenHill *scratch) {
    obj->xsp = (int16_t)-0x100;
    obj->ysp = (int16_t)-0x40;
    BossMove(obj, &scratch->boss_x, &scratch->boss_y);

    if (scratch->boss_x.f.u != (int16_t)(0x2960 + 0xA0)) { // boss_ghz_x+0xA0
        BGHZ_ShipUpdate(obj, scratch);
        return;
    }

    obj->xsp = 0;
    obj->ysp = 0;
    obj->routine_sec += 2; // -> BGHZ_ShipMove

    Object *ball = FindFreeObj();
    if (ball != NULL) {
        ball->type = ObjId_BossBall;
        ball->pos.l.x.f.u = scratch->boss_x.f.u;
        ball->pos.l.y.f.u = scratch->boss_y.f.u;
        ((Scratch_BossBall *)&ball->scratch)->parent_index = (uint8_t)(obj - objects);
    }
    scratch->generic_timer = 120 - 1;

    BGHZ_ShipUpdate(obj, scratch);
}

static void BGHZ_Reverse(Object *obj, Scratch_BossGreenHill *scratch) {
    if (!obj->status.o.f.x_flip)
        obj->xsp = (int16_t)-obj->xsp;
    BGHZ_ShipUpdate(obj, scratch);
}

static void BGHZ_ShipMove(Object *obj, Scratch_BossGreenHill *scratch) {
    if (--scratch->generic_timer >= 0) {
        BGHZ_Reverse(obj, scratch);
        return;
    }
    obj->routine_sec += 2; // -> BGHZ_ChgDir
    scratch->generic_timer = 0x40 - 1;
    obj->xsp = 0x100;
    if (scratch->boss_x.f.u != (int16_t)(0x2960 + 0xA0)) {
        BGHZ_Reverse(obj, scratch);
        return;
    }
    scratch->generic_timer = (0x40 * 2) - 1;
    obj->xsp = 0x40;
    BGHZ_Reverse(obj, scratch);
}

static void BGHZ_ChgDir(Object *obj, Scratch_BossGreenHill *scratch) {
    if (--scratch->generic_timer >= 0) {
        BossMove(obj, &scratch->boss_x, &scratch->boss_y);
        BGHZ_ShipUpdate(obj, scratch);
        return;
    }
    obj->status.o.f.x_flip = !obj->status.o.f.x_flip;
    scratch->generic_timer = 64 - 1;
    obj->routine_sec -= 2; // -> BGHZ_ShipMove
    obj->xsp = 0;
    BGHZ_ShipUpdate(obj, scratch);
}

static void BGHZ_Explode(Object *obj, Scratch_BossGreenHill *scratch) {
    if (--scratch->generic_timer >= 0) {
        BossDefeated(obj);
        return;
    }

    obj->status.o.f.x_flip = true;
    obj->status.o.f.flag7 = false; // clear defeated flag (set in ReactToItem)
    obj->xsp = 0;
    obj->routine_sec += 2; // -> BGHZ_Recover
    scratch->generic_timer = -38;

    if (!boss_status)
        boss_status = 1;
}

static void BGHZ_Recover(Object *obj, Scratch_BossGreenHill *scratch) {
    scratch->generic_timer++;

    if (scratch->generic_timer == 0) {
        obj->ysp = 0; // done falling
    } else if (scratch->generic_timer < 0) {
        obj->ysp = (int16_t)(obj->ysp + 0x18); // fall a little
    } else if (scratch->generic_timer < 0x30) {
        obj->ysp = (int16_t)(obj->ysp - 8); // rise
    } else if (scratch->generic_timer == 0x30) {
        obj->ysp = 0;
        QueueSound1(bgm_GHZ);
    } else if (scratch->generic_timer < 0x38) {
        // wait
    } else {
        obj->routine_sec += 2; // -> BGHZ_Escape
    }

    BossMove(obj, &scratch->boss_x, &scratch->boss_y);
    BGHZ_ShipUpdate(obj, scratch);
}

static void BGHZ_Escape(Object *obj, Scratch_BossGreenHill *scratch) {
    obj->xsp = 0x400; // move right quickly
    obj->ysp = (int16_t)-0x40; // move up a little

    if (limit_right2 != (uint16_t)(0x2960 + 0x160)) { // boss_ghz_end
        limit_right2 += 2; // keep unlocking the screen bounds
    } else if (!obj->render.f.on_screen) {
        ObjectDelete(obj); // has Eggman left the screen?
        return;
    }

    BossMove(obj, &scratch->boss_x, &scratch->boss_y);
    BGHZ_ShipUpdate(obj, scratch);
}

static void BGHZ_ShipMain(Object *obj, Scratch_BossGreenHill *scratch) {
    switch (obj->routine_sec) {
    case 0: BGHZ_ShipStart(obj, scratch); break;
    case 2: BGHZ_MakeBall(obj, scratch); break;
    case 4: BGHZ_ShipMove(obj, scratch); break;
    case 6: BGHZ_ChgDir(obj, scratch); break;
    case 8: BGHZ_Explode(obj, scratch); break;
    case 10: BGHZ_Recover(obj, scratch); break;
    case 12: BGHZ_Escape(obj, scratch); break;
    }

    AnimateSprite(obj, Animation_Eggman);

    uint8_t flip = obj->status.b & 3;
    obj->render.b = (uint8_t)((obj->render.b & ~3) | flip);
    DisplaySprite(obj);
}

// Shared display tail for the face and flame sub-objects: syncs
// position/status from the ship, animates, and displays.
static void BGHZ_Display(Object *obj, Scratch_BossGreenHill *scratch) {
    Object *parent = &objects[scratch->parent_index];
    obj->pos.l.x.f.u = parent->pos.l.x.f.u;
    obj->pos.l.y.f.u = parent->pos.l.y.f.u;
    obj->status.b = parent->status.b;

    AnimateSprite(obj, Animation_Eggman);

    uint8_t flip = obj->status.b & 3;
    obj->render.b = (uint8_t)((obj->render.b & ~3) | flip);
    DisplaySprite(obj);
}

static void BGHZ_FaceMain(Object *obj, Scratch_BossGreenHill *scratch) {
    Object *parent = &objects[scratch->parent_index];
    Scratch_BossGreenHill *pscratch = (Scratch_BossGreenHill *)&parent->scratch;

    int d0 = parent->routine_sec;
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
    if (parent->col_type == 0) {
        d1 = 5; // facehit
        goto writeAnim;
    }
    if (player->routine < 4)
        goto writeAnim;
    d1 = 4; // facelaugh

writeAnim:
    obj->anim = (uint8_t)d1;

    d0 -= 2;
    if (d0 != 0) {
        BGHZ_Display(obj, scratch);
        return;
    }
    obj->anim = 6; // facepanic
    if (!obj->render.f.on_screen) {
        ObjectDelete(obj);
        return;
    }
    BGHZ_Display(obj, scratch);
}

static void BGHZ_FlameMain(Object *obj, Scratch_BossGreenHill *scratch) {
    obj->anim = 7; // blank (default invisible state)

    Object *parent = &objects[scratch->parent_index];

    if (parent->routine_sec == 12) { // Escape
        obj->anim = 0xB; // escapeflame -- thruster animation for takeoff
        if (!obj->render.f.on_screen) {
            ObjectDelete(obj);
            return;
        }
        BGHZ_Display(obj, scratch);
        return;
    }

    if (parent->xsp != 0)
        obj->anim = 8; // flame

    BGHZ_Display(obj, scratch);
}

void Obj_BossGreenHill(Object *obj) {
    Scratch_BossGreenHill *scratch = (Scratch_BossGreenHill *)&obj->scratch;

    switch (obj->routine) {
    case 0: BGHZ_Main(obj, scratch); break;
    case 2: BGHZ_ShipMain(obj, scratch); break;
    case 4: BGHZ_FaceMain(obj, scratch); break;
    case 6: BGHZ_FlameMain(obj, scratch); break;
    }
}
