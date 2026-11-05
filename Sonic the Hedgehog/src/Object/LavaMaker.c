#include "LavaMaker.h"
#include "Object.h"
#include "ObjectSst.h"
#include "SstLoad.h"

#include "Game.h"
#include "LavaBall.h"
#include "Level.h"
#include "LevelScroll.h"
#include "Macros.h"

// Mappings_Fireballs is owned by GrassFire.c (see LavaBall.c's own comment
// on this) -- a bare extern here avoids a second definition and a link
// failure.
extern const uint8_t Mappings_Fireballs[];

// Object 13 - lava ball maker (MZ, SLZ). Invisible -- periodically spawns
// a LavaBall (object 14) at its own position, at a rate controlled by the
// subtype's upper nibble.
//
// Debug-only marker: no real hardware precedent, but shows the exact
// fireball it's about to spawn (same art, same orientation flip/anim as
// LavaBall.c's own init logic for this subtype) instead of an unrelated
// icon -- makes it obvious at a glance which of the 9 launch patterns a
// placed maker is using.

// Lava ball firing intervals (multiples of 30 frames), indexed by the
// subtype's own upper nibble. Real hardware's own table is only 6 entries
// long and doesn't range-check the nibble past that (relying on whatever
// ROM bytes happen to follow); padded out here to the full 16 possible
// nibble values instead of risking an out-of-bounds read, repeating the
// last real entry as a reasonable fallback.
static const uint8_t lavam_rates[16] = {
    30, 60, 90, 120, 150, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180,
};

// Peak height (pixels) each rise-and-fall subtype (0-3) actually reaches,
// computed by simulating LavaBall.c's own exact frame-by-frame physics
// (initial speed from lball_speeds, +0x18 gravity/frame, apex at the frame
// speed crosses zero) -- lets the debug marker sit where the real fireball
// would actually be at its highest, not just at the spawn point.
static const int16_t lavam_peak_height[4] = { 87, 136, 195, 265 };

void Obj_LavaMaker(Sst *self) {
    Own_LavaMaker *own = (Own_LavaMaker *)self->own.u8;

    switch (self->routine) {
    case 0: { // Main
        self->routine += 2;
        uint8_t subtype = self->subtype;
        own->fire_interval = lavam_rates[(subtype >> 4) & 0xF];
        self->anim_frame_timer = (int8_t)own->fire_interval;
        self->subtype = subtype & 0xF;
        __attribute__((fallthrough));
    }
    case 2: // MakeLava
        if (--self->anim_frame_timer == 0) {
            self->anim_frame_timer = (int8_t)own->fire_interval;

            if (SstChkObjectVisible(self)) {
                Sst *ball = Sst_SpawnCode(Obj_LavaBall, self->x_pos.f.u, self->y_pos.f.u);
                if (ball != NULL) {
                    ball->subtype = self->subtype; // subtype
                }
            }
        }
        break;
    }

    if (debug_cheat) {
        uint8_t subtype = self->subtype;

        // The marker moves to where the real fireball would actually be --
        // its own spawn position must stay untouched for the real spawn
        // logic above (which reads self->pos.l.x/y.f.u directly), so
        // save/restore around the temporary move.
        int16_t real_x = self->x_pos.f.u;
        int16_t real_y = self->y_pos.f.u;

        if (subtype <= 3) {
            self->y_pos.f.u = (int16_t)(real_y - lavam_peak_height[subtype]); // exact peak height
        } else if (subtype == 4) {
            self->y_pos.f.u = (int16_t)(real_y - SCREEN_HEIGHT); // straight up -- real distance depends on level geometry, so just go far
        } else if (subtype == 5) {
            self->y_pos.f.u = (int16_t)(real_y + SCREEN_HEIGHT); // straight down
        } else if (subtype == 6) {
            self->x_pos.f.u = (int16_t)(real_x - SCREEN_WIDTH); // left -- same "just go far" reasoning
        } else if (subtype == 7) {
            self->x_pos.f.u = (int16_t)(real_x + SCREEN_WIDTH); // right
        }
        // subtype 8 (do nothing) stays put at the spawn point

        // This is only ever a marker (see LavaMaker.h's own comment on
        // this) -- the real fireballs spawned above still get LavaBall.c's
        // own real collision as normal, this doesn't touch them.
        self->collision_flags = 0;
        self->mappings = Mappings_Fireballs;
        self->art_tile = TILE_MAP(0, 0, 0, 0,
            (LEVEL_ZONE(level_id) == ZoneId_SLZ) ? ArtTile_SLZ_Fireball : ArtTile_MZ_Fireball);
        self->render_flags = SST_RENDER_LEVEL;

        // Matches LavaBall.c's own Main-routine flip/anim setup exactly,
        // for each of the 9 launch patterns. A fixed frame, not
        // AnimateSprite -- that reads/writes self->frame_time, the exact
        // same field this object's own real spawn-interval countdown uses
        // (see Own_LavaMaker's own comment on this), so animating the
        // marker here would corrupt the real timer and make it fire far
        // more often than intended. One stationary marker at frame 0 is
        // enough to show orientation; the real spawns still animate
        // normally through LavaBall.c's own unrelated instances.
        SST_STATUS_SET(self, SST_STATUS_Y_FLIP, (subtype <= 4)); // rise-and-fall types start facing up; so does subtype 4 (straight up)
        SST_STATUS_SET(self, SST_STATUS_X_FLIP, (subtype == 6)); // left
        self->mapping_frame = 0;
        self->width_pixels = (subtype >= 6) ? 32 / 2 : 16 / 2;

        Sst_Draw(self);

        self->x_pos.f.u = real_x;
        self->y_pos.f.u = real_y;
        return;
    }

    SstDeleteIfNotInRange(self);
}
