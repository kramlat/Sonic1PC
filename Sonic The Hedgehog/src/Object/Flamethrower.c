#include "Flamethrower.h"
#include "Object.h"
#include "ObjectSst.h"
#include "SstLoad.h"
#include "Constants.h"

#include "Level.h"
#include "LevelScroll.h"
#include "Resource/Animation/Flamethrower.h"
#include "Resource/Mappings/Flamethrower.h"
#include "Sound.h"

// Object 6D - flame thrower (SBZ)

static void Flame_Animate(Sst *self, Own_Flamethrower *own) {
    Sst_Animate(self, Animation_Flamethrower); // stays on last 1/2 frames per animation indefinitely

    self->collision_flags = 0; // col_none -- harmless by default
    if (self->mapping_frame == own->hurtframe)
        self->collision_flags = 0x23 | 0x80; // col_24x48 | col_hurt

    SstOnScreenTest(self);
}

static void Flame_Action(Sst *self, Own_Flamethrower *own) {
    if (--own->timer >= 0) {
        Flame_Animate(self, own);
        return;
    }

    own->timer = own->pausetime; // begin pause time
    uint8_t before = self->anim & 1;
    self->anim ^= 1; // toggle between expanding/retracting animations
    if (before == 0) {
        Flame_Animate(self, own); // now retracting -- pause
        return;
    }

    own->timer = own->firetime; // now expanding again -- begin flaming time
    QueueSound2(sfx_Flamethrower);
    Flame_Animate(self, own);
}

void Obj_Flamethrower(Sst *self) {
    Own_Flamethrower *own = (Own_Flamethrower *)self->own.u8;

    if (self->routine == 0) {
        self->routine = 2; // advance to Flame_Action
        self->mappings = Mappings_Flamethrower;
        self->art_tile = TILE_MAP(1, 0, 0, 0, ArtTile_SBZ_Flamethrower); // | Tile_Prio
        self->render_flags |= SST_RENDER_LEVEL;
        self->priority = 1 * 0x80;
        self->width_pixels = 24 / 2;

        uint16_t d0 = (uint16_t)((self->subtype & 0xF0) * 2);
        own->timer = (int16_t)d0;
        own->firetime = (int16_t)d0;

        own->pausetime = (int16_t)((self->subtype & 0x0F) << 5);

        own->hurtframe = 0xA; // broken-pipe flamethrower
        if (((self->status & SST_STATUS_Y_FLIP) != 0)) {
            self->anim = 2; // "valve" animations
            own->hurtframe = 0x15; // valve flamethrower
        }
    }

    Flame_Action(self, own);
}
