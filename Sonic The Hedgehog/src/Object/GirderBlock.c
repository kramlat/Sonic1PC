#include "GirderBlock.h"
#include "Object.h"
#include "ObjectSst.h"
#include "SstLoad.h"
#include "Constants.h"

#include "Level.h"
#include "LevelScroll.h"
#include "Resource/Mappings/GirderBlock.h"

// Object 70 - large girder block (SBZ)

// X-speed, Y-speed, duration (frames)
static const struct { int16_t xsp, ysp; int16_t time; } Gird_Settings[4] = {
    {  0x100,     0, 96 }, // right
    {      0,  0x100, 48 }, // down
    { -0x100, -0x40,  96 }, // up/left
    {      0, -0x100, 24 }, // up
};

static void Gird_ChgMove(Sst *self, Own_GirderBlock *own) {
    int idx = (own->set & 0x18) >> 3;
    self->x_vel = Gird_Settings[idx].xsp;
    self->y_vel = Gird_Settings[idx].ysp;
    own->time = Gird_Settings[idx].time;
    own->set = (uint8_t)(own->set + 8);
    own->delay = 7; // short delay to wait at a corner
}

static void Gird_Action(Sst *self, Own_GirderBlock *own) {
    int16_t prev_x = self->x_pos.f.u;

    if (own->delay != 0) {
        if (--own->delay != 0)
            goto solid; // still waiting at a corner
    }

    Sst_MoveSprite2(self);
    if (--own->time != 0)
        goto solid;
    Gird_ChgMove(self, own);

solid:
    if ((self->render_flags & SST_RENDER_ON_SCREEN)) {
        int16_t x_rad = (int16_t)(self->width_pixels + 11 /* sonic_solid_width */);
        SstSolidObject(self, (uint16_t)x_rad, self->y_radius, (uint16_t)(self->y_radius + 1), prev_x, NULL, NULL);
    }

    SstOnScreenTest2(self, own->orig_x);
}

void Obj_GirderBlock(Sst *self) {
    Own_GirderBlock *own = (Own_GirderBlock *)self->own.u8;

    if (self->routine == 0) {
        self->routine = 2; // advance to Gird_Action
        self->mappings = Mappings_GirderBlock;
        self->art_tile = TILE_MAP(0, 2, 0, 0, ArtTile_SBZ_Girder); // | Tile_Pal3
        self->render_flags |= SST_RENDER_LEVEL;
        self->priority = 4 * 0x80;
        self->width_pixels = 192 / 2;
        self->y_radius = 48 / 2;
        own->orig_x = self->x_pos.f.u;
        own->orig_y = self->y_pos.f.u;
        Gird_ChgMove(self, own);
    }

    Gird_Action(self, own);
}
