#include "BossBall.h"
#include "SstLoad.h"
#include "Object/Explosion.h"
#include "ObjectSst.h"
#include "Object.h"
#include "Constants.h"

#include "BossGreenHill.h"
#include "Level.h"
#include "MathUtil.h"
#include "Resource/Mappings/BossItems.h"

extern const uint8_t Mappings_GHZSwing[]; // From Object/SwingingPlatform.c
extern const uint8_t Mappings_GHZBall[]; // From Object/SwingingPlatform.c

// Object 48 - wrecking ball on a chain that Eggman swings (GHZ boss). The
// controller object doubles as the visible chain anchor on the ship; it
// spawns 4 chain-link objects and a giant spiked ball, all of this same
// type, referenced via child_idx[] for the swing-position update and via
// parent_index for checking whether the boss has been defeated.

static const uint8_t GBall_PosData[6] = { 0, 0x10, 0x20, 0x30, 0x40, 0x60 };

static void GBall_UpdateSwingPosition(Sst *self, Own_BossBall *own, int16_t angle) {
    (void)self;
    int16_t sin, cos;
    CalcSine(angle, &sin, &cos);

    for (int i = 0; i <= own->child_count; i++) {
        Sst *child = &sst[own->child_idx[i]];
        Own_BossBall *cs = (Own_BossBall *)child->own.u8;
        int32_t length = cs->chain_length;
        child->y_pos.f.u = (int16_t)(own->orig_y + ((length * sin) >> 8));
        child->x_pos.f.u = (int16_t)(own->orig_x + ((length * cos) >> 8));
    }
}

// Alternate swing logic for the wrecking ball (real hardware's GBall_Move,
// part of Object 15's own source file -- kept here instead since this
// project splits swinging-platform and boss-ball code into separate files).
static void GBall_Move(Sst *self, Own_BossBall *own) {
    int16_t speed = own->speed;
    if (!own->direction) {
        speed = (int16_t)(speed + 8);
        if (speed == 0x200)
            own->direction = 1;
    } else {
        speed = (int16_t)(speed - 8);
        if (speed == (int16_t)-0x200)
            own->direction = 0;
    }
    own->speed = speed;

    // Real hardware does `add.w speed,obAngle(a0)` -- a 16-bit add into a
    // word straddling obAngle and the following (otherwise-unused) byte,
    // with only the high byte (self->angle) ever read back for CalcSine.
    // That makes the effective turn rate speed/256 per frame rather than
    // the raw speed value, giving a slow pendulum swing instead of several
    // full rotations per frame at max speed.
    uint16_t accum = (uint16_t)((self->angle << 8) | own->angle_frac);
    accum = (uint16_t)(accum + (uint16_t)speed);
    self->angle = (uint8_t)(accum >> 8);
    own->angle_frac = (uint8_t)accum;

    GBall_UpdateSwingPosition(self, own, self->angle);
}

// Animates, updates position, and destroys the base (chain anchor) on
// boss defeat.
static void GBall_UpdateBase(Sst *self, Own_BossBall *own) {
    Sst *parent = &sst[own->parent_index];

    uint8_t before = self->anim_frame;
    self->anim_frame = (uint8_t)(before + 32);
    if (self->anim_frame < before) // wrapped -- change frame every 8th call
        self->mapping_frame ^= 1;

    own->orig_x = parent->x_pos.f.u;
    own->orig_y = (int16_t)(parent->y_pos.f.u + own->anchor_pos);
    self->status = parent->status;

    if (((parent->status & 0x80) != 0)) { // has boss been beaten?
        self->code = Obj_ExplosionBomb;
        self->routine = 0;
    }
}

