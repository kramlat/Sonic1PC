#include "Staircase.h"

#include "Level.h"
#include "LevelScroll.h"
#include "Resource/Mappings/Staircase.h"

// Object 5B - blocks that form a staircase when touched (SLZ)
//
// The first (parent) block doubles as the controller: it holds the shared
// delay/touch/children_y state and keeps running Stair_Move every frame,
// but -- faithfully to the real disassembly -- it never itself runs
// Stair_Solid, so it's purely decorative and never solid. The other 3
// blocks are spawned as children, each independently running Stair_Solid
// and reading their own Y-offset out of the parent's children_y[] array.

static bool Stair_OutOfRange(int16_t x) {
    uint16_t obj_pos = (uint16_t)x & 0xFF80;
    uint16_t cam_pos = (uint16_t)(scrpos_x.f.u - 128) & 0xFF80;
    return (uint16_t)(obj_pos - cam_pos) > (128 + 320 + 192);
}

// Type 0 - moves down when Sonic stands on it
static void Stair_Type00(Object *obj, Scratch_Staircase *scratch) {
    if (scratch->delay != 0) {
        if (--scratch->delay == 0)
            obj->scratch.u8[0]++; // subtype -> Stair_Type01 (move down)
        return;
    }
    if (scratch->touch == 1)
        scratch->delay = 30;
}

// Type 2 - moves down when Sonic hits it from below
static void Stair_Type02(Object *obj, Scratch_Staircase *scratch) {
    if (scratch->delay != 0) {
        if (--scratch->delay != 0) {
            uint8_t d0 = (uint8_t)((scratch->delay >> 2) & 1);
            scratch->children_y[0] = d0;
            scratch->children_y[1] = d0 ^ 1;
            scratch->children_y[2] = d0;
            scratch->children_y[3] = d0 ^ 1;
            return;
        }
        obj->scratch.u8[0]++; // subtype -> Stair_Type01 (move down)
        return;
    }
    if (scratch->touch < 0)
        scratch->delay = 60;
}

// Type 1 (from type 0 or 2) - moves down automatically, staggered across
// the 4 blocks so they cascade rather than dropping in lockstep.
static void Stair_Type01(Object *obj, Scratch_Staircase *scratch) {
    (void)obj;
    if (scratch->children_y[0] == 128)
        return;

    scratch->children_y[0]++;
    uint8_t d = scratch->children_y[0];
    uint8_t half = d / 2;
    uint8_t quarter = d / 4;

    scratch->children_y[1] = (uint8_t)(quarter + half); // 3D/4
    scratch->children_y[2] = half;                       // D/2
    scratch->children_y[3] = quarter;                     // D/4
}

static void Stair_Move(Object *obj, Scratch_Staircase *scratch) {
    switch (obj->scratch.u8[0] & 7) {
    case 0: Stair_Type00(obj, scratch); break;
    case 1: case 3: Stair_Type01(obj, scratch); break;
    case 2: Stair_Type02(obj, scratch); break;
    }
}

static void Stair_Solid(Object *obj, Scratch_Staircase *scratch) {
    Object *parent = &objects[scratch->parent_index];
    Scratch_Staircase *pscratch = (Scratch_Staircase *)&parent->scratch;

    obj->pos.l.y.f.u = (int16_t)(scratch->orig_y + pscratch->children_y[scratch->parent_y_index]);

    int16_t x_rad = (int16_t)(obj->width_pixels + 11 /* sonic_solid_width */);
    int32_t touch = SolidObject(obj, (uint16_t)x_rad, 32 / 2, 34 / 2, obj->pos.l.x.f.u, NULL, NULL);

    if ((int8_t)touch < 0)
        pscratch->touch = (int8_t)touch;
    if (obj->status.o.f.player_stand)
        pscratch->touch = 1;
}

static void Stair_Main(Object *obj, Scratch_Staircase *scratch) {
    obj->routine = 2; // advance to Stair_Move

    bool flipped = obj->status.o.f.x_flip;
    int16_t x = obj->pos.l.x.f.u;
    uint8_t subtype = obj->scratch.u8[0];
    uint8_t self_index = (uint8_t)(obj - objects);

    Object *block = obj;
    for (int i = 0; i < 4; i++) {
        if (i > 0) {
            block = FindNextFreeObj(obj);
            if (block == NULL)
                break; // object RAM full -- stop spawning, remaining blocks never exist
            block->routine = 4; // Stair_Solid
        }

        block->type = ObjId_Staircase;
        block->mappings = Mappings_Staircase;
        block->tile = TILE_MAP(0, 2, 0, 0, ArtTile_Level); // ArtTile_Level | Tile_Pal3
        block->render.f.align_fg = true;
        block->priority = 3;
        block->width_pixels = 32 / 2;
        block->scratch.u8[0] = subtype;
        block->pos.l.x.f.u = x;
        block->pos.l.y.f.u = obj->pos.l.y.f.u;

        Scratch_Staircase *bscratch = (Scratch_Staircase *)&block->scratch;
        bscratch->orig_x = obj->pos.l.x.f.u;
        bscratch->orig_y = block->pos.l.y.f.u;
        bscratch->parent_y_index = (uint8_t)(flipped ? (3 - i) : i);
        bscratch->parent_index = self_index;

        x = (int16_t)(x + 32);
    }

    Stair_Move(obj, scratch);
}

void Obj_Staircase(Object *obj) {
    Scratch_Staircase *scratch = (Scratch_Staircase *)&obj->scratch;

    switch (obj->routine) {
    case 0: Stair_Main(obj, scratch); break;
    case 2: Stair_Move(obj, scratch); break;
    case 4: Stair_Solid(obj, scratch); break;
    }

    if (Stair_OutOfRange(scratch->orig_x))
        ObjectDelete(obj);
    else
        DisplaySprite(obj);
}
