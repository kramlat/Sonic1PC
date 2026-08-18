#include "GirderBlock.h"

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

static void Gird_ChgMove(Object *obj, Scratch_GirderBlock *scratch) {
    int idx = (scratch->set & 0x18) >> 3;
    obj->xsp = Gird_Settings[idx].xsp;
    obj->ysp = Gird_Settings[idx].ysp;
    scratch->time = Gird_Settings[idx].time;
    scratch->set = (uint8_t)(scratch->set + 8);
    scratch->delay = 7; // short delay to wait at a corner
}

static bool Gird_OutOfRange(int16_t x) {
    uint16_t obj_pos = (uint16_t)x & 0xFF80;
    uint16_t cam_pos = (uint16_t)(scrpos_x.f.u - 128) & 0xFF80;
    return (uint16_t)(obj_pos - cam_pos) > (128 + 320 + 192);
}

static void Gird_Action(Object *obj, Scratch_GirderBlock *scratch) {
    int16_t prev_x = obj->pos.l.x.f.u;

    if (scratch->delay != 0) {
        if (--scratch->delay != 0)
            goto solid; // still waiting at a corner
    }

    SpeedToPos(obj);
    if (--scratch->time != 0)
        goto solid;
    Gird_ChgMove(obj, scratch);

solid:
    if (obj->render.f.on_screen) {
        int16_t x_rad = (int16_t)(obj->width_pixels + 11 /* sonic_solid_width */);
        SolidObject(obj, (uint16_t)x_rad, obj->y_rad, (uint16_t)(obj->y_rad + 1), prev_x, NULL, NULL);
    }

    if (Gird_OutOfRange(scratch->orig_x))
        ObjectDelete(obj);
    else
        DisplaySprite(obj);
}

void Obj_GirderBlock(Object *obj) {
    Scratch_GirderBlock *scratch = (Scratch_GirderBlock *)&obj->scratch;

    if (obj->routine == 0) {
        obj->routine = 2; // advance to Gird_Action
        obj->mappings = Mappings_GirderBlock;
        obj->tile = TILE_MAP(0, 2, 0, 0, ArtTile_SBZ_Girder); // | Tile_Pal3
        obj->render.f.align_fg = true;
        obj->priority = 4;
        obj->width_pixels = 192 / 2;
        obj->y_rad = 48 / 2;
        scratch->orig_x = obj->pos.l.x.f.u;
        scratch->orig_y = obj->pos.l.y.f.u;
        Gird_ChgMove(obj, scratch);
    }

    Gird_Action(obj, scratch);
}
