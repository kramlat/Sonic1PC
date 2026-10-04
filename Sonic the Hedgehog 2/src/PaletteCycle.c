#include "PaletteCycle.h"
#include "Constants.h"

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

// Emerald Hill's cycle (Nick Arcade's "EHZ Water")
#include "Resource/S2Palette/EHZCycle.h"
// Chemical Plant's, Hidden Palace's and Hill Top's cycles (Nick Arcade's art/palettes/CPZ Cycle 1-3, HPZ Water Cycle, HPZ Underwater Cycle, Hill Top Lava and its delays)
#include "Resource/S2Palette/CPZCycle1.h"
#include "Resource/S2Palette/CPZCycle2.h"
#include "Resource/S2Palette/CPZCycle3.h"
#include "Resource/S2Palette/HPZCycle.h"
#include "Resource/S2Palette/HPZCycleWet.h"
#include "Resource/S2Palette/HTZCycle.h"
#include "Resource/S2Palette/HTZCycleDelay.h"
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

// Emerald Hill's cycle: 4 frames of 8 bytes every 8 ticks; the first 4 bytes go to colours 3 and 4 of palette line 2, the other 4 to colours 14 and 15
static void PCycle_EHZ(void) {
    if (--pcyc_time >= 0)
        return;
    pcyc_time = 7;
    const uint8_t* from = S2Palette_EHZCycle + ((++pcyc_num & 3) << 3);
    for (size_t i = 0; i < 4; i++, from += 2)
        dry_palette[1][i < 2 ? 3 + i : 12 + i] = (from[0] << 8) | (from[1] << 0);
}

static uint16_t PCyc_Word(const uint8_t *p) {
    return (uint16_t)((p[0] << 8) | p[1]);
}

// Chemical Plant (PalCycle_CPZ): every 8 ticks three cycles: colours 12-14 of line 4 from 9 frames of 6 bytes, colour 15 of line 4 from 21 words, colour 15 of line 3 from 16 words
static void PCycle_CPZ(void) {
    static uint16_t frame1, frame2, frame3;
    if (--pcyc_time >= 0)
        return;
    pcyc_time = 7;
    for (int i = 0; i < 3; i++)
        dry_palette[3][12 + i] = PCyc_Word(S2Palette_CPZCycle1 + frame1 + i * 2);
    frame1 += 6;
    if (frame1 >= 0x36)
        frame1 = 0;
    dry_palette[3][15] = PCyc_Word(S2Palette_CPZCycle2 + frame2);
    frame2 += 2;
    if (frame2 >= 0x2A)
        frame2 = 0;
    dry_palette[2][15] = PCyc_Word(S2Palette_CPZCycle3 + frame3);
    frame3 = (uint16_t)((frame3 + 2) & 0x1E);
}

// Hidden Palace (PalCycle_HPZ): every 5 ticks colours 9-12 of line 4, dry and underwater, from 4 frames of 8 bytes, stepping backwards
static void PCycle_HPZ(void) {
    static int16_t frame;
    if (--pcyc_time >= 0)
        return;
    pcyc_time = 4;
    int16_t at = frame;
    frame -= 2;
    if (frame < 0)
        frame = 6;
    for (int i = 0; i < 4; i++) {
        dry_palette[3][9 + i] = PCyc_Word(S2Palette_HPZCycle + at + i * 2);
        wet_palette[3][9 + i] = PCyc_Word(S2Palette_HPZCycleWet + at + i * 2);
    }
}

// Hill Top (PalCycle_HTZ): the lava, like Emerald Hill's cycle in layout (colours 3-4 and 14-15 of line 2), but 16 frames, each held for as long as its delay byte says
static void PCycle_HTZ(void) {
    if (--pcyc_time >= 0)
        return;
    pcyc_time = 0;
    int at = pcyc_num++ & 0xF;
    pcyc_time = S2Palette_HTZCycleDelay[at];
    const uint8_t *from = S2Palette_HTZCycle + (at << 3);
    for (int i = 0; i < 4; i++, from += 2)
        dry_palette[1][i < 2 ? 3 + i : 12 + i] = PCyc_Word(from);
}

