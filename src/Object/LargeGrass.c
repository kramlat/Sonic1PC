#include "LargeGrass.h"

#include "GrassFire.h"
#include "Level.h"
#include "LevelScroll.h"
#include "Macros.h"
#include "MathUtil.h"
#include "Object/Sonic.h"
#include "Resource/Mappings/MZLargeGrassyPlatforms.h"
#include "Sound.h"

// Object 2F - large grass-covered platforms (MZ)

// Slope collision data, one entry per 2px column across the platform's
// full solid-collision width (wider than the visible sprite itself -- the
// extra margin covers Sonic's own solidity radius beyond the visual
// edges, see Obj_LargeGrass's own x_rad computation). Not to scale:
//   Symmetrical (shape 0):  _/*\_
//   Asymmetrical(shape 1):  _/*\-
//   Column      (shape 2):  |**|
static const uint8_t lgrass_data_symmetrical[76] = {
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
    0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
    0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30,
    0x2F, 0x2E, 0x2D, 0x2C, 0x2B, 0x2A, 0x29, 0x28, 0x27, 0x26, 0x25, 0x24, 0x23, 0x22, 0x21,
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
};
static const uint8_t lgrass_data_asymmetrical[76] = {
    0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
    0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x2E, 0x2F,
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3A, 0x3B, 0x3C, 0x3D, 0x3E, 0x3F,
    0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40,
    0x3F, 0x3E, 0x3D, 0x3C, 0x3B, 0x3A, 0x39, 0x38, 0x37, 0x36, 0x35, 0x34, 0x33, 0x32, 0x31,
    0x30, 0x30, 0x30, 0x30, 0x30, 0x30,
};
static const uint8_t lgrass_data_column[44] = {
    0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30,
    0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30, 0x30,
};

const uint8_t *LGrass_Heightmap(uint8_t shape) {
    static const uint8_t *const heightmaps[3] = {
        lgrass_data_symmetrical,
        lgrass_data_asymmetrical,
        lgrass_data_column,
    };
    return heightmaps[shape];
}

void LGrass_AddChildToList(Object *platform, Object *child) {
    Scratch_LargeGrass *scratch = (Scratch_LargeGrass *)&platform->scratch;
    if (scratch->flames_count >= 8)
        return; // real hardware trusts the fixed 8-slot list never overflows -- this is just a safety margin
    scratch->flames[scratch->flames_count++] = (uint8_t)(child - objects);
}

// Types 1-4 - up/down oscillation, four widths sharing the same table of
// pre-baked frequency-2 oscillation counters used elsewhere in this
// project (see MovingBlock.c/BasicPlatform.c's own comments on
// oscillatory.state indexing) -- index 0-3 here correspond to real
// hardware's v_oscillate+2/+6/+A/+E respectively.
static void LGrass_MoveVertical(Object *obj, Scratch_LargeGrass *scratch, int osc_index, uint16_t amplitude, bool reverse) {
    int16_t d0 = (int16_t)(uint8_t)(oscillatory.state[osc_index][0] >> 8);
    if (reverse)
        d0 = (int16_t)(-d0 + (int16_t)(amplitude * 2));
    obj->pos.l.y.f.u = (int16_t)(scratch->orig_y - d0);
}

