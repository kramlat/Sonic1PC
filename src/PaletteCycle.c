#include "PaletteCycle.h"

#include "Level.h"
#include "Palette.h"
#include "Video.h"

// Palette cycle state
int16_t pcyc_num, pcyc_time;
uint16_t pcyc_buffer[0x18];

// Palette cycles
#include "Resource/Palette/GHZCycle.h"
#include "Resource/Palette/Sega1.h"
#include "Resource/Palette/Sega2.h"
#include "Resource/Palette/TitleCycle.h"

// Palette cycle routines
signed int PCycle_Sega()
{
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

static void PCycle_Water(const uint8_t* palette)
{
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

void PCycle_Title()
{
    PCycle_Water(Palette_TitleCycle);
}

void PCycle_SS()
{
}

// Palette cycle function
void PaletteCycle()
{
    switch (LEVEL_ZONE(level_id)) {
    case ZoneId_GHZ:
    case ZoneId_EndZ:
        PCycle_Water(Palette_GHZCycle);
        break;
    }
}
