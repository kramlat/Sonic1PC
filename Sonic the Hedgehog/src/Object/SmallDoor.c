#include "SmallDoor.h"
#include "Object.h"
#include "ObjectSst.h"
#include "SstLoad.h"
#include "Constants.h"

#include "Level.h"
#include "Object/Sonic.h"
#include "Resource/Animation/SmallDoor.h"
#include "Resource/Mappings/SmallDoor.h"

// Object 2A - small vertical door (SBZ)

static void ADoor_OpenShut(Sst *self) {
    self->anim = 0; // closing, by default

    int16_t sonic_x = player->x_pos.f.u;
    int16_t door_x = self->x_pos.f.u;

    if ((uint16_t)(sonic_x + 64) >= (uint16_t)door_x &&
        (uint16_t)(sonic_x - 64) < (uint16_t)door_x) {
        // Sonic is within 64px of the door -- check which side he's on
        bool sonic_past = (uint16_t)sonic_x >= (uint16_t)door_x;
        bool open = sonic_past ? ((self->status & SST_STATUS_X_FLIP) != 0) : !((self->status & SST_STATUS_X_FLIP) != 0);
        if (open)
            self->anim = 1; // opening
    }

    Sst_Animate(self, Animation_SmallDoor); // animations stay on final frames indefinitely

    if (self->mapping_frame == 0) { // fully closed
        int16_t x_rad = (int16_t)(12 / 2 + 11 /* sonic_solid_width */);
        SstSolidObject(self, (uint16_t)x_rad, 64 / 2, 64 / 2 + 1, self->x_pos.f.u, NULL, NULL);
    }

    SstOnScreenTest(self);
}

void Obj_SmallDoor(Sst *self) {
    if (self->routine == 0) {
        self->routine = 2; // advance to ADoor_OpenShut
        self->mappings = Mappings_SmallDoor;
        self->art_tile = TILE_MAP(0, 2, 0, 0, ArtTile_SBZ_Door); // | Tile_Pal3
        self->render_flags |= SST_RENDER_LEVEL;
        self->width_pixels = 16 / 2;
        self->priority = 4 * 0x80;
    }

    ADoor_OpenShut(self);
}
