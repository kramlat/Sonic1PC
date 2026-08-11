#include "LZWaterFeatures.h"

#include "Level.h"
#include "LevelScroll.h"
#include "Video.h"
#include "Sound.h"

#include "Backend/VDP.h"

// ---------------------------------------------------------------------------
// Dynamic water height -- per-act hardcoded water height scripting, ported
// from s1disasm's _inc/LZWaterFeatures.asm (DynWater_LZ1/LZ2/LZ3/SBZ3).
// wtr_pos3 is the target height; wtr_pos2 (the real, un-swayed height) moves
// 1px/frame toward it every call, done by LZDynamicWater's tail end below.
// wtr_routine tracks each act's own hardcoded phase progression.
// ---------------------------------------------------------------------------

static void DynWater_LZ1(void) {
    int16_t x = scrpos_x.f.u;
    int16_t target;

    if (wtr_routine == 0) {
        target = 0xB8;

        if (x < 0x600) goto setTarget;
        target = 0x108;

        if (player->pos.l.y.f.u < 0x200) {
            // Secret top path
            if (x < 0xC80) goto setTarget;
            target = 0xE8;

            if (x < 0x1500) goto setTarget;
            target = 0x108;
            goto setTarget;
        }

        if (x < 0xC00) goto setTarget;
        target = 0x318;

        if (x < 0x1080) goto setTarget;
        f_switch[5] |= 0x80;
        target = 0x5C8;

        if (x < 0x1380) goto setTarget;
        target = 0x3A8;

        if (target == wtr_pos2)
            wtr_routine = 1;
    } else if (wtr_routine == 1) {
        if (player->pos.l.y.f.u >= 0x2E0)
            return;
        target = 0x3A8;

        if (x < 0x1300) goto setTarget;
        target = 0x108;
        wtr_routine = 2;
    } else {
        return;
    }

setTarget:
    wtr_pos3 = target;
}

static void DynWater_LZ2(void) {
    int16_t x = scrpos_x.f.u;
    int16_t target = 0x328;

    if (x >= 0x500)
        target = 0x3C8;
    if (x >= 0xB00)
        target = 0x428;

    wtr_pos3 = target;
}

static void DynWater_LZ3(void) {
    int16_t x = scrpos_x.f.u;
    int16_t target;

    if (wtr_routine == 0) {
        target = 0x900; // Below 0x800 -- water doesn't appear at all

        if (x >= 0x600 && player->pos.l.y.f.u >= 0x3C0 && player->pos.l.y.f.u < 0x600) {
            target = 0x4C8;
            // Opens a wall: row 5, FG columns 12-13 -> chunks F8, F9.
            LEVEL_LAYOUT_FG(5)[12] = 0xF8;
            LEVEL_LAYOUT_FG(5)[13] = 0xF9;
            wtr_routine = 1;
            PlaySound(sfx_Rumbling);
        }

        wtr_pos3 = target;
        wtr_pos2 = target; // Changes instantly, not the usual 1px/frame
        return;
    } else if (wtr_routine == 1) {
        target = 0x4C8;

        if (x >= 0x770)
            target = 0x308;

        // "End area" (shallow water for the lamppost) triggers once past
        // x=0x1400 if the target's already been set to it, or Sonic is at
        // the end via the underwater bottom path (y>=0x600), or Sonic is
        // NOT on the waterslide/top path (y<0x280).
        if (x >= 0x1400 && (wtr_pos3 == 0x508 || player->pos.l.y.f.u >= 0x600 || player->pos.l.y.f.u < 0x280)) {
            target = 0x508;
            wtr_pos2 = target;
            if (x >= 0x1770)
                wtr_routine = 2;
        }

        wtr_pos3 = target;
    } else if (wtr_routine == 2) {
        target = 0x508;

        if (x >= 0x1860)
            target = 0x188;

        if (x >= 0x1AF0 || target == wtr_pos2)
            wtr_routine = 3;

        wtr_pos3 = target;
    } else if (wtr_routine == 3) {
        target = 0x188;

        if (x >= 0x1AF0) {
            target = 0x900;

            if (x >= 0x1BC0) {
                wtr_routine = 4;
                wtr_pos3 = 0x608;
                wtr_pos2 = 0x7C0; // Force a starting point to speed up rising
                f_switch[8] |= 1;
                return;
            }
        }

        wtr_pos3 = target;
        wtr_pos2 = target;
    } else if (wtr_routine == 4) {
        if (x >= 0x1E00)
            wtr_pos3 = 0x128;
    }
}

