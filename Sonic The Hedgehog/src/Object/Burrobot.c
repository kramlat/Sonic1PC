#include "Burrobot.h"
#include "Object.h"
#include "ObjectSst.h"
#include "SstLoad.h"

#include "Level.h"
#include "LevelCollision.h"
#include "LevelScroll.h"
#include "Macros.h"
#include "Object/Sonic.h"
#include "Resource/Animation/Burrobot.h"
#include "Resource/Mappings/Burrobot.h"

// Object 2D - Burrobot enemy (LZ). Waits underground until Sonic walks
// into range, then jumps out, walks back and forth checking ledges/floor
// alignment, and periodically either turns around or jumps again.

// Checks how far Sonic is horizontally and faces the object towards him;
// returns true if Sonic is within `zone` px. Sets *out_vel to the speed
// (and direction) to move towards Sonic, when non-NULL.
static bool Burro_CheckDistanceAndFaceSonic(Sst *self, int16_t zone, int16_t *out_vel) {
    int16_t vel = 0x80;
    SST_STATUS_SET(self, SST_STATUS_X_FLIP, true);

    int16_t d0 = (int16_t)(player->x_pos.f.u - self->x_pos.f.u);
    if (d0 < 0) {
        d0 = (int16_t)-d0;
        vel = (int16_t)-vel;
        SST_STATUS_SET(self, SST_STATUS_X_FLIP, false);
    }

    if (out_vel)
        *out_vel = vel;
    return d0 < zone;
}

static void Burro_Action_Move_NextAction(Sst *self, Own_Burrobot *own) {
    // 50/50 chance to jump again or turn around, based on the current VBlank counter
    if (!((uint8_t)frame_count & 4)) {
        self->routine_secondary += 2; // -> Burro_Action_Jump
        self->y_vel = -0x400;
        self->anim = 2;
        return;
    }

    self->routine_secondary -= 2; // -> Burro_Action_TurnAround
    own->timedelay = 60 - 1;
    self->x_vel = 0;
    self->anim = 0;
}

static void Burro_Action_TurnAround(Sst *self, Own_Burrobot *own) {
    if (--own->timedelay >= 0)
        return;

    self->routine_secondary += 2; // -> Burro_Action_Move
    own->timedelay = 255;
    self->x_vel = 0x80;
    self->anim = 1;
    self->status ^= SST_STATUS_X_FLIP;
    if (!((self->status & SST_STATUS_X_FLIP) != 0))
        return;
    self->x_vel = (int16_t)-self->x_vel;
}

static void Burro_Action_Move(Sst *self, Own_Burrobot *own) {
    if (--own->timedelay < 0) {
        Burro_Action_Move_NextAction(self, own);
        return;
    }

    Sst_MoveSprite2(self);

    own->checktype ^= 1;
    if (own->checktype) {
        int16_t floor_dist = SstFloorDist(self, self->x_pos.f.u);
        self->y_pos.f.u += floor_dist;
        return;
    }

    int16_t x = (int16_t)(self->x_pos.f.u + 12);
    if (!((self->status & SST_STATUS_X_FLIP) != 0))
        x = (int16_t)(self->x_pos.f.u - 12);
    int16_t floor_dist = SstFloorDist(self, x);
    if (floor_dist >= 0xC)
        Burro_Action_Move_NextAction(self, own);
}

static void Burro_Action_Jump(Sst *self, Own_Burrobot *own) {
    Sst_MoveSprite2(self);
    self->y_vel += 0x18;
    if (self->y_vel < 0)
        return;
    self->anim = 3;

    int16_t floor_dist = SstFloorDist(self, self->x_pos.f.u);
    if (floor_dist >= 0)
        return;
    self->y_pos.f.u += floor_dist;

    self->y_vel = 0;
    self->anim = 1;
    own->timedelay = 255;
    self->routine_secondary -= 2; // -> Burro_Action_Move
    Burro_CheckDistanceAndFaceSonic(self, 0, NULL); // face Sonic one last time (distance check ignored)
}

static void Burro_Action_ChkSonic(Sst *self, Own_Burrobot *own) {
    (void)own;
    int16_t vel;
    if (!Burro_CheckDistanceAndFaceSonic(self, 96, &vel))
        return;

    int16_t dy = (int16_t)(player->y_pos.f.u - self->y_pos.f.u);
    if (dy >= 0 || dy < -128 || debug_use)
        return;

    self->routine_secondary -= 2; // -> Burro_Action_Jump
    self->x_vel = vel;
    self->y_vel = -0x400;
}

void Obj_Burrobot(Sst *self) {
    Own_Burrobot *own = (Own_Burrobot *)self->own.u8;

    switch (self->routine) {
    case 0:
        self->routine += 2;
        self->y_radius = 38 / 2;
        self->x_radius = 16 / 2;
        self->mappings = Mappings_Burrobot;
        self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_Burrobot);
        self->render_flags |= SST_RENDER_LEVEL;
        self->priority = 4 * 0x80;
        self->collision_flags = 0x05; // col_24x36 | col_badnik
        self->width_pixels = 24 / 2;
        self->routine_secondary += 6; // run Burro_Action_ChkSonic first
        self->anim = 2;
        __attribute__((fallthrough));
    case 2:
        switch (self->routine_secondary) {
        case 0: Burro_Action_TurnAround(self, own); break;
        case 2: Burro_Action_Move(self, own); break;
        case 4: Burro_Action_Jump(self, own); break;
        case 6: Burro_Action_ChkSonic(self, own); break;
        }
        Sst_Animate(self, Animation_Burrobot);
        SstOnScreenTest(self);
        break;
    }
}
