#include "BossFinal.h"
#include "SstLoad.h"
#include "Object/Explosion.h"
#include "ObjectSst.h"
#include "Object.h"
#include "Constants.h"

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

static Own_BossFinal *BF(Sst *o) { return (Own_BossFinal *)o->own.u8; }

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

static bool BossFinal_EggmanRoutine(Sst *self, Own_BossFinal *s);

static void BossFinal_Main(Sst *self, Own_BossFinal *s) {
    memset(self->own.u8, 0, sizeof(self->own));
    uint8_t self_index = (uint8_t)(self - sst);

    for (int i = 0; i < 6; i++) {
        Sst *o = self;
        if (i > 0) {
            o = Sst_AllocateAfter(self);
            if (o == NULL)
                break;
            memset(o->own.u8, 0, sizeof(o->own));
        }
        o->code = Obj_BossFinal;
        o->x_pos.f.u = BF_ObjData[i].x;
        o->y_pos.f.u = BF_ObjData[i].y;
        o->art_tile = TILE_MAP(0, 0, 0, 0, BF_ObjData[i].tile);
        o->mappings = BF_ObjData[i].mappings;
        o->routine = BF_ObjData2[i].routine;
        o->anim = BF_ObjData2[i].anim;
        // Force AnimateSprite's reset guard on the first call (see BossSpringYard.c).
        o->prev_anim = (uint8_t)(BF_ObjData2[i].anim + 1);
        o->anim_frame = 0;
        o->anim_frame_timer = 0;
        o->mapping_frame = 0;
        o->priority = (uint16_t)(BF_ObjData2[i].priority * 0x80);
        o->width_pixels = BF_ObjData2[i].width;
        o->y_radius = (int8_t)BF_ObjData2[i].height;
        o->render_flags = SST_RENDER_LEVEL;
        self->render_flags |= SST_RENDER_ON_SCREEN; // the real ASM sets this on the boss itself, every iteration
        BF(o)->link = self_index;
    }

    // The plasma launcher, then the 4 cylinders.
    Sst *plasma = Sst_Allocate();
    if (plasma != NULL) {
        plasma->code = Obj_BossPlasma;
        s->plasma_index = (uint8_t)(plasma - sst);
        ((Own_BossPlasma *)plasma->own.u8)->parent_index = self_index;

        for (int i = 0; i < 4; i++) {
            Sst *cyl = Sst_AllocateAfter(self);
            if (cyl == NULL)
                break;
            s->cylinders[i] = (uint8_t)(cyl - sst);
            cyl->code = Obj_EggmanCylinder;
            ((Own_EggmanCylinder *)cyl->own.u8)->parent_index = self_index;
            ((Own_EggmanCylinder *)cyl->own.u8)->number = (uint8_t)(i * 2);
        }
    }

    ending_eggmobile_exploding = 0; // a fresh fight: Eggman hasn't been caught yet
    s->link = PH_WAIT; // the boss's own phase (this offset held its parent pointer in the original)
    self->collision_property = 8; // obBossHits
    s->attack_state = -1;

    if (!BossFinal_EggmanRoutine(self, s))
        Sst_Draw(self);
}

// ---------------------------------------------------------------------------
// Eggman himself (routine 2)
// ---------------------------------------------------------------------------

static void BF_Animate(Sst *self) { Sst_Animate(self, Animation_ScrapEggman); }

// Keeps the screen's right edge opening up and, until the ship phase, keeps the whole thing solid.
static void BF_ScreenScroll(Sst *self, Own_BossFinal *s) {
    if (limit_right2 < FZ_END)
        limit_right2 += 2;
    if (s->link >= PH_SHIP)
        return;
    SstSolidObject(self, 32 / 2 + SOLID_SONIC_WIDTH, 224 / 2, 226 / 2, self->x_pos.f.u, NULL, NULL);
}

static void BF_Animate_And_Scroll(Sst *self, Own_BossFinal *s) {
    BF_Animate(self);
    BF_ScreenScroll(self, s);
}

static void BF_Wait(Own_BossFinal *s) {
    if (plc_buffer[0].art == NULL && (uint16_t)scrpos_x.f.u >= (uint16_t)(FZ_X - SCREEN_WIDEADD2))
        s->link += 2; // art is loaded and the screen has reached the arena
    random_seed.v += 1; // seed the RNG for the fight
}

