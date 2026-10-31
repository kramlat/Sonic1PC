#include "PSB.h"
#include "Resource/Mappings/PSB.h"
#include "Resource/Animation/PSB.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"
#include "Constants.h"

//'PRESS START BUTTON' object
void Obj_PSB(Sst *self) {
    switch (self->routine) {
    case 0: // Initialization
        // Increment routine and set position
        self->routine += 2;
        self->x_pos.f.u = 0xD0 + (PLANE_WIDEADD * 4);
        self->y_pos.f.u = 0x130 + SCREEN_TALLADD2;

        // Set object drawing information
        self->mappings = Mappings_PSB;
        self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_Title_Foreground);

        // Handle different frames
        if (self->mapping_frame >= 2) {
            self->routine += 2;
            if (self->mapping_frame == 3) {
                // Trademark
                self->art_tile = TILE_MAP(0, 1, 0, 0, ArtTile_Title_Trademark);
                self->x_pos.f.u = 0x170 + (PLANE_WIDEADD * 4);
                self->y_pos.f.u = 0xF8 + SCREEN_TALLADD2;
            } else if (self->mapping_frame == 2) {
                // Sonic mask
                self->x_pos.f.u -= SCREEN_WIDEADD2; // Widescreen hack so you don't see the masking sprites
            }
            break;
        }
        // Fallthrough
    case 2: // Press Start Button
        Sst_Animate(self, Animation_PSB);
        break;
    case 4: // TM or Sonic mask
        break;
    }

    Sst_Draw(self);
}
