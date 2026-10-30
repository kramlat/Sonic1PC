#include "Jaws.h"

#include "Object.h"
#include "SstLoad.h"
#include "Resource/Animation/Jaws.h"
#include "Resource/Mappings/Jaws.h"

// Object 2C - Jaws enemy (LZ). Swims back and forth, turning around after
// a subtype-scaled delay.

typedef struct {
    int16_t turndelay_current; // delay before turning around (64 frames per subtype value)
    int16_t turndelay_base;    // base turn delay to reset to on turn
} Own_Jaws;

static void Jaws_Swim(Sst *self, Own_Jaws *own) {
    if (--own->turndelay_current < 0) {
        own->turndelay_current = own->turndelay_base;
        self->x_vel = (int16_t)-self->x_vel;
        self->status ^= SST_STATUS_X_FLIP;
        self->prev_anim = 1; // force animation to restart
    }

    Sst_Animate(self, Animation_Jaws);
    Sst_MoveSprite2(self);
    SstOnScreenTest(self);
}

void Obj_Jaws(Sst *self) {
    Own_Jaws *own = (Own_Jaws *)self->own.u8;

    switch (self->routine) {
    case 0: {
        self->routine += 2;
        self->mappings = Mappings_Jaws;
        self->art_tile = TILE_MAP(0, 1, 0, 0, ArtTile_Jaws); // | Tile_Pal2
        self->render_flags |= SST_RENDER_LEVEL;
        self->collision_flags = 0x0A; // col_32x24 | col_badnik
        self->priority = 4 * 0x80;
        self->width_pixels = 48 / 2; // Bug fix: real hardware's original 32/2 gets culled too early

        int16_t delay = (int16_t)((self->subtype << 6) - 1);
        own->turndelay_current = delay;
        own->turndelay_base = delay;

        self->x_vel = -0x40;
        if (self->status & SST_STATUS_X_FLIP)
            self->x_vel = (int16_t)-self->x_vel;
        __attribute__((fallthrough));
    }
    case 2:
        Jaws_Swim(self, own);
        break;
    }
}
