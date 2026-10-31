#include "Helix.h"
#include "Resource/Mappings/SpikedPoleHelix.h"
#include "Object.h"
#include "ObjectSst.h"
#include "SstLoad.h"

#include "Level.h"
#include "LevelScroll.h"
#include "Macros.h"

// Object 17 - rotating helix of spikes on a horizontal pole (GHZ3)

static void Hel_RotateSpikes(Sst *self, Own_Helix *own) {
    uint8_t frame = (uint8_t)(sprite_anim[0].frame + own->frame_base) & 7;
    self->mapping_frame = frame;
    self->collision_flags = (frame == 0) ? 0x84 : 0x00; // col_8x32 | col_hurt while "pointing up" (frame 0), harmless otherwise
}

// Deletes every child spike this parent spawned (parent itself is deleted
// separately by the caller) -- real hardware's DeleteChild is just a
// plain object-memory clear (no respawn-index bookkeeping to worry
// about, safe on any object).
static void Hel_DeleteChildren(Own_Helix *own) {
    for (int i = 0; i < own->children_count - 1; i++)
        Sst_Delete(&sst[own->children[i]]);
}

void Obj_Helix(Sst *self) {
    Own_Helix *own = (Own_Helix *)self->own.u8;

    switch (self->routine) {
    case 0: { // Main
        self->routine += 2;
        self->mappings = Mappings_SpikedPoleHelix;
        self->art_tile = TILE_MAP(0, 2, 0, 0, ArtTile_GHZ_Spike_Pole); // | Tile_Pal3
        self->render_flags = SST_RENDER_LEVEL;
        self->priority = 3 * 0x80;
        self->width_pixels = 16 / 2;

        int16_t base_y = self->y_pos.f.u;
        int16_t base_x = self->x_pos.f.u;
        SstCode code = self->code;

        int count = self->subtype; // total spike count
        own->children_count = 0;

        int16_t x = (int16_t)(base_x - ((count >> 1) << 4));
        int remaining = count - 2; // -1 for the parent spike, -1 for the dbf-style loop below

        if (remaining >= 0) {
            uint8_t frame = 0;
            int child_index = 0;
            Sst *prev = self;
            for (int i = 0; i <= remaining; i++) {
                Sst *child = Sst_AllocateAfter(prev);
                if (child == NULL)
                    break; // object RAM full -- helix just has fewer spikes than requested
                prev = child;

                own->children_count++;
                own->children[child_index++] = (uint8_t)(child - sst);

                child->routine = 8;
                child->code = code;
                child->y_pos.f.u = base_y;
                child->x_pos.f.u = x;
                child->mappings = self->mappings;
                child->art_tile = self->art_tile;
                child->render_flags = SST_RENDER_LEVEL;
                child->priority = 3 * 0x80;
                child->width_pixels = 16 / 2;

                ((Own_Helix *)child->own.u8)->frame_base = frame;
                frame = (frame + 1) & 7;
                x += 16;

                if (x == base_x) {
                    // Reached the parent's own slot in the row -- it
                    // already occupies this position, so just account for
                    // it in the frame/position sweep without spawning.
                    own->frame_base = frame;
                    frame = (frame + 1) & 7;
                    x += 16;
                    own->children_count++;
                }
            }
        }
        __attribute__((fallthrough));
    }
    case 2: // ParentSpike
    case 4: // (jump table entry exists but real hardware never actually sets routine 4 for a helix parent)
        Hel_RotateSpikes(self, own);
        if (SstOutOfRange(self->x_pos.f.u)) {
            Hel_DeleteChildren(own);
            SstDeleteIfNotInRange(self); // (forgets its place and goes)
            return;
        }
        Sst_Draw(self);
        break;
    case 6: // Delete (jump table entry exists but real hardware always deletes inline from the offscreen check above instead)
        Sst_Delete(self);
        break;
    case 8: // ChildSpike
        Hel_RotateSpikes(self, own);
        Sst_Draw(self);
        break;
    }
}
