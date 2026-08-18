#include "SBZConveyor.h"

#include "Game.h"
#include "Level.h"
#include "LevelScroll.h"
#include "Object/Sonic.h"

#include "Resource/Mappings/SBZConveyorMarker.h"

// Object 68 - conveyor belts (SBZ)
//
// This is just the invisible object that moves Sonic horizontally. The
// conveyor belt graphics themselves are part of the level chunks.
//
// Debug-only marker: no real hardware precedent, but shows 4 speed-shoe
// monitor icon tiles scaled to the trigger's real width (wide/narrow,
// matching Conv_Main's own 2-way choice) -- same corner-scaling idea as
// LavaTagMarker.

static void Conveyor_MoveSonic(Object *obj, Scratch_SBZConveyor *scratch) {
    int16_t half_width = scratch->width;
    int16_t full_width = (int16_t)(half_width * 2);

    int16_t d0 = (int16_t)(player->pos.l.x.f.u - obj->pos.l.x.f.u + half_width);
    if ((uint16_t)d0 >= (uint16_t)full_width)
        return;

    int16_t d1 = (int16_t)(player->pos.l.y.f.u - obj->pos.l.y.f.u + 48);
    if ((uint16_t)d1 >= 48)
        return;

    if (player->status.p.f.in_air)
        return;

    player->pos.l.x.f.u = (int16_t)(player->pos.l.x.f.u + scratch->speed);
}

static bool Conv_OutOfRange(int16_t x) {
    uint16_t obj_pos = (uint16_t)x & 0xFF80;
    uint16_t cam_pos = (uint16_t)(scrpos_x.f.u - 128) & 0xFF80;
    return (uint16_t)(obj_pos - cam_pos) > (128 + 320 + 192);
}

static void Conv_Main(Object *obj, Scratch_SBZConveyor *scratch) {
    obj->routine = 2; // advance to Conv_Action

    scratch->width = 256 / 2;
    if (obj->scratch.u8[0] & 0x0F)
        scratch->width = 112 / 2;

    int8_t upper_nibble = (int8_t)(obj->scratch.u8[0] & 0xF0);
    scratch->speed = (int16_t)(upper_nibble >> 4);
}

void Obj_SBZConveyor(Object *obj) {
    Scratch_SBZConveyor *scratch = (Scratch_SBZConveyor *)&obj->scratch;

    if (obj->routine == 0)
        Conv_Main(obj, scratch);

    Conveyor_MoveSonic(obj, scratch);

    if (debug_cheat) {
        // No collision of its own (a pooled slot's leftover col_type could
        // otherwise leak through now that this becomes visible/on-screen
        // for the first time) -- see LavaMaker.c's own comment on this.
        obj->col_type = 0;
        obj->mappings = Mappings_SBZConveyorMarker;
        obj->tile = TILE_MAP(1, 0, 0, 0, ArtTile_Monitor);
        obj->frame = (scratch->width == 256 / 2) ? 0 : 1; // 0 = wide, 1 = narrow
        DisplaySprite(obj);
    }

    if (Conv_OutOfRange(obj->pos.l.x.f.u))
        ObjectDelete(obj);
}
