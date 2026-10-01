#include "BossFinal.h"

#include <string.h>

#include "Game.h"
#include "Level.h"
#include "LevelScroll.h"
#include "MathUtil.h"
#include "Object/Sonic.h"
#include "PLC.h"
#include "Resource/Animation/FZEggInShip.h"
#include "Resource/Animation/PlasmaBallLauncher.h"
#include "Resource/Animation/PlasmaBalls.h"
#include "Resource/Mappings/FZDamagedEggmobile.h"
#include "Resource/Mappings/FZEggmanCylinders.h"
#include "Resource/Mappings/FZEggmobileLegs.h"
#include "Resource/Mappings/PlasmaBallLauncher.h"
#include "Resource/Mappings/PlasmaBalls.h"
#include "Sound.h"

// Eggman's ship graphics and animation are owned by BossGreenHill.c, the SBZ2 cutscene Eggman's by ScrapEggman.c --
// bare externs here, same as BossLabyrinth.c.
extern const uint8_t Animation_Eggman[];
extern const uint8_t Mappings_Eggman[];
extern const uint8_t Animation_ScrapEggman[];
extern const uint8_t Mappings_ScrapEggman[];

// boss_fz_x = 0x2450, boss_fz_y = 0x510, boss_fz_end = boss_fz_x+0x2B0 (real s1disasm _Constants.asm).
#define FZ_X   0x2450
#define FZ_Y   0x510
#define FZ_END (FZ_X + 0x2B0)

#define SOLID_SONIC_WIDTH 11 // sonic_solid_width

// Boss phases (the Eggman object's own link byte).
enum {
    PH_WAIT = 0, PH_CRUSH = 2, PH_PLASMA = 4, PH_FALL = 6, PH_RUN = 8, PH_JUMP = 0xA, PH_SHIP = 0xC, PH_ESCAPE = 0xE,
};

static Scratch_BossFinal *BF(Object *o) { return (Scratch_BossFinal *)&o->scratch; }

static const struct { int16_t x, y; uint16_t tile; const uint8_t *mappings; } BF_ObjData[6] = {
    { 0x100, 0x100, ArtTile_FZ_Eggman_No_Vehicle, Mappings_ScrapEggman },        // Eggman himself
    { FZ_X + 0x160, FZ_Y + 0x80, ArtTile_FZ_Boss, Mappings_FZEggmanCylinders },  // panel
    { FZ_X + 0x290, FZ_Y + 0x86, ArtTile_FZ_Eggman_Fleeing, Mappings_FZEggmobileLegs },
    { FZ_X + 0x290, FZ_Y + 0x86, ArtTile_FZ_Eggman_No_Vehicle, Mappings_ScrapEggman }, // cockpit
    { FZ_X + 0x290, FZ_Y + 0x86, ArtTile_Eggman, Mappings_Eggman },              // empty ship
    { FZ_X + 0x290, FZ_Y + 0x86, ArtTile_Eggman, Mappings_Eggman },              // flame
};

static const struct { uint8_t routine, anim, priority, width, height; } BF_ObjData2[6] = {
    { 2, 0, 4, 64 / 2, 50 / 2 },
    { 4, 0, 1, 36 / 2, 16 / 2 },
    { 6, 0, 3, 40 / 2, 24 / 2 }, // legs, cockpit and flame get real sizes (the original gives them none, so culling is iffy)
    { 8, 0, 3, 64 / 2, 56 / 2 },
    { 0xA, 0, 3, 64 / 2, 64 / 2 },
    { 0xC, 0, 3, 16 / 2, 6 / 2 },
};

// Cylinder pairs that extend together: {the one Eggman hides in, the decoy}.
static const uint8_t BF_CylinderPairs[4][2] = { { 0, 1 }, { 1, 2 }, { 2, 3 }, { 3, 0 } };

static bool BossFinal_EggmanRoutine(Object *obj, Scratch_BossFinal *s);

