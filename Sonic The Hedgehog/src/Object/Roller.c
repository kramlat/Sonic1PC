#include "Roller.h"
#include "Object.h"
#include "SstLoad.h"
#include "ObjectSst.h"

#include "Level.h"
#include "LevelCollision.h"
#include "LevelScroll.h"
#include "Macros.h"
#include "Object/Sonic.h"
#include "Resource/Animation/Roller.h"
#include "Resource/Mappings/Roller.h"

// Object 43 - Roller enemy (SYZ). Falls in hidden/curled state until it
// lands, then waits until Sonic passes 256px to its left, at which point
// it unfolds, waits briefly (destroyable while unfolded), then rolls
// rightward at high speed (invincible and damaging while rolling),
// jumping over any ledge/drop it encounters.

static void Roll_Action_FromLeft(Sst *self, Own_Roller *own) {
    (void)own;
    int16_t d0 = (int16_t)(player->x_pos.f.u - 256);
    if (d0 < 0)
        return;
    d0 = (int16_t)(d0 - self->x_pos.f.u);
    if (d0 < 0)
        return;

    self->routine_secondary += 4;
    self->anim = 2;
    self->x_vel = 0x700;
    self->collision_flags = 0x0E | 0x80; // col_28x28 | col_hurt
}

static void Roll_Action_Unfolded(Sst *self, Own_Roller *own) {
    if (self->anim == 2) { // advanced to rolling animation again (set by the animation script itself)
        self->routine_secondary += 2;
        return;
    }
    if (--own->wait_unfolded >= 0)
        return;

    self->anim = 1; // re-folding animation (sets itself to rolling animation once finished)
    self->x_vel = 0x700;
    self->collision_flags = 0x0E | 0x80; // col_28x28 | col_hurt
}

// Only once: stop and unfold Roller 48px to the left of Sonic, making it
// destroyable and starting its wait timer.
static void Roll_Action_StopAndUnfold(Sst *self, Own_Roller *own) {
    if ((int8_t)own->stateflags < 0)
        return;

    int16_t d0 = (int16_t)(player->x_pos.f.u - 48);
    d0 = (int16_t)(d0 - self->x_pos.f.u);
    if (d0 >= 0)
        return;

    self->anim = 0; // unfolding animation
    self->collision_flags = 0x0E; // col_28x28 | col_badnik
    self->x_vel = 0;
    own->wait_unfolded = 2 * 60;
    self->routine_secondary = 2;
    own->stateflags |= 0x80;
}

static void Roll_Action_Rolling(Sst *self, Own_Roller *own) {
    Roll_Action_StopAndUnfold(self, own);
    Sst_MoveSprite2(self);

    int16_t floor_dist = SstFloorDist(self, self->x_pos.f.u);
    if (floor_dist < -8 || floor_dist >= 0xC) {
        self->routine_secondary += 2;
        bool hit_before = (own->stateflags & 1) != 0;
        own->stateflags |= 1;
        if (hit_before)
            self->y_vel = -0x600;
        return;
    }
    self->y_pos.f.u += floor_dist;
}

static void Roll_Action_Jumping(Sst *self, Own_Roller *own) {
    (void)own;
    Sst_MoveSprite(self);
    if (self->y_vel < 0)
        return;

    // Landed once it is below the floor surface (a negative distance: bpl .return in the original). This was the other way round, so
    // as soon as it started falling the Roller was put on the floor straight away, wherever that was.
    int16_t floor_dist = SstFloorDist(self, self->x_pos.f.u);
    if (floor_dist >= 0)
        return;

    self->y_pos.f.u += floor_dist;
    self->routine_secondary -= 2;
    self->y_vel = 0;
}

void Obj_Roller(Sst *self) {
    Own_Roller *own = (Own_Roller *)self->own.u8;

    switch (self->routine) {
    case 0: {
        self->y_radius = 28 / 2;
        self->x_radius = 16 / 2;

        // Fall until landing on the floor (while invisible)
        Sst_MoveSprite(self);
        int16_t floor_dist = SstFloorDist(self, self->x_pos.f.u);
        if (floor_dist < 0) {
            self->y_pos.f.u += floor_dist;
            self->y_vel = 0;
            self->routine += 2;
            self->mappings = Mappings_Roller;
            self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_Roller);
            self->render_flags = SST_RENDER_LEVEL;
            self->priority = 4 * 0x80;
            self->width_pixels = 32 / 2;
        }

        // Bug fix: prevents invisibly falling forever if there's no floor beneath it
        if ((uint16_t)self->y_pos.f.u > 0x7FF)
            Sst_Delete(self);
        return; // do NOT display sprite yet
    }
    case 2:
        switch (self->routine_secondary) {
        case 0:
            Roll_Action_FromLeft(self, own);
            return; // stays invisible while waiting -- no animate/display this frame
        case 2:
            Roll_Action_Unfolded(self, own);
            break;
        case 4:
            Roll_Action_Rolling(self, own);
            break;
        case 6:
            Roll_Action_Jumping(self, own);
            break;
        }

        Sst_Animate(self, Animation_Roller);
        SstOnScreenTest(self);
        break;
    }
}