static void GBall_Main(Sst *self, Own_BossBall *own) {
    self->routine = 2; // advance to GBall_Base
    self->angle = 0x40; // vertical left and ceiling
    own->speed = (int16_t)-0x200; // "don't flash" sentinel, later reused as swing speed
    self->mappings = Mappings_BossItems;
    self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_Eggman_Weapons);

    uint8_t parent_index = own->parent_index;
    own->child_count = 0;

    Sst *last = self;
    for (int i = 0; i < 6; i++) {
        Sst *cur;
        if (i == 0) {
            cur = self;
        } else {
            cur = Sst_AllocateAfter(self);
            if (cur == NULL)
                break;
            cur->code = Obj_BossBall;
            cur->routine = 6; // GBall_Link
            cur->mappings = Mappings_GHZSwing;
            cur->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_GHZ_MZ_Swing);
            cur->mapping_frame = 1;
            own->child_count++;
        }

        cur->render_flags |= SST_RENDER_LEVEL;
        cur->width_pixels = 16 / 2;
        cur->priority = 6 * 0x80;
        ((Own_BossBall *)cur->own.u8)->parent_index = parent_index;
        own->child_idx[i] = (uint8_t)(cur - sst);
        last = cur;
    }

    last->routine = 8; // GBall_Ball
    last->mappings = Mappings_GHZBall;
    last->art_tile = TILE_MAP(0, 2, 0, 0, ArtTile_GHZ_Giant_Ball); // | Tile_Pal3
    last->mapping_frame = 1;
    last->priority = 5 * 0x80;
    last->collision_flags = 0x01 | 0x80; // col_40x40 | col_hurt
}

static void GBall_Base(Sst *self, Own_BossBall *own) {
    Sst *last = self;
    uint8_t last_target = 0;
    for (int i = 0; i <= own->child_count; i++) {
        Sst *child = &sst[own->child_idx[i]];
        Own_BossBall *cs = (Own_BossBall *)child->own.u8;
        uint8_t target = GBall_PosData[i];
        if (cs->chain_length != target)
            cs->chain_length++;
        last = child;
        last_target = target;
    }

    Own_BossBall *last_scratch = (Own_BossBall *)last->own.u8;
    if (last_scratch->chain_length == last_target) {
        Sst *parent = &sst[own->parent_index];
        if (parent->routine_secondary == 6) // has the ship started moving? (BGHZ_ShipMove)
            self->routine = 4; // advance to GBall_Base2
    }

    if (own->anchor_pos != 32)
        own->anchor_pos++;

    GBall_UpdateBase(self, own);
    GBall_UpdateSwingPosition(self, own, self->angle);
    Sst_Draw(self);
}

static void GBall_Base2(Sst *self, Own_BossBall *own) {
    GBall_UpdateBase(self, own);
    GBall_Move(self, own);
    Sst_Draw(self);
}

static void GBall_Link(Sst *self, Own_BossBall *own) {
    Sst *parent = &sst[own->parent_index];
    if (((parent->status & 0x80) != 0)) { // has Eggman's defeated flag been set?
        self->code = Obj_ExplosionBomb;
        self->routine = 0;
        return;
    }
    Sst_Draw(self);
}

static void GBall_Ball(Sst *self, Own_BossBall *own) {
    self->mapping_frame = (self->mapping_frame == 0) ? 1 : 0;

    Sst *parent = &sst[own->parent_index];
    if (!((parent->status & 0x80) != 0)) {
        Sst_Draw(self);
        return;
    }

    self->collision_flags = 0; // col_none -- disable collision
    SstBossDefeated(self);

    int8_t timer = (int8_t)(own->chain_length - 1); // reuses chain_length as a one-shot timer once defeated
    own->chain_length = (uint8_t)timer;
    if (timer >= 0) {
        Sst_Draw(self);
        return;
    }

    self->code = Obj_ExplosionBomb;
    self->routine = 0;
}

void Obj_BossBall(Sst *self) {
    Own_BossBall *own = (Own_BossBall *)self->own.u8;

    switch (self->routine) {
    case 0: GBall_Main(self, own); break;
    case 2: GBall_Base(self, own); break;
    case 4: GBall_Base2(self, own); break;
    case 6: GBall_Link(self, own); break;
    case 8: GBall_Ball(self, own); break;
    }
}
