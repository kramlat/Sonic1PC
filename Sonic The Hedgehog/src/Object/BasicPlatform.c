#include "BasicPlatform.h"
#include "Object.h"
#include "Resource/Mappings/SLZPlatforms.h"
#include "Resource/Mappings/SYZPlatforms.h"
#include "Resource/Mappings/GHZPlatforms.h"
#include "SstLoad.h"
#include "ObjectSst.h"

#include "Level.h"
#include "LevelScroll.h"
#include "Macros.h"
#include "MathUtil.h"

// Object 18 - basic platforms (GHZ, SYZ, SLZ)

static void Plat_Nudge(Sst *self, Own_BasicPlatform *own) {
    int16_t sin, cos;
    CalcSine(own->nudge, &sin, &cos);
    // Depress by at most 4px -- amplitude $400 in 16.16, matches the 4px
    // real hardware comment (asl.w #8 equivalent baked into the multiply).
    self->y_pos.f.u = (int16_t)(own->raw_y.f.u + ((sin * 0x400) >> 16));
}

// Refreshes the shared (all-platforms-in-lockstep) triangle-wave motion
// value into this platform's own angle field, matching the real driver's
// Plat_ChangeMotion (frequency 8, middle $40 -- v_oscillate+$1A/state[6]).
static void Plat_ChangeMotion(Sst *self) {
    self->angle = (uint8_t)(oscillatory.state[6][0] >> 8);
}

static void Plat_MoveHorizontal(Sst *self, Own_BasicPlatform *own, int8_t delta) {
    self->x_pos.f.u = (int16_t)(own->orig_x + delta);
    Plat_ChangeMotion(self);
}

static void Plat_MoveVertical(Sst *self, Own_BasicPlatform *own, int8_t delta) {
    own->raw_y.f.u = (int16_t)(own->orig_y + delta);
    Plat_ChangeMotion(self);
}

static void Plat_FallAfterStand(Sst *self, Own_BasicPlatform *own) {
    if (own->delay != 0) {
        if (--own->delay == 0) {
            own->delay = 32; // keep Sonic attached to the platform for 32 frames after it starts falling
            self->subtype++;  // advance to Falling
        }
        return;
    }
    if (((self->status & SST_STATUS_PLAYER_STAND) != 0))
        own->delay = 30; // half a second before falling
}

static void Plat_FallingDown(Sst *self, Own_BasicPlatform *own) {
    if (own->delay != 0) {
        if (--own->delay == 0) {
            if (((self->status & SST_STATUS_PLAYER_STAND) != 0)) {
                player->status |= SST_PSTATUS_IN_AIR;
                player->status &= (uint8_t)~SST_PSTATUS_OBJECT_STAND;
                player->routine = 2; // force Sonic to Sonic_Control
                self->status &= (uint8_t)~SST_STATUS_PLAYER_STAND;
                player->y_vel = self->y_vel;
            }
            self->routine = 8; // Plat_Action -- no longer calls PlatformObject, so it's no longer solid; also blocks re-entry
        }
    }

    own->raw_y.v += (int32_t)self->y_vel << 8;
    self->y_vel += 0x38; // gravity

    if (limit_btm2 + SCREEN_HEIGHT >= own->raw_y.f.u)
        return;
    self->routine = 6; // Plat_Delete
}

static void Plat_RiseOnSwitch(Sst *self, Own_BasicPlatform *own) {
    if (own->delay != 0) {
        if (--own->delay == 0)
            self->subtype++; // advance to Rising
        return;
    }
    if (f_switch[self->subtype >> 4])
        own->delay = 1 * 60;
}

static void Plat_Rising(Sst *self, Own_BasicPlatform *own) {
    own->raw_y.f.u -= 2;
    if (own->orig_y - 0x200 != own->raw_y.f.u)
        return;
    self->subtype = 0; // stationary
}

static void Plat_DownUp_LargeGHZ2(Sst *self, Own_BasicPlatform *own) {
    int8_t delta = (int8_t)(self->angle - 0x40);
    own->raw_y.f.u = (int16_t)(own->orig_y + (delta >> 1)); // asr.w #1 -- half range
    Plat_ChangeMotion(self);
}

