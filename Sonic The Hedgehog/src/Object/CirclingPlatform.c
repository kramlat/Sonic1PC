#include "CirclingPlatform.h"
#include "Object.h"
#include "ObjectSst.h"
#include "SstLoad.h"
#include "Constants.h"

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

static void Circ_Types(Sst *self, Own_CirclingPlatform *own) {
    int16_t d1 = Circ_Radius(8);
    int16_t d2 = Circ_Radius(9);

    uint8_t subtype = self->subtype;
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

    self->x_pos.f.u = (int16_t)(own->orig_x + d1);
    self->y_pos.f.u = (int16_t)(own->orig_y + d2);
}

static void Circ_Main(Sst *self, Own_CirclingPlatform *own) {
    self->routine = 2; // advance to Circ_ChkTouch
    self->mappings = Mappings_CirclingPlatform;
    self->art_tile = TILE_MAP(0, 2, 0, 0, ArtTile_Level); // ArtTile_Level | Tile_Pal3
    self->render_flags = SST_RENDER_LEVEL;
    self->priority = 4 * 0x80;
    self->width_pixels = 48 / 2;
    own->orig_x = self->x_pos.f.u;
    own->orig_y = self->y_pos.f.u;
}

static void Circ_ChkTouch(Sst *self, Own_CirclingPlatform *own) {
    SstPlatformObject(self, self->width_pixels); // may set self->routine = 4
    Circ_Types(self, own);
}

static void Circ_OnPlatform(Sst *self, Own_CirclingPlatform *own) {
    SstExitPlatform(self, self->width_pixels, self->width_pixels, NULL); // may set self->routine = 2

    int16_t prev_x = self->x_pos.f.u;
    Circ_Types(self, own);

    SstMvSonicOnPtfm(self, self->y_pos.f.u - 9, prev_x);
}

void Obj_CirclingPlatform(Sst *self) {
    Own_CirclingPlatform *own = (Own_CirclingPlatform *)self->own.u8;

    switch (self->routine) {
    case 0: Circ_Main(self, own); break;
    case 2: Circ_ChkTouch(self, own); break;
    case 4: Circ_OnPlatform(self, own); break;
    }

    if (Circ_OutOfRange(own->orig_x))
        Sst_Delete(self);
    else
        Sst_Draw(self);
}
