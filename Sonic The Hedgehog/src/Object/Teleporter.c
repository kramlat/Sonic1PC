#include "Teleporter.h"
#include "Object.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Constants.h"

#include "Game.h"
#include "Level.h"
#include "LevelScroll.h"
#include "MathUtil.h"
#include "Object/Sonic.h"
#include "Sound.h"

#include "Resource/Mappings/TeleporterMarker.h"

// Object 72 - invisible teleporter system inside tubes (SBZ act 2). Fully
// invisible in normal play -- no mappings/art/render setup at all, just
// drives Sonic directly through a sequence of tube-bend target coordinates.
//
// Debug-only marker: no real hardware precedent, but shows a dense 4x4
// grid of 1-Up monitor icon tiles -- purely thematic/decorative, there's
// no real trigger-boundary shape worth scaling to here.

typedef struct { int16_t x, y; } Tele_Point;

static const Tele_Point Tele_Points0[] = { { 0x794, 0x98C } };
static const Tele_Point Tele_Points1[] = { { 0x94, 0x38C } };
static const Tele_Point Tele_Points2[] = {
    { 0x794, 0x2E8 }, { 0x7A4, 0x2C0 }, { 0x7D0, 0x2AC }, { 0x858, 0x2AC }, { 0x884, 0x298 }, { 0x894, 0x270 }, { 0x894, 0x190 },
};
static const Tele_Point Tele_Points3[] = { { 0x894, 0x690 } };
static const Tele_Point Tele_Points4[] = {
    { 0x1194, 0x470 }, { 0x1184, 0x498 }, { 0x1158, 0x4AC }, { 0xFD0, 0x4AC }, { 0xFA4, 0x4C0 }, { 0xF94, 0x4E8 }, { 0xF94, 0x590 },
};
static const Tele_Point Tele_Points5[] = { { 0x1294, 0x490 } };
static const Tele_Point Tele_Points6[] = {
    { 0x1594, (int16_t)0xFFE8 }, { 0x1584, (int16_t)0xFFC0 }, { 0x1560, (int16_t)0xFFAC }, { 0x14D0, (int16_t)0xFFAC },
    { 0x14A4, (int16_t)0xFF98 }, { 0x1494, (int16_t)0xFF70 }, { 0x1494, (int16_t)0xFD90 },
};
static const Tele_Point Tele_Points7[] = { { 0x894, 0x90 } };

static const struct { uint8_t count; const Tele_Point *points; } Tele_Data[8] = {
    { 1, Tele_Points0 }, { 1, Tele_Points1 }, { 7, Tele_Points2 }, { 1, Tele_Points3 },
    { 7, Tele_Points4 }, { 1, Tele_Points5 }, { 7, Tele_Points6 }, { 1, Tele_Points7 },
};

// Computes the secondary-axis velocity and travel time for one tube
// segment. `primary_diff`/`primary_speed` describe the axis with the
// larger distance (fixed at speed, +-0x1000); `secondary_diff` is the
// other axis's raw signed distance. Matches the real disasm's own
// double-division trick exactly, including the fixed-point truncation
// that gives the final frame count (equivalent to |primary_diff|/16).
static int16_t Tele_ComputeAxis(int16_t primary_diff, int16_t primary_speed, int16_t secondary_diff, int16_t *out_secondary_vel) {
    int32_t q = ((int32_t)primary_diff << 16) / primary_speed;

    if (secondary_diff == 0)
        *out_secondary_vel = 0;
    else
        *out_secondary_vel = (int16_t)(((int32_t)secondary_diff << 16) / (int16_t)q);

    int16_t time = (int16_t)q;
    if (time < 0)
        time = (int16_t)-time;
    return (int16_t)(time >> 8);
}

// Sets Sonic's speed & direction in a teleport pipe, called at start and
// whenever a bend is hit.
static void Tele_NextDirection(Sst *self, Own_Teleport *own) {
    (void)self;
    int16_t raw_dx = (int16_t)(own->target_x - player->x_pos.f.u);
    int16_t vx = 0x1000;
    if (raw_dx < 0)
        vx = (int16_t)-vx;

    int16_t raw_dy = (int16_t)(own->target_y - player->y_pos.f.u);
    int16_t vy = 0x1000;
    if (raw_dy < 0)
        vy = (int16_t)-vy;

    uint16_t dx = (uint16_t)(raw_dx < 0 ? -raw_dx : raw_dx);
    uint16_t dy = (uint16_t)(raw_dy < 0 ? -raw_dy : raw_dy);

    int16_t new_vel, time;
    if (dy < dx) {
        time = Tele_ComputeAxis(raw_dx, vx, raw_dy, &new_vel);
        player->y_vel = new_vel;
        player->x_vel = vx;
    } else {
        time = Tele_ComputeAxis(raw_dy, vy, raw_dx, &new_vel);
        player->x_vel = new_vel;
        player->y_vel = vy;
    }
    own->time = time;
}

