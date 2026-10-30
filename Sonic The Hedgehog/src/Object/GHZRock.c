#include "GHZRock.h"
#include "Resource/Mappings/GHZRock.h"
#include "Object.h"
#include "ObjectSst.h"
#include "SstLoad.h"

#include "LevelScroll.h"

#include "Macros.h"

// GHZ rock object
void Obj_GHZRock(Sst *self) {
    switch (self->routine) {
    case 0: // Initialization
        // Increment routine
        self->routine += 2;

        // Set object drawing information
        self->mappings = Mappings_GHZRock;
        self->art_tile = TILE_MAP(0, 3, 0, 0, ArtTile_GHZ_Purple_Rock);
        self->render_flags = SST_RENDER_LEVEL;
        self->width_pixels = 19;
        self->priority = 4 * 0x80;
        // Fallthrough
    case 2: // Solid
        // Act as solid object and draw
        SstSolidObject(self, 27, 16, 16, self->x_pos.f.u, NULL, NULL);
        Sst_Draw(self);

        // Delete once off-screen
        SstDeleteIfNotInRange(self);
        break;
    }
}
