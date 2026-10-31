#include "GlassBlock.h"
#include "Object.h"
#include "SstLoad.h"
#include "ObjectSst.h"

#include "Level.h"
#include "LevelScroll.h"
#include "Macros.h"
#include "Resource/Mappings/MZLargeGreenGlassBlocks.h"

// Object 30 - large green glass pillars (MZ). Each placed pillar spawns a
// second "sheen"/reflection object alongside it (routine 4/8), which
// mirrors the pillar's own vertical movement but isn't solid itself.
// Two sizes (tall/short, picked by subtype < 3 or >= 3) and 5 movement
// types (stationary, two directions of oscillation, a stomp-down type
// that's an unused prototype leftover in the shipped game, and a
// switch-triggered descent).

typedef struct {
    uint8_t routine;
    uint8_t frame;
} GlassVar;

// {routine, frame} per spawned piece -- Y-offset from the pillar's own
// spawn position is always 0 for both pieces on real hardware too (a
// vestigial field), so it isn't carried over here.
static const GlassVar glass_vars_tall[2] = { { 2, 0 }, { 4, 1 } };  // pillar, sheen
static const GlassVar glass_vars_short[2] = { { 6, 2 }, { 8, 1 } }; // pillar, sheen

static void Glass_UpdateY(Sst *self, Own_GlassBlock *own, int16_t offset) {
    self->y_pos.f.u = (int16_t)(own->orig_y - offset);
}

// Types 1/2 - oscillate up and down (opposite directions), using the same
// pre-baked frequency-4 oscillation counter used elsewhere in this project
// (see MovingBlock.c/LargeGrass.c's own comments on oscillatory.state
// indexing) -- index 4 here corresponds to real hardware's v_oscillate+$12.
static void Glass_Type12_MoveSheen(Sst *self, Own_GlassBlock *own, bool is_sheen, int16_t d0) {
    if (is_sheen) {
        d0 = (int16_t)(-d0 + 64);
        d0 = (int16_t)((uint8_t)d0 >> 1); // sheen moves at half the pillar's speed
        d0 += 32;
    }
    Glass_UpdateY(self, own, d0);
}

static void Glass_Type1_UpDown(Sst *self, Own_GlassBlock *own, bool is_sheen) {
    int16_t d0 = (int16_t)(uint8_t)(oscillatory.state[4][0] >> 8);
    Glass_Type12_MoveSheen(self, own, is_sheen, d0);
}

static void Glass_Type2_DownUp(Sst *self, Own_GlassBlock *own, bool is_sheen) {
    int16_t d0 = (int16_t)(uint8_t)(oscillatory.state[4][0] >> 8);
    d0 = (int16_t)(-d0 + 64);
    Glass_Type12_MoveSheen(self, own, is_sheen, d0);
}

// Type 3 - descends 16px every other stomp (64px on the 5th), unused by
// any shipped level layout but reachable via debug placement.
static void Glass_Type3_Stomp(Sst *self, Own_GlassBlock *own, bool is_sheen) {
    if (is_sheen) {
        int16_t d0 = (int16_t)(uint8_t)(oscillatory.state[4][0] >> 8);
        Glass_UpdateY(self, own, (int16_t)(d0 - 16));
        return;
    }

    if (!((self->status & SST_STATUS_PLAYER_STAND) != 0)) {
        own->flags &= ~0x01;
    } else if (own->flags == 0) {
        own->flags = 0x01;
        bool was_first_touch = (own->stomp_first & 0x01) == 0;
        own->stomp_first |= 0x01;
        if (!was_first_touch) {
            own->flags |= 0x80;
            own->stomp_distance = 16;
            own->stomp_delay = 10;
            if (own->distance_y == 64)
                own->stomp_distance = 64;
        }
    }

    if (own->flags & 0x80) {
        bool ready_to_move = (own->stomp_delay == 0);
        if (!ready_to_move) {
            own->stomp_delay--;
            ready_to_move = (own->stomp_delay == 0);
        }
        if (ready_to_move) {
            bool keep_moving = false;
            if (own->distance_y != 0) {
                own->distance_y--;
                own->stomp_distance--;
                keep_moving = (own->stomp_distance != 0);
            }
            if (!keep_moving)
                own->flags &= ~0x80;
        }
    }

    Glass_UpdateY(self, own, own->distance_y);
}

