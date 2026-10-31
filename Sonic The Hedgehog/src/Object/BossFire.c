#include "BossFire.h"
#include "Object.h"
#include "ObjectSst.h"
#include "SstLoad.h"
#include "Constants.h"

#include "LevelCollision.h"
#include "Sound.h"

// Object 74 - lava that Eggman drops (MZ boss). Doubles as the falling
// fireball itself (subtype != 0, routines 0->2) and the "temp fire" floor
// warning marker it leaves behind as it spreads along the lava pool
// (subtype == 0, routines 0->4->6). boss_mz_x = 0x1800, boss_mz_y = 0x210
// (real s1disasm _Constants.asm; see BossMarble.c).
//
// Map_Fire/Ani_Fire are the real ASM's own shared labels for this art --
// this project's Mappings_Fireballs/Animation_Fireballs are already owned
// by GrassFire.c/LavaBall.c respectively (Object 2F/35 and Object 13/14
// both draw from the same resource), so bare externs here avoid a second
// definition and a link failure, same reasoning as LavaBall.c's own
// comment on this.
extern const uint8_t Mappings_Fireballs[];
extern const uint8_t Animation_Fireballs[];

static void BossFire_Action(Sst *self, Own_BossFire *own);
static void BossFire_TempFire(Sst *self, Own_BossFire *own);

static void BossFire_Main(Sst *self, Own_BossFire *own) {
    self->y_radius = 16 / 2;
    self->x_radius = 16 / 2;
    self->mappings = Mappings_Fireballs;
    self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_MZ_Fireball);
    self->render_flags |= SST_RENDER_LEVEL;
    self->priority = 5 * 0x80;
    own->boss_y = self->y_pos.f.u;
    self->width_pixels = 16 / 2;
    self->routine += 2;

    if (self->subtype == 0) {
        // Temp fire (floor warning) -- controls its own collision/routine
        // separately, so jump straight into TempFire this same frame.
        self->collision_flags = 0x8B; // col_16x16 | col_hurt
        self->routine += 2;
        BossFire_TempFire(self, own);
        return;
    }

    // Real fire, dropped by Eggman.
    own->generic_timer = 30;
    QueueSound2(sfx_Fireball);
    BossFire_Action(self, own);
}

static void BossFire_Drop(Sst *self, Own_BossFire *own) {
    self->status |= SST_STATUS_Y_FLIP; // face up while waiting to drop

    if ((int8_t)(--own->generic_timer) >= 0)
        return;

    self->collision_flags = 0x8B; // col_16x16 | col_hurt
    self->subtype = 0; // clear subtype
    self->y_vel += 0x18;
    SST_STATUS_SET(self, SST_STATUS_Y_FLIP, false); // now facing down

    if (SstFloorDist(self, self->x_pos.f.u) >= 0)
        return;
    self->routine_secondary += 2;
}

static void BossFire_MakeFlame(Sst *self, Own_BossFire *own) {
    self->y_pos.f.u -= 2;
    self->art_tile |= 0x8000; // high priority (foreground)
    self->x_vel = 0xA0;
    self->y_vel = 0;
    own->boss_x = self->x_pos.f.u;
    own->boss_y = self->y_pos.f.u;
    own->generic_timer = 3;

    // Clone the entire object (matches real ASM's literal 64-byte struct
    // copy) so the new flame starts identical, then spread the pair apart.
    Sst *clone = Sst_AllocateAfter(self);
    if (clone != NULL) {
        *clone = *self;
        clone->x_vel = (int16_t)-clone->x_vel;
        clone->routine_secondary += 2;
    }

    self->routine_secondary += 2;
}

static void BossFire_Duplicate2(Sst *self) {
    Sst *clone = Sst_AllocateAfter(self);
    if (clone == NULL)
        return;
    clone->x_pos.f.u = self->x_pos.f.u;
    clone->y_pos.f.u = self->y_pos.f.u;
    clone->code = self->code;
    clone->subtype = 0;   // subtype -- temp fire
    ((Own_BossFire *)clone->own.u8)->generic_timer = 103;
}

static void BossFire_Duplicate(Sst *self, Own_BossFire *own) {
    if (SstFloorDist(self, self->x_pos.f.u) >= 0) {
        self->routine_secondary += 2;
        return;
    }

    int16_t x = self->x_pos.f.u;
    if (x > (int16_t)(0x1800 + 0x140)) { // boss_mz_x+0x140 -- past the right edge (no left-edge check -- there's a gap there)
        self->routine += 2;
        return;
    }

    if (own->boss_x != x && (own->boss_x & 0x10) != (x & 0x10)) {
        BossFire_Duplicate2(self);
        own->spread_x = x;
    }

    own->boss_x = x;
}

static void BossFire_FallEdge(Sst *self, Own_BossFire *own) {
    SST_STATUS_SET(self, SST_STATUS_Y_FLIP, false);
    self->y_vel += 0x24;

    int16_t d0 = (int16_t)(self->x_pos.f.u - own->spread_x);
    if (d0 < 0)
        d0 = (int16_t)-d0;

    if (d0 == 0x12)
        self->art_tile &= ~0x8000; // low priority (falls behind the foreground lip)

    if (SstFloorDist(self, self->x_pos.f.u) >= 0)
        return;

    if (--own->generic_timer == 0) {
        Sst_Delete(self);
        return;
    }
    self->y_vel = 0;
    self->x_pos.f.u = own->spread_x;
    self->y_pos.f.u = own->boss_y;
    self->art_tile |= 0x8000; // high priority again
    self->routine_secondary -= 2; // -> BossFire_Duplicate, spawn another on top
}

static void BossFire_Action(Sst *self, Own_BossFire *own) {
    switch (self->routine_secondary) {
    case 0: BossFire_Drop(self, own); break;
    case 2: BossFire_MakeFlame(self, own); break;
    case 4: BossFire_Duplicate(self, own); break;
    case 6: BossFire_FallEdge(self, own); break;
    }

    Sst_MoveSprite2(self);
    Sst_AnimateFlips(self, Animation_Fireballs);

    if ((uint16_t)self->y_pos.f.u > (uint16_t)(0x210 + 0xD8)) { // boss_mz_y+0xD8 -- fallen into the lava
        Sst_Delete(self);
        return;
    }
    Sst_Draw(self);
}

static void BossFire_TempFire(Sst *self, Own_BossFire *own) {
    self->art_tile |= 0x8000; // high priority

    if (--own->generic_timer != 0) {
        Sst_AnimateFlips(self, Animation_Fireballs);
        Sst_Draw(self);
        return;
    }

    self->anim = 1; // "flame out"
    self->y_pos.f.u -= 4;
    self->collision_flags = 0;

    Sst_AnimateFlips(self, Animation_Fireballs);
    Sst_Draw(self);
}

static void BossFire_TempFireDel(Sst *self) {
    Sst_Delete(self);
}

void Obj_BossFire(Sst *self) {
    Own_BossFire *own = (Own_BossFire *)self->own.u8;

    switch (self->routine) {
    case 0: BossFire_Main(self, own); break;
    case 2: BossFire_Action(self, own); break;
    case 4: BossFire_TempFire(self, own); break;
    case 6: BossFire_TempFireDel(self); break;
    }
}
