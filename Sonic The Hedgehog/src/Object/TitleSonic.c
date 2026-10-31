#include "TitleSonic.h"
#include "Resource/Mappings/TitleSonic.h"
#include "Resource/Animation/TitleSonic.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"
#include "Constants.h"

// Title Sonic object
void Obj_TitleSonic(Sst *self)
{
    switch (self->routine) {
    case 0: // Initialization
        // Increment routine and initialize position
        self->routine += 2;

        self->x_pos.f.u = 0xF0 + (PLANE_WIDEADD * 4);
        self->y_pos.f.u = 0xDE + SCREEN_TALLADD2;

        // Set object drawing information
        self->mappings = Mappings_TitleSonic;
        self->art_tile = TILE_MAP(0, 1, 0, 0, ArtTile_Title_Sonic);
        self->priority = 1 * 0x80;

        // Initialize state
        self->anim_frame_timer = 29;
        Sst_Animate(self, Animation_TitleSonic);
        // Fallthrough
    case 2: // Waiting to appear
        // Wait for timer to clear
        if ((--self->anim_frame_timer) >= 0)
            return;

        // Increment routine
        self->routine += 2;
        Sst_Draw(self);
        break;
    case 4: // Moving upwards
        // Move upwards and increment routine when end point reached
        if ((self->y_pos.f.u -= 8) == 0x96 + SCREEN_TALLADD2)
            self->routine += 2;
        Sst_Draw(self);
        break;
    case 6: // Animating
        Sst_Animate(self, Animation_TitleSonic);
        Sst_Draw(self);
        break;
    }
}