static void BossFinal_Main(Object *obj, Scratch_BossFinal *s) {
    memset(&obj->scratch, 0, sizeof(obj->scratch));
    uint8_t self_index = (uint8_t)(obj - objects);

    for (int i = 0; i < 6; i++) {
        Object *o = obj;
        if (i > 0) {
            o = FindNextFreeObj(obj);
            if (o == NULL)
                break;
            memset(&o->scratch, 0, sizeof(o->scratch));
        }
        o->type = ObjId_BossFinal;
        o->pos.l.x.f.u = BF_ObjData[i].x;
        o->pos.l.y.f.u = BF_ObjData[i].y;
        o->tile = TILE_MAP(0, 0, 0, 0, BF_ObjData[i].tile);
        o->mappings = BF_ObjData[i].mappings;
        o->routine = BF_ObjData2[i].routine;
        o->anim = BF_ObjData2[i].anim;
        // Force AnimateSprite's reset guard on the first call (see BossSpringYard.c).
        o->prev_anim = (uint8_t)(BF_ObjData2[i].anim + 1);
        o->anim_frame = 0;
        o->frame_time.w = 0;
        o->frame = 0;
        o->priority = BF_ObjData2[i].priority;
        o->width_pixels = BF_ObjData2[i].width;
        o->y_rad = (int8_t)BF_ObjData2[i].height;
        o->render.b = 0;
        o->render.f.align_fg = true;
        obj->render.f.on_screen = true; // the real ASM sets this on the boss itself, every iteration
        BF(o)->link = self_index;
    }

    // The plasma launcher, then the 4 cylinders.
    Object *plasma = FindFreeObj();
    if (plasma != NULL) {
        plasma->type = ObjId_BossPlasma;
        s->plasma_index = (uint8_t)(plasma - objects);
        ((Scratch_BossPlasma *)&plasma->scratch)->parent_index = self_index;

        for (int i = 0; i < 4; i++) {
            Object *cyl = FindNextFreeObj(obj);
            if (cyl == NULL)
                break;
            s->cylinders[i] = (uint8_t)(cyl - objects);
            cyl->type = ObjId_EggmanCylinder;
            ((Scratch_EggmanCylinder *)&cyl->scratch)->parent_index = self_index;
            ((Scratch_EggmanCylinder *)&cyl->scratch)->number = (uint8_t)(i * 2);
        }
    }

    ending_eggmobile_exploding = 0; // a fresh fight: Eggman hasn't been caught yet
    s->link = PH_WAIT; // the boss's own phase (this offset held its parent pointer in the original)
    obj->col_property = 8; // obBossHits
    s->attack_state = -1;

    if (!BossFinal_EggmanRoutine(obj, s))
        DisplaySprite(obj);
}

// ---------------------------------------------------------------------------
// Eggman himself (routine 2)
// ---------------------------------------------------------------------------

static void BF_Animate(Object *obj) { AnimateSprite(obj, Animation_ScrapEggman); }

// Keeps the screen's right edge opening up and, until the ship phase, keeps the whole thing solid.
static void BF_ScreenScroll(Object *obj, Scratch_BossFinal *s) {
    if (limit_right2 < FZ_END)
        limit_right2 += 2;
    if (s->link >= PH_SHIP)
        return;
    SolidObject(obj, 32 / 2 + SOLID_SONIC_WIDTH, 224 / 2, 226 / 2, obj->pos.l.x.f.u, NULL, NULL);
}

static void BF_Animate_And_Scroll(Object *obj, Scratch_BossFinal *s) {
    BF_Animate(obj);
    BF_ScreenScroll(obj, s);
}

static void BF_Wait(Scratch_BossFinal *s) {
    if (plc_buffer[0].art == NULL && (uint16_t)scrpos_x.f.u >= (uint16_t)(FZ_X - SCREEN_WIDEADD2))
        s->link += 2; // art is loaded and the screen has reached the arena
    random_seed.v += 1; // seed the RNG for the fight
}

