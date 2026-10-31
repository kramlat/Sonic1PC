#include "Signpost.h"
#include "Resource/Mappings/Signpost.h"
#include "Object/Ring.h"
#include "Resource/Animation/Signpost.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"

#include "Game.h"
#include "Level.h"
#include "LevelScroll.h"
#include "Sound.h"

#include "Macros.h"

// Signpost object
static const int8_t sparkle_pos[8][2] = {
    { -24, -16 },
    { 8, 8 },
    { -16, 0 },
    { -24, -8 },
    { 0, -8 },
    { -16, 0 },
    { -24, 8 },
    { 24, 16 },
};

void Obj_Signpost(Sst *self) {
    Own_Signpost *own = (Own_Signpost *)self->own.u8;

    switch (self->routine) {
    case 0: // Initialization
        // Increment routine
        self->routine += 2;

        // Set object drawing information
        self->mappings = Mappings_Signpost;
        self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_Signpost);
        self->render_flags = SST_RENDER_LEVEL;
        self->width_pixels = 24;
        self->priority = 4 * 0x80;
        // Fallthrough
    case 2: // Wait for contact
        // Check if player is touching signpost
        if ((uint16_t)(player->x_pos.f.u - self->x_pos.f.u) < 0x20) {
            // Start spinning
            PlaySound(sfx_Signpost);
            time_count = false;
            limit_left2 = limit_right2;
            self->routine += 2;
        }
        break;
    case 4: // Spinning
        // Handle spinning animation
        if (--own->spin_time < 0) {
            // Do next animation and increment routine when stopped spinning
            own->spin_time = 60;
            if (++self->anim == 3)
                self->routine += 2;
        }

        // Handle sparkling
        if (--own->sparkle_time < 0) {
            // Update sparkle time and ID
            own->sparkle_time = 11;
            uint8_t sparkle_id = own->sparkle_id;
            own->sparkle_id = (own->sparkle_id + 2) & 0xE;

            // Spawn sparkle
            const int8_t* spawn_pos = sparkle_pos[sparkle_id >> 1];
            Sst *sparkle = Sst_Allocate();
            if (sparkle != NULL) {
                // Set sparkle type and routine
                sparkle->code = Obj_Ring;
                sparkle->routine = 6;

                // Set sparkle position
                sparkle->x_pos.f.u = self->x_pos.f.u + *spawn_pos++;
                sparkle->y_pos.f.u = self->y_pos.f.u + *spawn_pos++;

                // Set sparkle drawing information
                sparkle->mappings = Mappings_RingREV01;
                sparkle->art_tile = TILE_MAP(0, 1, 0, 0, ArtTile_Ring);
                sparkle->render_flags = SST_RENDER_LEVEL;
                sparkle->priority = 2 * 0x80;
                sparkle->width_pixels = 8;
            }
        }
        break;
    case 6: // Player run
        // Lock player controls
        if (debug_use)
            break;
        if (!((player->status & SST_PSTATUS_IN_AIR) != 0)) {
            lock_ctrl = true;
            jpad1_hold2 = JPAD_RIGHT;
            jpad1_press2 = 0;
        }

        // Check if level end sequence should play
        if (player->code != sst_object_codes[ObjId_Null] && (uint16_t)player->x_pos.f.u < (limit_right2 + 296 + SCREEN_WIDEADD))
            break;

        // Start level end sequence
        self->routine += 2;
        GotThroughAct();
        break;
    case 8: // Level end
        break;
    }

    // Animate, draw, and delete signpost(??) when off-screen
    Sst_Animate(self, Animation_Signpost);
    Sst_Draw(self);
    SstDeleteIfNotInRange2(self, self->x_pos.f.u);
}
