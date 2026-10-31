#include "Button.h"
#include "Resource/Mappings/Button.h"
#include "Object/PushBlock.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"

#include "Level.h"
#include "LevelScroll.h"
#include "Macros.h"
#include "Sound.h"

// Object 32 - buttons/switches (MZ, SYZ, LZ, SBZ)

// Checks whether a pushable MZ block (id_PushBlock) is currently resting
// on top of this button -- MZ act 1's special button that raises a spiked
// chandelier when a block is pushed onto it. Iterates every object
// of the status table looking for an on-screen PushBlock overlapping the
// button's own small hitbox.
static bool But_MZBlock(Sst *self) {
    int16_t left = (int16_t)(self->x_pos.f.u - 0x10);
    int16_t top = (int16_t)(self->y_pos.f.u - 8);

    for (int i = 0; i < SST_COUNT; i++) {
        const Sst *block = &sst[i];
        if (!(block->render_flags & SST_RENDER_ON_SCREEN) || block->code == NULL || block->code != Obj_PushBlock)
            continue;

        // Real ASM's interval test isn't a simple one-sided range -- it
        // accepts the block's edge approaching the button's own reference
        // edge from either direction, with a different allowed margin per
        // side (X is symmetric; Y allows more slack above than below).
        int16_t x_off = (int16_t)(block->x_pos.f.u - 0x10 - left);
        if (x_off >= 0) {
            if (x_off > 0x20)
                continue;
        } else if (x_off < -0x20) {
            continue;
        }
        int16_t y_off = (int16_t)(block->y_pos.f.u - 0x10 - top);
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

void Obj_Button(Sst *self) {
    switch (self->routine) {
    case 0: // Main
        self->routine += 2;
        self->mappings = Mappings_Button;
        self->art_tile = (LEVEL_ZONE(level_id) == ZoneId_MZ) ? TILE_MAP(0, 2, 0, 0, ArtTile_Button_Main) // | Tile_Pal3
                                                          : TILE_MAP(0, 0, 0, 0, ArtTile_Button_Main);
        self->render_flags = SST_RENDER_LEVEL;
        self->width_pixels = 32 / 2;
        self->priority = 4 * 0x80;
        self->y_pos.f.u += 3;
        break;
    case 2: { // Pressed
        if ((self->render_flags & SST_RENDER_ON_SCREEN)) {
            SstSolidObject(self, 32 / 2 + 11 /* sonic_solid_width */, 10 / 2, 10 / 2, self->x_pos.f.u, NULL, NULL);
            self->mapping_frame &= ~1; // unpressed frame

            uint8_t subtype = self->subtype;
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
                pressed = But_MZBlock(self) || ((self->status & SST_STATUS_PLAYER_STAND) != 0);
            else
                pressed = ((self->status & SST_STATUS_PLAYER_STAND) != 0);

            if (pressed) {
                if (!(f_switch[index] & (1 << bit)))
                    PlaySound(sfx_Switch);
                f_switch[index] |= (uint8_t)(1 << bit);
                self->mapping_frame |= 1; // pressed frame
            } else {
                f_switch[index] &= (uint8_t)~(1 << bit);
            }

            // Unused-in-game leftover: flashing red/gray animation when
            // bit 5 is set -- kept faithfully.
            if (subtype & 0x20) {
                if (--self->anim_frame_timer < 0) {
                    self->anim_frame_timer = 7;
                    self->mapping_frame ^= 2;
                }
            }
        }

        SstOnScreenTest2(self, self->x_pos.f.u);
        break;
    }
    }
}