static void BF_Crush(Object *obj, Scratch_BossFinal *s) {
    if (s->attack_state < 0) { // pick a pair of cylinders
        s->attack_state = 0;
        uint32_t rnd = RandomNumber();
        int d0 = (int)(rnd & 0xC); // 0, 4, 8 or 12: which pair
        int d1 = d0 + 2;           // its second entry
        if ((int32_t)rnd < 0) {    // top random bit: swap which cylinder Eggman hides in
            int t = d0;
            d0 = d1;
            d1 = t;
        }
        uint8_t eggman_cyl = BF_CylinderPairs[d0 >> 2][(d0 >> 1) & 1];
        uint8_t decoy_cyl = BF_CylinderPairs[d1 >> 2][(d1 >> 1) & 1];
        s->attack_state = (int16_t)(eggman_cyl * 2); // (any value >= 0: "a pair has been picked")

        Object *a = &objects[s->cylinders[eggman_cyl]];
        BF(a)->child_cmd = 0xFF; // extend
        ((Scratch_EggmanCylinder *)&a->scratch)->has_eggman = -1;
        Object *b = &objects[s->cylinders[decoy_cyl]];
        BF(b)->child_cmd = 1; // extend
        ((Scratch_EggmanCylinder *)&b->scratch)->has_eggman = 0;

        s->child_counter = 1; // (it counts down through 0 to -1 as the two cylinders finish)
        s->hit_flash = 0;
        QueueSound2(sfx_Rumbling);
    }

    if (s->child_counter < 0) { // both cylinders are back home
        if (obj->col_property == 0) { // defeated
            AddPoints(100); // 1000 points (AddPoints counts in tens, like the score)
            s->link = PH_FALL;
            obj->pos.l.x.f.u = FZ_X + 0x170;
            obj->pos.l.y.f.u = FZ_Y + 0x2C;
            obj->y_rad = 40 / 2;
        } else {
            s->link += 2; // the plasma attack
            s->attack_state = -1;
            s->child_counter = 0;
        }
        return;
    }

    // Eggman is riding inside the cylinder (it writes his position); face Sonic and see whether he's being hit.
    obj->status.o.f.x_flip = ((uint16_t)player->pos.l.x.f.u >= (uint16_t)obj->pos.l.x.f.u);
    int32_t side = SolidObject(obj, 64 / 2 + SOLID_SONIC_WIDTH, 40 / 2, 40 / 2, obj->pos.l.x.f.u, NULL, NULL);

    bool hit = false;
    if (side > 0) { // Sonic is against the side of the piston
        random_seed.v += 0x70000; // addq.w #7 on the high word of the seed
        if (player->anim == SonAnimId_Roll) {
            int16_t kick = 0x300;
            if (!obj->status.o.f.x_flip)
                kick = -kick;
            player->xsp = kick; // bounce Sonic back
            hit = true;
        }
    }

    if (hit && s->hit_flash == 0 && obj->col_property != 0) { // FixBugs forced: no hit-counter underflow on defeat
        obj->col_property--;
        s->hit_flash = 100;
        QueueSound2(sfx_HitBoss);
    }

    if (s->hit_flash != 0) {
        if (--s->hit_flash != 0) {
            obj->anim = 3; // hurt
            BF_Animate(obj);
            return;
        }
    }
    if (obj->col_property != 0)
        obj->anim = 1; // laughing
    BF_Animate(obj);
}

static void BF_Plasma(Scratch_BossFinal *s) {
    Object *launcher = &objects[s->plasma_index];
    if (s->attack_state < 0) { // fire
        s->attack_state = 0;
        BF(launcher)->child_cmd = 0xFF;
        QueueSound2(sfx_Electric);
    }
    if ((frame_count & 0xF) == 0)
        QueueSound2(sfx_Electric);

    if (s->child_counter != 0) { // the launcher sets this (to -1) once the plasma is all gone
        s->link -= 2;            // back to the cylinders
        s->attack_state = -1;
        s->child_counter = 0;
    }
}

static void BF_Fall(Object *obj, Scratch_BossFinal *s) {
    obj->width_pixels = 96 / 2;
    obj->status.o.f.x_flip = true;
    SpeedToPos(obj);
    obj->frame = 6;
    obj->ysp = (int16_t)(obj->ysp + 0x10);
    if ((uint16_t)obj->pos.l.y.f.u >= FZ_Y + 0x8C) {
        obj->pos.l.y.f.u = FZ_Y + 0x8C;
        s->link += 2; // Run
        obj->width_pixels = 64 / 2;
        obj->xsp = 0x100;
        obj->ysp = -0x100;
        dle_routine += 2; // DLE_FZ_End: scrolling right is allowed now
    }
    BF_ScreenScroll(obj, s);
}

static void BF_Run(Object *obj, Scratch_BossFinal *s) {
    obj->status.o.f.x_flip = true;
    obj->anim = 4;
    SpeedToPos(obj);
    obj->ysp = (int16_t)(obj->ysp + 0x10);
    if ((uint16_t)obj->pos.l.y.f.u >= FZ_Y + 0x93)
        obj->ysp = -0x40; // bounce upwards slightly

    // Run right, slowing down the further ahead of Sonic he gets.
    obj->xsp = 0x400;
    int16_t gap = (int16_t)(obj->pos.l.x.f.u - player->pos.l.x.f.u);
    if (gap < 0) {
        obj->xsp = 0x500; // Sonic caught up: run faster
    } else {
        uint16_t d = (uint16_t)gap;
        do {
            if (d < 0x70) break;
            d -= 0x70;
            obj->xsp = (int16_t)(obj->xsp - 0x100);
            if (d < 8) break;
            d -= 8;
            obj->xsp = (int16_t)(obj->xsp - 0x100);
            if (d < 8) break;
            d -= 8;
            obj->xsp = (int16_t)(obj->xsp - 0x80);
            if (d < 8) break;
            d -= 8;
            obj->xsp = (int16_t)(obj->xsp - 0x80);
            if (d < 8) break;
            d -= 8;
            obj->xsp = (int16_t)(obj->xsp - 0x80);
            if (d < 0x38) break;
            obj->xsp = 0; // far ahead: stop
        } while (0);
    }

    if ((uint16_t)obj->pos.l.x.f.u >= FZ_X + 0x250) {
        obj->pos.l.x.f.u = FZ_X + 0x250;
        obj->xsp = 0x240; // jump into the ship
        obj->ysp = -0x4C0;
        s->link += 2;
    }
    BF_Animate_And_Scroll(obj, s);
}

