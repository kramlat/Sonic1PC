#include "LZWaterFeatures.h"
#include "Viewport.h"

#include "Backend/Joypad.h"
#include "Game.h"
#include "Slide.h"
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
        f_switch[5] = 0x80; // real ASM overwrites the byte (forces the tunnel door closed, see FBlock_LZSmallDoor_Close)
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
            QueueSound2(sfx_Rumbling);
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
        target = 0x508; // shallow for the lamppost

        // Only once the camera reaches the first cork does the water rise
        // (so the corks lift as platforms), and routine 3 waits until it has
        // actually reached $188 -- or the camera is past the rest room.
        // Checking "target == wtr_pos2" outside this block advanced on the
        // very first frame (routine 1 had just snapped the water to $508),
        // and routine 3 then snapped it straight to $188 at the lamppost.
        if (x >= 0x1860) {
            target = 0x188;
            if (x >= 0x1AF0 || target == wtr_pos2)
                wtr_routine = 3;
        }

        wtr_pos3 = target;
    } else if (wtr_routine == 3) {
        target = 0x188;

        if (x >= 0x1AF0) {
            target = 0x900;

            if (x >= 0x1BC0) {
                wtr_routine = 4;
                wtr_pos3 = 0x608;
                wtr_pos2 = 0x7C0; // Force a starting point to speed up rising
                f_switch[8] = 1; // opens a hidden door/wall (its switch was probably cut in development)
                return;
            }
        }

        wtr_pos3 = target;
        wtr_pos2 = target;
    } else if (wtr_routine == 4) {
        // Boss shaft: once the camera is in the shaft, the water rises slowly
        // towards the top of the level behind Eggman.
#ifdef SCP_FIX_BUGS
        // Checking the shaft's left side means hugging the left wall on the
        // way up can no longer skip the rising water entirely.
        if (x >= 0x1DA0)
#else
        if (x >= 0x1E00)
#endif
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
        // Port change: LZ1's first tunnel stays completely off until the
        // switch-3 door across it is fully open (see FloatingBlock.c).
        if (act == 0 && i == 0 && !f_lz1tunnel_open)
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

// The water slides: 128x128 foreground chunks (LZWaterSlides in the disassembly) and the speed each carries Sonic at
static const SlideSurface LZ_Slides[] = {
    { 0x05, 10 }, { 0x06, 10 }, { 0x09, 10 }, { 0x0A, 10 }, { 0xFA, -10 }, { 0xFB, -10 }, { 0xFC, -10 }, { 0xFD, -10 }, { 0x0B, 11 }, { 0x0C, 11 }, { 0x0D, 11 },
    { 0x0E, 11 }, { 0x15, -11 }, { 0x16, -11 }, { 0xF8, -11 }, { 0xF9, -11 }, { 0x19, -12 }, { 0x1A, -12 }, { 0x1B, -12 }, { 0x1C, -12 }, { 0x17, -11 },
};

// Matches LZWaterFeatures in the disassembly.
void LZWaterFeatures(void) {
    if (LEVEL_ZONE(level_id) != ZoneId_LZ)
        return;

    if (!nobgscroll && player->routine < 6) {
        LZWindTunnels();
        Slide_Update(LZ_Slides, sizeof(LZ_Slides) / sizeof(LZ_Slides[0]));
        LZDynamicWater();
    }

    // BG scroll position update + ripple/flat-scroll fill (was Deform_LZ,
    // called via the conditionally gated DeformLayers() before -- moved
    // here in full since it needs to run reliably every frame, not just
    // when "debug_use || player->routine < 6" happens to be true).
    BGScroll_XY(scrshift_x << 7, scrshift_y << 7);
    vid_bg_scrpos_y_dup = bg_scrpos_y.f.u;

    LZWaterRipple();

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
        line = SCREEN_HEIGHT - 1;
    } else if (line >= SCREEN_HEIGHT - 1) {
        line = SCREEN_HEIGHT - 1;
    }
    hbla_counter = line;
    screen1p.hint_counter = (int16_t)((uint8_t)hbla_counter);
}
