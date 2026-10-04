// Water for Sonic 2 (Sonic 1's LZWaterFeatures.c made Nick Arcade's WaterEffects and DynamicWaterHeight): Hidden Palace's water. Its height is the real height (wtr_pos2) plus a small bob of the first oscillator
// (wtr_pos1); in the first act Tails' pad moves the target height (up and down, with limits) and the real height follows it a pixel a frame. The line where the water begins is where the underwater palette takes over
// (and the whole screen is underwater when the water is above its top). Everything under the water ripples (the effect of Sonic 1's Labyrinth, kept in every zone with water, for the games to come).
#include "LZWaterFeatures.h"

#include "Backend/Joypad.h"
#include "Game.h"
#include "Level.h"
#include "SplitScreen.h"
#include "LevelScroll.h"
#include "Object/Sonic.h"
#include "Video.h"

#include "Backend/VDP.h"

// Nick Arcade's DynWater_HPZ1: Tails' pad (the second one) moves the water
static void DynamicWaterHeight(void) {
    if (LEVEL_ZONE(level_id) == ZoneId_CPZ) {
        // DynamicWater_CPZ2: past x $1DE0 the water goes up to $510
        if (scrpos_x.f.u >= 0x1DE0)
            wtr_pos3 = 0x510;
    } else if (LEVEL_ZONE(level_id) == ZoneId_HPZ && LEVEL_ACT(level_id) == 0) { // (Neo Green Hill's water stays where it starts)
        uint8_t pad2 = Joypad_GetState2();
        if ((pad2 & JPAD_UP) && wtr_pos3 != 0)
            wtr_pos3--;
        if ((pad2 & JPAD_DOWN) && wtr_pos3 != 0x700)
            wtr_pos3++;
    }
    // (the other acts only have Sonic 1's Labyrinth scripts in Nick Arcade, which its zone does not use)

    if (wtr_pos3 > wtr_pos2)
        wtr_pos2++;
    else if (wtr_pos3 < wtr_pos2)
        wtr_pos2--;
}

// Underwater ripple: per-scanline horizontal offsets for the foreground and the background below the water's surface, from Sonic 1's tables, added to what the zone's own scrolling has set
void Water_Ripple(void) {
    uint8_t d2 = (uint8_t)(lz_deform >> 8);
    uint8_t d3 = d2;
    lz_deform += 0x80;

    d2 += (uint8_t)bg_scrpos_y.f.u;
    d3 += (uint8_t)scrpos_y.f.u;

    int16_t *bufp = &hscroll_buffer[0][0];
    for (int i = 0; i < SCREEN_HEIGHT; i++) {
        if ((uint16_t)(scrpos_y.f.u + i) >= (uint16_t)wtr_pos1) {
            bufp[0] += (int16_t)Lz_Scroll_Data[d3];
            bufp[1] += (int16_t)Drown_WobbleData[d2];
        }
        bufp += 2;
        d2++;
        d3++;
    }
}

// Matches WaterEffects in Nick Arcade (and runs where Sonic 1 ran LZWaterFeatures)
void LZWaterFeatures(void) {
    if (!Level_HasWater())
        return;

    if (!nobgscroll && player->routine < 6)
        DynamicWaterHeight();

    wtr_state = 0;
    uint8_t sway = (uint8_t)(oscillatory.state[0][0] >> 8);
    wtr_pos1 = (int16_t)(sway >> 1) + wtr_pos2;

    if (SplitScreen_Active()) {
        // A split screen has no H interrupt at all: each view is drawn with its own water line (SplitScreen_Scroll sets them against the cameras once they have moved)
        hbla_counter = SCREEN_HEIGHT - 1;
        VDP_SetHIntCounter((uint8_t)hbla_counter);
        return;
    }

    int16_t line = (int16_t)(wtr_pos1 - scrpos_y.f.u);
    if (line < 0) {
        wtr_state = 1; // the whole screen is underwater
        line = SCREEN_HEIGHT - 1;
    } else if (line >= SCREEN_HEIGHT - 1) {
        line = SCREEN_HEIGHT - 1;
    }
    hbla_counter = line;
    VDP_SetHIntCounter((uint8_t)hbla_counter);
}