static void BF_Jump(Object *obj, Scratch_BossFinal *s) {
    SpeedToPos(obj);
    if ((uint16_t)obj->pos.l.x.f.u >= FZ_X + 0x290)
        obj->xsp = 0;
    obj->ysp = (int16_t)(obj->ysp + 0x34);
    if (obj->ysp >= 0 && (uint16_t)obj->pos.l.y.f.u >= FZ_Y + 0x82) {
        obj->pos.l.y.f.u = FZ_Y + 0x82;
        obj->ysp = 0;
    }
    if ((obj->xsp | obj->ysp) == 0) { // landed in the ship
        s->link += 2;
        obj->ysp = -0x180;
        obj->col_property = 1; // escaping Eggman takes a single hit
    }
    BF_Animate_And_Scroll(obj, s);
}

static void BF_Ship(Object *obj, Scratch_BossFinal *s) {
    obj->mappings = Mappings_Eggman;
    obj->tile = TILE_MAP(0, 0, 0, 0, ArtTile_Eggman);
    obj->anim = 0;
    obj->status.o.f.x_flip = true;
    SpeedToPos(obj);
    if ((uint16_t)obj->pos.l.y.f.u < FZ_Y + 0x34) { // risen high enough
        obj->xsp = 0x180;
        obj->ysp = -0x18;
        obj->col_type = 0x0F; // col_48x48 | col_boss
        s->link += 2;
    }
    BF_Animate_And_Scroll(obj, s);
}

// Returns true if the boss deleted itself (and started the ending).
static bool BF_Escape(Object *obj, Scratch_BossFinal *s) {
    obj->status.o.f.x_flip = true;
    SpeedToPos(obj);

    bool skip_to_watch = false;
    if (s->attack_state == 0) { // escape timer expired / not running
        if (obj->col_type == 0) { // hit: flash, then either fall (defeated) or recover
            s->attack_state = 30;
            QueueSound2(sfx_HitBoss);
        } else {
            skip_to_watch = true;
        }
    }
    if (!skip_to_watch) {
        if (--s->attack_state == 0) {
            if (obj->status.o.f.flag7) { // defeated: sink slowly
                ending_eggmobile_exploding = 1; // the ending shows the unused exploding Eggmobile in its background
                obj->ysp = 0x60;
                s->attack_state = -1; // FixBugs: stop the timer re-arming
            } else {
                obj->col_type = 0x0F;
            }
        }
    }

    // Keep Sonic from running past the end, and freeze him once Eggman has gotten away.
    if (player->pos.l.x.f.u >= FZ_END + 0x90) {
        lock_ctrl = 1;
        jpad1_hold2 = 0;
        jpad1_press2 = 0;
        player->inertia = 0;
        if (obj->ysp < 0)
            jpad1_hold2 = JPAD_UP; // look up as he escapes
    }
    if (player->pos.l.x.f.u >= FZ_END + 0xE0)
        player->pos.l.x.f.u = FZ_END + 0xE0; // the ledge

    if ((uint16_t)obj->pos.l.x.f.u >= FZ_END + 0x200 && !obj->render.f.on_screen) {
        gamemode = GameMode_Ending; // (the ending isn't ported yet, so this lands back at the Sega screen)
        ObjectDelete(obj);
        return true;
    }
    BF_Animate(obj);
    return false;
}

// Returns true if the boss deleted itself.
static bool BossFinal_EggmanRoutine(Object *obj, Scratch_BossFinal *s) {
    switch (s->link) {
    case PH_WAIT:   BF_Wait(s); break;
    case PH_CRUSH:  BF_Crush(obj, s); break;
    case PH_PLASMA: BF_Plasma(s); break;
    case PH_FALL:   BF_Fall(obj, s); break;
    case PH_RUN:    BF_Run(obj, s); break;
    case PH_JUMP:   BF_Jump(obj, s); break;
    case PH_SHIP:   BF_Ship(obj, s); break;
    case PH_ESCAPE:
        if (BF_Escape(obj, s))
            return true;
        break;
    }
    return false;
}

static void BossFinal_Eggman(Object *obj, Scratch_BossFinal *s) {
    if (!BossFinal_EggmanRoutine(obj, s))
        DisplaySprite(obj);
}

// ---------------------------------------------------------------------------
// The helpers: panel (4), legs (6), cockpit (8), empty ship (A), flame (C)
// ---------------------------------------------------------------------------

