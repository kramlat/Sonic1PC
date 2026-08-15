#include "PaletteCycle.h"

#include "Level.h"
#include "Palette.h"
#include "SpecialStage.h"
#include "Video.h"

// Palette cycle state
int16_t pcyc_num, pcyc_time;
uint16_t pcyc_buffer[0x18];

// LZ / SBZ3 (act 4 reuse) conveyor reversal flag -- set by the (not yet
// ported) conveyor belt object when Sonic reverses its direction by
// touching it from the correct side; defaults to always-forward until then.
bool f_conveyrev = false;

// Palette cycles
#include "Resource/Palette/GHZCycle.h"
#include "Resource/Palette/Sega1.h"
#include "Resource/Palette/Sega2.h"
#include "Resource/Palette/TitleCycle.h"
#include "Resource/Palette/LZCycWaterfall.h"
#include "Resource/Palette/LZCycConveyor.h"
#include "Resource/Palette/LZCycConveyorUW.h"
#include "Resource/Palette/SBZ3CycWaterfall.h"
#include "Resource/Palette/SLZCycle.h"
#include "Resource/Palette/SYZCycle1.h"
#include "Resource/Palette/SYZCycle2.h"
#include "Resource/Palette/SBZCyc1.h"
#include "Resource/Palette/SBZCyc2.h"
#include "Resource/Palette/SBZCyc3.h"
#include "Resource/Palette/SBZCyc4.h"
#include "Resource/Palette/SBZCyc5.h"
#include "Resource/Palette/SBZCyc6.h"
#include "Resource/Palette/SBZCyc7.h"
#include "Resource/Palette/SBZCyc8.h"
#include "Resource/Palette/SBZCyc9.h"
#include "Resource/Palette/SBZCyc10.h"
// real disasm: palette/Cycle - Special Stage 1.bin
#include "Resource/Palette/SSCyc1.h"
// real disasm: palette/Cycle - Special Stage 2.bin
#include "Resource/Palette/SSCyc2.h"

// Palette cycle routines
int32_t PCycle_Sega(void) {
    uint16_t* to;
    const uint8_t* from;
    int16_t pal_num, pal_len;

    if (!(pcyc_time & 0x00FF)) {
        // Get palette pointers to use
        to = &dry_palette[1][0];
        from = Palette_Sega1;

        // Get area of palette to copy, clipping at 0
        pal_len = 6;
        pal_num = pcyc_num;

        while (pal_num < 0) {
            from += 2;
            pal_len--;
            pal_num += 2;
        }

        // Write palette
        to += pal_num >> 1;
        while (pal_len-- > 0) {
            if (!((to - &dry_palette[1][0]) & 0xF))
                to++;
            if ((to - &dry_palette[0][0]) < 0x40) {
                *to++ = (from[0] << 8) | (from[1] << 0);
                from += 2;
            } else
                to++;
        }

        // Handle cycle timer
        if (!((pal_num = pcyc_num + 2) & 0x1E))
            pal_num += 2;

        if (pal_num >= 0x64) {
            pcyc_time = 0x0401;
            pal_num = -0xC;
        }

        pcyc_num = pal_num;

        return 1;
    } else {
        // Palette timer
        pcyc_time = (((uint8_t)(pcyc_time >> 8) - 1) << 8) | (pcyc_time & 0x00FF);
        if (!(pcyc_time & 0x8000))
            return 1;
        pcyc_time = 0x0400 | (pcyc_time & 0x00FF);

        // Get palette index
        if ((pal_num = (pcyc_num + 0xC)) >= 0x30)
            return 0;

        // Get palette to copy
        pcyc_num = pal_num;
        from = Palette_Sega2 + pal_num;

        // Copy border palette
        to = &dry_palette[0][2];
        for (size_t i = 0; i < 5; i++) {
            *to++ = (from[0] << 8) | (from[1] << 0);
            from += 2;
        }

        // Copy filled palette
        to = &dry_palette[1][0];
        for (size_t i = 0; i < (0x30 - 3); i++) {
            if (!((to - &dry_palette[1][0]) & 0xF))
                to++;
            *to++ = (from[0] << 8) | (from[1] << 0);
        }

        return 1;
    }
}

