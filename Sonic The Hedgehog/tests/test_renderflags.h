#pragma once

// The render flags' two ways of being set by an object's first run, as the original does them (found by comparing the disassemblies: `ori.b #sprite_cam_field,obRender(a0)` ORs the level flag in and the
// flips the layout gave the object stay; `move.b #sprite_cam_field,obRender(a0)` assigns it and they go). An object is run once with both flips set, in the game's first zone, and its flips are looked at.
// (Objects that animate on their first run take their flips from their status either way: they are not in the lists.)
#include <string.h>

#include "Level.h"
#include "Object.h"
#include "Sst.h"
#include "SstLoad.h"


static inline uint8_t RenderFlags_AfterFirstRun(int id) {
    level_id = LEVEL_ID(0, 0);
    scrpos_x.v = scrpos_y.v = 0;
    player->code = sst_object_codes[1];
    player->routine = 2;
    if (sst_object_codes[id] != NULL) {
        Sst_Reset();
        static const SstObjectIndex index_ = {sst_object_codes, 0x100};
        SstLoadCamera camera = {0, 0, false, 0xFFF};
        SstLoad_Init(&index_, NULL, false, &camera); // (the camera the object keeps in range of)
        Sst *self = &sst[SST_FIRST_DYNAMIC + 5];
        self->code = sst_object_codes[id];
        self->render_flags = 3;
        self->x_pos.f.u = 0x100;
        self->y_pos.f.u = 0x100;
        self->code(self);
        return self->code != NULL ? (uint8_t)(self->render_flags & 3) : 0xFF;
    }
    return 0xFF; // (no routine: the loader leaves its entries alone)
}