static void BF_ChildDisplay(Object *obj, Object *boss) {
    obj->status.b = boss->status.b;
    uint8_t flip = obj->status.b & 3;
    obj->render.b = (uint8_t)((obj->render.b & ~3) | flip);
    DisplaySprite(obj);
}

static void BF_FlamePos(Object *obj, Object *boss) {
    obj->pos.l.x.f.u = boss->pos.l.x.f.u;
    obj->pos.l.y.f.u = boss->pos.l.y.f.u;
    BF_ChildDisplay(obj, boss);
}

// Is the boss still around? (Real ASM compares the first byte of the parent object with its own.)
static Object *BF_Parent(Object *obj, Scratch_BossFinal *s) {
    Object *boss = &objects[s->link];
    return (boss->type == obj->type && boss != obj) ? boss : NULL;
}

static void BF_Panel(Object *obj, Scratch_BossFinal *s) {
    Object *boss = &objects[s->link];
    obj->frame = 0xB;
    if ((uint16_t)player->pos.l.x.f.u >= (uint16_t)obj->pos.l.x.f.u && !obj->render.f.on_screen) {
        ObjectDelete(obj); // Sonic is past it and it's off screen
        return;
    }
    (void)boss;
    DisplaySprite(obj);
}

static void BF_Legs(Object *obj, Scratch_BossFinal *s) {
    obj->status.o.f.x_flip = true;
    Object *boss = &objects[s->link];
    if (boss->mappings != Mappings_Eggman) { // not in the ship yet: just stand there
        BF_ChildDisplay(obj, boss);
        return;
    }
    obj->pos.l.x.f.u = boss->pos.l.x.f.u; // the ship has taken off: the legs retract with it, then go
    obj->pos.l.y.f.u = boss->pos.l.y.f.u;
    if (obj->frame_time.b == 0)
        obj->frame_time.b = 0x14;
    if (--obj->frame_time.b <= 0) {
        if (++obj->frame > 2) {
            ObjectDelete(obj);
            return;
        }
    }
    BF_FlamePos(obj, boss);
}

static void BF_Cockpit(Object *obj, Scratch_BossFinal *s) {
    Object *boss = BF_Parent(obj, s);
    if (boss == NULL) {
        ObjectDelete(obj);
        return;
    }
    if (boss->mappings != Mappings_Eggman) { // before the ship phase: the empty cockpit
        obj->frame = 0xA;
        BF_ChildDisplay(obj, boss);
        return;
    }
    obj->anim = 1; // normal face
    if ((int8_t)boss->col_property <= 0) { // defeated: the cockpit explodes
        BossDefeated(obj);
        obj->priority = 2;
        obj->anim = 0;
        obj->mappings = Mappings_FZDamagedEggmobile;
        obj->tile = TILE_MAP(0, 0, 0, 0, ArtTile_FZ_Eggman_Fleeing);
        AnimateSprite(obj, Animation_FZEggInShip);
        BF_FlamePos(obj, boss);
        return;
    }
    obj->anim = 6; // panic face
    obj->mappings = Mappings_Eggman;
    obj->tile = TILE_MAP(0, 0, 0, 0, ArtTile_Eggman);
    AnimateSprite(obj, Animation_Eggman);
    BF_FlamePos(obj, boss);
}

static void BF_EmptyShip(Object *obj, Scratch_BossFinal *s) {
    obj->frame = 0;
    obj->status.o.f.x_flip = true;
    Object *boss = &objects[s->link];
    if (BF(boss)->link == PH_SHIP && boss->mappings == Mappings_Eggman) {
        ObjectDelete(obj);
        return;
    }
    BF_ChildDisplay(obj, boss);
}

static void BF_Flame(Object *obj, Scratch_BossFinal *s) {
    Object *boss = BF_Parent(obj, s);
    if (boss == NULL) {
        ObjectDelete(obj);
        return;
    }
    obj->anim = 7; // flame off
    if (BF(boss)->link < PH_SHIP) {
        BF_ChildDisplay(obj, boss);
        return;
    }
    if (boss->xsp != 0)
        obj->anim = 0xB; // flame on while he's moving
    AnimateSprite(obj, Animation_Eggman);
    BF_FlamePos(obj, boss);
}

void Obj_BossFinal(Object *obj) {
    Scratch_BossFinal *s = BF(obj);

    switch (obj->routine) {
    case 0: BossFinal_Main(obj, s); break;
    case 2: BossFinal_Eggman(obj, s); break;
    case 4: BF_Panel(obj, s); break;
    case 6: BF_Legs(obj, s); break;
    case 8: BF_Cockpit(obj, s); break;
    case 0xA: BF_EmptyShip(obj, s); break;
    case 0xC: BF_Flame(obj, s); break;
    }
}

