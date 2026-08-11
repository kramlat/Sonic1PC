#include "LavaMaker.h"

#include "LavaBall.h"
#include "Level.h"
#include "LevelScroll.h"
#include "Macros.h"

// Object 13 - lava ball maker (MZ, SLZ). Invisible -- periodically spawns
// a LavaBall (object 14) at its own position, at a rate controlled by the
// subtype's upper nibble.

// Lava ball firing intervals (multiples of 30 frames), indexed by the
// subtype's own upper nibble. Real hardware's own table is only 6 entries
// long and doesn't range-check the nibble past that (relying on whatever
// ROM bytes happen to follow); padded out here to the full 16 possible
// nibble values instead of risking an out-of-bounds read, repeating the
// last real entry as a reasonable fallback.
static const uint8_t lavam_rates[16] = {
    30, 60, 90, 120, 150, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180, 180,
};

void Obj_LavaMaker(Object *obj) {
    Scratch_LavaMaker *scratch = (Scratch_LavaMaker *)&obj->scratch;

    switch (obj->routine) {
    case 0: { // Main
        obj->routine += 2;
        uint8_t subtype = obj->scratch.u8[0];
        scratch->fire_interval = lavam_rates[(subtype >> 4) & 0xF];
        obj->frame_time.b = (int8_t)scratch->fire_interval;
        obj->scratch.u8[0] = subtype & 0xF;
        __attribute__((fallthrough));
    }
    case 2: // MakeLava
        if (--obj->frame_time.b == 0) {
            obj->frame_time.b = (int8_t)scratch->fire_interval;

            if (ChkObjectVisible(obj)) {
                Object *ball = FindFreeObj();
                if (ball != NULL) {
                    ball->type = ObjId_LavaBall;
                    ball->pos.l.x.f.u = obj->pos.l.x.f.u;
                    ball->pos.l.y.f.u = obj->pos.l.y.f.u;
                    ball->scratch.u8[0] = obj->scratch.u8[0]; // subtype
                }
            }
        }
        break;
    }

    if (IS_OFFSCREEN(obj->pos.l.x.f.u))
        ObjectDelete(obj);
}
