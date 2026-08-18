#include "Fan.h"

#include "Level.h"
#include "LevelScroll.h"
#include "Object/Sonic.h"
#include "Resource/Mappings/Fan.h"

// Object 5D - fans (SLZ)
//
// Subtype bitfield:
//   bit 0 = blows backwards (also flips its animation)
//   bit 1 = always on (skips the on/off timer entirely)

static bool Fan_OutOfRange(int16_t x) {
    uint16_t obj_pos = (uint16_t)x & 0xFF80;
    uint16_t cam_pos = (uint16_t)(scrpos_x.f.u - 128) & 0xFF80;
    return (uint16_t)(obj_pos - cam_pos) > (128 + 320 + 192);
}

static void Fan_Push(Object *obj) {
    int16_t d0 = (int16_t)(player->pos.l.x.f.u - obj->pos.l.x.f.u);
    if (!obj->status.o.f.x_flip)
        d0 = (int16_t)-d0;
    d0 = (int16_t)(d0 + 0x50);
    if ((uint16_t)d0 >= (0xA0 + 0x50))
        return; // too far horizontally

    uint16_t d1 = (uint16_t)(player->pos.l.y.f.u + 0x60 - obj->pos.l.y.f.u);
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

    if (!obj->status.o.f.x_flip)
        d0 = (int16_t)-d0;

    // Negate only the subpixel (low byte) portion of the push force --
    // matches the real disasm's own neg.b quirk exactly.
    uint8_t lo = (uint8_t)-(int8_t)(uint8_t)d0;
    d0 = (int16_t)((d0 & 0xFF00) | lo);

    d0 = (int16_t)(d0 >> 4);
    if (obj->scratch.u8[0] & 1) // blows backwards
        d0 = (int16_t)-d0;

    player->pos.l.x.f.u = (int16_t)(player->pos.l.x.f.u + d0);
}

static void Fan_Animate(Object *obj) {
    if (--obj->frame_time.b >= 0)
        return;
    obj->frame_time.b = 0;

    obj->anim_frame++;
    if (obj->anim_frame >= 3)
        obj->anim_frame = 0;

    uint8_t base = (obj->scratch.u8[0] & 1) ? 2 : 0;
    obj->frame = (uint8_t)(base + obj->anim_frame);
}

static void Fan_Action(Object *obj, Scratch_Fan *scratch) {
    if (!(obj->scratch.u8[0] & 2)) { // not always-on -- run the on/off timer
        if (--scratch->time < 0) {
            scratch->time = 2 * 60;
            scratch->fan_switch ^= 1;
            if (scratch->fan_switch)
                scratch->time = 3 * 60;
        }
    }

    if (scratch->fan_switch == 0) {
        if (!debug_use) // FixBugs: fans shouldn't push Sonic around in debug mode
            Fan_Push(obj);
        Fan_Animate(obj);
    }

    if (Fan_OutOfRange(obj->pos.l.x.f.u))
        ObjectDelete(obj);
    else
        DisplaySprite(obj);
}

void Obj_Fan(Object *obj) {
    Scratch_Fan *scratch = (Scratch_Fan *)&obj->scratch;

    if (obj->routine == 0) {
        obj->routine = 2; // advance to Fan_Action
        obj->mappings = Mappings_Fan;
        obj->tile = TILE_MAP(0, 2, 0, 0, ArtTile_SLZ_Fan); // | Tile_Pal3
        obj->render.f.align_fg = true;
        obj->width_pixels = 32 / 2;
        obj->priority = 4;
    }

    Fan_Action(obj, scratch);
}
