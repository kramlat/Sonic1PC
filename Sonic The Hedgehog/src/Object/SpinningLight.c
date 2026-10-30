#include "SpinningLight.h"

#include "Object.h"
#include "SstLoad.h"
#include "Resource/Mappings/Light.h"

// Object 12 - spinning light in hexagonal glass prism (SYZ). Purely
// decorative: cycles through 6 frames, no collision.

static void Light_Animate(Sst *self) {
    if ((int8_t)--self->anim_frame_timer < 0) {
        self->anim_frame_timer = 8 - 1;
        self->mapping_frame++;
        if (self->mapping_frame >= 6)
            self->mapping_frame = 0;
    }

    SstOnScreenTest(self);
}

void Obj_SpinningLight(Sst *self) {
    switch (self->routine) {
    case 0:
        self->routine += 2;
        self->mappings = Mappings_Light;
        self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_Level); // ArtTile_Level (part of the level's own already-loaded graphics)
        self->render_flags = SST_RENDER_LEVEL;
        self->width_pixels = 32 / 2;
        self->priority = 6 * 0x80;
        __attribute__((fallthrough));
    case 2:
        Light_Animate(self);
        break;
    }
}
