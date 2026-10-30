#include "LZBlocks.h"
#include "Object.h"
#include "SstLoad.h"
#include "ObjectSst.h"

#include "Level.h"
#include "LevelCollision.h"
#include "LevelScroll.h"
#include "Macros.h"
#include "MathUtil.h"
#include "Resource/Mappings/LZBlocks.h"

// Object 61 - multi-variant blocks (LZ): a block that sinks once stood on
// (optionally after a half-second delay, or after being touched from the
// side), a platform that rises the same way, a cork that bobs on the
// water surface, and a plain static block -- all sharing one object with
// a small gentle nudge-down animation while Sonic stands on the movable
// ones.

static const struct { uint8_t width, height; } LBlk_Var[4] = {
    { 32 / 2, 32 / 2 }, // block that sinks when stood on
    { 64 / 2, 24 / 2 }, // platform that rises when stood on
    { 32 / 2, 32 / 2 }, // cork block that floats on water
    { 32 / 2, 32 / 2 }, // generic solid block
};

static void LBlk_CheckStand(Sst *self, Own_LBlock *own) {
    if (own->time == 0) {
        if (!((self->status & SST_STATUS_PLAYER_STAND) != 0))
            return;
        own->time = 30;
        return;
    }
    if (--own->time != 0)
        return;
    self->subtype++; // advance to LBlk_Sink or LBlk_Rise
    own->untouched = false;
}

static void LBlk_Sink(Sst *self) {
    Sst_MoveSprite2(self);
    self->y_vel += 8;

    int16_t floor_dist = SstFloorDist(self, self->x_pos.f.u);
    if (floor_dist < 0) {
        self->y_pos.f.u += (int16_t)(floor_dist + 1);
        self->y_vel = 0;
        self->subtype = 0; // stationary
    }
}

static void LBlk_Rise(Sst *self) {
    Sst_MoveSprite2(self);
    self->y_vel -= 8;

    int16_t ceil_dist = SstHitCeiling(self);
    if (ceil_dist < 0) {
        self->y_pos.f.u -= ceil_dist;
        self->y_vel = 0;
        self->subtype = 0; // stationary
    }
}

static void LBlk_SideSink(Sst *self, Own_LBlock *own) {
    if (own->touchtype != 1) // touched from the sides?
        return;
    self->subtype++; // advance to LBlk_Sink
    own->untouched = false;
}

static void LBlk_OnWater(Sst *self) {
    int16_t d0 = (int16_t)(wtr_pos1 - self->y_pos.f.u);
    if (d0 == 0)
        return;

    if (d0 >= 0) {
        // Block is above water -- sink down to meet it
        if (d0 > 2)
            d0 = 2;
        self->y_pos.f.u += d0;

        int16_t floor_dist = SstFloorDist(self, self->x_pos.f.u);
        if (floor_dist < 0)
            self->y_pos.f.u += (int16_t)(floor_dist + 1);
        return;
    }

    // Block is below water -- rise up to meet it
    if (d0 < -2)
        d0 = -2;
    self->y_pos.f.u += d0;

    int16_t ceil_dist = SstHitCeiling(self);
    if (ceil_dist < 0)
        self->y_pos.f.u -= ceil_dist;
}

static void LBlk_Nudge(Sst *self, Own_LBlock *own) {
    if (!own->untouched)
        return;

    if (((self->status & SST_STATUS_PLAYER_STAND) != 0)) {
        if (own->nudge == 0x40)
            return;
        own->nudge += 4;
    } else {
        if (own->nudge == 0)
            return;
        own->nudge -= 4;
    }

    int16_t sin, cos;
    CalcSine(own->nudge, &sin, &cos);
    int16_t nudge_off = (int16_t)(((int32_t)sin * 0x400) >> 16);
    self->y_pos.f.u = (int16_t)(own->orig_y + nudge_off);
}

static void LBlk_Main(Sst *self, Own_LBlock *own) {
    self->routine += 2;
    self->mappings = Mappings_LZBlocks;
    self->art_tile = TILE_MAP(0, 2, 0, 0, ArtTile_LZ_Blocks); // | Tile_Pal3
    self->render_flags = SST_RENDER_LEVEL;
    self->priority = 3 * 0x80;

    uint8_t subtype = self->subtype;
    uint8_t index = (subtype >> 4) & 7;
    self->width_pixels = LBlk_Var[index].width;
    self->y_radius = (int8_t)LBlk_Var[index].height;
    self->mapping_frame = index;

    own->orig_x = self->x_pos.f.u;
    own->orig_y = self->y_pos.f.u;

    uint8_t type = subtype & 0xF;
    if (type != 0 && type != 7) // not stationary, and not the small secret platform in LZ1
        own->untouched = true;
}

static void LBlk_Action(Sst *self, Own_LBlock *own) {
    int16_t prev_x = self->x_pos.f.u;

    switch (self->subtype & 0xF) {
    case 0: break; // stationary
    case 1: LBlk_CheckStand(self, own); break;
    case 2: LBlk_Sink(self); break;
    case 3: LBlk_CheckStand(self, own); break;
    case 4: LBlk_Rise(self); break;
    case 5: LBlk_SideSink(self, own); break;
    case 6: LBlk_Sink(self); break;
    case 7: LBlk_OnWater(self); break;
    }

    if ((self->render_flags & SST_RENDER_ON_SCREEN)) {
        int16_t x_rad = (int16_t)(self->width_pixels + 11 /* sonic_solid_width */);
        int32_t touch = SstSolidObject(self, (uint16_t)x_rad, (uint16_t)self->y_radius, (uint16_t)(self->y_radius + 1), prev_x, NULL, NULL);
        own->touchtype = (int8_t)touch;
        LBlk_Nudge(self, own);
    }

    SstOnScreenTest2(self, own->orig_x);
}

void Obj_LabyrinthBlock(Sst *self) {
    Own_LBlock *own = (Own_LBlock *)self->own.u8;

    switch (self->routine) {
    case 0:
        LBlk_Main(self, own);
        __attribute__((fallthrough));
    case 2:
        LBlk_Action(self, own);
        break;
    }
}
