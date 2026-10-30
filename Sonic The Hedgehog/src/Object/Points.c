#include "Points.h"
#include "Constants.h"
#include "Object.h"

#include "Resource/Mappings/Points.h"

// Object 29 - the score that rises where something was destroyed. Its creator sets the frame (which score).

void Obj_Points(Sst *self) {
    switch (self->routine) {
        case 0: // Poi_Main
            self->routine += 2;
            self->mappings = Mappings_Points;
            self->art_tile = TILE_MAP(0, 1, 0, 0, ArtTile_Points);
            self->render_flags = SST_RENDER_LEVEL;
            self->priority = 1 * 0x80;
            self->width_pixels = 8;
            self->y_vel = -0x300; // move object upwards
            // Fall through
        case 2: // Poi_Slower
            if (self->y_vel >= 0) {
                Sst_Delete(self);
                return;
            }
            Sst_MoveSprite2(self);
            self->y_vel += 0x18; // reduce object speed
            break;
    }
    Sst_Draw(self);
}
