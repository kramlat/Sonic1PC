#include "SpinPlatform.h"
#include "Object.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Constants.h"

#include "Level.h"
#include "Object/Sonic.h"
#include "Resource/Animation/SpinPlatform.h"
#include "Resource/Mappings/SpinningPlatforms.h"
#include "Resource/Mappings/Trapdoor.h"
#include "Sound.h"

// Object 69 - stationary spinning platforms and trapdoors (SBZ)

static void Spin_SolidAndDisplay(Sst *self, int16_t width_half, int16_t height_half) {
    if (self->mapping_frame == 0) { // fully closed / not spinning
        int16_t x_rad = (int16_t)(width_half + 11 /* sonic_solid_width */);
        SstSolidObject(self, (uint16_t)x_rad, height_half, (uint16_t)(height_half + 1), self->x_pos.f.u, NULL, NULL);
        SstOnScreenTest(self);
        return;
    }

    if (((self->status & SST_STATUS_PLAYER_STAND) != 0)) {
        player->status &= (uint8_t)~SST_PSTATUS_OBJECT_STAND;
        self->status &= (uint8_t)~SST_STATUS_PLAYER_STAND;
    }
    SstOnScreenTest(self);
}

static void Spin_Trapdoor(Sst *self, Own_SpinPlatform *own) {
    if (--own->timer < 0) {
        own->timer = own->timelen;
        self->anim ^= 1; // toggle between open/close trapdoor animations
        if ((self->render_flags & SST_RENDER_ON_SCREEN))
            QueueSound2(sfx_Door);
    }

    Sst_AnimateFlips(self, Animation_SpinPlatform); // stays on last trapdoor frame indefinitely
    Spin_SolidAndDisplay(self, 128 / 2, 24 / 2);
}

static void Spin_Spinner(Sst *self, Own_SpinPlatform *own) {
    if (((uint16_t)frame_count & own->syncmask) == 0)
        own->spinning = 1;

    if (own->spinning) {
        if (--own->timer < 0) {
            own->timer = own->timelen;
            own->spinning = 0;
            self->anim ^= 1; // toggle between the (otherwise identical) spin animations
        }
    }

    Sst_AnimateFlips(self, Animation_SpinPlatform); // stays on last frame (ID 0) indefinitely
    Spin_SolidAndDisplay(self, 32 / 2, 14 / 2);
}

void Obj_SpinPlatform(Sst *self) {
    Own_SpinPlatform *own = (Own_SpinPlatform *)self->own.u8;

    if (self->routine == 0) {
        self->routine = 2; // advance to Spin_Trapdoor
        self->mappings = Mappings_Trapdoor;
        self->art_tile = TILE_MAP(0, 2, 0, 0, ArtTile_SBZ_Trap_Door); // | Tile_Pal3
        self->render_flags |= SST_RENDER_LEVEL;
        self->width_pixels = 128 / 2; // FixBugs: real hardware used 256/2 here, causing screen-wrap issues

        uint8_t subtype = self->subtype;
        own->timelen = (int16_t)((subtype & 0x0F) * 60);

        if (subtype & 0x80) { // spinning platform, not a trapdoor
            self->routine = 4; // advance to Spin_Spinner
            self->mappings = Mappings_SpinningPlatforms;
            self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_SBZ_Spinning_Platform);
            self->width_pixels = 32 / 2;
            self->anim = 2;

            int16_t t = (int16_t)((subtype & 0x0F) * 6);
            own->timer = t;
            own->timelen = t;
            own->syncmask = (uint16_t)((((subtype & 0x70) + 0x10) << 2) - 1);

            Spin_Spinner(self, own);
            return;
        }
    }

    if (self->routine == 4)
        Spin_Spinner(self, own);
    else
        Spin_Trapdoor(self, own);
}
