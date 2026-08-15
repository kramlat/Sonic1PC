#include "LZWaterFeatures.h"

#include "Backend/Joypad.h"
#include "Game.h"
#include "Level.h"
#include "LevelScroll.h"
#include "Object/Sonic.h"
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

// left, top, right, bottom boundaries per wind tunnel zone
static const int16_t LZWind_Data[5][4] = {
    { 0xA80, 0x300, 0xC10, 0x380 },  // LZ act 1, 1st zone
    { 0xF80, 0x100, 0x1410, 0x180 }, // LZ act 1, 2nd zone
    { 0x460, 0x400, 0x710, 0x480 },  // LZ act 2
    { 0xA20, 0x600, 0x1610, 0x6E0 }, // LZ act 3
    { 0xC80, 0x600, 0x13D0, 0x680 }, // LZ act 4 (reuses SBZ3's layout, see project notes)
};

// Matches LZWindTunnels in the disassembly.
static void LZWindTunnels(void) {
    if (debug_use)
        return;

    int act = LEVEL_ACT(level_id);
    int base_row = (act == 0) ? 0 : (1 + act);
    int count = (act == 0) ? 2 : 1;

    for (int i = 0; i < count; i++) {
        const int16_t *zone = LZWind_Data[base_row + i];
        int16_t left = zone[0], top = zone[1], right = zone[2], bottom = zone[3];

        if (player->pos.l.x.f.u < left || player->pos.l.x.f.u >= right)
            continue;
        if (player->pos.l.y.f.u < top || player->pos.l.y.f.u >= bottom)
            continue;

        // Inside a tunnel zone
        if (((uint8_t)frame_count & 0x3F) == 0)
            QueueSound2(sfx_Waterfall);

        if (f_wtunneldisallow)
            return;
        if (player->routine >= 4) {
            tunnel_mode = 0;
            return;
        }
        tunnel_mode = 1;

        // "Suction" pre-zone: nudge Sonic towards the tunnel mouth before
        // he's actually inside it.
        int16_t d0 = (int16_t)(player->pos.l.x.f.u - 128);
        if (d0 < left) {
            int16_t dy = (act == 1) ? -2 : 2; // LZ act 2 nudges the opposite way
            player->pos.l.y.f.u = (int16_t)(player->pos.l.y.f.u + dy);
        }

        player->pos.l.x.f.u = (int16_t)(player->pos.l.x.f.u + 4);
        player->xsp = 0x400;
        player->ysp = 0;
        player->anim = SonAnimId_Float2;
        player->status.p.f.in_air = true;
        player->status.p.f.roll_jump = false; // Bug fix (Knuckles in Sonic 2's own equivalent)

        if (jpad1_hold2 & JPAD_UP) {
            // Bug fix (also from Knuckles in Sonic 2): don't let Sonic rise
            // above the tunnel's own top boundary.
            if (player->pos.l.y.f.u > top)
                player->pos.l.y.f.u--;
        }
        if (jpad1_hold2 & JPAD_DOWN)
            player->pos.l.y.f.u++;

        return;
    }

    // Not inside any tunnel zone
    if (tunnel_mode)
        player->anim = SonAnimId_Walk;
    tunnel_mode = 0;
}

static const int8_t Slide_Speeds[21] = {
    10, 10, 10, 10, -10, -10, -10, -10, 11, 11, 11, 11, -11, -11, -11, -11, -12, -12, -12, -12, -11,
};
// 128x128 foreground chunk IDs that mark water-slide surfaces in LZ,
// positionally parallel with Slide_Speeds above.
static const uint8_t Slide_Chunks[21] = {
    0x05, 0x06, 0x09, 0x0A, 0xFA, 0xFB, 0xFC, 0xFD, 0x0B, 0x0C, 0x0D, 0x0E,
    0x15, 0x16, 0xF8, 0xF9, 0x19, 0x1A, 0x1B, 0x1C, 0x17,
};

// Matches LZWaterSlides in the disassembly.
static void LZWaterSlides(void) {
    Scratch_Sonic *scratch = (Scratch_Sonic *)&player->scratch;

    if (player->status.p.f.in_air) {
        if (f_slidemode) {
            scratch->control_lock = 5;
            f_slidemode = false;
        }
        return;
    }

    uint8_t chunk = LEVEL_LAYOUT_FG((player->pos.l.y.f.u >> 7) & 0xF)[(player->pos.l.x.f.u >> 7) & 0x7F];

    int match = -1;
    for (int i = 0; i < 21; i++) {
        if (Slide_Chunks[i] == chunk) {
            match = i;
            break;
        }
    }

    if (match < 0) {
        if (f_slidemode) {
            scratch->control_lock = 5;
            f_slidemode = false;
        }
        return;
    }

    int8_t speed = Slide_Speeds[match];
    player->status.p.f.x_flip = speed < 0;
    player->inertia = (int16_t)(speed << 8);
    player->anim = SonAnimId_WaterSlid;
    f_slidemode = true;

    if (((uint8_t)frame_count & 0x1F) == 0)
        QueueSound2(sfx_Waterfall);
}

// Matches LZWaterFeatures in the disassembly.
void LZWaterFeatures(void) {
    if (LEVEL_ZONE(level_id) != ZoneId_LZ)
        return;

    if (!nobgscroll && player->routine < 6) {
        LZWindTunnels();
        LZWaterSlides();
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
