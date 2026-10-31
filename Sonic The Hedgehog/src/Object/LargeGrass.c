#include "LargeGrass.h"
#include "Object.h"
#include "ObjectSst.h"
#include "SstLoad.h"

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

void LGrass_AddChildToList(Sst *platform, Sst *child) {
    Own_LargeGrass *own = (Own_LargeGrass *)platform->own.u8;
    if (own->flames_count >= 8)
        return; // real hardware trusts the fixed 8-slot list never overflows -- this is just a safety margin
    own->flames[own->flames_count++] = (uint8_t)(child - sst);
}

// Types 1-4 - up/down oscillation, four widths sharing the same table of
// pre-baked frequency-2 oscillation counters used elsewhere in this
// project (see MovingBlock.c/BasicPlatform.c's own comments on
// oscillatory.state indexing) -- index 0-3 here correspond to real
// hardware's v_oscillate+2/+6/+A/+E respectively.
static void LGrass_MoveVertical(Sst *self, Own_LargeGrass *own, int osc_index, uint16_t amplitude, bool reverse) {
    int16_t d0 = (int16_t)(uint8_t)(oscillatory.state[osc_index][0] >> 8);
    if (reverse)
        d0 = (int16_t)(-d0 + (int16_t)(amplitude * 2));
    self->y_pos.f.u = (int16_t)(own->orig_y - d0);
}

// Type 5 - burnable: depresses like a spring while stood on, then bursts
// into flame partway down and stays lit until the platform scrolls
// offscreen.
static void LGrass_Burnable(Sst *self, Own_LargeGrass *own) {
    int16_t d0 = own->nudge;
    if (((self->status & SST_STATUS_PLAYER_STAND) != 0)) {
        d0 += 4;
        if (d0 >= 0x40)
            d0 = 0x40;
    } else {
        d0 -= 2;
        if (d0 < 0)
            d0 = 0;
    }
    own->nudge = (uint8_t)d0;

    int16_t sin, cos;
    CalcSine((uint8_t)d0, &sin, &cos);
    int16_t depression = (int16_t)(sin >> 4);
    self->y_pos.f.u = (int16_t)(own->orig_y + depression);

    // Catch fire once depressed halfway, and stay lit until the platform
    // itself goes offscreen (see Obj_LargeGrass's own cleanup).
    if (own->nudge == 0x40 / 2 && !own->burning) {
        own->burning = 1;

        Sst *flame = Sst_AllocateAfter(self);
        if (flame != NULL) {
            flame->code = Obj_GrassFire;
            flame->x_pos.f.u = (int16_t)(self->x_pos.f.u - 128 / 2);

            Own_GrassFire *fscratch = (Own_GrassFire *)flame->own.u8;
            fscratch->orig_y = (int16_t)(own->orig_y + 8 - 3);
            fscratch->platform = (uint8_t)(self - sst);

            LGrass_AddChildToList(self, flame);
        }
    }

    // Propagate this frame's depression distance to every live child flame.
    for (int i = 0; i < own->flames_count; i++) {
        Sst *flame = &sst[own->flames[i]];
        ((Own_GrassFire *)flame->own.u8)->nudge = depression;
    }
}

static void LGrass_Types(Sst *self, Own_LargeGrass *own) {
    uint8_t type = self->subtype;
    bool reverse = (type & 8) != 0;
    switch (type & 7) {
    case 0: // stationary
        break;
    case 1:
        LGrass_MoveVertical(self, own, 0, 0x10, reverse);
        break;
    case 2:
        LGrass_MoveVertical(self, own, 1, 0x18, reverse);
        break;
    case 3:
        LGrass_MoveVertical(self, own, 2, 0x20, reverse);
        break;
    case 4:
        LGrass_MoveVertical(self, own, 3, 0x30, reverse);
        break;
    case 5:
        LGrass_Burnable(self, own);
        break;
    }
}

void Obj_LargeGrass(Sst *self) {
    Own_LargeGrass *own = (Own_LargeGrass *)self->own.u8;

    switch (self->routine) {
    case 0: { // Main
        self->routine += 2;
        self->mappings = Mappings_MZLargeGrassyPlatforms;
        self->art_tile = TILE_MAP(1, 2, 0, 0, ArtTile_Level); // ArtTile_Level | Tile_Pal3 | Tile_Prio
        self->render_flags = SST_RENDER_LEVEL;
        self->priority = 5 * 0x80;
        own->orig_y = self->y_pos.f.u;
        own->orig_x = self->x_pos.f.u;

        uint8_t subtype = self->subtype;
        uint8_t shape = (subtype >> 4) & 3; // 0=symmetrical, 1=asymmetrical, 2=column
        static const uint8_t shape_width[3] = { 128 / 2, 128 / 2, 64 / 2 };
        self->mapping_frame = shape; // also doubles as this platform's own shape ID for the rest of its lifetime
        self->width_pixels = shape_width[shape];
        self->subtype = subtype & 0xF; // reduce to just the movement type (0-5)

        self->height_pixels = 128 / 2; // (what the original's explicit height is)
        __attribute__((fallthrough));
    }
    case 2: { // Action
        LGrass_Types(self, own);

        int16_t x_rad = (int16_t)(self->width_pixels + 11 /* sonic_solid_width */);

        if (((self->status & SST_STATUS_PLAYER_STAND) != 0)) {
            if (!SstExitPlatform(self, (uint16_t)x_rad, (uint16_t)x_rad, NULL))
                SstSlopeObject_AssumeStoodOn(self, (uint16_t)x_rad, LGrass_Heightmap(self->mapping_frame), self->x_pos.f.u);
        } else if ((self->render_flags & SST_RENDER_ON_SCREEN)) {
            uint16_t y_rad = (self->mapping_frame == 2) ? 96 / 2 : 64 / 2;
            SstSolidObject_Heightmap(self, (uint16_t)x_rad, y_rad, LGrass_Heightmap(self->mapping_frame));
        }

        if (own->burning && !(self->render_flags & SST_RENDER_ON_SCREEN)) {
            for (int i = 0; i < own->flames_count; i++)
                Sst_Delete(&sst[own->flames[i]]);
            own->flames_count = 0;
            own->burning = 0;
            own->nudge = 0;
        }

        SstOnScreenTest(self);
        break;
    }
    }
}
