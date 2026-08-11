#include "Waterfall.h"

#include "LevelScroll.h"
#include "Sound.h"

// Invisible waterfall sound effect trigger (GHZ). No sprite -- it just
// plays the SFX periodically while in range, and deletes itself once the
// camera scrolls far enough away.
void Obj_Waterfall(Object *obj) {
    switch (obj->routine) {
        case 0: // WSnd_Main
            obj->routine = 2;
            obj->render.f.align_fg = true;
            break;

        case 2: { // WSnd_PlaySnd
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