// Executes movement behavior for the platform's own subtype (low nibble)
static void Plat_Move(Sst *self, Own_BasicPlatform *own) {
    switch (self->subtype & 0xF) {
    case 0x0: // stationary
    case 0x9:
        break;
    case 0x1: // right-left
        Plat_MoveHorizontal(self, own, (int8_t)(self->angle - 0x40));
        break;
    case 0x2: // down-up
        Plat_MoveVertical(self, own, (int8_t)(self->angle - 0x40));
        break;
    case 0x3:
        Plat_FallAfterStand(self, own);
        break;
    case 0x4:
        Plat_FallingDown(self, own);
        break;
    case 0x5: // left-right
        Plat_MoveHorizontal(self, own, (int8_t)(0x40 - self->angle));
        break;
    case 0x6: // up-down
        Plat_MoveVertical(self, own, (int8_t)(0x40 - self->angle));
        break;
    case 0x7:
        Plat_RiseOnSwitch(self, own);
        break;
    case 0x8:
        Plat_Rising(self, own);
        break;
    case 0xA:
        Plat_DownUp_LargeGHZ2(self, own);
        break;
    case 0xB: { // down-up, slow (frequency 2, middle $30 -- v_oscillate+$E/state[3])
        int8_t delta = (int8_t)((uint8_t)(oscillatory.state[3][0] >> 8) - 0x30);
        own->raw_y.f.u = (int16_t)(own->orig_y + delta);
        break;
    }
    case 0xC: { // up-down, slow
        int8_t delta = (int8_t)(0x30 - (uint8_t)(oscillatory.state[3][0] >> 8));
        own->raw_y.f.u = (int16_t)(own->orig_y + delta);
        break;
    }
    }
}

static void Plat_ChkDel(Sst *self, Own_BasicPlatform *own) {
    SstOnScreenTest2(self, own->orig_x);
}

void Obj_BasicPlatform(Sst *self) {
    Own_BasicPlatform *own = (Own_BasicPlatform *)self->own.u8;

    switch (self->routine) {
    case 0: // Main
        self->routine += 2;

        self->art_tile = TILE_MAP(0, 2, 0, 0, ArtTile_Level); // ArtTile_Level | Tile_Pal3 (real hardware's palette lines are 1-indexed -- Tile_Pal3 = 0-indexed line 2)
        self->mappings = Mappings_GHZPlatforms;
        self->width_pixels = 64 / 2;

        if (LEVEL_ZONE(level_id) == ZoneId_SYZ) {
            self->mappings = Mappings_SYZPlatforms;
        } else if (LEVEL_ZONE(level_id) == ZoneId_SLZ) {
            self->mappings = Mappings_SLZPlatforms;
            self->subtype = 3; // SLZ always forces Plat_FallAfterStand, regardless of level data
        }

        self->render_flags = SST_RENDER_LEVEL;
        self->priority = 4 * 0x80;
        own->raw_y.f.u = self->y_pos.f.u;
        own->orig_y = self->y_pos.f.u;
        own->orig_x = self->x_pos.f.u;
        self->angle = 0x80; // begin oscillating movement from the center

        self->mapping_frame = 0;
        if (self->subtype == 0xA) {
            self->mapping_frame = 1; // large vertical GHZ2 platform
            self->width_pixels = 64 / 2;
        }
        // Fallthrough
    case 2: // Solid
        if (own->nudge == 0) {
            // reduce nudging while Sonic isn't on the platform
        } else {
            own->nudge -= 4;
        }
        SstPlatformObject(self, self->width_pixels);
        // Fallthrough
    case 8: // Action
        Plat_Move(self, own);
        Plat_Nudge(self, own);
        Plat_ChkDel(self, own);
        break;
    case 4: { // StoodOn
        if (own->nudge != 0x40)
            own->nudge += 4;

        // ExitPlatform (shared helper, see its own definition in Object.c)
        // sets self->routine back to 2 and clears both objects' stood-on
        // flags once Sonic steps/jumps off -- but the rest of THIS frame
        // keeps running regardless (matches the real disasm exactly: no
        // branch on ExitPlatform's own result here), so the platform still
        // moves and repositions Sonic one final time even on the exact
        // frame he leaves it.
        int16_t prev_x = self->x_pos.f.u;
        SstExitPlatform(self, self->width_pixels, self->width_pixels, NULL);

        Plat_Move(self, own);
        Plat_Nudge(self, own);
        SstMvSonicOnPtfm(self, self->y_pos.f.u - 8, prev_x);

        Plat_ChkDel(self, own);
        break;
    }
    case 6: // Delete
        Sst_Delete(self);
        break;
    }
}
