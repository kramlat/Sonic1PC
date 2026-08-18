#include "VanishSonic.h"

#include "Level.h"
#include "Object/Sonic.h"
#include "PLC.h"
#include "Resource/Animation/VanishSonic.h"
#include "Resource/Mappings/VanishSonic.h"
#include "Sound.h"

static void Van_DeleteSonic(Object *obj) {
    obj->pos.l.x.f.u = player->pos.l.x.f.u;
    obj->pos.l.y.f.u = player->pos.l.y.f.u;
    obj->status.b = player->status.b;

    AnimateSprite(obj, Animation_VanishSonic); // advances obj->routine on finish (afRoutine)

    if (obj->frame == 2 && player->type != ObjId_Null) { // once, at the animation's middle
        player->type = ObjId_Null; // delete Sonic
        QueueSound2(sfx_SSGoal);
    }

    DisplaySprite(obj);
}

static void Van_ReloadSonic(Object *obj, Scratch_VanishSonic *scratch) {
    if (--scratch->time != 0)
        return; // wait for timer to run out (object stays invisible meanwhile)
    player->type = ObjId_Sonic; // reload Sonic
    ObjectDelete(obj);
}

void Obj_VanishSonic(Object *obj) {
    Scratch_VanishSonic *scratch = (Scratch_VanishSonic *)&obj->scratch;

    if (obj->routine == 0) {
        if (plc_buffer[0].art != NULL)
            return; // wait until entry effect patterns finish decompressing

        obj->routine = 2; // advance to Van_DeleteSonic
        obj->mappings = Mappings_VanishSonic;
        obj->render.f.align_fg = true;
        obj->priority = 1;
        obj->width_pixels = 112 / 2;
        obj->tile = TILE_MAP(0, 0, 0, 0, ArtTile_Warp);
        scratch->time = 2 * 60;
    }

    switch (obj->routine) {
    case 2: Van_DeleteSonic(obj); break;
    case 4: Van_ReloadSonic(obj, scratch); break;
    }
}
