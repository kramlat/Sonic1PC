#include "Pole.h"
#include "Object.h"
#include "SstLoad.h"
#include "ObjectSst.h"

#include "Backend/Joypad.h"
#include "Game.h"
#include "Level.h"
#include "LevelScroll.h"
#include "Macros.h"
#include "Object/Sonic.h"
#include "Resource/Mappings/PoleThatBreaks.h"

// Object 0B - breakable pole in wind tunnels that Sonic hangs onto (LZ).
// Grabbed via the shared col_special dispatch (see Sonic.c's ReactToItem,
// col_type 0x21), then lets Sonic move up/down along it with raw input
// (control is frozen via lock_multi/f_playerctrl bit 0 while grabbed,
// which skips Sonic's own movement switch entirely) until either the player
// lets go, or -- if the subtype gave it a nonzero break timer -- it snaps
// on its own.

static void Pole_Release(Sst *self, Own_Pole *own) {
    self->collision_flags = 0;
    self->routine += 2; // -> Pole_Display
    lock_multi = 0;
    f_wtunneldisallow = false;
    own->grabbed = false;
}

static void Pole_CheckMoveUpDown(Sst *self) {
    int16_t top = (int16_t)(self->y_pos.f.u - 24);
    if (jpad1_hold1 & JPAD_UP) {
        player->y_pos.f.u--;
        if (player->y_pos.f.u < top)
            player->y_pos.f.u = top;
    }

    int16_t bottom = (int16_t)(top + 12 + 24);
    if (jpad1_hold1 & JPAD_DOWN) {
        player->y_pos.f.u++;
        if (player->y_pos.f.u > bottom)
            player->y_pos.f.u = bottom;
    }
}

static void Pole_Action(Sst *self, Own_Pole *own) {
    if (own->grabbed) {
        if (own->breaktime != 0) {
            if (--own->breaktime == 0) {
                self->mapping_frame = 1; // broken pole frame
                Pole_Release(self, own);
                SstOnScreenTest(self);
                return;
            }
        }

        Pole_CheckMoveUpDown(self);

        if (!(jpad1_press1 & (JPAD_A | JPAD_C | JPAD_B))) {
            SstOnScreenTest(self);
            return;
        }
        Pole_Release(self, own);
        SstOnScreenTest(self);
        return;
    }

    // Not grabbed yet -- check for a fresh grab
    if (!self->collision_property) {
        SstOnScreenTest(self);
        return;
    }

    int16_t target_x = (int16_t)(self->x_pos.f.u + 20);
    if (target_x >= player->x_pos.f.u) {
        SstOnScreenTest(self);
        return;
    }
    self->collision_property = 0;
    if (player->routine >= 4) { // Sonic is hurt or dying
        SstOnScreenTest(self);
        return;
    }

    player->x_vel = 0;
    player->y_vel = 0;
    player->x_pos.f.u = target_x;
    player->status &= (uint8_t)~SST_PSTATUS_X_FLIP;
    player->anim = SonAnimId_Hang;
    lock_multi = 1;
    f_wtunneldisallow = true;
    own->grabbed = true;

    SstOnScreenTest(self);
}

void Obj_Pole(Sst *self) {
    Own_Pole *own = (Own_Pole *)self->own.u8;

    switch (self->routine) {
    case 0:
        self->routine += 2;
        self->mappings = Mappings_PoleThatBreaks;
        self->art_tile = TILE_MAP(0, 2, 0, 0, ArtTile_LZ_Pole); // | Tile_Pal3
        self->render_flags = SST_RENDER_LEVEL;
        self->width_pixels = 16 / 2;
        self->priority = 4 * 0x80;
        self->collision_flags = 0x21 | 0xC0; // col_8x64 | col_special
        own->breaktime = (int16_t)(self->subtype * 60);
        __attribute__((fallthrough));
    case 2:
        Pole_Action(self, own);
        break;
    case 4:
        SstOnScreenTest(self);
        break;
    }
}
