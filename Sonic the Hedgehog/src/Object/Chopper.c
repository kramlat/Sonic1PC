#include "Chopper.h"
#include "Constants.h"
#include "Object.h"
#include "SstLoad.h"

// Chopper assets
#include "Resource/Mappings/Chopper.h"
#include "Resource/Animation/Chopper.h"

// Object 2B - Chopper (GHZ): jumps up out of the water again and again
typedef struct {
    int16_t orig_y;
} Own_Chopper;

void Obj_Chopper(Sst *self) {
    Own_Chopper *own = (Own_Chopper *)self->own.u8;
    switch (self->routine) {
        case 0: // Initialization
            self->routine += 2;
            self->mappings = Mappings_Chopper;
            self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_Chopper);
            self->render_flags = SST_RENDER_LEVEL;
            self->priority = 4 * 0x80;
            self->collision_flags = 0x09;
            self->width_pixels = 16;
            self->y_vel = -0x700;
            own->orig_y = self->y_pos.f.u;
            break;

        case 2: // Moving
            Sst_MoveSprite2(self);
            self->y_vel += 0x18;
            if (own->orig_y < self->y_pos.f.u) { // (back at the water)
                self->y_pos.f.u = own->orig_y;
                self->y_vel = -0x700;
            }

            self->anim = 1;
            if ((own->orig_y - 0xC0) < self->y_pos.f.u) {
                self->anim = 0;
                if (self->y_vel >= 0)
                    self->anim = 2;
            }
            Sst_Animate(self, Animation_Chopper);
            break;
    }
    // Draw and unload once off-screen
    SstOnScreenTest(self);
}