static void BF_Crush(Sst *self, Own_BossFinal *s) {
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

        Sst *a = &sst[s->cylinders[eggman_cyl]];
        BF(a)->child_cmd = 0xFF; // extend
        ((Own_EggmanCylinder *)a->own.u8)->has_eggman = -1;
        Sst *b = &sst[s->cylinders[decoy_cyl]];
        BF(b)->child_cmd = 1; // extend
        ((Own_EggmanCylinder *)b->own.u8)->has_eggman = 0;

        s->child_counter = 1; // (it counts down through 0 to -1 as the two cylinders finish)
        s->hit_flash = 0;
        QueueSound2(sfx_Rumbling);
    }

    if (s->child_counter < 0) { // both cylinders are back home
        if (self->collision_property == 0) { // defeated
            AddPoints(100); // 1000 points (AddPoints counts in tens, like the score)
            s->link = PH_FALL;
            self->x_pos.f.u = FZ_X + 0x170;
            self->y_pos.f.u = FZ_Y + 0x2C;
            self->y_radius = 40 / 2;
        } else {
            s->link += 2; // the plasma attack
            s->attack_state = -1;
            s->child_counter = 0;
        }
        return;
    }

    // Eggman is riding inside the cylinder (it writes his position); face Sonic and see whether he's being hit.
    SST_STATUS_SET(self, SST_STATUS_X_FLIP, ((uint16_t)player->x_pos.f.u >= (uint16_t)self->x_pos.f.u));
    int32_t side = SstSolidObject(self, 64 / 2 + SOLID_SONIC_WIDTH, 40 / 2, 40 / 2, self->x_pos.f.u, NULL, NULL);

    bool hit = false;
    if (side > 0) { // Sonic is against the side of the piston
        random_seed.v += 0x70000; // addq.w #7 on the high word of the seed
        if (player->anim == SonAnimId_Roll) {
            int16_t kick = 0x300;
            if (!((self->status & SST_STATUS_X_FLIP) != 0))
                kick = -kick;
            player->x_vel = kick; // bounce Sonic back
            hit = true;
        }
    }

    if (hit && s->hit_flash == 0 && self->collision_property != 0) { // FixBugs forced: no hit-counter underflow on defeat
        self->collision_property--;
        s->hit_flash = 100;
        QueueSound2(sfx_HitBoss);
    }

    if (s->hit_flash != 0) {
        if (--s->hit_flash != 0) {
            self->anim = 3; // hurt
            BF_Animate(self);
            return;
        }
    }
    if (self->collision_property != 0)
        self->anim = 1; // laughing
    BF_Animate(self);
}

