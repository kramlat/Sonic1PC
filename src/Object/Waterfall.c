#include "Waterfall.h"

#include "Game.h"
#include "Level.h"
#include "LevelScroll.h"
#include "Sound.h"

#include "Resource/Mappings/WaterfallMarker.h"

// Invisible waterfall sound effect trigger (GHZ). No sprite -- it just
// plays the SFX periodically while in range, and deletes itself once the
// camera scrolls far enough away.
//
// Debug-only marker: no real hardware precedent for this specific object
// (it's genuinely invisible even in real hardware's own debug mode), but
// following the same convention as InvisibleBarrier/LavaTag's own markers
// -- a tight 2x2 icon cluster, goggles instead of Eggman (swimming/water
// association) so it doesn't get confused with those.
void Obj_Waterfall(Object *obj) {
    switch (obj->routine) {
        case 0: // WSnd_Main
            obj->routine = 2;
            obj->render.f.align_fg = true;
            break;

        case 2: { // WSnd_PlaySnd
            if (debug_cheat) {
                obj->mappings = Mappings_WaterfallMarker;
                obj->tile = TILE_MAP(1, 0, 0, 0, ArtTile_Monitor);
                obj->frame = 0;
                DisplaySprite(obj);
            }

            // Only play the waterfall SFX every 64 frames -- scratch.u8[0]
            // is a free-running countdown local to this object.
            if (obj->scratch.u8[0]-- == 0) {
                obj->scratch.u8[0] = 0x3F;
                QueueSound2(sfx_Waterfall);
            }

            // out_of_range check: matches the real disasm's out_of_range
            // macro -- both positions rounded down to the nearest $80,
            // deleted once further than 128+320+192 apart.
            uint16_t obj_pos = (uint16_t)obj->pos.l.x.f.u & 0xFF80;
            uint16_t cam_pos = (uint16_t)(scrpos_x.f.u - 128) & 0xFF80;
            if ((uint16_t)(obj_pos - cam_pos) > (128 + 320 + 192))
                ObjectDelete(obj);
            break;
        }
    }
}
