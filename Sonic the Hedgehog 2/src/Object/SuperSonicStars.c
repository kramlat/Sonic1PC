// Object 7E of the alpha: Super Sonic's stars. They go off round Sonic when he runs at $800 or more and he is Super (super_sonic_flag), six frames at a time, and are gone with the flag. Nothing in the alpha
// makes the object (it has no transformation), and the art tile it names ($5F2) has no art in the alpha either, so nothing here makes it: it is the object, there for when a build has the transformation and the art.
#include "Object/SuperSonicStars.h"
#include "Constants.h"

#include "Object/CharControl.h"
#include "Object/Sonic.h"
#include "Object/SuperSonic.h"

#include "Resource/Mappings/SuperSonicStars.h"

typedef struct {
    uint8_t animating; // 0x30: a round of the stars is going
    uint8_t settled;   // 0x31: a round is over and the stars have stayed where it began
} Scratch_Stars;

void Obj_SuperSonicStars(Object *obj) {
    Scratch_Stars *stars = (Scratch_Stars *)&obj->scratch;

    if (obj->routine == 0) {
        obj->routine += 2;
        obj->mappings = Mappings_SuperSonicStars;
        obj->render.b = 0;
        obj->render.f.level_fg = true;
        obj->priority = 1;
        obj->width_pixels = 0x18;
        obj->tile = TILE_MAP(0, 0, 0, 0, ArtTile_SuperSonicStars);
        if (player->tile & TILE_PRIORITY_AND)
            obj->tile |= TILE_PRIORITY_AND;
    }

    if (!super_sonic_flag) {
        ObjectDelete(obj);
        return;
    }

    if (stars->animating) {
        if (--obj->frame_time.b < 0) {
            obj->frame_time.b = 1;
            if (++obj->frame >= 6) { // (a round is over: the frames go back to the first, and the stars wait for the next)
                obj->frame = 0;
                stars->animating = 0;
                stars->settled = 1;
                return;
            }
        }
        if (!stars->settled) {
            obj->pos.l.x.f.u = player->pos.l.x.f.u;
            obj->pos.l.y.f.u = player->pos.l.y.f.u;
        }
        DisplaySprite(obj);
        return;
    }

    if (CharObjControl(player) == 0) {
        const int16_t speed = (int16_t)(player->inertia < 0 ? -player->inertia : player->inertia);
        if (speed >= 0x800) { // fast enough: a round begins at Sonic
            obj->frame = 0;
            stars->animating = 1;
            obj->pos.l.x.f.u = player->pos.l.x.f.u;
            obj->pos.l.y.f.u = player->pos.l.y.f.u;
            DisplaySprite(obj);
            return;
        }
    }
    stars->animating = 0;
    stars->settled = 0;
}