static void BF_Plasma(Own_BossFinal *s) {
    Sst *launcher = &sst[s->plasma_index];
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

static void BF_Fall(Sst *self, Own_BossFinal *s) {
    self->width_pixels = 96 / 2;
    SST_STATUS_SET(self, SST_STATUS_X_FLIP, true);
    Sst_MoveSprite2(self);
    self->mapping_frame = 6;
    self->y_vel = (int16_t)(self->y_vel + 0x10);
    if ((uint16_t)self->y_pos.f.u >= FZ_Y + 0x8C) {
        self->y_pos.f.u = FZ_Y + 0x8C;
        s->link += 2; // Run
        self->width_pixels = 64 / 2;
        self->x_vel = 0x100;
        self->y_vel = -0x100;
        dle_routine += 2; // DLE_FZ_End: scrolling right is allowed now
    }
    BF_ScreenScroll(self, s);
}

static void BF_Run(Sst *self, Own_BossFinal *s) {
    SST_STATUS_SET(self, SST_STATUS_X_FLIP, true);
    self->anim = 4;
    Sst_MoveSprite2(self);
    self->y_vel = (int16_t)(self->y_vel + 0x10);
    if ((uint16_t)self->y_pos.f.u >= FZ_Y + 0x93)
        self->y_vel = -0x40; // bounce upwards slightly

    // Run right, slowing down the further ahead of Sonic he gets.
    self->x_vel = 0x400;
    int16_t gap = (int16_t)(self->x_pos.f.u - player->x_pos.f.u);
    if (gap < 0) {
        self->x_vel = 0x500; // Sonic caught up: run faster
    } else {
        uint16_t d = (uint16_t)gap;
        do {
            if (d < 0x70) break;
            d -= 0x70;
            self->x_vel = (int16_t)(self->x_vel - 0x100);
            if (d < 8) break;
            d -= 8;
            self->x_vel = (int16_t)(self->x_vel - 0x100);
            if (d < 8) break;
            d -= 8;
            self->x_vel = (int16_t)(self->x_vel - 0x80);
            if (d < 8) break;
            d -= 8;
            self->x_vel = (int16_t)(self->x_vel - 0x80);
            if (d < 8) break;
            d -= 8;
            self->x_vel = (int16_t)(self->x_vel - 0x80);
            if (d < 0x38) break;
            self->x_vel = 0; // far ahead: stop
        } while (0);
    }

    if ((uint16_t)self->x_pos.f.u >= FZ_X + 0x250) {
        self->x_pos.f.u = FZ_X + 0x250;
        self->x_vel = 0x240; // jump into the ship
        self->y_vel = -0x4C0;
        s->link += 2;
    }
    BF_Animate_And_Scroll(self, s);
}

static void BF_Jump(Sst *self, Own_BossFinal *s) {
    Sst_MoveSprite2(self);
    if ((uint16_t)self->x_pos.f.u >= FZ_X + 0x290)
        self->x_vel = 0;
    self->y_vel = (int16_t)(self->y_vel + 0x34);
    if (self->y_vel >= 0 && (uint16_t)self->y_pos.f.u >= FZ_Y + 0x82) {
        self->y_pos.f.u = FZ_Y + 0x82;
        self->y_vel = 0;
    }
    if ((self->x_vel | self->y_vel) == 0) { // landed in the ship
        s->link += 2;
        self->y_vel = -0x180;
        self->collision_property = 1; // escaping Eggman takes a single hit
    }
    BF_Animate_And_Scroll(self, s);
}

static void BF_Ship(Sst *self, Own_BossFinal *s) {
    self->mappings = Mappings_Eggman;
    self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_Eggman);
    self->anim = 0;
    SST_STATUS_SET(self, SST_STATUS_X_FLIP, true);
    Sst_MoveSprite2(self);
    if ((uint16_t)self->y_pos.f.u < FZ_Y + 0x34) { // risen high enough
        self->x_vel = 0x180;
        self->y_vel = -0x18;
        self->collision_flags = 0x0F; // col_48x48 | col_boss
        s->link += 2;
    }
    BF_Animate_And_Scroll(self, s);
}

// Returns true if the boss deleted itself (and started the ending).
static bool BF_Escape(Sst *self, Own_BossFinal *s) {
    SST_STATUS_SET(self, SST_STATUS_X_FLIP, true);
    Sst_MoveSprite2(self);

    bool skip_to_watch = false;
    if (s->attack_state == 0) { // escape timer expired / not running
        if (self->collision_flags == 0) { // hit: flash, then either fall (defeated) or recover
            s->attack_state = 30;
            QueueSound2(sfx_HitBoss);
        } else {
            skip_to_watch = true;
        }
    }
    if (!skip_to_watch) {
        if (--s->attack_state == 0) {
            if (((self->status & 0x80) != 0)) { // defeated: sink slowly
                ending_eggmobile_exploding = 1; // the ending shows the unused exploding Eggmobile in its background
                self->y_vel = 0x60;
                s->attack_state = -1; // FixBugs: stop the timer re-arming
            } else {
                self->collision_flags = 0x0F;
            }
        }
    }

    // Keep Sonic from running past the end, and freeze him once Eggman has gotten away.
    if (player->x_pos.f.u >= FZ_END + 0x90) {
        lock_ctrl = 1;
        jpad1_hold2 = 0;
        jpad1_press2 = 0;
        player->ground_vel = 0;
        if (self->y_vel < 0)
            jpad1_hold2 = JPAD_UP; // look up as he escapes
    }
    if (player->x_pos.f.u >= FZ_END + 0xE0)
        player->x_pos.f.u = FZ_END + 0xE0; // the ledge

    if ((uint16_t)self->x_pos.f.u >= FZ_END + 0x200 && !(self->render_flags & SST_RENDER_ON_SCREEN)) {
        gamemode = GameMode_Ending;
        Sst_Delete(self);
        return true;
    }
    BF_Animate(self);
    return false;
}

