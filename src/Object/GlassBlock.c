#include "GlassBlock.h"

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

static void Glass_UpdateY(Object *obj, Scratch_GlassBlock *scratch, int16_t offset) {
    obj->pos.l.y.f.u = (int16_t)(scratch->orig_y - offset);
}

// Types 1/2 - oscillate up and down (opposite directions), using the same
// pre-baked frequency-4 oscillation counter used elsewhere in this project
// (see MovingBlock.c/LargeGrass.c's own comments on oscillatory.state
// indexing) -- index 4 here corresponds to real hardware's v_oscillate+$12.
static void Glass_Type12_MoveSheen(Object *obj, Scratch_GlassBlock *scratch, bool is_sheen, int16_t d0) {
    if (is_sheen) {
        d0 = (int16_t)(-d0 + 64);
        d0 = (int16_t)((uint8_t)d0 >> 1); // sheen moves at half the pillar's speed
        d0 += 32;
    }
    Glass_UpdateY(obj, scratch, d0);
}

static void Glass_Type1_UpDown(Object *obj, Scratch_GlassBlock *scratch, bool is_sheen) {
    int16_t d0 = (int16_t)(uint8_t)(oscillatory.state[4][0] >> 8);
    Glass_Type12_MoveSheen(obj, scratch, is_sheen, d0);
}

static void Glass_Type2_DownUp(Object *obj, Scratch_GlassBlock *scratch, bool is_sheen) {
    int16_t d0 = (int16_t)(uint8_t)(oscillatory.state[4][0] >> 8);
    d0 = (int16_t)(-d0 + 64);
    Glass_Type12_MoveSheen(obj, scratch, is_sheen, d0);
}

// Type 3 - descends 16px every other stomp (64px on the 5th), unused by
// any shipped level layout but reachable via debug placement.
static void Glass_Type3_Stomp(Object *obj, Scratch_GlassBlock *scratch, bool is_sheen) {
    if (is_sheen) {
        int16_t d0 = (int16_t)(uint8_t)(oscillatory.state[4][0] >> 8);
        Glass_UpdateY(obj, scratch, (int16_t)(d0 - 16));
        return;
    }

    if (!obj->status.o.f.player_stand) {
        scratch->flags &= ~0x01;
    } else if (scratch->flags == 0) {
        scratch->flags = 0x01;
        bool was_first_touch = (scratch->stomp_first & 0x01) == 0;
        scratch->stomp_first |= 0x01;
        if (!was_first_touch) {
            scratch->flags |= 0x80;
            scratch->stomp_distance = 16;
            scratch->stomp_delay = 10;
            if (scratch->distance_y == 64)
                scratch->stomp_distance = 64;
        }
    }

    if (scratch->flags & 0x80) {
        bool ready_to_move = (scratch->stomp_delay == 0);
        if (!ready_to_move) {
            scratch->stomp_delay--;
            ready_to_move = (scratch->stomp_delay == 0);
        }
        if (ready_to_move) {
            bool keep_moving = false;
            if (scratch->distance_y != 0) {
                scratch->distance_y--;
                scratch->stomp_distance--;
                keep_moving = (scratch->stomp_distance != 0);
            }
            if (!keep_moving)
                scratch->flags &= ~0x80;
        }
    }

    Glass_UpdateY(obj, scratch, scratch->distance_y);
}

// Type 4 - descends 2px/frame once the matching switch (subtype's own
// upper nibble selects the switch group) has been pressed.
static void Glass_Type4_Switch(Object *obj, Scratch_GlassBlock *scratch, bool is_sheen) {
    if (is_sheen) {
        int16_t d0 = (int16_t)(uint8_t)(oscillatory.state[4][0] >> 8);
        Glass_UpdateY(obj, scratch, (int16_t)(d0 - 16));
        return;
    }

    bool move = (scratch->flags != 0);
    if (!move) {
        uint8_t switch_index = (obj->scratch.u8[0] >> 4) & 0xF;
        if (f_switch[switch_index]) {
            scratch->flags = 1;
            move = true;
        }
    }
    if (move && scratch->distance_y != 0)
        scratch->distance_y -= 2;

    Glass_UpdateY(obj, scratch, scratch->distance_y);
}