static void Tele_Main(Sst *self, Own_Teleport *own) {
    self->routine = 2; // advance to Tele_Action
    own->current = 0;
    const Tele_Point *p = Tele_Data[self->subtype].points;
    own->target_x = p[0].x;
    own->target_y = p[0].y;
}

static void Tele_Action(Sst *self, Own_Teleport *own) {
    int16_t d0 = (int16_t)(player->x_pos.f.u - self->x_pos.f.u);
    if (((self->status & SST_STATUS_X_FLIP) != 0))
        d0 = (int16_t)(d0 + 15);
    if ((uint16_t)d0 >= 16)
        return;

    int16_t d1 = (int16_t)(player->y_pos.f.u - self->y_pos.f.u + 32);
    if ((uint16_t)d1 >= 64)
        return;

    if (debug_use) // FixBugs: don't allow activating teleporters in debug mode
        return;
    if (lock_multi) // already inside a teleporter (or other control override)
        return;

    if (self->subtype == 7 && rings < 50) // special 50-rings teleporter
        return;

    self->routine = 4; // advance to Tele_PreBump
    lock_multi = 0x81; // lock controls and disable object interaction
    player->anim = SonAnimId_Roll;
    player->ground_vel = 0x800; // fast ground speed for fast rolling animation
    player->x_vel = 0;
    player->y_vel = 0;
    self->status &= (uint8_t)~SST_STATUS_PLAYER_PUSH;
    player->status &= (uint8_t)~SST_PSTATUS_PUSHING;
    player->status |= SST_PSTATUS_IN_AIR;
    player->x_pos.f.u = self->x_pos.f.u;
    player->y_pos.f.u = self->y_pos.f.u;
    own->prebump = 0;

    QueueSound2(sfx_Roll);
}

static void Tele_PreBump(Sst *self, Own_Teleport *own) {
    int16_t sin, cos;
    CalcSine(own->prebump, &sin, &cos);
    own->prebump = (uint8_t)(own->prebump + 2);

    int16_t bump = (int16_t)(sin >> 5);
    player->y_pos.f.u = (int16_t)(self->y_pos.f.u - bump);

    if (own->prebump != 0x80)
        return;

    Tele_NextDirection(self, own); // begin teleportation
    self->routine = 6; // advance to Tele_Teleporting
    QueueSound2(sfx_Teleport);
}

static void Tele_Teleporting(Sst *self, Own_Teleport *own) {
    if (--own->time >= 0) {
        // Direct copy of SpeedToPos, targeting Sonic instead of this object.
        player->x_pos.v += player->x_vel << 8;
        player->y_pos.v += player->y_vel << 8;
        return;
    }

    player->x_pos.f.u = own->target_x;
    player->y_pos.f.u = own->target_y;

    uint8_t group = self->subtype;
    uint8_t next = (uint8_t)(own->current + 1);
    if (next < Tele_Data[group].count) {
        own->current = next;
        own->target_x = Tele_Data[group].points[next].x;
        own->target_y = Tele_Data[group].points[next].y;
        Tele_NextDirection(self, own);
        return;
    }

    player->y_pos.f.u &= 0x7FF; // SBZ2 is a Y-wrapping level
    self->routine = 0; // reset teleporter back to Tele_Main
    lock_multi = 0; // clear Sonic control override flags
    player->x_vel = 0;
    player->y_vel = 0x200; // quickly land on floor again
}

void Obj_Teleporter(Sst *self) {
    Own_Teleport *own = (Own_Teleport *)self->own.u8;

    if (self->routine == 0)
        Tele_Main(self, own);

    switch (self->routine) {
    case 2: Tele_Action(self, own); break;
    case 4: Tele_PreBump(self, own); break;
    case 6:
        Tele_Teleporting(self, own);
        return; // never checked for out-of-range while actively teleporting
    }

    if (debug_cheat) {
        // No collision of its own (a pooled slot's leftover col_type could
        // otherwise leak through now that this becomes visible/on-screen
        // for the first time) -- see LavaMaker.c's own comment on this.
        self->collision_flags = 0;
        self->mappings = Mappings_TeleporterMarker;
        self->art_tile = TILE_MAP(1, 0, 0, 0, ArtTile_Monitor);
        self->mapping_frame = 0;
        Sst_Draw(self);
    }

    SstDeleteIfNotInRange2(self, self->x_pos.f.u);
}
