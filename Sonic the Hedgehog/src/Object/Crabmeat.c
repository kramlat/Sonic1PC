#include "Crabmeat.h"
#include "Resource/Animation/Crabmeat.h"
#include "Resource/Mappings/Crabmeat.h"
#include "Object.h"
#include "ObjectSst.h"
#include "SstLoad.h"
#include "Constants.h"

static uint8_t Obj_Crabmeat_SetAni(Sst *self) {
    if (self->angle & 0x80) {
        if (self->angle <= 0xFA)
            return (((self->status & SST_STATUS_X_FLIP) != 0) ? 2 : 1);
    } else {
        if (self->angle >= 0x06)
            return (((self->status & SST_STATUS_X_FLIP) != 0) ? 1 : 2);
    }
    return 0;
}

void Obj_Crabmeat_Construct(Sst *self) {
    // Initialize collision
    self->y_radius = 16;
    self->x_radius = 8;

    // Initialize object drawing information
    self->mappings = Mappings_Crabmeat;
    self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_Crabmeat);
    self->render_flags = SST_RENDER_LEVEL;
    self->priority = 3 * 0x80;

    // Initialize more stuff
    self->collision_flags = 0x06;
    self->width_pixels = 21;
}

void Obj_Crabmeat_Fall(Sst *self, int16_t floor_dist) {
    // Clip and increment routine
    self->y_pos.f.u += floor_dist;
    self->y_vel = 0;
    self->angle = angle_buffer0;
    self->routine += 2;
}

bool Obj_Crabmeat_Check_Timer(Own_Crabmeat *own) {
    return --own->time_delay >= 0;
}

void Obj_Crabmeat_TurnAround(Sst *self, Own_Crabmeat *own) {
    // Turn around
    self->routine_secondary += 2;
    own->time_delay = 127;
    self->x_vel = 0x80;
    self->anim = 3 + Obj_Crabmeat_SetAni(self);
    self->status ^= SST_STATUS_X_FLIP;
    if (self->status & SST_STATUS_X_FLIP)
        self->x_vel = -self->x_vel;
}

void Obj_Crabmeat_Fire_Projectile(Sst *self, uint8_t routine, int16_t x, int16_t y, int16_t xsp) {
    Sst *proj = Sst_SpawnCode(Obj_Crabmeat, self->x_pos.f.u + x, self->y_pos.f.u + y);
    if (proj != NULL) {
        proj->routine = routine;
        proj->x_vel = xsp;
    }
}

void Obj_Crabmeat_Fire(Sst *self, Own_Crabmeat *own) {
    // Fire
    own->time_delay = 59;
    self->anim = 6;

    // Create projectiles
    Obj_Crabmeat_Fire_Projectile(self, 6, -16, 0, -0x100);
    Obj_Crabmeat_Fire_Projectile(self, 6, 16, 0, 0x100);
}

bool Obj_Crabmeat_Check_CrabMode(Own_Crabmeat *own) {
    return ((own->crab_mode ^= 1) & 1);
}

bool Obj_Crabmeat_Check_EdgeDist(int16_t floor_dist) {
    return (floor_dist >= -8 && floor_dist < 12);
}

int16_t Obj_Crabmeat_Get_EdgeDist(Sst *self) {
    return SstFloorDist(self, self->x_pos.f.u + (((self->status & SST_STATUS_X_FLIP) != 0) ? -16 : 16));
}

void Obj_Crabmeat_NonZIF(Sst *self, int16_t floor_dist) {
    self->y_pos.f.u += floor_dist;
    self->angle = angle_buffer0;
    self->anim = 3 + Obj_Crabmeat_SetAni(self);
}

void Obj_Crabmeat_Stop(Sst *self, Own_Crabmeat *own) {
    // Stop for a moment
    self->routine_secondary -= 2;
    own->time_delay = 59;
    self->x_vel = 0;
    self->anim = Obj_Crabmeat_SetAni(self);
}

void Obj_Crabmeat_Projectile_Construct(Sst *self) {
    // Increment routine
    self->routine += 2;

    // Initialize object drawing information
    self->mappings = Mappings_Crabmeat;
    self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_Crabmeat);
    self->render_flags = SST_RENDER_LEVEL;
    self->priority = 3 * 0x80;

    // Initialize more stuff
    self->collision_flags = 0x87;
    self->width_pixels = 8;
    self->y_vel = -0x400;
    self->anim = 7;
}

bool Obj_Crabmeat_Check_BelowLevel(Sst *self) {
    return self->y_pos.f.u >= limit_btm2 + SCREEN_HEIGHT;
}

void Obj_Crabmeat(Sst *self) {
    Own_Crabmeat *own = (Own_Crabmeat *)self->own.u8;

    int16_t floor_dist;

    switch (self->routine) {
    case 0: // Initialization
        Obj_Crabmeat_Construct(self);

        // Fall before enabling
        Sst_MoveSprite(self);
        if ((floor_dist = SstFloorDist(self, self->x_pos.f.u)) >= 0)
            break;

        Obj_Crabmeat_Fall(self, floor_dist);
        // Fallthrough
    case 2: // Moving
        switch (self->routine_secondary) {
        case 0: // Wait to fire
            // Wait for timer to expire
            if (Obj_Crabmeat_Check_Timer(own))
                break;

            // Check if we should fire
            if (!(self->render_flags & SST_RENDER_ON_SCREEN) || ((own->crab_mode ^= 2) & 2)) {
                Obj_Crabmeat_TurnAround(self, own);
            } else {
                Obj_Crabmeat_Fire(self, own);
            }
            break;
        case 2: // Walking
            // Wait for timer to expire
            if (Obj_Crabmeat_Check_Timer(own)) {
                // Move then perform collision
                Sst_MoveSprite2(self);

                if (!Obj_Crabmeat_Check_CrabMode(own)) {
                    // Check if we're too close to edge
                    floor_dist = Obj_Crabmeat_Get_EdgeDist(self);
                    if (Obj_Crabmeat_Check_EdgeDist(floor_dist))
                        break;
                } else {
                    // Clip out of floor
                    floor_dist = SstFloorDist(self, self->x_pos.f.u);
                    Obj_Crabmeat_NonZIF(self,floor_dist);
                    break;
                }
            }

            Obj_Crabmeat_Stop(self,own);
            break;
        }

        // Animate, draw, and unload when off-screen
        Sst_AnimateFlips(self, Animation_Crabmeat);
        SstOnScreenTest(self);
        break;
    case 4: // Delete
        Sst_Delete(self);
        break;
    case 6: // Projectile initialization
        Obj_Crabmeat_Projectile_Construct(self);
        // Fallthrough
    case 8: // Projectile move
        // Move and animate
        Sst_MoveSprite(self);
        Sst_AnimateFlips(self, Animation_Crabmeat);
        Sst_Draw(self);

        // Delete if fallen below stage
        if (Obj_Crabmeat_Check_BelowLevel(self))
            Sst_Delete(self);
        break;
    }
}