// The Simon Wai prototype's cycles of the other zones (the tables are those of its palette cycle routines, as words of 9-bit colour). Each routine counts a timer down and, when it runs out, reloads it and
// writes the next frame of each of its cycles over colours of the zone's palette (the frames move on after the one written).
static const uint16_t PCyc_Wz[8] = { 0x0248, 0x046A, 0x048C, 0x06CE, 0x0248, 0x046A, 0x048C, 0x06CE };
static const uint16_t PCyc_Mz1[6] = { 0x0006, 0x0008, 0x000A, 0x000C, 0x000A, 0x0008 };
static const uint16_t PCyc_Mz2[6] = { 0x0422, 0x0866, 0x0ECC, 0x0422, 0x0866, 0x0ECC };
static const uint16_t PCyc_Mz3[10] = { 0x00A0, 0x0000, 0x00EE, 0x0000, 0x002E, 0x0000, 0x0E2E, 0x0000, 0x0E80, 0x0000 };
static const uint16_t PCyc_Ooz[8] = { 0x0400, 0x0602, 0x0804, 0x0806, 0x0400, 0x0602, 0x0804, 0x0806 };
static const uint16_t PCyc_Mcz[4] = { 0x000C, 0x006E, 0x00CE, 0x08EE };
static const uint16_t PCyc_Cnz1[18] = {
    0x000C, 0x00CC, 0x004C, 0x004C, 0x000C, 0x00CC, 0x00CC, 0x004C, 0x000C,
    0x00EC, 0x0080, 0x00C4, 0x00C4, 0x00EC, 0x0080, 0x0080, 0x00C4, 0x00EC,
};
static const uint16_t PCyc_Cnz2[9] = { 0x0044, 0x0088, 0x00EE, 0x0088, 0x00EE, 0x0044, 0x00EE, 0x0044, 0x0088 };
static const uint16_t PCyc_Cnz3[42] = {
    0x00EC, 0x0EEE, 0x00EA, 0x00E4, 0x06C0, 0x0CC4, 0x0E80, 0x0E40, 0x0E04, 0x0C08, 0x0C2E, 0x000E, 0x006E, 0x00AE,
    0x00AE, 0x00EC, 0x0EEE, 0x00EA, 0x00E4, 0x06C0, 0x0CC4, 0x0E80, 0x0E40, 0x0E04, 0x0C08, 0x0C2E, 0x000E, 0x006E,
    0x00EE, 0x00AE, 0x00EC, 0x0EEE, 0x00EA, 0x00E4, 0x06C0, 0x0CC4, 0x0E80, 0x0E40, 0x0E04, 0x0C08, 0x0C2E, 0x000E,
};
static const uint16_t PCyc_Ghz[16] = {
    0x0A86, 0x0E86, 0x0EA8, 0x0ECA, 0x0ECA, 0x0A86, 0x0E86, 0x0EA8, 0x0EA8, 0x0ECA, 0x0A86, 0x0E86, 0x0E86, 0x0EA8, 0x0ECA, 0x0A86,
};

// Wood (PalCycle_WZ): every 3 ticks colours 3-6 of line 4, from 4 words of a table of 8, stepping backwards
static void PCycle_WZ(void) {
    static int16_t frame;
    if (--pcyc_time >= 0)
        return;
    pcyc_time = 2;
    int16_t at = frame;
    frame -= 2;
    if (frame < 0)
        frame = 6;
    for (int i = 0; i < 4; i++)
        dry_palette[3][3 + i] = PCyc_Wz[at / 2 + i];
}

