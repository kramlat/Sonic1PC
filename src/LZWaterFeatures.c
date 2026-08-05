#include "LZWaterFeatures.h"

#include "Level.h"
#include "LevelScroll.h"
#include "Video.h"

#include "Backend/VDP.h"

// Matches LZWaterFeatures in the disassembly.
void LZWaterFeatures(void) {
    if (LEVEL_ZONE(level_id) != ZoneId_LZ)
        return;

    if (!nobgscroll && player->routine < 6) {
        // TODO: LZWindTunnels(), LZWaterSlides(), LZDynamicWater() -- the
        // per-act scripted water heights, wind tunnels and water slides
        // aren't ported yet, so wtr_pos2 (the water's actual, un-swayed
        // height) just stays wherever GM_Level_Branch left it (0).
    }

    wtr_state = 0;

    // Surface sway: oscillator 0 (frequency 2, amplitude $10 -- see
    // OscillateNumDo) drives a small, continuous up/down bob layered on top
    // of the real water height, which is what makes the water's surface
    // line (and the underwater palette swap riding on it) visibly ripple
    // even when the water itself isn't otherwise rising or falling.
    uint8_t sway = (uint8_t)(oscillatory.state[0][0] >> 8);
    wtr_pos1 = (int16_t)(sway >> 1) + wtr_pos2;

    int16_t line = (int16_t)(wtr_pos1 - scrpos_y.f.u);
    if (line < 0) {
        // Water surface is above the top of the screen -- the whole visible
        // screen is underwater.
        wtr_state = 1;
        line = 223;
    } else if (line >= 223) {
        line = 223;
    }
    hbla_counter = line;
    VDP_SetHIntCounter((uint8_t)hbla_counter);
}
