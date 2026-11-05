#include "Fan.h"
#include "Object.h"
#include "ObjectSst.h"
#include "SstLoad.h"
#include "Constants.h"

#include "Level.h"
#include "LevelScroll.h"
#include "Object/Sonic.h"
#include "Resource/Mappings/Fan.h"

// Object 5D - fans (SLZ)
//
// Subtype bitfield:
//   bit 0 = blows backwards (also flips its animation)
//   bit 1 = always on (skips the on/off timer entirely)

static void Fan_Push(Sst *self) {
    int16_t d0 = (int16_t)(player->x_pos.f.u - self->x_pos.f.u);
    if (!((self->status & SST_STATUS_X_FLIP) != 0))
        d0 = (int16_t)-d0;
    d0 = (int16_t)(d0 + 0x50);
    if ((uint16_t)d0 >= (0xA0 + 0x50))
        return; // too far horizontally

    uint16_t d1 = (uint16_t)(player->y_pos.f.u + 0x60 - self->y_pos.f.u);
    if (d1 >= 0x70)
        return; // out of vertical range

    // Sonic is in range of the fan
    bool close = (uint16_t)d0 < 0x50;
    d0 = (int16_t)(d0 - 0x50);
    if (close) {
        d0 = (int16_t)~d0;      // make push force positive again
        d0 = (int16_t)(d0 * 2); // double push force when close
    }
    d0 = (int16_t)(d0 + 0x60); // base push force

    if (!((self->status & SST_STATUS_X_FLIP) != 0))
        d0 = (int16_t)-d0;

    // Negate only the subpixel (low byte) portion of the push force --
    // matches the real disasm's own neg.b quirk exactly.
    uint8_t lo = (uint8_t)-(int8_t)(uint8_t)d0;
    d0 = (int16_t)((d0 & 0xFF00) | lo);

    d0 = (int16_t)(d0 >> 4);
    if (self->subtype & 1) // blows backwards
        d0 = (int16_t)-d0;

    player->x_pos.f.u = (int16_t)(player->x_pos.f.u + d0);
}

static void Fan_Animate(Sst *self) {
    if (--self->anim_frame_timer >= 0)
        return;
    self->anim_frame_timer = 0;

    self->anim_frame++;
    if (self->anim_frame >= 3)
        self->anim_frame = 0;

    uint8_t base = (self->subtype & 1) ? 2 : 0;
    self->mapping_frame = (uint8_t)(base + self->anim_frame);
}

static void Fan_Action(Sst *self, Own_Fan *own) {
    if (!(self->subtype & 2)) { // not always-on -- run the on/off timer
        if (--own->time < 0) {
            own->time = 2 * 60;
            own->fan_switch ^= 1;
            if (own->fan_switch)
                own->time = 3 * 60;
        }
    }

    if (own->fan_switch == 0) {
        if (!debug_use) // FixBugs: fans shouldn't push Sonic around in debug mode
            Fan_Push(self);
        Fan_Animate(self);
    }

    SstOnScreenTest(self);
}

void Obj_Fan(Sst *self) {
    Own_Fan *own = (Own_Fan *)self->own.u8;

    if (self->routine == 0) {
        self->routine = 2; // advance to Fan_Action
        self->mappings = Mappings_Fan;
        self->art_tile = TILE_MAP(0, 2, 0, 0, ArtTile_SLZ_Fan); // | Tile_Pal3
        self->render_flags |= SST_RENDER_LEVEL;
        self->width_pixels = 32 / 2;
        self->priority = 4 * 0x80;
    }

    Fan_Action(self, own);
}
