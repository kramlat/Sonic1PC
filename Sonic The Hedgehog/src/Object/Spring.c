#include "Spring.h"
#include "Constants.h"

#include "Level.h"
#include "LevelScroll.h"
#include "Sonic.h"
#include "Sound.h"
#include "ObjectSst.h"
#include "SstLoad.h"
#include "Resource/Animation/Spring.h"
#include "Resource/Mappings/Spring.h"

// Spring object
static const int16_t spring_power[] = { -0x1000, -0xA00 };

void Obj_Spring(Sst *self) {
    Own_Spring *own = (Own_Spring *)self->own.u8;

    switch (self->routine) {
    case 0: // Initialization
        // Increment routine
        self->routine += 2;

        // Set object drawing information
        self->mappings = Mappings_Spring;
        self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_Spring_Horizontal);
        self->render_flags |= SST_RENDER_LEVEL;
        self->width_pixels = 16;
        self->priority = 4 * 0x80;

        // Initialize based off of subtype
        if (self->subtype & 0x10) {
            // Left/right
            self->routine = 8;
            self->anim = 1;
            self->mapping_frame = 3;
            self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_Spring_Vertical);
            self->width_pixels = 8;
        }
        if (self->subtype & 0x20) {
            // Down
            self->routine = 14;
            self->status |= SST_STATUS_Y_FLIP;
        }

        // Check if yellow spring
        if (self->subtype & 0x02)
            self->art_tile |= TILE_MAP(0, 1, 0, 0, ArtTile_Level);
        own->power = spring_power[(self->subtype & 0xF) >> 1];
        break;
    case 2: // Up
        // Act as solid and bounce when Sonic hits top
        SstSolidObject(self, 27, 8, 16, self->x_pos.f.u, NULL, NULL);
        if (!self->routine_secondary)
            break;

        // Increment routine
        self->routine += 2;

        // Launch Sonic
        player->pos.l.y.f.u += 8;
        player->ysp = own->power;
        player->status.p.f.in_air = true;
        player->status.p.f.object_stand = false;
        player->anim = SonAnimId_Spring;
        player->routine = 2;

        // Reset object state
        self->status &= (uint8_t)~SST_STATUS_PLAYER_STAND;
        self->routine_secondary = 0;
        PlaySound(sfx_Spring);
        // Fallthrough
    case 4: // Up bouncing
        Sst_Animate(self, Animation_Spring);
        break;
    case 6: // Up reset
        self->prev_anim = 1;
        self->routine -= 4;
        break;
    case 8: // Left/right
        // Act as solid and bounce when Sonic hits side
        SstSolidObject(self, 19, 14, 15, self->x_pos.f.u, NULL, NULL);
        if (self->routine == 2)
            self->routine = 8;
        if (!(self->status & SST_STATUS_PLAYER_PUSH))
            break;

        // Increment routine
        self->routine += 2;

        // Launch Sonic
        if ((self->status & SST_STATUS_X_FLIP)) {
            player->xsp = own->power;
            player->pos.l.x.f.u += 8;
        } else {
            player->xsp = -own->power;
            player->pos.l.x.f.u -= 8;
        }
        Scratch_Sonic* sscratch = (Scratch_Sonic*)&player->scratch;
        sscratch->control_lock = 15;
        player->inertia = player->xsp;
        player->status.p.f.x_flip ^= 1;
        if (!player->status.p.f.in_ball)
            player->anim = SonAnimId_Walk;

        // Reset object state
        self->status &= (uint8_t)~SST_STATUS_PLAYER_PUSH;
        player->status.p.f.pushing = false;
        PlaySound(sfx_Spring);
        break;
    case 10: // Left/right bouncing
        Sst_Animate(self, Animation_Spring);
        break;
    case 12: // Left/right reset
        self->prev_anim = 2;
        self->routine -= 4;
        break;
    case 14:; // Down
        // Act as solid and bounce when Sonic hits top
        int16_t y_off;
        SstSolidObject(self, 27, 8, 16, self->x_pos.f.u, NULL, &y_off);
        if (self->routine == 2)
            self->routine = 14;
        if (!self->routine_secondary || y_off >= 0)
            break;

        // Increment routine
        self->routine += 2;

        // Launch Sonic
        player->pos.l.y.f.u -= 8;
        player->ysp = -own->power;
        player->status.p.f.in_air = true;
        player->status.p.f.object_stand = false;
        player->anim = SonAnimId_Spring;
        player->routine = 2;

        // Reset object state
        self->status &= (uint8_t)~SST_STATUS_PLAYER_STAND;
        self->routine_secondary = 0;
        PlaySound(sfx_Spring);
        // Fallthrough
    case 16: // Down bouncing
        Sst_Animate(self, Animation_Spring);
        break;
    case 18: // Down reset
        self->prev_anim = 1;
        self->routine -= 4;
        break;
    }

    // Draw and delete once off-screen
    SstOnScreenTest(self);
}
