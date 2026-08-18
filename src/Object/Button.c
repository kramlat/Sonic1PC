#include "Button.h"

#include "Level.h"
#include "LevelScroll.h"
#include "Macros.h"
#include "Sound.h"

// Object 32 - buttons/switches (MZ, SYZ, LZ, SBZ)

// Checks whether a pushable MZ block (id_PushBlock) is currently resting
// on top of this button -- MZ act 1's special button that raises a spiked
// chandelier when a block is pushed onto it. Iterates every dynamic
// object in level RAM looking for an on-screen PushBlock overlapping the
// button's own small hitbox.
static bool But_MZBlock(Object *obj) {
    int16_t left = (int16_t)(obj->pos.l.x.f.u - 0x10);
    int16_t top = (int16_t)(obj->pos.l.y.f.u - 8);

    Object *block = level_objects;
    for (int i = 0; i < LEVEL_OBJECTS; i++, block++) {
        if (!block->render.f.on_screen || block->type != ObjId_PushBlock)
            continue;

        // Real ASM's interval test isn't a simple one-sided range -- it
        // accepts the block's edge approaching the button's own reference
        // edge from either direction, with a different allowed margin per
        // side (X is symmetric; Y allows more slack above than below).
        int16_t x_off = (int16_t)(block->pos.l.x.f.u - 0x10 - left);
        if (x_off >= 0) {
            if (x_off > 0x20)
                continue;
        } else if (x_off < -0x20) {
            continue;
        }
        int16_t y_off = (int16_t)(block->pos.l.y.f.u - 0x10 - top);
        if (y_off >= 0) {
            if (y_off > 0x10)
                continue;
        } else if (y_off < -0x20) {
            continue;
        }

        return true;
    }
    return false;
}

void Obj_Button(Object *obj) {
    switch (obj->routine) {
    case 0: // Main
        obj->routine += 2;
        obj->mappings = Mappings_Button;
        obj->tile = (LEVEL_ZONE(level_id) == ZoneId_MZ) ? TILE_MAP(0, 2, 0, 0, ArtTile_Button_Main) // | Tile_Pal3
                                                          : TILE_MAP(0, 0, 0, 0, ArtTile_Button_Main);
        obj->render.b = 0;
        obj->render.f.align_fg = true;
        obj->width_pixels = 32 / 2;
        obj->priority = 4;
        obj->pos.l.y.f.u += 3;
        break;
    case 2: { // Pressed
        if (obj->render.f.on_screen) {
            SolidObject(obj, 32 / 2 + 11 /* sonic_solid_width */, 10 / 2, 10 / 2, obj->pos.l.x.f.u, NULL, NULL);
            obj->frame &= ~1; // unpressed frame

            uint8_t subtype = obj->scratch.u8[0];
            uint8_t index = subtype & 0xF;
            // Unused-in-game leftover: bit 6 redirects the pressed-state
            // flag to bit 7 of the same f_switch byte instead of bit 0 --
            // kept faithfully, matches real hardware's own dead feature.
            uint8_t bit = (subtype & 0x40) ? 7 : 0;

            bool pressed;
            // Bit 7 (subtype's own top bit) marks MZ1's special
            // chandelier-raising button -- guarded to MZ specifically,
            // matching the FixBugs fix (several buttons in OTHER zones'
            // layouts also happen to have this bit set for unrelated
            // reasons, making the unguarded check unsafe).
            if ((int8_t)subtype < 0 && LEVEL_ZONE(level_id) == ZoneId_MZ)
                pressed = But_MZBlock(obj) || obj->status.o.f.player_stand;
            else
                pressed = obj->status.o.f.player_stand;

            if (pressed) {
                if (!(f_switch[index] & (1 << bit)))
                    PlaySound(sfx_Switch);
                f_switch[index] |= (uint8_t)(1 << bit);
                obj->frame |= 1; // pressed frame
            } else {
                f_switch[index] &= (uint8_t)~(1 << bit);
            }

            // Unused-in-game leftover: flashing red/gray animation when
            // bit 5 is set -- kept faithfully.
            if (subtype & 0x20) {
                if (--obj->frame_time.b < 0) {
                    obj->frame_time.b = 7;
                    obj->frame ^= 2;
                }
            }
        }

        if (IS_OFFSCREEN(obj->pos.l.x.f.u)) {
            ObjectDelete(obj);
            return;
        }
        DisplaySprite(obj);
        break;
    }
    }
}