// Type 4 - descends 2px/frame once the matching switch (subtype's own
// upper nibble selects the switch group) has been pressed.
static void Glass_Type4_Switch(Sst *self, Own_GlassBlock *own, bool is_sheen) {
    if (is_sheen) {
        int16_t d0 = (int16_t)(uint8_t)(oscillatory.state[4][0] >> 8);
        Glass_UpdateY(self, own, (int16_t)(d0 - 16));
        return;
    }

    bool move = (own->flags != 0);
    if (!move) {
        uint8_t switch_index = (self->subtype >> 4) & 0xF;
        if (f_switch[switch_index]) {
            own->flags = 1;
            move = true;
        }
    }
    if (move && own->distance_y != 0)
        own->distance_y -= 2;

    Glass_UpdateY(self, own, own->distance_y);
}

static void Glass_Types(Sst *self, Own_GlassBlock *own) {
    uint8_t subtype = self->subtype;
    bool is_sheen = (subtype & 8) != 0;
    switch (subtype & 7) {
    case 0: // stationary
        break;
    case 1:
        Glass_Type1_UpDown(self, own, is_sheen);
        break;
    case 2:
        Glass_Type2_DownUp(self, own, is_sheen);
        break;
    case 3:
        Glass_Type3_Stomp(self, own, is_sheen);
        break;
    case 4:
        Glass_Type4_Switch(self, own, is_sheen);
        break;
    }
}

static void Glass_Main(Sst *self, Own_GlassBlock *own) {
    const GlassVar *vars = glass_vars_tall;
    int16_t height = 144 / 2;
    uint8_t subtype = self->subtype;
    if (subtype >= 3) {
        vars = glass_vars_short;
        height = 112 / 2;
    }
    self->y_radius = (int8_t)height;

    // Spawns the pillar (reusing this very object) and its sheen
    // companion. If object RAM is full when spawning the sheen, the
    // pillar itself is simply left without one -- real hardware's own
    // failure path instead corrupts the pillar's own collision/subtype by
    // applying the sheen-specific adjustments below to it, an edge case
    // so obscure (only possible with object RAM already exhausted) that
    // it's not worth reproducing.
    Sst *sheen = NULL;
    for (int i = 0; i < 2; i++) {
        Sst *seg = self;
        if (i > 0) {
            seg = Sst_AllocateAfter(self);
            if (seg == NULL)
                break;
            sheen = seg;
        }
        seg->routine = vars[i].routine;
        seg->code = Obj_GlassBlock;
        seg->x_pos.f.u = self->x_pos.f.u;
        seg->y_pos.f.u = self->y_pos.f.u;
        seg->mappings = Mappings_MZLargeGreenGlassBlocks;
        seg->art_tile = TILE_MAP(1, 2, 0, 0, ArtTile_MZ_Glass_Pillar); // | Tile_Pal3 | Tile_Prio
        seg->render_flags = SST_RENDER_LEVEL;

        Own_GlassBlock *sscratch = (Own_GlassBlock *)seg->own.u8;
        sscratch->orig_y = seg->y_pos.f.u;
        seg->subtype = subtype;
        seg->width_pixels = 64 / 2;
        seg->priority = 4 * 0x80;
        seg->mapping_frame = vars[i].frame;
        sscratch->parent_index = (uint8_t)(self - sst);
    }

    if (sheen != NULL) {
        sheen->width_pixels = 32 / 2;
        sheen->priority = 3 * 0x80;
        sheen->subtype = (uint8_t)((subtype + 8) & 0xF);
    }

    own->distance_y = 144;
    self->height_pixels = self->y_radius;
}

static void Glass_Pillar(Sst *self, Own_GlassBlock *own, uint16_t y_rad1, uint16_t y_rad2) { // Routine 2, 6 -- tall/short pillar
    Glass_Types(self, own);
    SstSolidObject(self, 64 / 2 + 11, y_rad1, y_rad2, self->x_pos.f.u, NULL, NULL);
}

static void Glass_Sheen(Sst *self, Own_GlassBlock *own, bool triggered_size) { // Routine 4, 8 -- tall/short sheen
    Sst *parent = &sst[own->parent_index];
    Own_GlassBlock *pscratch = (Own_GlassBlock *)parent->own.u8;
    own->distance_y = pscratch->distance_y;
    if (triggered_size)
        own->orig_y = parent->y_pos.f.u;
    Glass_Types(self, own);
}

void Obj_GlassBlock(Sst *self) {
    Own_GlassBlock *own = (Own_GlassBlock *)self->own.u8;

    switch (self->routine) {
    case 0:
        Glass_Main(self, own);
        break;
    case 2: // tall pillar
        Glass_Pillar(self, own, 144 / 2, 146 / 2);
        break;
    case 6: // short pillar
        Glass_Pillar(self, own, 112 / 2, 114 / 2);
        break;
    case 4:
        Glass_Sheen(self, own, false);
        break;
    case 8:
        Glass_Sheen(self, own, true);
        break;
    }

    SstOnScreenTest2(self, self->x_pos.f.u);
}
