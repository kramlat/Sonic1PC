#include "ShieldInvincibility.h"
#include "Resource/Animation/ShieldInvincibility.h"
#include "Resource/Mappings/ShieldInvincibility.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"
#include "Constants.h"

#include "Level.h"
#include "Object/Sonic.h"

void Obj_ShieldInvincibility(Sst *self);

void ShieldInvincibility_Make(uint8_t anim) {
    for (int i = 0; i < SST_COUNT; i++)
        if (sst[i].code == Obj_ShieldInvincibility && sst[i].anim == anim)
            return;
    Sst *self = Sst_SpawnCode(Obj_ShieldInvincibility, 0, 0);
    if (self != NULL)
        self->anim = anim;
}

// Shield and invincibility object
void Obj_ShieldInvincibility(Sst *self) {
    Own_Invincibility *own = (Own_Invincibility *)self->own.u8;

    switch (self->routine) {
    case 0: // Initialiation
        // Increment routine
        self->routine += 2;

        // Set object drawing information
        self->mappings = Mappings_ShieldInvincibility;
        self->render_flags = SST_RENDER_LEVEL;
        self->priority = 1 * 0x80;
        self->width_pixels = 16;

        // Check if invincibility or shield
        if (!self->anim) {
            // Shield
            self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_Shield);
        } else {
            // Invincibility
            self->routine += 2;
            self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_Invincibility);
        }
        break;
    case 2: // Shield
        // Check if shield should exist
        if (invincibility)
            break;
        if (!shield) {
            Sst_Delete(self);
            break;
        }

        // Update shield position
        self->x_pos.f.u = player->x_pos.f.u;
        self->y_pos.f.u = player->y_pos.f.u;
        self->status = player->status;

        // Animate and draw shield
        Sst_Animate(self, Animation_ShieldInvincibility);
        Sst_Draw(self);
        break;
    case 4: // Invincibility
        // Check if invincibility should exist
        if (!invincibility) {
            Sst_Delete(self);
            break;
        }

        // Get trail position
        uint8_t track_ind = track_pos.f.l;
        uint8_t track_off = (self->anim - 1) << 3;
        track_off = (track_off << 1) + track_off + 4;
        track_ind -= track_off + own->trail;
        if ((own->trail += 4) >= 0x18)
            own->trail = 0;

        // Set object position
        int16_t* pos = track_sonic[track_ind >> 2];
        self->x_pos.f.u = *pos++;
        self->y_pos.f.u = *pos++;
        self->status = player->status;

        // Animate and draw object
        Sst_Animate(self, Animation_ShieldInvincibility);
        Sst_Draw(self);
        break;
    }
}
