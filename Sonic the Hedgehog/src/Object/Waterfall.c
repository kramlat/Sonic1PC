#include "Waterfall.h"
#include "Constants.h"
#include "Object.h"

#include "Game.h"
#include "Level.h"
#include "LevelScroll.h"
#include "Sound.h"
#include "SstLoad.h"

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
void Obj_Waterfall(Sst *self) {
    switch (self->routine) {
        case 0: // WSnd_Main
            self->routine = 2;
            self->render_flags = SST_RENDER_LEVEL;
            break;

        case 2: { // WSnd_PlaySnd
            if (debug_cheat) {
                self->mappings = Mappings_WaterfallMarker;
                self->art_tile = TILE_MAP(1, 0, 0, 0, ArtTile_Monitor);
                self->mapping_frame = 0;
                Sst_Draw(self);
            }

            // Only play the waterfall SFX every 64 frames -- scratch.u8[0]
            // is a free-running countdown local to this object.
            if (self->own.u8[0]-- == 0) {
                self->own.u8[0] = 0x3F;
                QueueSound2(sfx_Waterfall);
            }

            // out_of_range check: matches the real disasm's out_of_range macro -- both positions rounded down
            // to the nearest $80, deleted (and its place in the respawn table given back) once further than
            // 128+320+192 apart.
            SstDeleteIfNotInRange(self);
            break;
        }
    }
}
