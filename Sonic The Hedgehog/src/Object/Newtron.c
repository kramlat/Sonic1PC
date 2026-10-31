#include "Newtron.h"
#include "Resource/Mappings/Newtron.h"
#include "Object/BuzzBomber.h"
#include "Resource/Animation/Newtron.h"
#include "Object.h"
#include "ObjectSst.h"
#include "SstLoad.h"
#include "Constants.h"

#include "Level.h"
#include "LevelCollision.h"

void Obj_Newtron(Sst *self) {
    Own_Newtron *own = (Own_Newtron *)self->own.u8;

    int16_t floor_dist;

    switch (self->routine) {
    case 0: // Initialization
        // Increment routine
        self->routine += 2;

        // Set object drawing information
        self->mappings = Mappings_Newtron;
        self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_Newtron);
        self->render_flags = SST_RENDER_LEVEL;
        self->priority = 4 * 0x80;
        self->width_pixels = 20;
        self->y_radius = 16;
        self->x_radius = 8;
        // Fallthrough
    case 2:
        switch (self->routine_secondary) {
        case 0: // Check if near Sonic
        {
            // Get X difference and check if we're close enough to Sonic
            uint16_t x_off;
            if (self->x_pos.f.u < player->x_pos.f.u) {
                SST_STATUS_SET(self, SST_STATUS_X_FLIP, true);
                x_off = player->x_pos.f.u - self->x_pos.f.u;
            } else {
                SST_STATUS_SET(self, SST_STATUS_X_FLIP, false);
                x_off = self->x_pos.f.u - player->x_pos.f.u;
            }
            if (x_off >= 0x80)
                break;

            // Setup next state depending on if we're green or blue
            self->routine_secondary += 2;
            self->anim = 1;

            if (self->subtype) {
                self->art_tile = TILE_MAP(0, 1, 0, 0, ArtTile_Newtron);
                self->routine_secondary = 8;
                self->anim = 4;
            }
            break;
        }
        case 2: // Flying Newtron
        {
            // Wait until animation has finished
            if (self->mapping_frame < 4) {
                if (self->x_pos.f.u < player->x_pos.f.u)
                    SST_STATUS_SET(self, SST_STATUS_X_FLIP, true);
                else
                    SST_STATUS_SET(self, SST_STATUS_X_FLIP, false);
                break;
            }

            // Change collision type once frame is 1?
            if (self->mapping_frame == 1)
                self->collision_flags = 0x0C; // Different collision type than below

            // Fall onto ground
            Sst_MoveSprite(self);

            floor_dist = SstFloorDist(self, self->x_pos.f.u);
            if (floor_dist < 0) {
                // Clip and change state
                self->y_pos.f.u += floor_dist;
                self->y_vel = 0;
                self->routine_secondary += 2;
                self->anim = 2;
                if (self->art_tile & TILE_MAP(0, 1, 0, 0, ArtTile_Level)) // Condition never met
                    self->anim++;
                self->collision_flags = 0x0D;
                self->x_vel = 0x200;
                if (!((self->status & SST_STATUS_X_FLIP) != 0))
                    self->x_vel = -self->x_vel;
            }
            break;
        }
        case 4: // Fly along floor
            // Move and check floor
            Sst_MoveSprite2(self);

            floor_dist = SstFloorDist(self, self->x_pos.f.u);
            if (floor_dist >= -8 && floor_dist < 12) {
                self->y_pos.f.u += floor_dist;
                break;
            }

            // Didn't find floor, start flying without collision
            self->routine_secondary += 2;
            break;
        case 6: // Fly in mid-air
            Sst_MoveSprite2(self);
            break;
        case 8: // Shooting Newtron
            // Change collision type once frame is 1
            if (self->mapping_frame == 1)
                self->collision_flags = 0x0C;

            // Check if we should fire
            if (self->mapping_frame == 2 && !own->fired) {
                // Fire missile
                own->fired = true;

                Sst *msl = Sst_Allocate();
                if (msl == NULL)
                    break;

                msl->code = Obj_BuzzMissile;
                msl->x_pos.f.u = self->x_pos.f.u;
                msl->y_pos.f.u = self->y_pos.f.u - 8;
                if (((self->status & SST_STATUS_X_FLIP) != 0)) {
                    msl->x_vel = 0x200;
                    msl->x_pos.f.u += 20;
                } else {
                    msl->x_vel = -0x200;
                    msl->x_pos.f.u -= 20;
                }
                msl->status = self->status;
                msl->subtype = 1; // subtype
            }
            break;
        }

        // Animate, draw, and unload once off-screen
        Sst_Animate(self, Animation_Newtron);
        SstOnScreenTest(self);
        break;
    case 4: // Delete
        Sst_Delete(self);
        break;
    }
}