// Returns true if the boss deleted itself.
static bool BossFinal_EggmanRoutine(Sst *self, Own_BossFinal *s) {
    switch (s->link) {
    case PH_WAIT:   BF_Wait(s); break;
    case PH_CRUSH:  BF_Crush(self, s); break;
    case PH_PLASMA: BF_Plasma(s); break;
    case PH_FALL:   BF_Fall(self, s); break;
    case PH_RUN:    BF_Run(self, s); break;
    case PH_JUMP:   BF_Jump(self, s); break;
    case PH_SHIP:   BF_Ship(self, s); break;
    case PH_ESCAPE:
        if (BF_Escape(self, s))
            return true;
        break;
    }
    return false;
}

static void BossFinal_Eggman(Sst *self, Own_BossFinal *s) {
    if (!BossFinal_EggmanRoutine(self, s))
        Sst_Draw(self);
}

// ---------------------------------------------------------------------------
// The helpers: panel (4), legs (6), cockpit (8), empty ship (A), flame (C)
// ---------------------------------------------------------------------------

static void BF_ChildDisplay(Sst *self, Sst *boss) {
    self->status = boss->status;
    uint8_t flip = self->status & 3;
    self->render_flags = (uint8_t)((self->render_flags & ~3) | flip);
    Sst_Draw(self);
}

static void BF_FlamePos(Sst *self, Sst *boss) {
    self->x_pos.f.u = boss->x_pos.f.u;
    self->y_pos.f.u = boss->y_pos.f.u;
    BF_ChildDisplay(self, boss);
}

// Is the boss still around? (Real ASM compares the first byte of the parent object with its own.)
static Sst *BF_Parent(Sst *self, Own_BossFinal *s) {
    Sst *boss = &sst[s->link];
    return (boss->code == self->code && boss != self) ? boss : NULL;
}

static void BF_Panel(Sst *self, Own_BossFinal *s) {
    Sst *boss = &sst[s->link];
    self->mapping_frame = 0xB;
    if ((uint16_t)player->x_pos.f.u >= (uint16_t)self->x_pos.f.u && !(self->render_flags & SST_RENDER_ON_SCREEN)) {
        Sst_Delete(self); // Sonic is past it and it's off screen
        return;
    }
    (void)boss;
    Sst_Draw(self);
}

static void BF_Legs(Sst *self, Own_BossFinal *s) {
    SST_STATUS_SET(self, SST_STATUS_X_FLIP, true);
    Sst *boss = &sst[s->link];
    if (boss->mappings != Mappings_Eggman) { // not in the ship yet: just stand there
        BF_ChildDisplay(self, boss);
        return;
    }
    self->x_pos.f.u = boss->x_pos.f.u; // the ship has taken off: the legs retract with it, then go
    self->y_pos.f.u = boss->y_pos.f.u;
    if (self->anim_frame_timer == 0)
        self->anim_frame_timer = 0x14;
    if (--self->anim_frame_timer <= 0) {
        if (++self->mapping_frame > 2) {
            Sst_Delete(self);
            return;
        }
    }
    BF_FlamePos(self, boss);
}

static void BF_Cockpit(Sst *self, Own_BossFinal *s) {
    Sst *boss = BF_Parent(self, s);
    if (boss == NULL) {
        Sst_Delete(self);
        return;
    }
    if (boss->mappings != Mappings_Eggman) { // before the ship phase: the empty cockpit
        self->mapping_frame = 0xA;
        BF_ChildDisplay(self, boss);
        return;
    }
    self->anim = 1; // normal face
    if ((int8_t)boss->collision_property <= 0) { // defeated: the cockpit explodes
        SstBossDefeated(self);
        self->priority = 2 * 0x80;
        self->anim = 0;
        self->mappings = Mappings_FZDamagedEggmobile;
        self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_FZ_Eggman_Fleeing);
        Sst_Animate(self, Animation_FZEggInShip);
        BF_FlamePos(self, boss);
        return;
    }
    self->anim = 6; // panic face
    self->mappings = Mappings_Eggman;
    self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_Eggman);
    Sst_Animate(self, Animation_Eggman);
    BF_FlamePos(self, boss);
}