// ---------------------------------------------------------------------------
// Object 84 - the cylinders
// ---------------------------------------------------------------------------

static const int16_t Cyl_PosData[4][2] = {
    { FZ_X + 0x80, FZ_Y + 0x110 }, // floor, left
    { FZ_X + 0x100, FZ_Y + 0x110 }, // floor, right
    { FZ_X + 0x40, FZ_Y - 0x50 },  // ceiling, left
    { FZ_X + 0xC0, FZ_Y - 0x50 },  // ceiling, right
};

static Scratch_EggmanCylinder *CY(Object *o) { return (Scratch_EggmanCylinder *)&o->scratch; }

static void Cyl_Main(Object *obj, Scratch_EggmanCylinder *c) {
    uint8_t number = c->number; // set by the boss; position table is indexed by number/2
    obj->render.b = 0;
    obj->render.f.align_fg = true;
    obj->render.f.on_screen = true;
    obj->render.f.yrad_height = true; // use the real height rather than the default 32px
    obj->tile = TILE_MAP(0, 0, 0, 0, ArtTile_FZ_Boss);
    obj->mappings = Mappings_FZEggmanCylinders;
    obj->pos.l.x.f.u = Cyl_PosData[number >> 1][0];
    obj->pos.l.y.f.u = Cyl_PosData[number >> 1][1];
    c->base_y.f.u = Cyl_PosData[number >> 1][1];
    c->base_y.f.l = 0;
    obj->width_pixels = 64 / 2;
    obj->y_rad = (int8_t)(192 / 2);
    obj->priority = 3;
    obj->routine += 2;
}

// Real ASM tests the carry out of a 32-bit add/subtract on the displacement.
static bool Cyl_Add(Scratch_EggmanCylinder *c, uint32_t amount) {
    uint32_t old = (uint32_t)c->displacement.v;
    uint32_t now = old + amount;
    c->displacement.v = (int32_t)now;
    return now < old; // carry
}

static bool Cyl_Sub(Scratch_EggmanCylinder *c, uint32_t amount) {
    uint32_t old = (uint32_t)c->displacement.v;
    c->displacement.v = (int32_t)(old - amount);
    return old < amount; // borrow
}

static void Cyl_Finished(Object *obj, Scratch_EggmanCylinder *c) {
    c->displacement.v = 0;
    Object *boss = &objects[c->parent_index];
    BF(boss)->child_counter--; // one less cylinder running
    BF(boss)->attack_state = 0;
    obj->routine -= 2;
}

static void Cyl_Bottom(Object *obj, Scratch_EggmanCylinder *c) {
    Object *boss = &objects[c->parent_index];
    if (c->child_cmd != 0) { // extending
        if (c->displacement.f.u < -0x10)
            Cyl_Sub(c, 0x28000); // faster once past 16px
        Cyl_Sub(c, 0x8000);
        if (c->displacement.f.u <= -0xA0) {
            c->displacement.f.l = 0;
            c->displacement.f.u = -0xA0;
            c->child_cmd = 0;
        }
        return;
    }
    if (boss->col_property == 0) { // Eggman is beaten: explode, retract at half speed
        BossDefeated(obj);
        Cyl_Sub(c, 0x10000);
    }
    if (Cyl_Add(c, 0x20000)) // back home
        Cyl_Finished(obj, c);
}

static void Cyl_Top(Object *obj, Scratch_EggmanCylinder *c) {
    obj->render.f.y_flip = true; // faces downward
    Object *boss = &objects[c->parent_index];
    if (c->child_cmd != 0) {
        if (c->displacement.f.u >= 0x10)
            Cyl_Add(c, 0x28000);
        Cyl_Add(c, 0x8000);
        if (c->displacement.f.u >= 0xA0) {
            c->displacement.f.l = 0;
            c->displacement.f.u = 0xA0;
            c->child_cmd = 0;
        }
        return;
    }
    if (boss->col_property == 0) {
        BossDefeated(obj);
        Cyl_Add(c, 0x10000);
    }
    if (Cyl_Sub(c, 0x20000))
        Cyl_Finished(obj, c);
}

