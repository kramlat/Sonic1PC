#include "GHZTunnel.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"
#include "Constants.h"

#include "Game.h"
#include "Level.h"
#include "LevelScroll.h"
#include "Object/Sonic.h"

#define PM_BIT_SIZE_MASK         0x03
#define PM_BIT_HORIZONTAL        0x04
#define PM_BIT_PATH2_FORWARD     0x08 // entrance
#define PM_BIT_PATH2_BACKWARD    0x10 // exit
#define PM_BIT_GROUNDONLY        0x80

static const int16_t PM_Sizes[4] = { 0x20, 0x40, 0x80, 0x100 };

// Mappings_PathSwapper is owned by PathSwapper.c (Object/PathSwapper.c
// directly #includes the actual array definition there) -- a bare extern
// here avoids a second definition and a link failure, same reasoning as
// LavaBall.c's own comment on Mappings_Fireballs.
extern const uint8_t Mappings_PathSwapper[];

// Applies the pinball-mode action for one crossing direction. The
// render.f.x_flip bit (repurposed the same way PathSwapper reuses its own
// obRender X-flip bit) acts as a "no path change" guard: if set, the
// crossing does nothing to must_roll.
static void PM_Trigger(Sst *self, Own_GHZTunnel *own, bool forward) {
    if (self->subtype & PM_BIT_GROUNDONLY) {
        if (((player->status & SST_PSTATUS_IN_AIR) != 0))
            return;
    }

    if (!((self->render_flags & SST_RENDER_X_FLIP) != 0)) {
        bool target_path2 = forward ? (self->subtype & PM_BIT_PATH2_FORWARD) != 0
        : (self->subtype & PM_BIT_PATH2_BACKWARD) != 0;
        SST_STATUS_SET(player, SST_PSTATUS_MUST_ROLL, target_path2 ? 1 : 0);

        // Force the roll on entrance. must_roll alone only prevents the
        // unroll in Sonic_RollSpeed -- it doesn't put Sonic into a ball in
        // the first place, so a Sonic walking into the trigger would never
        // actually start rolling without this.
        if (target_path2)
            Sonic_ChkRoll(player);
    }
}

// x-crossing: forward = crossed left-to-right, backward = right-to-left.
// Perpendicular-axis check ensures Sonic is within the trigger's vertical
// extent.
static void PM_MainX(Sst *self) {
    Own_GHZTunnel *own = (Own_GHZTunnel *)self->own.u8;
    if (debug_use)
        return;

    int16_t obj_x = self->x_pos.f.u;
    int16_t player_x = player->x_pos.f.u;
    bool forward = !own->passed;

    if (forward) {
        if (obj_x > player_x)
            return;
        own->passed = true;
    } else {
        if (obj_x <= player_x)
            return;
        own->passed = false;
    }

    int16_t top = self->y_pos.f.u - own->size;
    int16_t bottom = self->y_pos.f.u + own->size;
    int16_t player_y = player->y_pos.f.u;
    if (player_y < top || player_y >= bottom)
        return;

    PM_Trigger(self, own, forward);
}

// y-crossing: forward = crossed top-to-bottom, backward = bottom-to-top.
// Used for vertical drop tunnels (Casino Night / Metropolis / Wing Fortress
// style), where Sonic falls through a horizontal trigger line rather than
// walking through a vertical one.
static void PM_MainY(Sst *self) {
    Own_GHZTunnel *own = (Own_GHZTunnel *)self->own.u8;
    if (debug_use)
        return;

    int16_t obj_y = self->y_pos.f.u;
    int16_t player_y = player->y_pos.f.u;
    bool forward = !own->passed;

    if (forward) {
        if (obj_y > player_y)
            return;
        own->passed = true;
    } else {
        if (obj_y <= player_y)
            return;
        own->passed = false;
    }

    int16_t left = self->x_pos.f.u - own->size;
    int16_t right = self->x_pos.f.u + own->size;
    int16_t player_x = player->x_pos.f.u;
    if (player_x < left || player_x >= right)
        return;

    PM_Trigger(self, own, forward);
}

