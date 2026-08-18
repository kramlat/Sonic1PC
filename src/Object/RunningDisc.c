#include "RunningDisc.h"

#include "Game.h"
#include "Level.h"
#include "LevelScroll.h"
#include "MathUtil.h"
#include "Object/Sonic.h"
#include "Resource/Mappings/RunningDisc.h"
#include "Resource/Mappings/RunningDiscMarker.h"

// This is just the invisible object that controls Sonic, as well as the
// small circular spot that moves inside the gear. The gear graphics
// themselves are part of the level chunks.
//
// Debug-only marker: works exactly like SBZConveyor's own touch-and-carry
// trigger (Disc_MoveSonic mirrors Conveyor_MoveSonic almost exactly), so it
// gets the same 4 speed-shoe-icon corner marker -- scaled to the gear's
// square attach trigger (triggersize) instead of a rectangle, and drawn as
// a second sprite at the gear's own center since the spot's own sprite
// still needs its normal display too.

static bool Disc_OutOfRange(int16_t x) {
    uint16_t obj_pos = (uint16_t)x & 0xFF80;
    uint16_t cam_pos = (uint16_t)(scrpos_x.f.u - 128) & 0xFF80;
    return (uint16_t)(obj_pos - cam_pos) > (128 + 320 + 192);
}

static void Disc_DetachSonic(Scratch_RunningDisc *scratch) {
    if (scratch->sonic_attached) {
        // real hardware also clears Sonic's sticktoconvex flag here -- see header note
        scratch->sonic_attached = 0;
    }
}

static void Disc_AttachSonic(Scratch_RunningDisc *scratch) {
    if (!scratch->sonic_attached) {
        scratch->sonic_attached = 1;
        if (!player->status.p.f.in_ball)
            player->anim = SonAnimId_Walk;
        player->status.p.f.pushing = false;
        player->prev_anim = SonAnimId_Run; // restart Sonic's animation
        // real hardware also sets Sonic's sticktoconvex flag here -- see header note
    }

    int16_t inertia = player->inertia;
    if (scratch->spot_speed >= 0) { // gear moving clockwise (it always is)
        if (inertia < 0x400) { player->inertia = 0x400; return; }
        if (inertia > (int16_t)0xF00) player->inertia = (int16_t)0xF00;
    } else {
        if (inertia > -0x400) { player->inertia = -0x400; return; }
        if (inertia < (int16_t)-0xF00) player->inertia = (int16_t)-0xF00;
    }
}

static void Disc_MoveSonic(Scratch_RunningDisc *scratch) {
    int16_t trig = scratch->triggersize;
    int16_t diameter = (int16_t)(trig * 2);

    int16_t d0 = (int16_t)(player->pos.l.x.f.u - scratch->orig_x + trig);
    if ((uint16_t)d0 >= (uint16_t)diameter) {
        Disc_DetachSonic(scratch);
        return;
    }

    int16_t d1 = (int16_t)(player->pos.l.y.f.u - scratch->orig_y + trig);
    if ((uint16_t)d1 >= (uint16_t)diameter) {
        Disc_DetachSonic(scratch);
        return;
    }

    if (player->status.p.f.in_air) {
        scratch->sonic_attached = 0;
        return;
    }
    Disc_AttachSonic(scratch);
}

static void Disc_MoveSpot(Object *obj, Scratch_RunningDisc *scratch) {
    // Real hardware does `add.w spot_speed,obAngle(a0)` -- a 16-bit add
    // into a word straddling obAngle and the following (otherwise-unused)
    // byte, with only the high byte (obj->angle) ever read back for
    // CalcSine. That makes the effective turn rate spot_speed/256 per
    // frame rather than the raw spot_speed value.
    uint16_t accum = (uint16_t)((obj->angle << 8) | scratch->angle_frac);
    accum = (uint16_t)(accum + (uint16_t)scratch->spot_speed);
    obj->angle = (uint8_t)(accum >> 8);
    scratch->angle_frac = (uint8_t)accum;

    int16_t sin, cos;
    CalcSine(obj->angle, &sin, &cos);

    uint8_t distance = (uint8_t)scratch->spot_distance;
    int16_t dy = (int16_t)(((int32_t)sin * distance) >> 8);
    int16_t dx = (int16_t)(((int32_t)cos * distance) >> 8);

    obj->pos.l.y.f.u = (int16_t)(scratch->orig_y + dy);
    obj->pos.l.x.f.u = (int16_t)(scratch->orig_x + dx);
}

static void Disc_Action(Object *obj, Scratch_RunningDisc *scratch) {
    Disc_MoveSonic(scratch);
    Disc_MoveSpot(obj, scratch);

    if (Disc_OutOfRange(scratch->orig_x)) {
        ObjectDelete(obj);
        return;
    }

    if (debug_cheat) {
        // Draw the marker as a separate sprite at the gear's own center,
        // saving/restoring the spot's own fields around it -- this object
        // only has room for one sprite's worth of state at a time.
        int16_t spot_x = obj->pos.l.x.f.u;
        int16_t spot_y = obj->pos.l.y.f.u;
        const void *spot_mappings = obj->mappings;
        uint16_t spot_tile = obj->tile;
        uint8_t spot_frame = obj->frame;

        obj->pos.l.x.f.u = scratch->orig_x;
        obj->pos.l.y.f.u = scratch->orig_y;
        obj->col_type = 0; // marker itself has no collision
        obj->mappings = Mappings_RunningDiscMarker;
        obj->tile = TILE_MAP(1, 0, 0, 0, ArtTile_Monitor);
        obj->frame = (scratch->triggersize == 0x48) ? 0 : 1; // 0 = large, 1 = small (leftover, practically unused)
        DisplaySprite(obj);

        obj->pos.l.x.f.u = spot_x;
        obj->pos.l.y.f.u = spot_y;
        obj->mappings = spot_mappings;
        obj->tile = spot_tile;
        obj->frame = spot_frame;
    }

    DisplaySprite(obj);
}

static void Disc_Main(Object *obj, Scratch_RunningDisc *scratch) {
    obj->routine = 2; // advance to Disc_Action
    obj->mappings = Mappings_RunningDisc;
    obj->tile = TILE_MAP(1, 2, 0, 0, ArtTile_SBZ_Disc); // | Tile_Pal3 | Tile_Prio
    obj->render.f.align_fg = true;
    obj->priority = 4;
    obj->width_pixels = 16 / 2;

    scratch->orig_x = obj->pos.l.x.f.u;
    scratch->orig_y = obj->pos.l.y.f.u;
    scratch->spot_distance = 0x18;
    scratch->triggersize = 0x48;

    // Leftover from the prototype, where it was planned to also have
    // smaller-sized gears. In the final game, only large gears exist, so
    // this branch is never actually taken.
    uint8_t subtype = obj->scratch.u8[0];
    if (subtype & 0x0F) {
        scratch->spot_distance = 0x10;
        scratch->triggersize = 0x38;
    }

    scratch->spot_speed = (int16_t)((int8_t)(subtype & 0xF0) * 8);

    uint8_t status = obj->status.b;
    uint8_t rotated = (uint8_t)((status >> 2) | (uint8_t)(status << 6)); // ror.b #2
    obj->angle = rotated & 0xC0;
}

void Obj_RunningDisc(Object *obj) {
    Scratch_RunningDisc *scratch = (Scratch_RunningDisc *)&obj->scratch;

    if (obj->routine == 0)
        Disc_Main(obj, scratch);

    Disc_Action(obj, scratch);
}
