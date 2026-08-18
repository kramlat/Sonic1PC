#include "GHZTunnel.h"

#include "Game.h"
#include "Level.h"
#include "LevelScroll.h"
#include "Object/Sonic.h"

// Mappings_PathSwapper is owned by PathSwapper.c (Object/PathSwapper.c
// directly #includes the actual array definition there) -- a bare extern
// here avoids a second definition and a link failure, same reasoning as
// LavaBall.c's own comment on Mappings_Fireballs.
extern const uint8_t Mappings_PathSwapper[];

// Object 10 - GHZ/HTZ-style tunnel roll trigger. Real slot 10 was an
// unused/deleted beta object (a Sonic artwork test); repurposed here for
// Sonic 2's "pinball mode" (Object 84 in s2disasm) concept, used the same
// simple way HTZ does it: a single-axis, single-player trigger placed at
// each tunnel mouth, rather than CNZ's full multi-directional/2-player
// tripwire. Subtype 0 = entrance (forces Sonic into a roll he can't jump
// or stop out of); any other subtype = exit (releases him back to normal
// control -- physics naturally un-rolls him next time his speed hits 0 or
// he jumps, same as real hardware).
//
// Debug-only preview: unlike PathSwapper (Object 03), this is a project-
// specific object with no real-hardware DebugPathSwappers precedent of its
// own, but reuses the exact same 4-ring marker (Mappings_PathSwapper,
// ArtTile_Ring) while debug_cheat is active, at a fixed vertical/medium
// size -- there's no subtype-driven orientation to key off here (subtype
// means entrance/exit, not size), so it's just one constant frame.

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

        obj->mappings = Mappings_PathSwapper;
        obj->tile = TILE_MAP(0, 1, 0, 0, ArtTile_Ring); // | Tile_Pal2
        obj->render.f.align_fg = true;
        obj->width_pixels = 32 / 2;
        obj->priority = 5;
        obj->frame = 1; // vertical, medium size -- fixed, no subtype-driven variant here
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

    if (debug_cheat) {
        RememberState(obj);
        return;
    }

    if (IS_OFFSCREEN(obj->pos.l.x.f.u))
        ObjectDelete(obj);
}