// Returns true if the cylinder deleted itself.
static bool Cyl_UpdatePos(Object *obj, Scratch_EggmanCylinder *c) {
    int16_t y = (int16_t)((c->base_y.v + c->displacement.v) >> 16);
    obj->pos.l.y.f.u = y;
    if (obj->routine == 4 && c->has_eggman < 0) { // the cylinder Eggman is in carries his hitbox with it
        int16_t off = (c->number <= 2) ? -0xA : 0xE;
        Object *boss = &objects[c->parent_index];
        boss->pos.l.y.f.u = (int16_t)(y + off);
        boss->pos.l.x.f.u = obj->pos.l.x.f.u;
    }

    SolidObject(obj, 64 / 2 + SOLID_SONIC_WIDTH, 192 / 2, 194 / 2, obj->pos.l.x.f.u, NULL, NULL);

    // Sprite frame from how far it has extended: floor cylinders (displacement < 0) grow a frame per 16px after
    // the first 8; ceiling ones after the first 39.
    int frame = 0;
    int16_t d = c->displacement.f.u;
    if (d < 0) {
        uint16_t dist = (uint16_t)-d;
        if (dist >= 8) {
            frame = 1 + ((dist - 8) >> 4);
        }
    } else if ((uint16_t)d >= 0x27) {
        frame = 1 + (((uint16_t)d - 0x27) >> 4);
    }
    obj->frame = (uint8_t)frame;

    // Once Sonic is a screen past it, it can go (he can't walk back after Eggman is beaten).
    int16_t dx = (int16_t)(player->pos.l.x.f.u - obj->pos.l.x.f.u);
    if (dx >= 0 && dx - 0x140 >= 0 && !obj->render.f.on_screen) {
        ObjectDelete(obj);
        return true;
    }
    DisplaySprite(obj);
    return false;
}

void Obj_EggmanCylinder(Object *obj) {
    Scratch_EggmanCylinder *c = CY(obj);

    switch (obj->routine) {
    case 0:
        Cyl_Main(obj, c);
        // fall through into Action, like the real routine 0
        __attribute__((fallthrough));
    case 2: // Action: waiting for the boss's command
        if (c->number > 2)
            obj->render.f.y_flip = true; // ceiling cylinder
        c->displacement.v = 0;
        if (c->child_cmd != 0)
            obj->routine += 2;
        Cyl_UpdatePos(obj, c);
        break;
    case 4: // Move
        if (c->number <= 2)
            Cyl_Bottom(obj, c);
        else
            Cyl_Top(obj, c);
        Cyl_UpdatePos(obj, c);
        break;
    }
}

// ---------------------------------------------------------------------------
// Object 86 - the plasma launcher and its energy balls
// ---------------------------------------------------------------------------

static Scratch_BossPlasma *PL(Object *o) { return (Scratch_BossPlasma *)&o->scratch; }

static void Plasma_Main(Object *obj) {
    obj->pos.l.x.f.u = FZ_X + 0x138;
    obj->pos.l.y.f.u = FZ_Y + 0x2C;
    obj->tile = TILE_MAP(0, 0, 0, 0, ArtTile_FZ_Boss);
    obj->mappings = Mappings_PlasmaBallLauncher;
    obj->anim = 0;
    obj->prev_anim = 1;
    obj->anim_frame = 0;
    obj->frame_time.w = 0;
    obj->frame = 0;
    obj->priority = 3;
    obj->width_pixels = 16 / 2;
    obj->y_rad = 16 / 2;
    obj->render.b = 0;
    obj->render.f.align_fg = true;
    obj->render.f.on_screen = true;
    obj->routine += 2;
}

// Collision, then animate and display; deletes the launcher once Sonic is a screen past it.
static void Plasma_Collision(Object *obj) {
    SolidObject(obj, 16 / 2 + SOLID_SONIC_WIDTH, 16 / 2, 34 / 2, obj->pos.l.x.f.u, NULL, NULL);
    int16_t dx = (int16_t)(player->pos.l.x.f.u - obj->pos.l.x.f.u);
    if (dx >= 0 && dx - 0x140 >= 0 && !obj->render.f.on_screen) {
        ObjectDelete(obj);
        return;
    }
    AnimateSprite(obj, Animation_PlasmaBallLauncher);
    DisplaySprite(obj);
}

static void Plasma_Generator(Object *obj, Scratch_BossPlasma *p) {
    Object *boss = &objects[p->parent_index];
    if (BF(boss)->link == PH_FALL) { // the boss has fallen: the launcher blows up
        obj->type = ObjId_ExplosionBomb; // real id_Explosion
        obj->routine = 0;
        DisplaySprite(obj);
        return;
    }
    obj->anim = 0;
    if (p->child_cmd != 0) {
        obj->routine += 2; // MakeBalls
        obj->anim = 1;
    }
    Plasma_Collision(obj);
}