static void DynWater_SBZ3(void) {
    int16_t target = 0x228;
    if (scrpos_x.f.u >= 0xF00)
        target = 0x4C8;
    wtr_pos3 = target;
}

// Underwater screen ripple (REV01): per-scanline horizontal-scroll offsets
// for FG/BG once underwater, using Lz_Scroll_Data (FG ripple) and
// Drown_WobbleData (BG ripple, shared with the drowning bubbles' own wobble)
// as lookup tables, animated by lz_deform incrementing every frame. This
// used to live in Deform_LZ (LevelScroll.c), called via the conditionally
// gated DeformLayers() -- moved here since LZWaterFeatures() runs
// unconditionally every frame in LZ, which the ripple needs to actually
// animate continuously instead of only updating around pause transitions.
static void LZWaterRipple(void) {
    // Real ASM reads the WORD's high byte here (big-endian byte access),
    // then adds $80 to the whole word -- so the value used for the lookup
    // below only changes every other frame (the low byte absorbs the +$80,
    // carrying into the high byte every 2 additions). Using the low byte
    // directly (an earlier version of this code did) animates twice as
    // fast as intended.
    uint8_t d2 = (uint8_t)(lz_deform >> 8);
    uint8_t d3 = d2;
    lz_deform += 0x80;

    d2 += (uint8_t)bg_scrpos_y.f.u;
    d3 += (uint8_t)scrpos_y.f.u;

    int16_t fg_x_base = -scrpos_x.f.u;
    int16_t bg_x_base = -bg_scrpos_x.f.u;
    int16_t *bufp = &hscroll_buffer[0][0];

    for (int i = 0; i < SCREEN_HEIGHT; i++) {
        if ((scrpos_y.f.u + i) >= (uint16_t)wtr_pos1) {
            *bufp++ = fg_x_base + (int16_t)Lz_Scroll_Data[d3];
            *bufp++ = bg_x_base + (int16_t)Drown_WobbleData[d2];
        } else {
            *bufp++ = fg_x_base; *bufp++ = bg_x_base;
        }
        d2++; d3++;
    }
}

static void LZDynamicWater(void) {
    switch (LEVEL_ACT(level_id)) {
    case 0: DynWater_LZ1(); break;
    case 1: DynWater_LZ2(); break;
    case 2: DynWater_LZ3(); break;
    case 3: DynWater_SBZ3(); break;
    }

    // Move actual water height 1px/frame toward the target set above.
    if (wtr_pos3 > wtr_pos2)
        wtr_pos2++;
    else if (wtr_pos3 < wtr_pos2)
        wtr_pos2--;
}

// Matches LZWaterFeatures in the disassembly.
void LZWaterFeatures(void) {
    if (LEVEL_ZONE(level_id) != ZoneId_LZ)
        return;

    if (!nobgscroll && player->routine < 6) {
        // TODO: LZWindTunnels(), LZWaterSlides() -- Sonic movement inside
        // wind tunnels and water slide chunks aren't ported yet.
        LZDynamicWater();
    }

    // BG scroll position update + ripple/flat-scroll fill (was Deform_LZ,
    // called via the conditionally gated DeformLayers() before -- moved
    // here in full since it needs to run reliably every frame, not just
    // when "debug_use || player->routine < 6" happens to be true).
#ifdef SCP_REV00
    BGScroll_Block1(scrshift_x << 7, scrshift_y << 7);
    vid_bg_scrpos_y_dup = bg_scrpos_y.f.u;

    // No ripple effect in REV00 -- just a flat scroll.
    {
        int16_t fg_x = -scrpos_x.f.u;
        int16_t bg_x = -bg_scrpos_x.f.u;
        int16_t *bufp = &hscroll_buffer[0][0];
        for (int i = 0; i < SCREEN_HEIGHT; i++) { *bufp++ = fg_x; *bufp++ = bg_x; }
    }
#else
    BGScroll_XY(scrshift_x << 7, scrshift_y << 7);
    vid_bg_scrpos_y_dup = bg_scrpos_y.f.u;

    LZWaterRipple();
#endif

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
