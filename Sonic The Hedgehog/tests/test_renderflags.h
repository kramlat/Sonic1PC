#pragma once

// The render flags' two ways of being set by an object's first run, as the original does them (found by comparing the disassemblies: `ori.b #sprite_cam_field,obRender(a0)` ORs the level flag in and the
// flips the layout gave the object stay; `move.b #sprite_cam_field,obRender(a0)` assigns it and they go). An object is run once with both flips set, in the game's first zone, and its flips are looked at.
// (Objects that animate on their first run take their flips from their status either way: they are not in the lists.)
#include <string.h>

#include "Level.h"
#include "Object.h"

static inline uint8_t RenderFlags_AfterFirstRun(int id) {
    memset(objects, 0, sizeof(Object) * 0x60);
    level_id = LEVEL_ID(0, 0);
    scrpos_x.v = scrpos_y.v = 0;
    player->type = 1;
    player->routine = 2;
    Object *obj = &objects[0x20];
    obj->type = (uint8_t)id;
    obj->render.b = 3;
    obj->pos.l.x.f.u = 0x100;
    obj->pos.l.y.f.u = 0x100;
    game_objects[id](obj);
    return obj->type == id ? (uint8_t)(obj->render.b & 3) : 0xFF; // (0xFF: it deleted itself)
}
