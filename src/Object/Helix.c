#include "Helix.h"

#include "Level.h"
#include "LevelScroll.h"
#include "Macros.h"

// Object 17 - rotating helix of spikes on a horizontal pole (GHZ3)

static void Hel_RotateSpikes(Object *obj, Scratch_Helix *scratch) {
    uint8_t frame = (uint8_t)(sprite_anim[0].frame + scratch->frame_base) & 7;
    obj->frame = frame;
    obj->col_type = (frame == 0) ? 0x84 : 0x00; // col_8x32 | col_hurt while "pointing up" (frame 0), harmless otherwise
}

// Deletes every child spike this parent spawned (parent itself is deleted
// separately by the caller) -- real hardware's DeleteChild is just a
// plain object-memory clear (see ObjectDelete's own comment: no
// respawn-index bookkeeping to worry about, safe on any object).
static void Hel_DeleteChildren(Scratch_Helix *scratch) {
    for (int i = 0; i < scratch->children_count - 1; i++)
        ObjectDelete(&objects[scratch->children[i]]);
}

void Obj_Helix(Object *obj) {
    Scratch_Helix *scratch = (Scratch_Helix *)&obj->scratch;

    switch (obj->routine) {
    case 0: { // Main
        obj->routine += 2;
        obj->mappings = Mappings_SpikedPoleHelix;
        obj->tile = TILE_MAP(0, 2, 0, 0, 0x398); // ArtTile_GHZ_Spike_Pole | Tile_Pal3
        obj->render.b = 0;
        obj->render.f.align_fg = true;
        obj->priority = 3;
        obj->width_pixels = 16 / 2;

        int16_t base_y = obj->pos.l.y.f.u;
        int16_t base_x = obj->pos.l.x.f.u;
        uint8_t id = obj->type;

        int count = scratch->children_count; // subtype, aliased -- total spike count
        scratch->children_count = 0;

        int16_t x = (int16_t)(base_x - ((count >> 1) << 4));
        int remaining = count - 2; // -1 for the parent spike, -1 for the dbf-style loop below

        if (remaining >= 0) {
            uint8_t frame = 0;
            int child_index = 0;
            Object *prev = obj;
            for (int i = 0; i <= remaining; i++) {
                Object *child = FindNextFreeObj(prev);
                if (child == NULL)
                    break; // object RAM full -- helix just has fewer spikes than requested
                prev = child;

                scratch->children_count++;
                scratch->children[child_index++] = (uint8_t)(child - objects);

                child->routine = 8;
                child->type = id;
                child->pos.l.y.f.u = base_y;
                child->pos.l.x.f.u = x;
                child->mappings = obj->mappings;
                child->tile = obj->tile;
                child->render.b = 0;
                child->render.f.align_fg = true;
                child->priority = 3;
                child->width_pixels = 16 / 2;

                ((Scratch_Helix *)&child->scratch)->frame_base = frame;
                frame = (frame + 1) & 7;
                x += 16;

                if (x == base_x) {
                    // Reached the parent's own slot in the row -- it
                    // already occupies this position, so just account for
                    // it in the frame/position sweep without spawning.
                    scratch->frame_base = frame;
                    frame = (frame + 1) & 7;
                    x += 16;
                    scratch->children_count++;
                }
            }
        }
        __attribute__((fallthrough));
    }
    case 2: // ParentSpike
    case 4: // (jump table entry exists but real hardware never actually sets routine 4 for a helix parent)
        Hel_RotateSpikes(obj, scratch);
        if (IS_OFFSCREEN(obj->pos.l.x.f.u)) {
            Hel_DeleteChildren(scratch);
            ObjectDelete(obj);
            return;
        }
        DisplaySprite(obj);
        break;
    case 6: // Delete (jump table entry exists but real hardware always deletes inline from the offscreen check above instead)
        ObjectDelete(obj);
        break;
    case 8: // ChildSpike
        Hel_RotateSpikes(obj, scratch);
        DisplaySprite(obj);
        break;
    }
}
