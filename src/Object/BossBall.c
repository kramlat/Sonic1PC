#include "BossBall.h"

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

static void GBall_UpdateSwingPosition(Object *obj, Scratch_BossBall *scratch, int16_t angle) {
    (void)obj;
    int16_t sin, cos;
    CalcSine(angle, &sin, &cos);

    for (int i = 0; i <= scratch->child_count; i++) {
        Object *child = &objects[scratch->child_idx[i]];
        Scratch_BossBall *cs = (Scratch_BossBall *)&child->scratch;
        int32_t length = cs->chain_length;
        child->pos.l.y.f.u = (int16_t)(scratch->orig_y + ((length * sin) >> 8));
        child->pos.l.x.f.u = (int16_t)(scratch->orig_x + ((length * cos) >> 8));
    }
}

// Alternate swing logic for the wrecking ball (real hardware's GBall_Move,
// part of Object 15's own source file -- kept here instead since this
// project splits swinging-platform and boss-ball code into separate files).
static void GBall_Move(Object *obj, Scratch_BossBall *scratch) {
    int16_t speed = scratch->speed;
    if (!scratch->direction) {
        speed = (int16_t)(speed + 8);
        if (speed == 0x200)
            scratch->direction = 1;
    } else {
        speed = (int16_t)(speed - 8);
        if (speed == (int16_t)-0x200)
            scratch->direction = 0;
    }
    scratch->speed = speed;

    // Real hardware does `add.w speed,obAngle(a0)` -- a 16-bit add into a
    // word straddling obAngle and the following (otherwise-unused) byte,
    // with only the high byte (obj->angle) ever read back for CalcSine.
    // That makes the effective turn rate speed/256 per frame rather than
    // the raw speed value, giving a slow pendulum swing instead of several
    // full rotations per frame at max speed.
    uint16_t accum = (uint16_t)((obj->angle << 8) | scratch->angle_frac);
    accum = (uint16_t)(accum + (uint16_t)speed);
    obj->angle = (uint8_t)(accum >> 8);
    scratch->angle_frac = (uint8_t)accum;

    GBall_UpdateSwingPosition(obj, scratch, obj->angle);
}

// Animates, updates position, and destroys the base (chain anchor) on
// boss defeat.
static void GBall_UpdateBase(Object *obj, Scratch_BossBall *scratch) {
    Object *parent = &objects[scratch->parent_index];

    uint8_t before = obj->anim_frame;
    obj->anim_frame = (uint8_t)(before + 32);
    if (obj->anim_frame < before) // wrapped -- change frame every 8th call
        obj->frame ^= 1;

    scratch->orig_x = parent->pos.l.x.f.u;
    scratch->orig_y = (int16_t)(parent->pos.l.y.f.u + scratch->anchor_pos);
    obj->status.b = parent->status.b;

    if (parent->status.o.f.flag7) { // has boss been beaten?
        obj->type = ObjId_Explosion;
        obj->routine = 2; // plain explosion -- no animal, no points
    }
}

static void GBall_Main(Object *obj, Scratch_BossBall *scratch) {
    obj->routine = 2; // advance to GBall_Base
    obj->angle = 0x40; // vertical left and ceiling
    scratch->speed = (int16_t)-0x200; // "don't flash" sentinel, later reused as swing speed
    obj->mappings = Mappings_BossItems;
    obj->tile = TILE_MAP(0, 0, 0, 0, 0x46C); // ArtTile_Eggman_Weapons

    uint8_t parent_index = scratch->parent_index;
    scratch->child_count = 0;

    Object *last = obj;
    for (int i = 0; i < 6; i++) {
        Object *cur;
        if (i == 0) {
            cur = obj;
        } else {
            cur = FindNextFreeObj(obj);
            if (cur == NULL)
                break;
            cur->type = ObjId_BossBall;
            cur->routine = 6; // GBall_Link
            cur->mappings = Mappings_GHZSwing;
            cur->tile = TILE_MAP(0, 0, 0, 0, 0x380); // ArtTile_GHZ_MZ_Swing
            cur->frame = 1;
            scratch->child_count++;
        }

        cur->render.f.align_fg = true;
        cur->width_pixels = 16 / 2;
        cur->priority = 6;
        ((Scratch_BossBall *)&cur->scratch)->parent_index = parent_index;
        scratch->child_idx[i] = (uint8_t)(cur - objects);
        last = cur;
    }

    last->routine = 8; // GBall_Ball
    last->mappings = Mappings_GHZBall;
    last->tile = TILE_MAP(0, 2, 0, 0, 0x3AA); // ArtTile_GHZ_Giant_Ball | Tile_Pal3
    last->frame = 1;
    last->priority = 5;
    last->col_type = 0x01 | 0x80; // col_40x40 | col_hurt
}

static void GBall_Base(Object *obj, Scratch_BossBall *scratch) {
    Object *last = obj;
    uint8_t last_target = 0;
    for (int i = 0; i <= scratch->child_count; i++) {
        Object *child = &objects[scratch->child_idx[i]];
        Scratch_BossBall *cs = (Scratch_BossBall *)&child->scratch;
        uint8_t target = GBall_PosData[i];
        if (cs->chain_length != target)
            cs->chain_length++;
        last = child;
        last_target = target;
    }

    Scratch_BossBall *last_scratch = (Scratch_BossBall *)&last->scratch;
    if (last_scratch->chain_length == last_target) {
        Object *parent = &objects[scratch->parent_index];
        if (parent->routine_sec == 6) // has the ship started moving? (BGHZ_ShipMove)
            obj->routine = 4; // advance to GBall_Base2
    }

    if (scratch->anchor_pos != 32)
        scratch->anchor_pos++;

    GBall_UpdateBase(obj, scratch);
    GBall_UpdateSwingPosition(obj, scratch, obj->angle);
    DisplaySprite(obj);
}

static void GBall_Base2(Object *obj, Scratch_BossBall *scratch) {
    GBall_UpdateBase(obj, scratch);
    GBall_Move(obj, scratch);
    DisplaySprite(obj);
}

static void GBall_Link(Object *obj, Scratch_BossBall *scratch) {
    Object *parent = &objects[scratch->parent_index];
    if (parent->status.o.f.flag7) { // has Eggman's defeated flag been set?
        obj->type = ObjId_Explosion;
        obj->routine = 2; // plain explosion -- no animal, no points
        return;
    }
    DisplaySprite(obj);
}

static void GBall_Ball(Object *obj, Scratch_BossBall *scratch) {
    obj->frame = (obj->frame == 0) ? 1 : 0;

    Object *parent = &objects[scratch->parent_index];
    if (!parent->status.o.f.flag7) {
        DisplaySprite(obj);
        return;
    }

    obj->col_type = 0; // col_none -- disable collision
    BossDefeated(obj);

    int8_t timer = (int8_t)(scratch->chain_length - 1); // reuses chain_length as a one-shot timer once defeated
    scratch->chain_length = (uint8_t)timer;
    if (timer >= 0) {
        DisplaySprite(obj);
        return;
    }

    obj->type = ObjId_Explosion;
    obj->routine = 2; // plain explosion -- no animal, no points
}

void Obj_BossBall(Object *obj) {
    Scratch_BossBall *scratch = (Scratch_BossBall *)&obj->scratch;

    switch (obj->routine) {
    case 0: GBall_Main(obj, scratch); break;
    case 2: GBall_Base(obj, scratch); break;
    case 4: GBall_Base2(obj, scratch); break;
    case 6: GBall_Link(obj, scratch); break;
    case 8: GBall_Ball(obj, scratch); break;
    }
}
