#pragma once

// The render flags' two ways of being set by an object's first run, as the original does them (found by comparing the disassemblies: `ori.b #sprite_cam_field,obRender(a0)` ORs the level flag in and the
// flips the layout gave the object stay; `move.b #sprite_cam_field,obRender(a0)` assigns it and they go). An object is run once with both flips set, in the game's first zone, and its flips are looked at.
// (Objects that animate on their first run take their flips from their status either way: they are not in the lists.)
#include <string.h>

#include "Level.h"
#include "Object.h"
#include "Sst.h"

extern const SstCode sst_object_codes[0x100]; // (the objects that have moved to the status table run from there)

static inline uint8_t RenderFlags_AfterFirstRun(int id) {
    memset(objects, 0, sizeof(Object) * 0x60);
    level_id = LEVEL_ID(0, 0);
    scrpos_x.v = scrpos_y.v = 0;
    player->type = 1;
    player->routine = 2;
    if (sst_object_codes[id] != NULL) {
        Sst_Reset();
        Sst *self = &sst[SST_FIRST_DYNAMIC + 5];
        self->code = sst_object_codes[id];
        self->render_flags = 3;
        self->x_pos.f.u = 0x100;
        self->y_pos.f.u = 0x100;
        self->code(self);
        return self->code != NULL ? (uint8_t)(self->render_flags & 3) : 0xFF;
    }
    Object *obj = &objects[0x20];
    obj->type = (uint8_t)id;
    obj->render.b = 3;
    obj->pos.l.x.f.u = 0x100;
    obj->pos.l.y.f.u = 0x100;
    game_objects[id](obj);
    return obj->type == id ? (uint8_t)(obj->render.b & 3) : 0xFF; // (0xFF: it deleted itself)
}
