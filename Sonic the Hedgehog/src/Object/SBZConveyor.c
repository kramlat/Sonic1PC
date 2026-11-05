#include "SBZConveyor.h"
#include "Object.h"
#include "ObjectSst.h"
#include "SstLoad.h"
#include "Constants.h"

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

static void Conveyor_MoveSonic(Sst *self, Own_SBZConveyor *own) {
    int16_t half_width = own->width;
    int16_t full_width = (int16_t)(half_width * 2);

    int16_t d0 = (int16_t)(player->x_pos.f.u - self->x_pos.f.u + half_width);
    if ((uint16_t)d0 >= (uint16_t)full_width)
        return;

    int16_t d1 = (int16_t)(player->y_pos.f.u - self->y_pos.f.u + 48);
    if ((uint16_t)d1 >= 48)
        return;

    if (((player->status & SST_PSTATUS_IN_AIR) != 0))
        return;

    player->x_pos.f.u = (int16_t)(player->x_pos.f.u + own->speed);
}

static void Conv_Main(Sst *self, Own_SBZConveyor *own) {
    self->routine = 2; // advance to Conv_Action

    own->width = 256 / 2;
    if (self->subtype & 0x0F)
        own->width = 112 / 2;

    int8_t upper_nibble = (int8_t)(self->subtype & 0xF0);
    own->speed = (int16_t)(upper_nibble >> 4);
}

void Obj_SBZConveyor(Sst *self) {
    Own_SBZConveyor *own = (Own_SBZConveyor *)self->own.u8;

    if (self->routine == 0)
        Conv_Main(self, own);

    Conveyor_MoveSonic(self, own);

    if (debug_cheat) {
        // No collision of its own (a pooled slot's leftover col_type could
        // otherwise leak through now that this becomes visible/on-screen
        // for the first time) -- see LavaMaker.c's own comment on this.
        self->collision_flags = 0;
        self->mappings = Mappings_SBZConveyorMarker;
        self->art_tile = TILE_MAP(1, 0, 0, 0, ArtTile_Monitor);
        self->mapping_frame = (own->width == 256 / 2) ? 0 : 1; // 0 = wide, 1 = narrow
        Sst_Draw(self);
    }

    SstDeleteIfNotInRange(self);
}
