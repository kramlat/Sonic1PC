#include "Pole.h"

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

static void Pole_Release(Object *obj, Scratch_Pole *scratch) {
    obj->col_type = 0;
    obj->routine += 2; // -> Pole_Display
    lock_multi = 0;
    f_wtunneldisallow = false;
    scratch->grabbed = false;
}

static void Pole_CheckMoveUpDown(Object *obj) {
    int16_t top = (int16_t)(obj->pos.l.y.f.u - 24);
    if (jpad1_hold1 & JPAD_UP) {
        player->pos.l.y.f.u--;
        if (player->pos.l.y.f.u < top)
            player->pos.l.y.f.u = top;
    }

    int16_t bottom = (int16_t)(top + 12 + 24);
    if (jpad1_hold1 & JPAD_DOWN) {
        player->pos.l.y.f.u++;
        if (player->pos.l.y.f.u > bottom)
            player->pos.l.y.f.u = bottom;
    }
}

static void Pole_Action(Object *obj, Scratch_Pole *scratch) {
    if (scratch->grabbed) {
        if (scratch->breaktime != 0) {
            if (--scratch->breaktime == 0) {
                obj->frame = 1; // broken pole frame
                Pole_Release(obj, scratch);
                RememberState(obj);
                return;
            }
        }

        Pole_CheckMoveUpDown(obj);

        if (!(jpad1_press1 & (JPAD_A | JPAD_C | JPAD_B))) {
            RememberState(obj);
            return;
        }
        Pole_Release(obj, scratch);
        RememberState(obj);
        return;
    }

    // Not grabbed yet -- check for a fresh grab
    if (!obj->col_property) {
        RememberState(obj);
        return;
    }

    int16_t target_x = (int16_t)(obj->pos.l.x.f.u + 20);
    if (target_x >= player->pos.l.x.f.u) {
        RememberState(obj);
        return;
    }
    obj->col_property = 0;
    if (player->routine >= 4) { // Sonic is hurt or dying
        RememberState(obj);
        return;
    }

    player->xsp = 0;
    player->ysp = 0;
    player->pos.l.x.f.u = target_x;
    player->status.p.f.x_flip = false;
    player->anim = SonAnimId_Hang;
    lock_multi = 1;
    f_wtunneldisallow = true;
    scratch->grabbed = true;

    RememberState(obj);
}

void Obj_Pole(Object *obj) {
    Scratch_Pole *scratch = (Scratch_Pole *)&obj->scratch;

    switch (obj->routine) {
    case 0:
        obj->routine += 2;
        obj->mappings = Mappings_PoleThatBreaks;
        obj->tile = TILE_MAP(0, 2, 0, 0, 0x3DE); // ArtTile_LZ_Pole | Tile_Pal3
        obj->render.b = 0;
        obj->render.f.align_fg = true;
        obj->width_pixels = 16 / 2;
        obj->priority = 4;
        obj->col_type = 0x21 | 0xC0; // col_8x64 | col_special
        scratch->breaktime = (int16_t)(obj->scratch.u8[0] * 60);
        __attribute__((fallthrough));
    case 2:
        Pole_Action(obj, scratch);
        break;
    case 4:
        RememberState(obj);
        break;
    }
}