static void PCycle_Water(const uint8_t *palette) {
    // Wait for cycle timer
    if (--pcyc_time >= 0)
        return;

    // Increment cycle
    pcyc_time = 5;
    pcyc_num++;

    // Write palette
    uint16_t pal_num = pcyc_num & 3;
    const uint8_t* from = palette + (pal_num << 3);
    uint16_t* to = &dry_palette[2][8];
    for (size_t i = 0; i < 4; i++) {
        *to++ = (from[0] << 8) | (from[1] << 0);
        from += 2;
    }
}

void PCycle_Title(void) {
    PCycle_Water(Palette_TitleCycle);
}

static uint16_t PCyc_Color(const uint8_t *src) {
    return (uint16_t)((src[0] << 8) | src[1]);
}

typedef struct {
    uint8_t time;       // as stored in real ROM (time_arg-1 already baked in); when this reads negative, the real timer instead uses 0x1FF
    uint8_t pal_offset; // palette cycle offset; bit 7 selects the Pal_SSCyc2 set (PalCycle_SS_2), bit 0 (when set, PalCycle_SS_2 only) is the "extra palette line 4" flag
} SSPalEntry;

// Real hardware's own SS_Timing_Values table also carries a BG canvas mode
// + VRAM nametable address per entry here, driving the special stage's
// rotating-canvas 3D tunnel background (SS_BGLoad/SS_BGAnimate) -- omitted
// since this port has no equivalent background-canvas renderer yet; only
// the palette cycling itself (what "PalCycle_SS" literally means) is
// ported for now.
static const SSPalEntry ss_pal_timing[32] = {
    { 4 - 1, 0x92 }, { 4 - 1, 0x90 }, { 4 - 1, 0x8E }, { 4 - 1, 0x8C }, { 4 - 1, 0x8B },
    { 4 - 1, 0x80 }, { 4 - 1, 0x82 }, { 4 - 1, 0x84 }, { 4 - 1, 0x86 }, { 4 - 1, 0x88 },
    { 8 - 1, 0x00 }, { 8 - 1, 0x0C }, { 0 - 1, 0x18 }, { 0 - 1, 0x18 }, { 8 - 1, 0x0C }, { 8 - 1, 0x00 },
    { 4 - 1, 0x88 }, { 4 - 1, 0x86 }, { 4 - 1, 0x84 }, { 4 - 1, 0x82 }, { 4 - 1, 0x81 },
    { 4 - 1, 0x8A }, { 4 - 1, 0x8C }, { 4 - 1, 0x8E }, { 4 - 1, 0x90 }, { 4 - 1, 0x92 },
    { 8 - 1, 0x24 }, { 8 - 1, 0x30 }, { 0 - 1, 0x3C }, { 0 - 1, 0x3C }, { 8 - 1, 0x30 }, { 8 - 1, 0x24 },
};

void PCycle_SS(void) {
    if (pause_state)
        return;
    if (--palss_time >= 0)
        return;

    const SSPalEntry *entry = &ss_pal_timing[palss_num & 0x1F];
    palss_num++;

    int8_t time = (int8_t)entry->time;
    palss_time = (time < 0) ? (0x200 - 1) : time;

    uint8_t offset = entry->pal_offset;
    if (!(offset & 0x80)) {
        const uint8_t *src = Palette_SSCyc1 + offset;
        uint16_t *dst = &dry_palette[2][7];
        for (int i = 0; i < 6; i++) {
            *dst++ = PCyc_Color(src);
            src += 2;
        }
        return;
    }

    // PalCycle_SS_2 -- v_palss_index is always 0 on real hardware (dead
    // code path otherwise), so this reduces to just the >=$8A check.
    int16_t d1 = ((offset & 0x7F) < 0x0A) ? 0 : 1;
    const uint8_t *src = Palette_SSCyc2 + (d1 * 0x2A);

    offset &= 0x7F;
    bool extra_line4 = offset & 1;
    offset &= (uint8_t)~1;

    if (extra_line4) {
        uint16_t *dst = &dry_palette[3][7];
        for (int i = 0; i < 6; i++) {
            *dst++ = PCyc_Color(src);
            src += 2;
        }
    }

    src += 0xC;
    uint16_t *dst;
    if (offset >= 0xA) {
        offset -= 0xA;
        dst = &dry_palette[3][0xD];
    } else {
        dst = &dry_palette[2][0xD];
    }
    src += offset * 3;
    for (int i = 0; i < 3; i++) {
        *dst++ = PCyc_Color(src);
        src += 2;
    }
}

