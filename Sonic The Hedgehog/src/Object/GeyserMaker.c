#include "GeyserMaker.h"
#include "Object.h"
#include "SstLoad.h"
#include "ObjectSst.h"

#include "LavaGeyser.h"
#include "Level.h"
#include "LevelScroll.h"
#include "Macros.h"
#include "Object/Sonic.h"

// Mappings_LavaGeyser and Animation_LavaGeyser are owned by LavaGeyser.c
// (its own includes directly bring in the actual array definitions) -- a
// bare extern here avoids a second definition and a link failure.
extern const uint8_t Mappings_LavaGeyser[];
extern const uint8_t Animation_LavaGeyser[];

// Object 4C - lava geyser / lavafall producer (MZ). Invisible until Sonic
// walks underneath (subtype 0, geyser) or on a fixed timer regardless
// (subtype 1, lavafall), at which point it spawns a LavaGeyser (object
// 4D) and bubbles until that finishes. Subtype-0 makers are only ever
// spawned programmatically by PushBlock (to launch the block into the
// air) -- never placed directly in a level layout.

static void GMake_Display(Sst *self) {
    if (SstOutOfRange(self->x_pos.f.u)) {
        SstLoad_Forget(self);
        Sst_Delete(self);
        return;
    }
    Sst_Animate(self, Animation_LavaGeyser);
    Sst_Draw(self);
}

void Obj_GeyserMaker(Sst *self) {
    Own_GeyserMaker *own = (Own_GeyserMaker *)self->own.u8;

    switch (self->routine) {
    case 0: // Main
        self->routine += 2;
        self->mappings = Mappings_LavaGeyser;
        self->art_tile = TILE_MAP(1, 3, 0, 0, ArtTile_MZ_Lava); // | Tile_Pal4 | Tile_Prio
        self->render_flags = SST_RENDER_LEVEL;
        self->priority = 1 * 0x80;
        self->width_pixels = 112 / 2;
        own->time = 120;
        __attribute__((fallthrough));
    case 2: // Wait
        if (--own->timer < 0) {
            own->timer = own->time;

            int16_t sonic_y = player->y_pos.f.u;
            int16_t maker_y = self->y_pos.f.u;
            if (sonic_y < maker_y && sonic_y >= (int16_t)(maker_y - 0x170))
                self->routine += 2;
        }
        SstDeleteIfNotInRange2(self, self->x_pos.f.u);
        break;
    case 4: // ChkType
        if (self->subtype == 0) { // geyser -- fall straight into Display this same frame
            GMake_Display(self);
            break;
        }
        self->routine += 2;
        SstDeleteIfNotInRange2(self, self->x_pos.f.u);
        break;
    case 6: { // MakeLava
        self->routine += 2;

        Sst *geyser = Sst_AllocateAfter(self);
        if (geyser != NULL) {
            geyser->code = Obj_LavaGeyser;
            geyser->x_pos.f.u = self->x_pos.f.u;
            geyser->y_pos.f.u = self->y_pos.f.u;
            geyser->subtype = self->subtype;
            ((Own_LavaGeyser *)geyser->own.u8)->parent_index = (uint8_t)(self - sst);
        }

        self->anim = 1; // ".bubble2"
        if (self->subtype != 0) {
            self->anim = 4; // ".blank" for lavafall
        } else {
            // Shoot the parent pushable block up into the air.
            Sst *parent = &sst[own->parent_index];
            parent->status |= 0x02; // bit1 -- PushBlock's own "shot up by geyser" flag, see PushBlock.c
            parent->y_vel = (int16_t)-0x580;
        }
        GMake_Display(self);
        break;
    }
    case 8: // Display
        GMake_Display(self);
        break;
    case 0xA: // Delete
        self->anim = 0; // ".bubble1"
        self->routine = 2;
        if (self->subtype == 0) {
            Sst_Delete(self); // geyser makers are one-shot
            return;
        }
        SstDeleteIfNotInRange2(self, self->x_pos.f.u); // lavafall makers stay alive indefinitely otherwise
        break;
    }
}