static void BF_EmptyShip(Sst *self, Own_BossFinal *s) {
    self->mapping_frame = 0;
    SST_STATUS_SET(self, SST_STATUS_X_FLIP, true);
    Sst *boss = &sst[s->link];
    if (BF(boss)->link == PH_SHIP && boss->mappings == Mappings_Eggman) {
        Sst_Delete(self);
        return;
    }
    BF_ChildDisplay(self, boss);
}

static void BF_Flame(Sst *self, Own_BossFinal *s) {
    Sst *boss = BF_Parent(self, s);
    if (boss == NULL) {
        Sst_Delete(self);
        return;
    }
    self->anim = 7; // flame off
    if (BF(boss)->link < PH_SHIP) {
        BF_ChildDisplay(self, boss);
        return;
    }
    if (boss->x_vel != 0)
        self->anim = 0xB; // flame on while he's moving
    Sst_Animate(self, Animation_Eggman);
    BF_FlamePos(self, boss);
}

void Obj_BossFinal(Sst *self) {
    Own_BossFinal *s = BF(self);

    switch (self->routine) {
    case 0: BossFinal_Main(self, s); break;
    case 2: BossFinal_Eggman(self, s); break;
    case 4: BF_Panel(self, s); break;
    case 6: BF_Legs(self, s); break;
    case 8: BF_Cockpit(self, s); break;
    case 0xA: BF_EmptyShip(self, s); break;
    case 0xC: BF_Flame(self, s); break;
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

static Own_EggmanCylinder *CY(Sst *o) { return (Own_EggmanCylinder *)o->own.u8; }

static void Cyl_Main(Sst *self, Own_EggmanCylinder *c) {
    uint8_t number = c->number; // set by the boss; position table is indexed by number/2
    self->render_flags = SST_RENDER_LEVEL;
    self->render_flags |= SST_RENDER_ON_SCREEN;
    self->height_pixels = self->y_radius; // use the real height rather than the default 32px
    self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_FZ_Boss);
    self->mappings = Mappings_FZEggmanCylinders;
    self->x_pos.f.u = Cyl_PosData[number >> 1][0];
    self->y_pos.f.u = Cyl_PosData[number >> 1][1];
    c->base_y.f.u = Cyl_PosData[number >> 1][1];
    c->base_y.f.l = 0;
    self->width_pixels = 64 / 2;
    self->y_radius = (int8_t)(192 / 2);
    self->priority = 3 * 0x80;
    self->routine += 2;
}

// Real ASM tests the carry out of a 32-bit add/subtract on the displacement.
static bool Cyl_Add(Own_EggmanCylinder *c, uint32_t amount) {
    uint32_t old = (uint32_t)c->displacement.v;
    uint32_t now = old + amount;
    c->displacement.v = (int32_t)now;
    return now < old; // carry
}

static bool Cyl_Sub(Own_EggmanCylinder *c, uint32_t amount) {
    uint32_t old = (uint32_t)c->displacement.v;
    c->displacement.v = (int32_t)(old - amount);
    return old < amount; // borrow
}

static void Cyl_Finished(Sst *self, Own_EggmanCylinder *c) {
    c->displacement.v = 0;
    Sst *boss = &sst[c->parent_index];
    BF(boss)->child_counter--; // one less cylinder running
    BF(boss)->attack_state = 0;
    self->routine -= 2;
}

static void Cyl_Bottom(Sst *self, Own_EggmanCylinder *c) {
    Sst *boss = &sst[c->parent_index];
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
    if (boss->collision_property == 0) { // Eggman is beaten: explode, retract at half speed
        SstBossDefeated(self);
        Cyl_Sub(c, 0x10000);
    }
    if (Cyl_Add(c, 0x20000)) // back home
        Cyl_Finished(self, c);
}

static void Cyl_Top(Sst *self, Own_EggmanCylinder *c) {
    SST_RENDER_SET(self, SST_RENDER_Y_FLIP, true); // faces downward
    Sst *boss = &sst[c->parent_index];
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
    if (boss->collision_property == 0) {
        SstBossDefeated(self);
        Cyl_Add(c, 0x10000);
    }
    if (Cyl_Sub(c, 0x20000))
        Cyl_Finished(self, c);
}

// Returns true if the cylinder deleted itself.
static bool Cyl_UpdatePos(Sst *self, Own_EggmanCylinder *c) {
    int16_t y = (int16_t)((c->base_y.v + c->displacement.v) >> 16);
    self->y_pos.f.u = y;
    if (self->routine == 4 && c->has_eggman < 0) { // the cylinder Eggman is in carries his hitbox with it
        int16_t off = (c->number <= 2) ? -0xA : 0xE;
        Sst *boss = &sst[c->parent_index];
        boss->y_pos.f.u = (int16_t)(y + off);
        boss->x_pos.f.u = self->x_pos.f.u;
    }

    SstSolidObject(self, 64 / 2 + SOLID_SONIC_WIDTH, 192 / 2, 194 / 2, self->x_pos.f.u, NULL, NULL);

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
    self->mapping_frame = (uint8_t)frame;

    // Once Sonic is a screen past it, it can go (he can't walk back after Eggman is beaten).
    int16_t dx = (int16_t)(player->x_pos.f.u - self->x_pos.f.u);
    if (dx >= 0 && dx - 0x140 >= 0 && !(self->render_flags & SST_RENDER_ON_SCREEN)) {
        Sst_Delete(self);
        return true;
    }
    Sst_Draw(self);
    return false;
}

void Obj_EggmanCylinder(Sst *self) {
    Own_EggmanCylinder *c = CY(self);

    switch (self->routine) {
    case 0:
        Cyl_Main(self, c);
        // fall through into Action, like the real routine 0
        __attribute__((fallthrough));
    case 2: // Action: waiting for the boss's command
        if (c->number > 2)
            SST_RENDER_SET(self, SST_RENDER_Y_FLIP, true); // ceiling cylinder
        c->displacement.v = 0;
        if (c->child_cmd != 0)
            self->routine += 2;
        Cyl_UpdatePos(self, c);
        break;
    case 4: // Move
        if (c->number <= 2)
            Cyl_Bottom(self, c);
        else
            Cyl_Top(self, c);
        Cyl_UpdatePos(self, c);
        break;
    }
}

// ---------------------------------------------------------------------------
// Object 86 - the plasma launcher and its energy balls
// ---------------------------------------------------------------------------

static Own_BossPlasma *PL(Sst *o) { return (Own_BossPlasma *)o->own.u8; }

static void Plasma_Main(Sst *self) {
    self->x_pos.f.u = FZ_X + 0x138;
    self->y_pos.f.u = FZ_Y + 0x2C;
    self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_FZ_Boss);
    self->mappings = Mappings_PlasmaBallLauncher;
    self->anim = 0;
    self->prev_anim = 1;
    self->anim_frame = 0;
    self->anim_frame_timer = 0;
    self->mapping_frame = 0;
    self->priority = 3 * 0x80;
    self->width_pixels = 16 / 2;
    self->y_radius = 16 / 2;
    self->render_flags = SST_RENDER_LEVEL;
    self->render_flags |= SST_RENDER_ON_SCREEN;
    self->routine += 2;
}

