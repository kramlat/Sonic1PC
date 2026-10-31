#include "WaterSurface.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"

#include "Level.h"
#include "LevelScroll.h"
#include "Game.h"

#include "Backend/Joypad.h"

#include "Resource/Mappings/WaterSurface.h"

typedef struct {
    uint8_t subtype;   // 0x28
    uint8_t pad0[7];   // 0x29-0x2F
    int16_t orig_x;    // surf_origX, 0x30 -- $60 for the left half, $120 for the right
    uint8_t freeze;    // surf_freeze, 0x32 -- swaps to a seamless frame while paused
} Own_WaterSurface;

void Obj_WaterSurface(Sst *self) {
    Own_WaterSurface *own = (Own_WaterSurface *)self->own.u8;

    if (self->routine == 0) {
        self->routine = 2;
        self->mappings = Mappings_WaterSurface;
        self->art_tile = TILE_MAP(1, 2, 0, 0, ArtTile_LZ_Water_Surface); // priority + Tile_Pal3 (palette line 2: the water's own colours)
        self->render_flags = SST_RENDER_LEVEL;
        self->width_pixels = 256 / 2;
        own->orig_x = self->x_pos.f.u;
    }

    // Wrap the camera X-position every 32px so the surface looks
    // camera-independent, then flicker between two adjacent 32px
    // positions every other frame to fake a continuously scrolling strip.
    int16_t x = (int16_t)(scrpos_x.f.u & 0xFFE0);
    x = (int16_t)(x + own->orig_x);
    if (frame_count & 1)
        x += 0x20;
    self->x_pos.f.u = x;

    self->y_pos.f.u = wtr_pos1;

    // If the game gets paused with the flickering surface on screen, swap
    // to an alternate frame that lacks the gaps so it looks continuous.
    if (own->freeze) {
        if (pause_state)
            goto display;
        own->freeze = 0;
        self->mapping_frame -= 3;
    } else if (jpad1_press1 & JPAD_START) {
        self->mapping_frame += 3;
        own->freeze = 1;
        goto display;
    }

    if (--self->anim_frame_timer < 0) {
        self->anim_frame_timer = 8 - 1;
        if (++self->mapping_frame >= 3)
            self->mapping_frame = 0;
    }

display:
    Sst_Draw(self);
}
