#include "VanishingPlatform.h"
#include "Resource/Animation/VanishingPlatforms.h"
#include "Resource/Mappings/VanishingPlatforms.h"
#include "Object.h"
#include "ObjectSst.h"
#include "SstLoad.h"

#include "Level.h"
#include "Macros.h"
#include "Object/Sonic.h"

// Object 6C - vanishing platforms (SBZ)

static void VanP_Animate(Sst *self) {
    Sst_Animate(self, Animation_VanishingPlatforms);
    SstOnScreenTest(self);
}

// Shared body of VanP_Detect (routine 2) and VanP_StoodOn (routine 4)
static void VanP_DetectOrStoodOn(Sst *self) {
    Own_VanishPlatform *own = (Own_VanishPlatform *)self->own.u8;

    if (--own->timer < 0) {
        own->timer = 0x80 - 1; // reset timer (while vanishing, anim == 0)

        if (self->anim != 0) { // currently appearing -- use the visible-duration timer instead
            own->timer = own->timelen;
        }

        self->anim ^= 1;
    }

    Sst_Animate(self, Animation_VanishingPlatforms);

    if (self->mapping_frame & 2) {
        // Not visible -- can't be solid
        if (((self->status & SST_STATUS_PLAYER_STAND) != 0)) {
            player->status &= (uint8_t)~SST_PSTATUS_OBJECT_STAND;
            self->status &= (uint8_t)~SST_STATUS_PLAYER_STAND;
            self->routine = 2; // VanP_Detect
        }
        SstOnScreenTest(self);
        return;
    }

    if (self->routine != 2) {
        // Sonic is already standing on the platform
        int16_t prev_x = self->x_pos.f.u;
        SstExitPlatform(self, self->width_pixels, self->width_pixels, NULL);
        SstMvSonicOnPtfm(self, self->y_pos.f.u - 9, prev_x);
        SstOnScreenTest(self);
        return;
    }

    SstPlatformObject(self, self->width_pixels);
    SstOnScreenTest(self);
}

void Obj_VanishPlatform(Sst *self) {
    Own_VanishPlatform *own = (Own_VanishPlatform *)self->own.u8;

    switch (self->routine) {
        case 0: { // VanP_Main
            self->routine += 6; // VanP_Sync
            self->mappings = Mappings_VanishingPlatforms;
            self->art_tile = TILE_MAP(0, 2, 0, 0, ArtTile_SBZ_Vanishing_Block); // | Tile_Pal3
            self->render_flags |= SST_RENDER_LEVEL;
            self->width_pixels = 32 / 2;
            self->priority = 4 * 0x80;

            uint8_t subtype = self->subtype;
            int16_t d0 = ((subtype & 0xF) + 1) << 7;
            int16_t d1 = d0;
            d0 -= 1;
            own->timer = d0;
            own->timelen = d0;

            uint16_t d0b = (subtype & 0xF0);
            d1 += 0x80;
            d0b = ((uint32_t)d0b * (uint32_t)d1) >> 8;
            own->syncoffset = d0b;
            d1 -= 1;
            own->syncmask = d1;
            break;
        }

        case 2: // VanP_Detect
        case 4: // VanP_StoodOn
            VanP_DetectOrStoodOn(self);
            break;

        case 6: { // VanP_Sync
            uint16_t d0 = frame_count - own->syncoffset;
            d0 &= own->syncmask;
            if (d0 == 0) {
                self->routine = 2; // VanP_Detect
                VanP_DetectOrStoodOn(self);
                break;
            }
            VanP_Animate(self);
            break;
        }
    }
}
