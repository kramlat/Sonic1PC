#include "PathSwapper.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"

#include "Game.h"
#include "Level.h"
#include "LevelCollision.h"
#include "LevelScroll.h"
#include "Macros.h"

#include "Resource/Mappings/PathSwapper.h"
#include "Sound.h"

// Object 03 - Path Swapper / Collision Switcher. See PathSwapper.h and
// s1disasm ProjectSonic1TwoEight's "_incObj/03 Collision Switcher.asm" (the
// real source this was ported from) for the full picture. Two orientations
// (vertical trigger line checked via X-crossing = MainX, horizontal trigger
// line checked via Y-crossing = MainY), each with a forward and a backward
// crossing, each independently choosing a target collision path and Sonic's
// sprite priority. Purely invisible/logic-only in normal play -- except
// when DebugPathSwappers (real hardware's own Two-Eight-branch feature,
// unconditionally on here) is active: while debug_cheat is set, it stays
// visible (4 ring pieces, arranged per its own subtype -- vertical or
// horizontal, one of 3 sizes) and plays the lamppost SFX on every crossing,
// instead of instantly deleting itself.

#define PSWAP_BIT_SIZE_MASK      0x03
#define PSWAP_BIT_HORIZONTAL     0x04
#define PSWAP_BIT_PATH2_FORWARD  0x08 // rightdown
#define PSWAP_BIT_PATH2_BACKWARD 0x10 // leftup
#define PSWAP_BIT_PRIORITY_FORWARD  0x20 // rightdown
#define PSWAP_BIT_PRIORITY_BACKWARD 0x40 // leftup
#define PSWAP_BIT_GROUNDONLY     0x80

static const int16_t PSwapper_Sizes[4] = { 0x20, 0x40, 0x80, 0x100 };

// Applies the path/priority switch for one crossing direction. "priority
// only" (render.f.x_flip, repurposed the same way the real object reuses
// its obRender X-flip bit) skips the collision-path change but still
// updates Sonic's draw priority.
static void PSwapper_Trigger(Sst *self, Own_PathSwapper *own, bool forward) {
    if (self->subtype & PSWAP_BIT_GROUNDONLY) {
        if (((player->status & SST_PSTATUS_IN_AIR) != 0))
            return;
    }

    if (!((self->render_flags & SST_RENDER_X_FLIP) != 0)) {
        bool target_path2 = forward ? (self->subtype & PSWAP_BIT_PATH2_FORWARD) != 0
                                     : (self->subtype & PSWAP_BIT_PATH2_BACKWARD) != 0;
        collision_path = target_path2 ? 1 : 0;
    }

    bool priority = forward ? (self->subtype & PSWAP_BIT_PRIORITY_FORWARD) != 0
                             : (self->subtype & PSWAP_BIT_PRIORITY_BACKWARD) != 0;
    player->art_tile &= ~0x8000;
    if (priority)
        player->art_tile |= 0x8000;

    if (debug_cheat)
        PlaySound(sfx_Lamppost);
}

static void PSwapper_MainX(Sst *self) {
    Own_PathSwapper *own = (Own_PathSwapper *)self->own.u8;
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

    PSwapper_Trigger(self, own, forward);
}

static void PSwapper_MainY(Sst *self) {
    Own_PathSwapper *own = (Own_PathSwapper *)self->own.u8;
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

    PSwapper_Trigger(self, own, forward);
}

void Obj_PathSwapper(Sst *self) {
    Own_PathSwapper *own = (Own_PathSwapper *)self->own.u8;

    switch (self->routine) {
    case 0: { // PSwapper_Init
        own->size = PSwapper_Sizes[self->subtype & PSWAP_BIT_SIZE_MASK];

        // Debug-only preview (see file header) -- harmless to set up
        // unconditionally even when debug_cheat is off, since the object
        // just never gets displayed in that case.
        self->mappings = Mappings_PathSwapper;
        self->art_tile = TILE_MAP(0, 1, 0, 0, ArtTile_Ring); // | Tile_Pal2
        self->render_flags |= SST_RENDER_LEVEL;
        self->width_pixels = 32 / 2;
        self->priority = 5 * 0x80;
        self->mapping_frame = self->subtype & (PSWAP_BIT_SIZE_MASK | PSWAP_BIT_HORIZONTAL);

        if (self->subtype & PSWAP_BIT_HORIZONTAL) {
            self->routine = 4; // PSwapper_MainY
            // If Sonic spawned already below the trigger line, treat it as
            // already-crossed so the first real trigger is the reverse one.
            if (self->y_pos.f.u < player->y_pos.f.u)
                own->passed = true;
        } else {
            self->routine = 2; // PSwapper_MainX
            if (self->x_pos.f.u < player->x_pos.f.u)
                own->passed = true;
        }
        break;
    }
    case 2:
        PSwapper_MainX(self);
        break;
    case 4:
        PSwapper_MainY(self);
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