// Metropolis (PalCycle_Mz): three separate cycles of line 3, each with its own timer: colour 5 (every 18 ticks, 6 words), colours 1-3 (every 3, 3 frames of a table of 6) and colour 15 (every 10, 10 words)
static void PCycle_MTZ(void) {
    static int16_t time2, time3, frame1, frame2, frame3;
    if (--pcyc_time < 0) {
        pcyc_time = 0x11;
        dry_palette[2][5] = PCyc_Mz1[frame1 / 2];
        frame1 += 2;
        if (frame1 >= 0xC)
            frame1 = 0;
    }
    if (--time2 < 0) {
        time2 = 2;
        for (int i = 0; i < 3; i++)
            dry_palette[2][1 + i] = PCyc_Mz2[frame2 / 2 + i];
        frame2 += 2;
        if (frame2 >= 6)
            frame2 = 0;
    }
    if (--time3 < 0) {
        time3 = 9;
        dry_palette[2][15] = PCyc_Mz3[frame3 / 2];
        frame3 += 2;
        if (frame3 >= 0x14)
            frame3 = 0;
    }
}

// Oil Ocean (PalCycle_OOz): every 8 ticks colours 10-13 of line 3, 4 words of a table of 8
static void PCycle_OOZ(void) {
    static uint16_t frame;
    if (--pcyc_time >= 0)
        return;
    pcyc_time = 7;
    uint16_t at = frame;
    frame = (uint16_t)((frame + 2) & 6);
    for (int i = 0; i < 4; i++)
        dry_palette[2][10 + i] = PCyc_Ooz[at / 2 + i];
}

// Dust Hill (PalCycle_DHz): every 2 ticks colour 12 of line 2
static void PCycle_MCZ(void) {
    static uint16_t frame;
    if (--pcyc_time >= 0)
        return;
    pcyc_time = 1;
    dry_palette[1][11] = PCyc_Mcz[frame / 2];
    frame = (uint16_t)((frame + 2) & 6);
}

// Casino Night (PalCycle_CNz): every 8 ticks the lights of lines 3 and 4 (three colours of each of two groups of line 3 and three of line 4, each frame a step along their tables of 3 frames) and
// three colours of line 4 from 14 steps of three tables
static void PCycle_CNZ(void) {
    static int16_t frame1, frame2;
    if (--pcyc_time >= 0)
        return;
    pcyc_time = 7;
    int at = frame1 / 2;
    frame1 += 2;
    if (frame1 >= 6)
        frame1 = 0;
    for (int i = 0; i < 3; i++) {
        dry_palette[2][5 + i] = PCyc_Cnz1[at + i * 3];
        dry_palette[2][11 + i] = PCyc_Cnz1[at + 9 + i * 3];
        dry_palette[3][2 + i] = PCyc_Cnz2[at + i * 3];
    }
    int at3 = frame2 / 2;
    frame2 += 2;
    if (frame2 >= 0x1C)
        frame2 = 0;
    for (int i = 0; i < 3; i++)
        dry_palette[3][9 + i] = PCyc_Cnz3[at3 + i * 14];
}

// Neo Green Hill (PalCycle_NGHz): Green Hill's cycle table, every 6 ticks, to colours 3-6 of line 3
static void PCycle_ARZ(void) {
    if (--pcyc_time >= 0)
        return;
    pcyc_time = 5;
    const uint16_t *from = PCyc_Ghz + ((pcyc_num++ & 3) << 2);
    for (int i = 0; i < 4; i++)
        dry_palette[2][2 + i] = from[i];
}

// The title screen has no palette cycle: neither prototype's title loop calls one (Nick Arcade's PalCycle_S1TitleScreen is never referenced). Kept so the title's loop still has
// its call.
void PCycle_Title(void) {
}

static uint16_t PCyc_Color(const uint8_t *src) {
    return (uint16_t)((src[0] << 8) | src[1]);
}