static void Glass_Types(Object *obj, Scratch_GlassBlock *scratch) {
    uint8_t subtype = obj->scratch.u8[0];
    bool is_sheen = (subtype & 8) != 0;
    switch (subtype & 7) {
    case 0: // stationary
        break;
    case 1:
        Glass_Type1_UpDown(obj, scratch, is_sheen);
        break;
    case 2:
        Glass_Type2_DownUp(obj, scratch, is_sheen);
        break;
    case 3:
        Glass_Type3_Stomp(obj, scratch, is_sheen);
        break;
    case 4:
        Glass_Type4_Switch(obj, scratch, is_sheen);
        break;
    }
}

static void Glass_Main(Object *obj, Scratch_GlassBlock *scratch) {
    const GlassVar *vars = glass_vars_tall;
    int16_t height = 144 / 2;
    uint8_t subtype = obj->scratch.u8[0];
    if (subtype >= 3) {
        vars = glass_vars_short;
        height = 112 / 2;
    }
    obj->y_rad = (int8_t)height;

    obj->pos.l.y.f.u = (int16_t)(obj->pos.l.y.f.u + height);

    // Spawns the pillar (reusing this very object) and its sheen
    // companion. If object RAM is full when spawning the sheen, the
    // pillar itself is simply left without one -- real hardware's own
    // failure path instead corrupts the pillar's own collision/subtype by
    // applying the sheen-specific adjustments below to it, an edge case
    // so obscure (only possible with object RAM already exhausted) that
    // it's not worth reproducing.
    Object *sheen = NULL;
    for (int i = 0; i < 2; i++) {
        Object *seg = obj;
        if (i > 0) {
            seg = FindNextFreeObj(obj);
            if (seg == NULL)
                break;
            sheen = seg;
        }
        seg->routine = vars[i].routine;
        seg->type = ObjId_GlassBlock;
        seg->pos.l.x.f.u = obj->pos.l.x.f.u;
        seg->pos.l.y.f.u = obj->pos.l.y.f.u;
        seg->mappings = Mappings_MZLargeGreenGlassBlocks;
        seg->tile = TILE_MAP(1, 2, 0, 0, 0x38E); // ArtTile_MZ_Glass_Pillar | Tile_Pal3 | Tile_Prio
        seg->render.b = 0;
        seg->render.f.align_fg = true;

        Scratch_GlassBlock *sscratch = (Scratch_GlassBlock *)&seg->scratch;
        sscratch->orig_y = seg->pos.l.y.f.u;
        seg->scratch.u8[0] = subtype;
        seg->width_pixels = 64 / 2;
        seg->priority = 4;
        seg->frame = vars[i].frame;
        sscratch->parent_index = (uint8_t)(obj - objects);
    }

    if (sheen != NULL) {
        sheen->width_pixels = 32 / 2;
        sheen->priority = 3;
        sheen->scratch.u8[0] = (uint8_t)((subtype + 8) & 0xF);
    }

    scratch->distance_y = 144;
    obj->render.f.yrad_height = true;
}

static void Glass_Pillar(Object *obj, Scratch_GlassBlock *scratch, uint16_t y_rad1, uint16_t y_rad2) { // Routine 2, 6 -- tall/short pillar
    Glass_Types(obj, scratch);
    SolidObject(obj, 64 / 2 + 11, y_rad1, y_rad2, obj->pos.l.x.f.u, NULL, NULL);
}

static void Glass_Sheen(Object *obj, Scratch_GlassBlock *scratch, bool triggered_size) { // Routine 4, 8 -- tall/short sheen
    Object *parent = &objects[scratch->parent_index];
    Scratch_GlassBlock *pscratch = (Scratch_GlassBlock *)&parent->scratch;
    scratch->distance_y = pscratch->distance_y;
    if (triggered_size)
        scratch->orig_y = parent->pos.l.y.f.u;
    Glass_Types(obj, scratch);
}

void Obj_GlassBlock(Object *obj) {
    Scratch_GlassBlock *scratch = (Scratch_GlassBlock *)&obj->scratch;

    switch (obj->routine) {
    case 0:
        Glass_Main(obj, scratch);
        break;
    case 2: // tall pillar
        Glass_Pillar(obj, scratch, 144 / 2, 146 / 2);
        break;
    case 6: // short pillar
        Glass_Pillar(obj, scratch, 112 / 2, 114 / 2);
        break;
    case 4:
        Glass_Sheen(obj, scratch, false);
        break;
    case 8:
        Glass_Sheen(obj, scratch, true);
        break;
    }

    if (IS_OFFSCREEN(obj->pos.l.x.f.u)) {
        ObjectDelete(obj);
        return;
    }
    DisplaySprite(obj);
}