// Type 5 - burnable: depresses like a spring while stood on, then bursts
// into flame partway down and stays lit until the platform scrolls
// offscreen.
static void LGrass_Burnable(Object *obj, Scratch_LargeGrass *scratch) {
    int16_t d0 = scratch->nudge;
    if (obj->status.o.f.player_stand) {
        d0 += 4;
        if (d0 >= 0x40)
            d0 = 0x40;
    } else {
        d0 -= 2;
        if (d0 < 0)
            d0 = 0;
    }
    scratch->nudge = (uint8_t)d0;

    int16_t sin, cos;
    CalcSine((uint8_t)d0, &sin, &cos);
    int16_t depression = (int16_t)(sin >> 4);
    obj->pos.l.y.f.u = (int16_t)(scratch->orig_y + depression);

    // Catch fire once depressed halfway, and stay lit until the platform
    // itself goes offscreen (see Obj_LargeGrass's own cleanup).
    if (scratch->nudge == 0x40 / 2 && !scratch->burning) {
        scratch->burning = 1;

        Object *flame = FindNextFreeObj(obj);
        if (flame != NULL) {
            flame->type = ObjId_GrassFire;
            flame->pos.l.x.f.u = (int16_t)(obj->pos.l.x.f.u - 128 / 2);

            Scratch_GrassFire *fscratch = (Scratch_GrassFire *)&flame->scratch;
            fscratch->orig_y = (int16_t)(scratch->orig_y + 8 - 3);
            fscratch->platform = obj;

            LGrass_AddChildToList(obj, flame);
        }
    }

    // Propagate this frame's depression distance to every live child flame.
    for (int i = 0; i < scratch->flames_count; i++) {
        Object *flame = &objects[scratch->flames[i]];
        ((Scratch_GrassFire *)&flame->scratch)->nudge = depression;
    }
}

static void LGrass_Types(Object *obj, Scratch_LargeGrass *scratch) {
    uint8_t type = obj->scratch.u8[0];
    bool reverse = (type & 8) != 0;
    switch (type & 7) {
    case 0: // stationary
        break;
    case 1:
        LGrass_MoveVertical(obj, scratch, 0, 0x10, reverse);
        break;
    case 2:
        LGrass_MoveVertical(obj, scratch, 1, 0x18, reverse);
        break;
    case 3:
        LGrass_MoveVertical(obj, scratch, 2, 0x20, reverse);
        break;
    case 4:
        LGrass_MoveVertical(obj, scratch, 3, 0x30, reverse);
        break;
    case 5:
        LGrass_Burnable(obj, scratch);
        break;
    }
}

void Obj_LargeGrass(Object *obj) {
    Scratch_LargeGrass *scratch = (Scratch_LargeGrass *)&obj->scratch;

    switch (obj->routine) {
    case 0: { // Main
        obj->routine += 2;
        obj->mappings = Mappings_MZLargeGrassyPlatforms;
        obj->tile = TILE_MAP(1, 2, 0, 0, 0); // ArtTile_Level | Tile_Pal3 | Tile_Prio
        obj->render.b = 0;
        obj->render.f.align_fg = true;
        obj->priority = 5;
        scratch->orig_y = obj->pos.l.y.f.u;
        scratch->orig_x = obj->pos.l.x.f.u;

        uint8_t subtype = obj->scratch.u8[0];
        uint8_t shape = (subtype >> 4) & 3; // 0=symmetrical, 1=asymmetrical, 2=column
        static const uint8_t shape_width[3] = { 128 / 2, 128 / 2, 64 / 2 };
        obj->frame = shape; // also doubles as this platform's own shape ID for the rest of its lifetime
        obj->width_pixels = shape_width[shape];
        obj->scratch.u8[0] = subtype & 0xF; // reduce to just the movement type (0-5)

        obj->y_rad = 128 / 2;
        obj->render.f.yrad_height = true;
        __attribute__((fallthrough));
    }
    case 2: { // Action
        LGrass_Types(obj, scratch);

        int16_t x_rad = (int16_t)(obj->width_pixels + 11 /* sonic_solid_width */);

        if (obj->status.o.f.player_stand) {
            if (!ExitPlatform(obj, (uint16_t)x_rad, (uint16_t)x_rad, NULL))
                SlopeObject_AssumeStoodOn(obj, (uint16_t)x_rad, LGrass_Heightmap(obj->frame), obj->pos.l.x.f.u);
        } else if (obj->render.f.on_screen) {
            uint16_t y_rad = (obj->frame == 2) ? 96 / 2 : 64 / 2;
            SolidObject_Heightmap(obj, (uint16_t)x_rad, y_rad, LGrass_Heightmap(obj->frame));
        }

        if (scratch->burning && !obj->render.f.on_screen) {
            for (int i = 0; i < scratch->flames_count; i++)
                ObjectDelete(&objects[scratch->flames[i]]);
            scratch->flames_count = 0;
            scratch->burning = 0;
            scratch->nudge = 0;
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
