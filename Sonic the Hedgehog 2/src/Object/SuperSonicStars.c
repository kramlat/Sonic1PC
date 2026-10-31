// Object 7E of the alpha: Super Sonic's stars. They go off round Sonic when he runs at $800 or more and he is Super (super_sonic_flag), six frames at a time, and are gone with the flag. Nothing in the alpha
// makes the object (it has no transformation), and the art tile it names ($5F2) has no art in the alpha either, so nothing here makes it: it is the object, there for when a build has the transformation and the art.
#include "Object/SuperSonicStars.h"
#include "Constants.h"
#include "ObjectSst.h"

#include "Object/CharControl.h"
#include "Object/Sonic.h"
#include "Object/SuperSonic.h"

#include "Resource/Mappings/SuperSonicStars.h"

typedef struct {
    uint8_t animating; // a round of the stars is going
    uint8_t settled;   // a round is over and the stars have stayed where it began
} Own_Stars;

void Obj_SuperSonicStars(Sst *self) {
    Own_Stars *stars = (Own_Stars *)self->own.u8;

    if (self->routine == 0) {
        self->routine += 2;
        self->mappings = Mappings_SuperSonicStars;
        self->render_flags = SST_RENDER_LEVEL;
        self->priority = 1 * 0x80;
        self->width_pixels = 0x18;
        self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_SuperSonicStars);
        if (player->art_tile & TILE_PRIORITY_AND)
            self->art_tile |= TILE_PRIORITY_AND;
    }

    if (!super_sonic_flag) {
        Sst_Delete(self);
        return;
    }

    if (stars->animating) {
        if (--self->anim_frame_timer < 0) {
            self->anim_frame_timer = 1;
            if (++self->mapping_frame >= 6) { // (a round is over: the frames go back to the first, and the stars wait for the next)
                self->mapping_frame = 0;
                stars->animating = 0;
                stars->settled = 1;
                return;
            }
        }
        if (!stars->settled) {
            self->x_pos.f.u = player->x_pos.f.u;
            self->y_pos.f.u = player->y_pos.f.u;
        }
        Sst_Draw(self);
        return;
    }

    if (CharObjControl(player) == 0) {
        const int16_t speed = (int16_t)(player->ground_vel < 0 ? -player->ground_vel : player->ground_vel);
        if (speed >= 0x800) { // fast enough: a round begins at Sonic
            self->mapping_frame = 0;
            stars->animating = 1;
            self->x_pos.f.u = player->x_pos.f.u;
            self->y_pos.f.u = player->y_pos.f.u;
            Sst_Draw(self);
            return;
        }
    }
    stars->animating = 0;
    stars->settled = 0;
}
