#include "LavaBall.h"
#include "Object.h"
#include "ObjectSst.h"
#include "SstLoad.h"

#include "Level.h"
#include "LevelCollision.h"
#include "LevelScroll.h"
#include "Macros.h"
#include "Resource/Animation/Fireballs.h"
#include "Sound.h"

// Object 14 - lava balls (MZ, SLZ). Launched by LavaMaker (object 13) in
// one of 9 fixed patterns selected by subtype: 0-3 rise and fall back down
// (different launch speeds), 4/5 fly straight up/down until hitting a
// ceiling/floor, 6/7 fly sideways left/right until hitting a wall, and 8
// just sits still (only ever reached by the wall/ceiling/floor-hit types
// switching themselves to it).
//
// Mappings_Fireballs is owned by GrassFire.c (Object/GrassFire.h directly
// #includes the actual array definition there) -- a bare extern here
// avoids a second definition and a link failure, same reasoning as
// InvisibleBarrier.c's own comment on this.
extern const uint8_t Mappings_Fireballs[];

static const int16_t lball_speeds[9] = {
    -0x400, -0x500, -0x600, -0x700, -0x200, 0x200, -0x200, 0x200, 0,
};

static void LBall_RiseAndFall(Sst *self, Own_LavaBall *own) {
    self->y_vel += 0x18; // fall down faster than it rose

    if (own->orig_y < self->y_pos.f.u) // fallen back past its spawn height
        self->routine += 2;                  // -> LBall_Delete
    SST_STATUS_SET(self, SST_STATUS_Y_FLIP, (self->y_vel < 0)); // face up while still rising, down otherwise
}

static void LBall_Up(Sst *self) {
    self->status |= SST_STATUS_Y_FLIP; // face up

    if (SstHitCeiling(self) < 0) {
        self->subtype = 8; // -> LBall_DoNothing
        self->anim = 1;          // vertical collide animation
        self->y_vel = 0;
    }
}

static void LBall_Down(Sst *self) {
    SST_STATUS_SET(self, SST_STATUS_Y_FLIP, false); // face down

    if (SstFloorDist(self, self->x_pos.f.u) < 0) {
        self->subtype = 8; // -> LBall_DoNothing
        self->anim = 1;          // vertical collide animation
        self->y_vel = 0;
    }
}

static void LBall_Left(Sst *self) {
    self->status |= SST_STATUS_X_FLIP; // face left

    if (SstHitWallLeft(self, -8) < 0) {
        self->subtype = 8; // -> LBall_DoNothing
        self->anim = 3;          // horizontal collide animation
        self->x_vel = 0;
    }
}

static void LBall_Right(Sst *self) {
    SST_STATUS_SET(self, SST_STATUS_X_FLIP, false); // face right

    if (SstHitWallRight(self, 8) < 0) {
        self->subtype = 8; // -> LBall_DoNothing
        self->anim = 3;          // horizontal collide animation
        self->x_vel = 0;
    }
}

void Obj_LavaBall(Sst *self) {
    Own_LavaBall *own = (Own_LavaBall *)self->own.u8;

    switch (self->routine) {
    case 0: { // Main
        self->routine += 2;
        self->y_radius = 16 / 2;
        self->x_radius = 16 / 2;
        self->mappings = Mappings_Fireballs;
        self->art_tile = (LEVEL_ZONE(level_id) == ZoneId_SLZ)
            ? TILE_MAP(0, 0, 0, 0, ArtTile_SLZ_Fireball)
            : TILE_MAP(0, 0, 0, 0, ArtTile_MZ_Fireball);
        self->render_flags = SST_RENDER_LEVEL;
        self->priority = 3 * 0x80;
        self->collision_flags = 0x8B; // col_16x16 | col_hurt
        own->orig_y = self->y_pos.f.u;

        if (own->from_boss)
            self->priority += 2 * 0x80;

        uint8_t subtype = self->subtype;
        self->y_vel = lball_speeds[subtype];
        self->width_pixels = 16 / 2;

        if (subtype >= 6) {
            self->width_pixels = 32 / 2;
            self->anim = 2; // horizontal animation
            self->x_vel = self->y_vel;
            self->y_vel = 0;
        }

        PlaySound(sfx_Fireball);
        __attribute__((fallthrough));
    }
    case 2: // Action
        switch (self->subtype) {
        case 0:
        case 1:
        case 2:
        case 3:
            LBall_RiseAndFall(self, own);
            break;
        case 4:
            LBall_Up(self);
            break;
        case 5:
            LBall_Down(self);
            break;
        case 6:
            LBall_Left(self);
            break;
        case 7:
            LBall_Right(self);
            break;
        case 8:
            break; // do nothing
        }

        Sst_MoveSprite2(self);
        Sst_AnimateFlips(self, Animation_Fireballs); // wall/ceiling/floor-collided balls advance obRoutine to LBall_Delete themselves

        SstOnScreenTest(self);
        break;
    case 4: // Delete
        Sst_Delete(self);
        break;
    }
}