static void Plasma_MakeBalls(Object *obj, Scratch_BossPlasma *p) {
    if (p->child_cmd != 0) {
        p->child_cmd = 0; // runs once
        p->target_x += 4; // spread offset
        p->child_counter = 0;

        for (int i = 0; i < 4; i++) {
            Object *ball = FindNextFreeObj(obj);
            if (ball == NULL)
                break;
            ball->type = ObjId_BossPlasma;
            ball->pos.l.x.f.u = obj->pos.l.x.f.u;
            ball->pos.l.y.f.u = FZ_Y + 0x2C;
            ball->routine = 8;
            ball->tile = TILE_MAP(0, 1, 0, 0, ArtTile_FZ_Boss); // | Tile_Pal2
            ball->mappings = Mappings_PlasmaBalls;
            ball->prev_anim = 1;
            ball->anim = 0;
            ball->y_rad = 24 / 2;
            ball->width_pixels = 24 / 2;
            ball->col_type = 0;
            ball->priority = 3;
            ball->render.b = 0;
            ball->render.f.align_fg = true;
            ball->render.f.on_screen = true;
            Scratch_BossPlasma *bp = PL(ball);
            bp->timer = 62;
            bp->parent_index = (uint8_t)(obj - objects);

            uint32_t rnd = RandomNumber();
            // Spacing between the balls. The original's spacing (-$4F) together with a Drop that pushes the
            // leftmost ball further left instead of back in bounds lets it escape the arena; the fixed pair is used.
#ifdef SCP_FIX_BUGS
            int16_t spacing = -0x59;
#else
            int16_t spacing = -0x4F;
#endif
            int16_t x = (int16_t)(p->child_counter * spacing + FZ_X + 0x128); // start at the rightmost position
            int16_t jitter = (int16_t)((rnd & 0x1F) - 0x10);                  // -16..15
            bp->target_x = (int16_t)(x + jitter);
            p->child_counter++;
            p->balls_alive = p->child_counter;
        }
    }
    if (p->child_counter == 0) // every ball has spread out to its X
        obj->routine += 2;      // Finish
    Plasma_Collision(obj);
}

static void Plasma_Finish(Object *obj, Scratch_BossPlasma *p) {
    obj->anim = 2;
    if (p->balls_alive == 0) { // all gone
        obj->routine = 2;
        BF(&objects[p->parent_index])->child_counter = -1; // tell the boss
    }
    Plasma_Collision(obj);
}

// Returns true if the ball deleted itself.
static bool Ball_DeleteChild(Object *obj, Scratch_BossPlasma *p) {
    PL(&objects[p->parent_index])->balls_alive--;
    ObjectDelete(obj);
    return true;
}

static bool Ball_Routine(Object *obj, Scratch_BossPlasma *p) {
    switch (obj->routine_sec) {
    case 0: // Spread
        obj->xsp = (int16_t)((p->target_x - obj->pos.l.x.f.u) << 4);
        p->timer = 180;
        obj->routine_sec += 2;
        break;
    case 2: // Drop
        if (obj->xsp != 0) {
            SpeedToPos(obj);
            int16_t over = (int16_t)(obj->pos.l.x.f.u - p->target_x);
            if (over < 0) { // reached the spread X
                obj->xsp = 0;
#ifdef SCP_FIX_BUGS
                obj->pos.l.x.f.u = (int16_t)(obj->pos.l.x.f.u - over); // back to the target
#else
                obj->pos.l.x.f.u = (int16_t)(obj->pos.l.x.f.u + over); // (pushes it further out of bounds)
#endif
                PL(&objects[p->parent_index])->child_counter--;
            }
        }
        obj->anim = 0; // hovering
        if (--p->timer == 0) {
            obj->routine_sec += 2;
            obj->anim = 1; // attacking
            obj->col_type = 0x98; // col_24x24 | col_hurt
            p->timer = 180;
            obj->xsp = (int16_t)(player->pos.l.x.f.u - obj->pos.l.x.f.u); // aim at Sonic
            obj->ysp = 0x140;
        }
        break;
    case 4: // Move
        SpeedToPos(obj);
        if ((uint16_t)obj->pos.l.y.f.u >= FZ_Y + 0xD0)
            return Ball_DeleteChild(obj, p);
        if (--p->timer == 0)
            return Ball_DeleteChild(obj, p);
        break;
    }
    return false;
}

void Obj_BossPlasma(Object *obj) {
    Scratch_BossPlasma *p = PL(obj);

    switch (obj->routine) {
    case 0: Plasma_Main(obj); // fall through into Generator, like the real routine 0
        __attribute__((fallthrough));
    case 2: Plasma_Generator(obj, p); break;
    case 4: Plasma_MakeBalls(obj, p); break;
    case 6: Plasma_Finish(obj, p); break;
    case 8: // a plasma ball
        if (Ball_Routine(obj, p))
            return;
        AnimateSprite(obj, Animation_PlasmaBalls);
        DisplaySprite(obj);
        break;
    }
}
