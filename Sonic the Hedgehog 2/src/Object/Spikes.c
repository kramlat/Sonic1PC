// Spikes for Sonic 2 (the Simon Wai prototype's object 36): Sonic 1's spikes made solid for both characters, on palette line 1. The subtype's high nibble is the kind: 0-3 upright (16, 32, 48 or 64 wide), 4-7 sideways
// (16, 32, 48 or 64 tall), upside down when the object is flipped; its low nibble how they move (0 not, 1 up and down, 2 left and right).
#include "Object/Spikes.h"
#include "SstLoad.h"
#include "Resource/Mappings/Spikes.h"
#include "ObjectSst.h"
#include "Object.h"

#include "Level.h"
#include "LevelScroll.h"
#include "Object/Sonic.h"
#include "Object/Tails.h"
#include "SolidSst.h"
#include "Sound.h"

#include "Macros.h"

// The size of each kind: half its width and its height (the mapping's frame is the kind)
static const uint8_t spike_sizes[8][2] = { { 0x10, 0x10 }, { 0x20, 0x10 }, { 0x30, 0x10 }, { 0x40, 0x10 }, { 0x10, 0x10 }, { 0x10, 0x20 }, { 0x10, 0x30 }, { 0x10, 0x40 } };

static Sst *Character(int who) {
    return who == SolidChar_Sonic ? player : (TAILS_OBJ->code != NULL ? TAILS_OBJ : NULL);
}

// Touch_ChkHurt2: the character is pushed back out of the spikes and hurt (unless he is invincible or already hurt)
void Spikes_HurtCharacter(Sst *self, Sst *chr, int who) {
    if (invincibility)
        return;
    // (the prototype's Touch_ChkHurt2 has no check for the flashing after a hit -- Sonic 1's "Proper Spike Bug Fix" is not in it: spikes hurt a character who is still flashing, and the port keeps that)
    if (chr->routine >= 4)
        return;
    chr->y_pos.v -= chr->y_vel << 8;
    if (who == SolidChar_Sonic)
        HurtSonicFrom(chr, self->x_pos.f.u, true);
    else
        Tails_Hurt(chr, self);
}

static void Spike_Wait(Sst *self) {
    Own_Spikes *own = (Own_Spikes *)self->own.u8;

    // Wait for direction switch
    if (own->timer) {
        if (--own->timer)
            return;
        if ((self->render_flags & SST_RENDER_ON_SCREEN)) {
            ;
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

    if (self->routine == 0) {
        self->routine += 2;
        self->mappings = Mappings_Spikes;
        self->art_tile = TILE_MAP(0, 1, 0, 0, ArtTile_Spikes);
        self->render_flags |= SST_RENDER_LEVEL;
        self->priority = 4 * 0x80;

        unsigned kind = self->subtype >> 4;
        self->subtype &= 0xF;
        self->width_pixels = spike_sizes[kind][0];
        self->y_radius = (int8_t)spike_sizes[kind][1];
        self->mapping_frame = (uint8_t)kind;
        if (kind >= 4)
            self->routine += 2; // sideways
        if (((self->status & SST_STATUS_Y_FLIP) != 0))
            self->routine = 6;  // upside down
        own->orig_x = self->x_pos.f.u;
        own->orig_y = self->y_pos.f.u;
    }

    int16_t old_x = self->x_pos.f.u; // (the sideways ones are solid at where they were before they moved)
    switch (self->subtype) {
    case 1: // up and down
        Spike_Wait(self);
        self->y_pos.f.u = own->orig_y + own->move.f.u;
        break;
    case 2: // left and right
        Spike_Wait(self);
        self->x_pos.f.u = own->orig_x + own->move.f.u;
        break;
    }
    int16_t x = self->routine == 4 ? old_x : self->x_pos.f.u;

    for (int who = SolidChar_Sonic; who <= SolidChar_Tails; who++) {
        Sst *chr = Character(who);
        if (chr == NULL)
            continue;
        int32_t solid = SolidSst_Character(self, chr, who, (int16_t)(self->width_pixels + 0xB), self->y_radius, (int16_t)(self->y_radius + 1), x, NULL);
        switch (self->routine) {
        case 2: // upright: hurts whoever stands on it
            if (self->status & (1 << (3 + who)))
                Spikes_HurtCharacter(self, chr, who);
            break;
        case 4: // sideways: whoever runs into it
            if (solid == 1) {
                Spikes_HurtCharacter(self, chr, who);
                self->status &= (uint8_t)~(1 << (5 + who));
            }
            break;
        case 6: // upside down: whoever hits it from below
            if (solid == -2)
                Spikes_HurtCharacter(self, chr, who);
            break;
        }
    }

    // Draw and unload once offscreen
    Sst_Draw(self);
    SstDeleteIfNotInRange2(self, own->orig_x);
}
