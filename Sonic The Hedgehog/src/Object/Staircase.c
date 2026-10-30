#include "Staircase.h"
#include "Object.h"
#include "ObjectSst.h"
#include "SstLoad.h"
#include "Constants.h"

#include "Level.h"
#include "LevelScroll.h"
#include "Resource/Mappings/Staircase.h"

// Object 5B - blocks that form a staircase when touched (SLZ)
//
// The first (parent) block doubles as the controller: it holds the shared
// delay/touch/children_y state and runs Stair_Move every frame. In the original
// Stair_Move has no return of its own and falls straight into Stair_Solid, so the
// parent is a solid, moving block like the 3 spawned children (which only run
// Stair_Solid, reading their Y-offset out of the parent's children_y[] array).

// Type 0 - moves down when Sonic stands on it
static void Stair_Type00(Sst *self, Own_Staircase *own) {
    if (own->delay != 0) {
        if (--own->delay == 0)
            self->subtype++; // subtype -> Stair_Type01 (move down)
        return;
    }
    if (own->touch == 1)
        own->delay = 30;
}

// Type 2 - moves down when Sonic hits it from below
static void Stair_Type02(Sst *self, Own_Staircase *own) {
    if (own->delay != 0) {
        if (--own->delay != 0) {
            uint8_t d0 = (uint8_t)((own->delay >> 2) & 1);
            own->children_y[0] = d0;
            own->children_y[1] = d0 ^ 1;
            own->children_y[2] = d0;
            own->children_y[3] = d0 ^ 1;
            return;
        }
        self->subtype++; // subtype -> Stair_Type01 (move down)
        return;
    }
    if (own->touch < 0)
        own->delay = 60;
}

// Type 1 (from type 0 or 2) - moves down automatically, staggered across
// the 4 blocks so they cascade rather than dropping in lockstep.
static void Stair_Type01(Sst *self, Own_Staircase *own) {
    (void)self;
    if (own->children_y[0] == 128)
        return;

    own->children_y[0]++;
    uint8_t d = own->children_y[0];
    uint8_t half = d / 2;
    uint8_t quarter = d / 4;

    own->children_y[1] = (uint8_t)((d * 3) / 4);    // 3D/4 -- floor of the sum, not quarter + half
    own->children_y[2] = half;                       // D/2
    own->children_y[3] = quarter;                     // D/4
}

static void Stair_Move(Sst *self, Own_Staircase *own) {
    switch (self->subtype & 7) {
    case 0: Stair_Type00(self, own); break;
    case 1: case 3: Stair_Type01(self, own); break;
    case 2: Stair_Type02(self, own); break;
    }
}

static void Stair_Solid(Sst *self, Own_Staircase *own);

static void Stair_Solid(Sst *self, Own_Staircase *own) {
    Sst *parent = &sst[own->parent_index];
    Own_Staircase *pscratch = (Own_Staircase *)parent->own.u8;

    self->y_pos.f.u = (int16_t)(own->orig_y + pscratch->children_y[own->parent_y_index]);

    int16_t x_rad = (int16_t)(self->width_pixels + 11 /* sonic_solid_width */);
    int32_t touch = SstSolidObject(self, (uint16_t)x_rad, 32 / 2, 34 / 2, self->x_pos.f.u, NULL, NULL);

    if ((int8_t)touch < 0)
        pscratch->touch = (int8_t)touch;
    if (((self->status & SST_STATUS_PLAYER_STAND) != 0))
        pscratch->touch = 1;
}

static void Stair_Main(Sst *self, Own_Staircase *own) {
    self->routine = 2; // advance to Stair_Move

    bool flipped = ((self->status & SST_STATUS_X_FLIP) != 0);
    int16_t x = self->x_pos.f.u;
    uint8_t subtype = self->subtype;
    uint8_t self_index = (uint8_t)(self - sst);

    Sst *block = self;
    for (int i = 0; i < 4; i++) {
        if (i > 0) {
            block = Sst_AllocateAfter(self);
            if (block == NULL)
                break; // object RAM full -- stop spawning, remaining blocks never exist
            block->routine = 4; // Stair_Solid
        }

        block->code = self->code;
        block->mappings = Mappings_Staircase;
        block->art_tile = TILE_MAP(0, 2, 0, 0, ArtTile_Level); // ArtTile_Level | Tile_Pal3
        block->render_flags |= SST_RENDER_LEVEL;
        block->priority = 3 * 0x80;
        block->width_pixels = 32 / 2;
        block->subtype = subtype;
        block->x_pos.f.u = x;
        block->y_pos.f.u = self->y_pos.f.u;

        Own_Staircase *bscratch = (Own_Staircase *)block->own.u8;
        bscratch->orig_x = self->x_pos.f.u;
        bscratch->orig_y = block->y_pos.f.u;
        bscratch->parent_y_index = (uint8_t)(flipped ? (3 - i) : i);
        bscratch->parent_index = self_index;

        x = (int16_t)(x + 32);
    }

    Stair_Move(self, own);
    Stair_Solid(self, own); // the parent falls through into Stair_Solid too
}

void Obj_Staircase(Sst *self) {
    Own_Staircase *own = (Own_Staircase *)self->own.u8;

    switch (self->routine) {
    case 0: Stair_Main(self, own); break;
    case 2:
        Stair_Move(self, own);
        Stair_Solid(self, own);
        break;
    case 4: Stair_Solid(self, own); break;
    }

    SstOnScreenTest2(self, own->orig_x);
}
