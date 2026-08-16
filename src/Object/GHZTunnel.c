#include "GHZTunnel.h"

#include "Level.h"
#include "LevelScroll.h"
#include "Object/Sonic.h"

// Object 10 - GHZ/HTZ-style tunnel roll trigger. Real slot 10 was an
// unused/deleted beta object (a Sonic artwork test); repurposed here for
// Sonic 2's "pinball mode" (Object 84 in s2disasm) concept, used the same
// simple way HTZ does it: a single-axis, single-player trigger placed at
// each tunnel mouth, rather than CNZ's full multi-directional/2-player
// tripwire. Subtype 0 = entrance (forces Sonic into a roll he can't jump
// or stop out of); any other subtype = exit (releases him back to normal
// control -- physics naturally un-rolls him next time his speed hits 0 or
// he jumps, same as real hardware).

static bool GHZT_ChkTouch(Object *obj) {
    int16_t x_off = (int16_t)(player->pos.l.x.f.u - obj->pos.l.x.f.u + obj->x_rad);
    if ((uint16_t)x_off >= (uint16_t)(obj->x_rad * 2))
        return false;

    int16_t y_off = (int16_t)(player->pos.l.y.f.u - obj->pos.l.y.f.u + obj->y_rad);
    if ((uint16_t)y_off >= (uint16_t)(obj->y_rad * 2))
        return false;

    return true;
}

void Obj_GHZTunnel(Object *obj) {
    if (obj->routine == 0) {
        obj->routine = 2;
        obj->x_rad = 16;
        obj->y_rad = 20;
    }

    if (GHZT_ChkTouch(obj)) {
        if (obj->scratch.u8[0] == 0) {
            // Entrance -- force the roll (Sonic_ChkRoll no-ops if he's
            // already rolling, matching real Object 84's own guard).
            Sonic_ChkRoll(player);
            player->status.p.f.must_roll = true;
        } else {
            // Exit -- just release the lock; don't force him out of a roll.
            player->status.p.f.must_roll = false;
        }
    }

    if (IS_OFFSCREEN(obj->pos.l.x.f.u))
        ObjectDelete(obj);
}
