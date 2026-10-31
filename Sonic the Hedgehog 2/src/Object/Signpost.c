#include "Object/Signpost.h"
#include "SstLoad.h"
void Obj_Ring(Sst *self);
#include "ObjectSst.h"
#include "Object.h"

#include "Game.h"
#include "Level.h"
#include "LevelScroll.h"
#include "Sound.h"
#include "SplitScreen.h"
#include "Object/Tails.h"
#include "Resource/Mappings/Signpost.h"
#include "Resource/Animation/Signpost.h"

#include "Macros.h"

// The signpost for Sonic 2 (the Simon Wai prototype's object 0D): Sonic 1's with its own art, mappings and animations, and its own sparkle places (two of them are mirrored). Its art is loaded over the spikes' place (tile $434) when the act ends, and its sparkles are rings of tile $7B2 (Sonic 1's ring place, which is not the ring's here: the sparkles are not what they were)
static const int8_t sparkle_pos[8][2] = {
    { -24, -16 },
    { 8, 8 },
    { -16, 0 },
    { 24, -8 },
    { 0, -8 },
    { 16, 0 },
    { -24, 8 },
    { 24, 16 },
};

// (the type is cleared and the rest of the object is left as it is, so that the camera, which follows his place, stays where it is)
static void DespawnWhenOffScreenRight(Sst *chr) {
    if (chr->code == sst_object_codes[ObjId_Null])
        return;
    const int16_t x = (int16_t)(chr->x_pos.f.u - scrpos_x.f.u);
    if (x >= SCREEN_WIDTH)
        chr->code = sst_object_codes[ObjId_Null];
}

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
            SplitScreen_LockCameras(self->x_pos.f.u, self->width_pixels); // (the views the sign is in)
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
                sparkle->art_tile = TILE_MAP(0, 1, 0, 0, 0x7B2);
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
    case 8: // Level end: the characters run on to the right, and are gone (despawned) when they leave the screen, so that they do not fall out of the level and die while the results card is up
        DespawnWhenOffScreenRight(player);
        if (TAILS_OBJ->code != sst_object_codes[ObjId_Null])
            DespawnWhenOffScreenRight(TAILS_OBJ);
        break;
    }

    // Animate, draw, and delete signpost(??) when off-screen
    Sst_Animate(self, Animation_Signpost);
    Sst_Draw(self);
    SstDeleteIfNotInRange2(self, self->x_pos.f.u);
}