// Collision, then animate and display; deletes the launcher once Sonic is a screen past it.
static void Plasma_Collision(Sst *self) {
    SstSolidObject(self, 16 / 2 + SOLID_SONIC_WIDTH, 16 / 2, 34 / 2, self->x_pos.f.u, NULL, NULL);
    int16_t dx = (int16_t)(player->x_pos.f.u - self->x_pos.f.u);
    if (dx >= 0 && dx - 0x140 >= 0 && !(self->render_flags & SST_RENDER_ON_SCREEN)) {
        Sst_Delete(self);
        return;
    }
    Sst_Animate(self, Animation_PlasmaBallLauncher);
    Sst_Draw(self);
}

static void Plasma_Generator(Sst *self, Own_BossPlasma *p) {
    Sst *boss = &sst[p->parent_index];
    if (BF(boss)->link == PH_FALL) { // the boss has fallen: the launcher blows up
        self->code = Obj_ExplosionBomb; // real id_Explosion
        self->routine = 0;
        Sst_Draw(self);
        return;
    }
    self->anim = 0;
    if (p->child_cmd != 0) {
        self->routine += 2; // MakeBalls
        self->anim = 1;
    }
    Plasma_Collision(self);
}

static void Plasma_MakeBalls(Sst *self, Own_BossPlasma *p) {
    if (p->child_cmd != 0) {
        p->child_cmd = 0; // runs once
        p->target_x += 4; // spread offset
        p->child_counter = 0;

        for (int i = 0; i < 4; i++) {
            Sst *ball = Sst_AllocateAfter(self);
            if (ball == NULL)
                break;
            ball->code = Obj_BossPlasma;
            ball->x_pos.f.u = self->x_pos.f.u;
            ball->y_pos.f.u = FZ_Y + 0x2C;
            ball->routine = 8;
            ball->art_tile = TILE_MAP(0, 1, 0, 0, ArtTile_FZ_Boss); // | Tile_Pal2
            ball->mappings = Mappings_PlasmaBalls;
            ball->prev_anim = 1;
            ball->anim = 0;
            ball->y_radius = 24 / 2;
            ball->width_pixels = 24 / 2;
            ball->collision_flags = 0;
            ball->priority = 3 * 0x80;
            ball->render_flags = SST_RENDER_LEVEL;
            ball->render_flags |= SST_RENDER_ON_SCREEN;
            Own_BossPlasma *bp = PL(ball);
            bp->timer = 62;
            bp->parent_index = (uint8_t)(self - sst);

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
        self->routine += 2;      // Finish
    Plasma_Collision(self);
}

static void Plasma_Finish(Sst *self, Own_BossPlasma *p) {
    self->anim = 2;
    if (p->balls_alive == 0) { // all gone
        self->routine = 2;
        BF(&sst[p->parent_index])->child_counter = -1; // tell the boss
    }
    Plasma_Collision(self);
}

// Returns true if the ball deleted itself.
static bool Ball_DeleteChild(Sst *self, Own_BossPlasma *p) {
    PL(&sst[p->parent_index])->balls_alive--;
    Sst_Delete(self);
    return true;
}

static bool Ball_Routine(Sst *self, Own_BossPlasma *p) {
    switch (self->routine_secondary) {
    case 0: // Spread
        self->x_vel = (int16_t)((p->target_x - self->x_pos.f.u) << 4);
        p->timer = 180;
        self->routine_secondary += 2;
        break;
    case 2: // Drop
        if (self->x_vel != 0) {
            Sst_MoveSprite2(self);
            int16_t over = (int16_t)(self->x_pos.f.u - p->target_x);
            if (over < 0) { // reached the spread X
                self->x_vel = 0;
#ifdef SCP_FIX_BUGS
                self->x_pos.f.u = (int16_t)(self->x_pos.f.u - over); // back to the target
#else
                self->x_pos.f.u = (int16_t)(self->x_pos.f.u + over); // (pushes it further out of bounds)
#endif
                PL(&sst[p->parent_index])->child_counter--;
            }
        }
        self->anim = 0; // hovering
        if (--p->timer == 0) {
            self->routine_secondary += 2;
            self->anim = 1; // attacking
            self->collision_flags = 0x98; // col_24x24 | col_hurt
            p->timer = 180;
            self->x_vel = (int16_t)(player->x_pos.f.u - self->x_pos.f.u); // aim at Sonic
            self->y_vel = 0x140;
        }
        break;
    case 4: // Move
        Sst_MoveSprite2(self);
        if ((uint16_t)self->y_pos.f.u >= FZ_Y + 0xD0)
            return Ball_DeleteChild(self, p);
        if (--p->timer == 0)
            return Ball_DeleteChild(self, p);
        break;
    }
    return false;
}

void Obj_BossPlasma(Sst *self) {
    Own_BossPlasma *p = PL(self);

    switch (self->routine) {
    case 0: Plasma_Main(self); // fall through into Generator, like the real routine 0
        __attribute__((fallthrough));
    case 2: Plasma_Generator(self, p); break;
    case 4: Plasma_MakeBalls(self, p); break;
    case 6: Plasma_Finish(self, p); break;
    case 8: // a plasma ball
        if (Ball_Routine(self, p))
            return;
        Sst_Animate(self, Animation_PlasmaBalls);
        Sst_Draw(self);
        break;
    }
}
