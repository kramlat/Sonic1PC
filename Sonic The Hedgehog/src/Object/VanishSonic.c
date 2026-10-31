#include "VanishSonic.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"
#include "Constants.h"

#include "Level.h"
#include "Object/Sonic.h"
#include "PLC.h"
#include "Resource/Animation/VanishSonic.h"
#include "Resource/Mappings/VanishSonic.h"
#include "Sound.h"

static void Van_DeleteSonic(Sst *self) {
    self->x_pos.f.u = player->x_pos.f.u;
    self->y_pos.f.u = player->y_pos.f.u;
    self->status = player->status;

    Sst_Animate(self, Animation_VanishSonic); // advances self->routine on finish (afRoutine)

    if (self->mapping_frame == 2 && player->code != sst_object_codes[ObjId_Null]) { // once, at the animation's middle
        player->code = sst_object_codes[ObjId_Null]; // delete Sonic
        QueueSound2(sfx_SSGoal);
    }

    Sst_Draw(self);
}

static void Van_ReloadSonic(Sst *self, Own_VanishSonic *own) {
    if (--own->time != 0)
        return; // wait for timer to run out (object stays invisible meanwhile)
    player->code = Obj_Sonic; // reload Sonic
    Sst_Delete(self);
}

void Obj_VanishSonic(Sst *self) {
    Own_VanishSonic *own = (Own_VanishSonic *)self->own.u8;

    if (self->routine == 0) {
        if (plc_buffer[0].art != NULL)
            return; // wait until entry effect patterns finish decompressing

        self->routine = 2; // advance to Van_DeleteSonic
        self->mappings = Mappings_VanishSonic;
        self->render_flags = SST_RENDER_LEVEL;
        self->priority = 1 * 0x80;
        self->width_pixels = 112 / 2;
        self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_Warp);
        own->time = 2 * 60;
    }

    switch (self->routine) {
    case 2: Van_DeleteSonic(self); break;
    case 4: Van_ReloadSonic(self, own); break;
    }
}
