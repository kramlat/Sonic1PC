#include "WaterSurface.h"

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
} Scratch_WaterSurface;

void Obj_WaterSurface(Object *obj) {
    Scratch_WaterSurface *scratch = (Scratch_WaterSurface*)&obj->scratch;

    if (obj->routine == 0) {
        obj->routine = 2;
        obj->mappings = Mappings_WaterSurface;
        obj->tile = TILE_MAP(1, 3, 0, 0, ArtTile_LZ_Water_Surface);
        obj->render.f.align_fg = true;
        obj->width_pixels = 256 / 2;
        scratch->orig_x = obj->pos.l.x.f.u;
    }

    // Wrap the camera X-position every 32px so the surface looks
    // camera-independent, then flicker between two adjacent 32px
    // positions every other frame to fake a continuously scrolling strip.
    int16_t x = (int16_t)(scrpos_x.f.u & 0xFFE0);
    x = (int16_t)(x + scratch->orig_x);
    if (frame_count & 1)
        x += 0x20;
    obj->pos.l.x.f.u = x;

    obj->pos.l.y.f.u = wtr_pos1;

    // If the game gets paused with the flickering surface on screen, swap
    // to an alternate frame that lacks the gaps so it looks continuous.
    if (scratch->freeze) {
        if (pause_state)
            goto display;
        scratch->freeze = 0;
        obj->frame -= 3;
    } else if (jpad1_press1 & JPAD_START) {
        obj->frame += 3;
        scratch->freeze = 1;
        goto display;
    }

    if (--obj->frame_time.b < 0) {
        obj->frame_time.b = 8 - 1;
        if (++obj->frame >= 3)
            obj->frame = 0;
    }

display:
    DisplaySprite(obj);
}
