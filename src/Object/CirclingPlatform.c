#include "CirclingPlatform.h"

#include "Level.h"
#include "LevelScroll.h"
#include "MathUtil.h"
#include "Resource/Mappings/CirclingPlatform.h"

// Object 5A - platforms moving in circles (SLZ)
//
// Subtype bitfield:
//   bit 0 = if set, shift 180 degrees ahead
//   bit 1 = if set, shift 90 degrees ahead
//   bit 2 = if set, circle clockwise (otherwise, counterclockwise)

static bool Circ_OutOfRange(int16_t x) {
    uint16_t obj_pos = (uint16_t)x & 0xFF80;
    uint16_t cam_pos = (uint16_t)(scrpos_x.f.u - 128) & 0xFF80;
    return (uint16_t)(obj_pos - cam_pos) > (128 + 320 + 192);
}

// oscillatory.state[8]/[9] (real v_oscillate+$22/+$26) drive the circle's
// X/Y radii -- state[N][0] is a 16-bit accumulator where only the high
// byte is the slow-moving integer value (the low byte is a fast-changing
// sub-pixel remainder), offset by the table's own $50 middle value.
static int16_t Circ_Radius(int idx) {
    return (int16_t)(int8_t)((uint8_t)(oscillatory.state[idx][0] >> 8) - 0x50);
}

static void Circ_Types(Object *obj, Scratch_CirclingPlatform *scratch) {
    int16_t d1 = Circ_Radius(8);
    int16_t d2 = Circ_Radius(9);

    uint8_t subtype = obj->scratch.u8[0];
    if (subtype & 1) { // shift 180 degrees ahead
        d1 = (int16_t)-d1;
        d2 = (int16_t)-d2;
    }
    if (subtype & 2) { // shift 90 degrees ahead
        d1 = (int16_t)-d1;
        int16_t tmp = d1;
        d1 = d2;
        d2 = tmp;
    }
    if (subtype & 4) // circle clockwise
        d1 = (int16_t)-d1;

    obj->pos.l.x.f.u = (int16_t)(scratch->orig_x + d1);
    obj->pos.l.y.f.u = (int16_t)(scratch->orig_y + d2);
}

static void Circ_Main(Object *obj, Scratch_CirclingPlatform *scratch) {
    obj->routine = 2; // advance to Circ_ChkTouch
    obj->mappings = Mappings_CirclingPlatform;
    obj->tile = TILE_MAP(0, 2, 0, 0, 0); // ArtTile_Level | Tile_Pal3
    obj->render.f.align_fg = true;
    obj->priority = 4;
    obj->width_pixels = 48 / 2;
    scratch->orig_x = obj->pos.l.x.f.u;
    scratch->orig_y = obj->pos.l.y.f.u;
}

static void Circ_ChkTouch(Object *obj, Scratch_CirclingPlatform *scratch) {
    PlatformObject(obj, obj->width_pixels); // may set obj->routine = 4
    Circ_Types(obj, scratch);
}

static void Circ_OnPlatform(Object *obj, Scratch_CirclingPlatform *scratch) {
    ExitPlatform(obj, obj->width_pixels, obj->width_pixels, NULL); // may set obj->routine = 2

    int16_t prev_x = obj->pos.l.x.f.u;
    Circ_Types(obj, scratch);

    MvSonicOnPtfm(obj, obj->pos.l.y.f.u - 9, prev_x);
}

void Obj_CirclingPlatform(Object *obj) {
    Scratch_CirclingPlatform *scratch = (Scratch_CirclingPlatform *)&obj->scratch;

    switch (obj->routine) {
    case 0: Circ_Main(obj, scratch); break;
    case 2: Circ_ChkTouch(obj, scratch); break;
    case 4: Circ_OnPlatform(obj, scratch); break;
    }

    if (Circ_OutOfRange(scratch->orig_x))
        ObjectDelete(obj);
    else
        DisplaySprite(obj);
}