// Object 10 - GHZ/HTZ-style tunnel roll trigger. Repurposed for Sonic 2's
// "pinball mode" (Object 84 in s2disasm), using the same simple single-axis
// trigger style HTZ uses rather than CNZ's full multi-directional/2-player
// tripwire.
//
// Subtype is a bit-packed layout matching PathSwapper's own:
//
//   bits 0-1: size (PM_Sizes[])
//   bit 2:    horizontal -- set = y-crossing (horizontal trigger line,
//             used for vertical drop tunnels); clear = x-crossing (vertical
//             trigger line, used for horizontal tunnels)
//   bit 3:    forward crossing sets must_roll (typically the entrance)
//   bit 4:    backward crossing sets must_roll (typically the exit)
//   bits 5-6: reserved (unused -- pinball mode doesn't switch priority)
//   bit 7:    ground-only -- trigger only fires if Sonic is grounded
//
// The forward/backward path bits select which crossing direction forces the
// roll. A typical tunnel has one trigger with PM_BIT_PATH2_FORWARD (the
// entrance) and another with PM_BIT_PATH2_BACKWARD (the exit), so entering
// forces the roll and leaving releases it. Sonic unrolls when his speed
// hits 0 (Sonic_RollSpeed), same as real hardware.
//
// Debug-only preview: no real-hardware DebugPathSwappers precedent of its
// own, but reuses the exact same 4-ring marker (Mappings_PathSwapper,
// ArtTile_Ring) while debug_cheat is active, at a fixed vertical/medium
// size -- subtype bits here mean orientation/size/path, not a simple
// entrance/exit flag, so the preview is just one constant frame.

void Obj_GHZTunnel(Sst *self) {
    Own_GHZTunnel *own = (Own_GHZTunnel *)self->own.u8;

    switch (self->routine) {
        case 0: { // PM_Init
            own->size = PM_Sizes[self->subtype & PM_BIT_SIZE_MASK];

            // Debug-only preview (see file header) -- harmless to set up
            // unconditionally even when debug_cheat is off, since the object
            // just never gets displayed in that case.
            self->mappings = Mappings_PathSwapper;
            self->art_tile = TILE_MAP(0, 1, 0, 0, ArtTile_Ring); // | Tile_Pal2
            self->render_flags |= SST_RENDER_LEVEL;
            self->width_pixels = 32 / 2;
            self->priority = 5 * 0x80;
            self->mapping_frame = self->subtype & (PM_BIT_SIZE_MASK | PM_BIT_HORIZONTAL);

            if (self->subtype & PM_BIT_HORIZONTAL) {
                self->routine = 4; // PM_MainY
                // If Sonic spawned already below the trigger line, treat it as
                // already-crossed so the first real trigger is the reverse one.
                if (self->y_pos.f.u < player->y_pos.f.u)
                    own->passed = true;
            } else {
                self->routine = 2; // PM_MainX
                if (self->x_pos.f.u < player->x_pos.f.u)
                    own->passed = true;
            }
            break;
        }
        case 2:
            PM_MainX(self);
            break;
        case 4:
            PM_MainY(self);
            break;
    }

    if (debug_cheat) {
        // Stay visible and tracked like any normal object while the debug
        // cheat is active (matches real hardware's own DebugPathSwappers).
        SstOnScreenTest(self);
        return;
    }

    // Otherwise: purely invisible/logic-only. Matches RememberState's own
    // off-screen handling (respawn-flag clear + delete) but never displays.
    if (SstOutOfRange(self->x_pos.f.u)) {
        if (self->respawn)
            (*SstLoad_State(self)) &= 0x7F;
        SstLoad_Forget(self);
        Sst_Delete(self);
    }
}