// Marble Zone doesn't have any palette cycles (anymore) -- superseded by
// its animated level graphics (see LevelDraw.c's AniArt_MZLava/Magma/Torch).
void PCycle_MZ(void) {
}

void PCycle_LZ(void) {
    // Waterfalls
    if (--pcyc_time < 0) {
        pcyc_time = 3 - 1;
        int16_t num = pcyc_num++;
        num &= 3;
        const uint8_t *src = (LEVEL_ACT(level_id) == 3 ? Palette_SBZ3CycWaterfall : Palette_LZCycWaterfall) + (num << 3);

        for (int i = 0; i < 4; i++) {
            uint16_t color = PCyc_Color(src);
            dry_palette[2][0xB + i] = color;
            wet_palette[2][0xB + i] = color;
            src += 2;
        }
    }

    // Conveyor belts
    static const uint8_t sequence[8] = { 1, 0, 0, 1, 0, 0, 1, 0 };
    if (!sequence[frame_count & 7])
        return;

    int16_t dir = f_conveyrev ? -1 : 1;
    int16_t off = (int16_t)(pcyc_buffer[0] & 3);
    off += dir;
    if ((uint16_t)off >= 3)
        off = (off < 0) ? 2 : 0;
    pcyc_buffer[0] = (uint16_t)off;

    int16_t byte_off = off * 6;
    const uint8_t *dry_src = Palette_LZCycConveyor + byte_off;
    dry_palette[3][0xB] = PCyc_Color(dry_src);
    dry_palette[3][0xC] = PCyc_Color(dry_src + 2);
    dry_palette[3][0xD] = PCyc_Color(dry_src + 4);

    const uint8_t *wet_src = Palette_LZCycConveyorUW + byte_off;
    wet_palette[3][0xB] = PCyc_Color(wet_src);
    wet_palette[3][0xC] = PCyc_Color(wet_src + 2);
    wet_palette[3][0xD] = PCyc_Color(wet_src + 4);
}

void PCycle_SLZ(void) {
    // Lanterns, red lights, cyan lights
    if (--pcyc_time >= 0)
        return;
    pcyc_time = 8 - 1;

    int16_t num = pcyc_num + 1;
    if (num >= 6)
        num = 0;
    pcyc_num = num;

    const uint8_t *src = Palette_SLZCycle + (num * 6);
    // Real ASM writes B, then D-E (skips C) -- the second write uses a
    // fixed +4-byte destination offset rather than a post-incremented
    // pointer, so the middle color slot is genuinely left untouched.
    dry_palette[2][0xB] = PCyc_Color(src);
    dry_palette[2][0xD] = PCyc_Color(src + 2);
    dry_palette[2][0xE] = PCyc_Color(src + 4);
}

void PCycle_SYZ(void) {
    // Flashy scenery lights
    if (--pcyc_time >= 0)
        return;
    pcyc_time = 6 - 1;

    int16_t num = pcyc_num++;
    num &= 3;
    int16_t d1 = num << 2;
    int16_t d0 = d1 << 1;

    // Rotating black/yellow -- contiguous write, 4 colors
    const uint8_t *src_by = Palette_SYZCycle1 + d0;
    dry_palette[3][7] = PCyc_Color(src_by);
    dry_palette[3][8] = PCyc_Color(src_by + 2);
    dry_palette[3][9] = PCyc_Color(src_by + 4);
    dry_palette[3][0xA] = PCyc_Color(src_by + 6);

    // Pulsating red/white -- same B-then-D skip pattern as SLZ
    const uint8_t *src_rw = Palette_SYZCycle2 + d1;
    dry_palette[3][0xB] = PCyc_Color(src_rw);
    dry_palette[3][0xD] = PCyc_Color(src_rw + 2);
}

typedef struct {
    uint8_t initial_timer;
    uint8_t colours;
    const uint8_t *source;
    uint16_t *dest;
} SBZCycEntry;

