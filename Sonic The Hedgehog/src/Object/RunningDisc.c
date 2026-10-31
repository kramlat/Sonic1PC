#include "RunningDisc.h"
#include "Object.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Constants.h"

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

// Sonic's "stick to convex" flag (the original's sticktoconvex, at 0x38 of his object): while it is set, his floor-following code keeps him on
// the gear's tight curve instead of letting him leave the ground, and his slope repel does not shake him off it.
static void SetStickToConvex(bool on) {
    ((Scratch_Sonic *)player->own.u8)->x38.floor_clip = on ? 1 : 0;
}

static void Disc_DetachSonic(Own_RunningDisc *own) {
    if (own->sonic_attached) {
        SetStickToConvex(false);
        own->sonic_attached = 0;
    }
}

static void Disc_AttachSonic(Own_RunningDisc *own) {
    if (!own->sonic_attached) {
        own->sonic_attached = 1;
        if (!((player->status & SST_PSTATUS_IN_BALL) != 0))
            player->anim = SonAnimId_Walk;
        player->status &= (uint8_t)~SST_PSTATUS_PUSHING;
        player->prev_anim = SonAnimId_Run; // restart Sonic's animation
        SetStickToConvex(true);
    }

    int16_t inertia = player->ground_vel;
    if (own->spot_speed >= 0) { // gear moving clockwise (it always is)
        if (inertia < 0x400) { player->ground_vel = 0x400; return; }
        if (inertia > (int16_t)0xF00) player->ground_vel = (int16_t)0xF00;
    } else {
        if (inertia > -0x400) { player->ground_vel = -0x400; return; }
        if (inertia < (int16_t)-0xF00) player->ground_vel = (int16_t)-0xF00;
    }
}

static void Disc_MoveSonic(Own_RunningDisc *own) {
    int16_t trig = own->triggersize;
    int16_t diameter = (int16_t)(trig * 2);

    int16_t d0 = (int16_t)(player->x_pos.f.u - own->orig_x + trig);
    if ((uint16_t)d0 >= (uint16_t)diameter) {
        Disc_DetachSonic(own);
        return;
    }

    int16_t d1 = (int16_t)(player->y_pos.f.u - own->orig_y + trig);
    if ((uint16_t)d1 >= (uint16_t)diameter) {
        Disc_DetachSonic(own);
        return;
    }

    if (((player->status & SST_PSTATUS_IN_AIR) != 0)) {
        own->sonic_attached = 0;
        return;
    }
    Disc_AttachSonic(own);
}

static void Disc_MoveSpot(Sst *self, Own_RunningDisc *own) {
    // Real hardware does `add.w spot_speed,obAngle(a0)` -- a 16-bit add
    // into a word straddling obAngle and the following (otherwise-unused)
    // byte, with only the high byte (self->angle) ever read back for
    // CalcSine. That makes the effective turn rate spot_speed/256 per
    // frame rather than the raw spot_speed value.
    uint16_t accum = (uint16_t)((self->angle << 8) | own->angle_frac);
    accum = (uint16_t)(accum + (uint16_t)own->spot_speed);
    self->angle = (uint8_t)(accum >> 8);
    own->angle_frac = (uint8_t)accum;

    int16_t sin, cos;
    CalcSine(self->angle, &sin, &cos);

    uint8_t distance = (uint8_t)own->spot_distance;
    int16_t dy = (int16_t)(((int32_t)sin * distance) >> 8);
    int16_t dx = (int16_t)(((int32_t)cos * distance) >> 8);

    self->y_pos.f.u = (int16_t)(own->orig_y + dy);
    self->x_pos.f.u = (int16_t)(own->orig_x + dx);
}

static void Disc_Action(Sst *self, Own_RunningDisc *own) {
    Disc_MoveSonic(own);
    Disc_MoveSpot(self, own);

    if (SstOutOfRange(own->orig_x)) {
        SstLoad_Forget(self);
        Sst_Delete(self);
        return;
    }

    if (debug_cheat) {
        // Draw the marker as a separate sprite at the gear's own center,
        // saving/restoring the spot's own fields around it -- this object
        // only has room for one sprite's worth of state at a time.
        int16_t spot_x = self->x_pos.f.u;
        int16_t spot_y = self->y_pos.f.u;
        const void *spot_mappings = self->mappings;
        uint16_t spot_tile = self->art_tile;
        uint8_t spot_frame = self->mapping_frame;

        self->x_pos.f.u = own->orig_x;
        self->y_pos.f.u = own->orig_y;
        self->collision_flags = 0; // marker itself has no collision
        self->mappings = Mappings_RunningDiscMarker;
        self->art_tile = TILE_MAP(1, 0, 0, 0, ArtTile_Monitor);
        self->mapping_frame = (own->triggersize == 0x48) ? 0 : 1; // 0 = large, 1 = small (leftover, practically unused)
        Sst_Draw(self);

        self->x_pos.f.u = spot_x;
        self->y_pos.f.u = spot_y;
        self->mappings = spot_mappings;
        self->art_tile = spot_tile;
        self->mapping_frame = spot_frame;
    }

    Sst_Draw(self);
}

static void Disc_Main(Sst *self, Own_RunningDisc *own) {
    self->routine = 2; // advance to Disc_Action
    self->mappings = Mappings_RunningDisc;
    self->art_tile = TILE_MAP(1, 2, 0, 0, ArtTile_SBZ_Disc); // | Tile_Pal3 | Tile_Prio
    self->render_flags = SST_RENDER_LEVEL;
    self->priority = 4 * 0x80;
    self->width_pixels = 16 / 2;

    own->orig_x = self->x_pos.f.u;
    own->orig_y = self->y_pos.f.u;
    own->spot_distance = 0x18;
    own->triggersize = 0x48;

    // Leftover from the prototype, where it was planned to also have
    // smaller-sized gears. In the final game, only large gears exist, so
    // this branch is never actually taken.
    uint8_t subtype = self->subtype;
    if (subtype & 0x0F) {
        own->spot_distance = 0x10;
        own->triggersize = 0x38;
    }

    own->spot_speed = (int16_t)((int8_t)(subtype & 0xF0) * 8);

    uint8_t status = self->status;
    uint8_t rotated = (uint8_t)((status >> 2) | (uint8_t)(status << 6)); // ror.b #2
    self->angle = rotated & 0xC0;
}

void Obj_RunningDisc(Sst *self) {
    Own_RunningDisc *own = (Own_RunningDisc *)self->own.u8;

    if (self->routine == 0)
        Disc_Main(self, own);

    Disc_Action(self, own);
}
