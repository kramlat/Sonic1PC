#include "Spikes.h"

#include "Level.h"
#include "LevelScroll.h"
#include "Object/Sonic.h"
#include "ObjectSst.h"
#include "SstLoad.h"
#include "Resource/Mappings/Spikes.h"
#include "Sound.h"

#include "Macros.h"

// Spikes object
static const uint8_t spike_set[][2] = {
    { 0, 20 },
    { 1, 10 },
    { 2, 4 },
    { 3, 28 },
    { 4, 64 },
    { 5, 16 },
};

static void Spike_Hurt(Sst *self) {
    // Check if player can be hurt
    if (invincibility)
        return;
    // "Proper Spike Bug Fix" -- without this, touching spikes while still
    // flashing from a previous hit can hurt Sonic a second time.
    Scratch_Sonic *scratch = (Scratch_Sonic *)player->own.u8;
    if (scratch->flash_time)
        return;
    if (player->routine >= 4)
        return;

    // Hurt player
    player->y_pos.v -= player->y_vel << 8;
    HurtSonicFrom(player, self->x_pos.f.u, true);
}

static void Spike_Wait(Sst *self) {
    Own_Spikes *own = (Own_Spikes *)self->own.u8;

    // Wait for direction switch
    if (own->timer) {
        if (--own->timer)
            return;
        if (self->render_flags & SST_RENDER_ON_SCREEN) {
            PlaySound(sfx_SpikesMove);
        }
        return;
    }

    // Switch direction
    if (own->dir) {
        // Move to target position
        if ((own->move.v -= 0x800) >= 0x8000) {
            own->move.v = 0x0000;
            own->dir = 0;
            own->timer = 60;
        }
    } else {
        // Move to target position
        if ((own->move.v += 0x800) >= 0x2000) {
            own->move.v = 0x2000;
            own->dir = 1;
            own->timer = 60;
        }
    }
}

void Obj_Spikes(Sst *self) {
    Own_Spikes *own = (Own_Spikes *)self->own.u8;

    switch (self->routine) {
    case 0: {
        // Increment routine
        self->routine += 2;

        // Set object drawing information
        self->mappings = Mappings_Spikes;
        self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_Spikes);
        self->render_flags |= SST_RENDER_LEVEL;
        self->priority = 4 * 0x80;

        // Initialize spikes
        const uint8_t* set = spike_set[self->subtype >> 4];
        self->subtype &= 0xF;

        self->mapping_frame = *set++;
        self->width_pixels = *set++;
        own->orig_x = self->x_pos.f.u;
        own->orig_y = self->y_pos.f.u;
    }
        // Fallthrough
    case 2: {
        // Spike movement
        switch (self->subtype) {
        case 0: // Don't move
            break;
        case 1: // Move up and down
            Spike_Wait(self);
            self->y_pos.f.u = own->orig_y + own->move.f.u;
            break;
        case 2: // Move left and right
            Spike_Wait(self);
            self->x_pos.f.u = own->orig_x + own->move.f.u;
            break;
        }

        // Handle spike solidity and damage
        int16_t height = 4;
        if (self->mapping_frame == 5 || (height = 20, self->mapping_frame == 1)) {
            // Sideways
            signed int solid = SstSolidObject(self, 27, height, height + 1, self->x_pos.f.u, NULL, NULL);
            if (!(self->status & SST_STATUS_PLAYER_STAND) && solid == 1)
                Spike_Hurt(self);
        } else {
            // Up/down
            signed int solid = SstSolidObject(self, self->width_pixels + 11, 16, 17, self->x_pos.f.u, NULL, NULL);
            if ((self->status & SST_STATUS_PLAYER_STAND) || solid < 0)
                Spike_Hurt(self);
        }

        // Draw and unload once offscreen
        SstOnScreenTest2(self, own->orig_x);
        break;
    }
    }
}