typedef struct {
    uint8_t time;       // as stored in real ROM (time_arg-1 already baked in); when this reads negative, the real timer instead uses 0x1FF
    uint8_t anim;       // background mode (SS_BG_Modes index: 0 grid, 2/4 fish morph, 6 fish, 8/$A bird morph, $C bird)
    uint16_t bg_plane;  // nametable plane B shows: ArtTile_SS_Plane_5 (clouds/bubbles) or _6
    uint8_t pal_offset; // palette cycle offset; bit 7 selects the Pal_SSCyc2 set (PalCycle_SS_2), bit 0 (when set, PalCycle_SS_2 only) is the "extra palette line 4" flag
} SSPalEntry;

// The real SS_Timing_Values table: it also carries the BG mode and nametable plane per entry, which SS_BGSetMode
// (SpecialStageBG.c) applies when an entry comes up.
#define P5 ArtTile_SS_Plane_5
#define P6 ArtTile_SS_Plane_6
static const SSPalEntry ss_pal_timing[32] = {
    { 4 - 1, 0, P6, 0x92 }, { 4 - 1, 0, P6, 0x90 }, { 4 - 1, 0, P6, 0x8E }, { 4 - 1, 0, P6, 0x8C }, { 4 - 1, 0, P6, 0x8B },
    { 4 - 1, 0, P6, 0x80 }, { 4 - 1, 0, P6, 0x82 }, { 4 - 1, 0, P6, 0x84 }, { 4 - 1, 0, P6, 0x86 }, { 4 - 1, 0, P6, 0x88 },
    { 8 - 1, 8, P6, 0x00 }, { 8 - 1, 0xA, P6, 0x0C }, { 0 - 1, 0xC, P6, 0x18 }, { 0 - 1, 0xC, P6, 0x18 }, { 8 - 1, 0xA, P6, 0x0C }, { 8 - 1, 8, P6, 0x00 },
    { 4 - 1, 0, P5, 0x88 }, { 4 - 1, 0, P5, 0x86 }, { 4 - 1, 0, P5, 0x84 }, { 4 - 1, 0, P5, 0x82 }, { 4 - 1, 0, P5, 0x81 },
    { 4 - 1, 0, P5, 0x8A }, { 4 - 1, 0, P5, 0x8C }, { 4 - 1, 0, P5, 0x8E }, { 4 - 1, 0, P5, 0x90 }, { 4 - 1, 0, P5, 0x92 },
    { 8 - 1, 2, P5, 0x24 }, { 8 - 1, 4, P5, 0x30 }, { 0 - 1, 6, P5, 0x3C }, { 0 - 1, 6, P5, 0x3C }, { 8 - 1, 4, P5, 0x30 }, { 8 - 1, 2, P5, 0x24 },
};
#undef P5
#undef P6

void PCycle_SS(void) {
    if (pause_state)
        return;
    if (--palss_time >= 0)
        return;

    const SSPalEntry *entry = &ss_pal_timing[palss_num & 0x1F];
    palss_num++;

    int8_t time = (int8_t)entry->time;
    palss_time = (time < 0) ? (0x200 - 1) : time;

    SS_BGSetMode(entry->anim, entry->bg_plane); // which background canvas and cloud layer show

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
    case ZoneId_CPZ:
        PCycle_CPZ();
        break;
    case ZoneId_EHZ:
        PCycle_EHZ();
        break;
    case ZoneId_HPZ:
        PCycle_HPZ();
        break;
    case ZoneId_HTZ:
        PCycle_HTZ();
        break;
    case ZoneId_WZ:
        PCycle_WZ();
        break;
    case ZoneId_MTZ:
    case ZoneId_MTZ3:
        PCycle_MTZ();
        break;
    case ZoneId_OOZ:
        PCycle_OOZ();
        break;
    case ZoneId_MCZ:
        PCycle_MCZ();
        break;
    case ZoneId_CNZ:
        PCycle_CNZ();
        break;
    case ZoneId_ARZ:
        PCycle_ARZ();
        break;
    }
}
