#include "MotoBug.h"
#include "Constants.h"

#include "Object.h"
#include "ObjectSst.h"
#include "SstLoad.h"
#include "Resource/Mappings/Motobug.h"
#include "Resource/Animation/Motobug.h"

typedef struct {
    int16_t time;
    uint8_t pad;
    int8_t smoke_delay;
} Own_Motobug;

// Motobug object
void Obj_Motobug(Sst *self) {
    Own_Motobug *own = (Own_Motobug *)self->own.u8;

    int16_t floor_dist;

    switch (self->routine) {
    case 0: // Initialization
        // Set object drawing information
        self->mappings = Mappings_Motobug;
        self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_Moto_Bug);
        self->render_flags = SST_RENDER_LEVEL;
        self->priority = 4 * 0x80;
        self->width_pixels = 20;

        // Check if we're smoke
        if (!self->anim) {
            // Motobug
            // Initialize collision
            self->y_radius = 14;
            self->x_radius = 8;
            self->collision_flags = 0x0C;

            // Fall before enabling
            Sst_MoveSprite(self);
            if ((floor_dist = SstFloorDist(self, self->x_pos.f.u)) >= 0)
                break;

            // Clip and increment routine
            self->y_pos.f.u += floor_dist;
            self->y_vel = 0;
            self->routine += 2;
            self->status ^= SST_STATUS_X_FLIP;
        } else {
            // Smoke
            // Increment routine and draw
            self->routine += 4;
            Sst_Animate(self, Animation_Motobug);
            Sst_Draw(self);
            break;
        }
        // Fallthrough
    case 2: // Moving around
        switch (self->routine_secondary) {
        case 0:
            // Wait for timer to expire
            if (--own->time < 0) {
                // Turn around and begin moving
                self->routine_secondary += 2;
                self->x_vel = -0x100;
                self->anim = 1;
                self->status ^= SST_STATUS_X_FLIP;
                if (self->status & SST_STATUS_X_FLIP)
                    self->x_vel = -self->x_vel;
            }
            break;
        case 2:
            // Move and check floor
            Sst_MoveSprite2(self);
            floor_dist = SstFloorDist(self, self->x_pos.f.u);

            if (floor_dist >= -8 && floor_dist < 12) {
                // Clip
                self->y_pos.f.u += floor_dist;

                // Emit smoke every 16 frames
                if (--own->smoke_delay < 0) {
                    own->smoke_delay = 15;

                    // Setup smoke object
                    Sst *smoke = Sst_SpawnCode(Obj_Motobug, self->x_pos.f.u, self->y_pos.f.u);
                    if (smoke == NULL)
                        break;
                    smoke->status = self->status;
                    smoke->anim = 2;
                }
            } else {
                // Stop moving
                self->routine_secondary -= 2;
                own->time = 59;
                self->x_vel = 0;
                self->anim = 0;
            }
            break;
        }

        // Animate, draw, and unload when off-screen
        Sst_Animate(self, Animation_Motobug);
        SstOnScreenTest(self);
        break;
    case 4: // Smoke
        Sst_Animate(self, Animation_Motobug);
        Sst_Draw(self);
        break;
    case 6: // Delete (smoke)
        Sst_Delete(self);
        break;
    }
}