static void PCycle_SBZ_RunScript(const SBZCycEntry *script, int count) {
    uint8_t *buf = (uint8_t *)pcyc_buffer;
    for (int i = 0; i < count; i++) {
        uint8_t *timer = &buf[i * 2];
        uint8_t *index = &buf[i * 2 + 1];

        if ((int8_t)(--*timer) >= 0)
            continue;

        *timer = script[i].initial_timer;

        uint8_t next = (uint8_t)(*index + 1);
        if (next >= script[i].colours)
            next = 0;
        *index = next;

        *script[i].dest = PCyc_Color(script[i].source + ((next & 0xF) << 1));
    }
}

void PCycle_SBZ(void) {
    bool act1 = LEVEL_ACT(level_id) == 0;

    static const SBZCycEntry script_act1[] = {
        { 8 - 1, 8, Palette_SBZCyc1, &dry_palette[2][8] },   // FG multi-colored small blinking lights
        { 14 - 1, 8, Palette_SBZCyc2, &dry_palette[2][9] },  // FG slow red/yellow pulse
        { 15 - 1, 8, Palette_SBZCyc3, &dry_palette[3][7] },  // BG very slow red pulse
        { 12 - 1, 8, Palette_SBZCyc5, &dry_palette[3][8] },  // BG slow red pulse
        { 8 - 1, 8, Palette_SBZCyc6, &dry_palette[3][9] },   // BG slow teal pulse
        { 29 - 1, 16, Palette_SBZCyc7, &dry_palette[3][0xF] }, // BG very slow yellow/cyan pulse
        { 4 - 1, 3, Palette_SBZCyc8, &dry_palette[3][0xC] },     // electrocutor pink/purple 1
        { 4 - 1, 3, Palette_SBZCyc8 + 2, &dry_palette[3][0xD] }, // electrocutor pink/purple 2
        { 4 - 1, 3, Palette_SBZCyc8 + 4, &dry_palette[3][0xE] }, // electrocutor pink/purple 3
    };
    static const SBZCycEntry script_act2fz[] = {
        { 8 - 1, 8, Palette_SBZCyc1, &dry_palette[2][8] },
        { 14 - 1, 8, Palette_SBZCyc2, &dry_palette[2][9] },
        { 10 - 1, 8, Palette_SBZCyc9, &dry_palette[3][8] },  // BG multi-colored small blinking lights (act 2 only)
        { 8 - 1, 8, Palette_SBZCyc6, &dry_palette[3][9] },
        { 4 - 1, 3, Palette_SBZCyc8, &dry_palette[3][0xC] },
        { 4 - 1, 3, Palette_SBZCyc8 + 2, &dry_palette[3][0xD] },
        { 4 - 1, 3, Palette_SBZCyc8 + 4, &dry_palette[3][0xE] },
    };

    if (act1)
        PCycle_SBZ_RunScript(script_act1, sizeof(script_act1) / sizeof(script_act1[0]));
    else
        PCycle_SBZ_RunScript(script_act2fz, sizeof(script_act2fz) / sizeof(script_act2fz[0]));

    // Conveyor belts (spinning platforms and floor), act 2 gear wheels, electrocutor stems
    if (--pcyc_time >= 0)
        return;

    const uint8_t *src_base = act1 ? Palette_SBZCyc4 : Palette_SBZCyc10;
    pcyc_time = (act1 ? 2 : 1) - 1;

    int16_t dir = f_conveyrev ? 1 : -1;
    int16_t off = (int16_t)(pcyc_num & 3);
    off += dir;
    if ((uint16_t)off >= 3)
        off = (off < 0) ? 2 : 0;
    pcyc_num = off;

    const uint8_t *src = src_base + (off << 1);
    dry_palette[2][0xC] = PCyc_Color(src);
    dry_palette[2][0xD] = PCyc_Color(src + 2);
    dry_palette[2][0xE] = PCyc_Color(src + 4);
}

// Palette cycle function
void PaletteCycle(void) {
    // Fix palettes getting corrupted during level transitions between different zones
    if (restart)
        return;

    switch (LEVEL_ZONE(level_id)) {
    case ZoneId_GHZ:
    case ZoneId_EndZ:
        PCycle_Water(Palette_GHZCycle);
        break;
    case ZoneId_LZ:
        PCycle_LZ();
        break;
    case ZoneId_MZ:
        PCycle_MZ();
        break;
    case ZoneId_SLZ:
        PCycle_SLZ();
        break;
    case ZoneId_SYZ:
        PCycle_SYZ();
        break;
    case ZoneId_SBZ:
        PCycle_SBZ();
        break;
    }
}
