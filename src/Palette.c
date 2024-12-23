#include "Palette.h"

#include "PLC.h"
#include "Video.h"

#include "Backend/VDP.h"

#include <stdlib.h>

// Palette state
int16_t pal_chgspeed;

uint16_t dry_palette[4][16];
uint16_t dry_palette_dup[4][16];
uint16_t wet_palette[4][16];
uint16_t wet_palette_dup[4][16];

PaletteFade palette_fade;

// Palettes
#include "Resource/Palette/Continue.h"
#include "Resource/Palette/Ending.h"
#include "Resource/Palette/GHZ.h"
#include "Resource/Palette/LZ.h"
#include "Resource/Palette/LZWater.h"
#include "Resource/Palette/LevelSel.h"
#include "Resource/Palette/MZ.h"
#include "Resource/Palette/SBZ1.h"
#include "Resource/Palette/SBZ2.h"
#include "Resource/Palette/SBZ3.h"
#include "Resource/Palette/SBZ3Water.h"
#include "Resource/Palette/SLZ.h"
#include "Resource/Palette/SSResults.h"
#include "Resource/Palette/SYZ.h"
#include "Resource/Palette/SegaBG.h"
#include "Resource/Palette/Sonic.h"
#include "Resource/Palette/SonicLZ.h"
#include "Resource/Palette/SonicSBZ.h"
#include "Resource/Palette/Special.h"
#include "Resource/Palette/Title.h"

static struct PalettePointer {
    const uint16_t* palette;
    uint16_t* target;
    size_t colours;
} palette_pointers[] = {
    /* PalId_SegaBG    */ { (const uint16_t*)Palette_SegaBG, &dry_palette[0][0], 0x40 },
    /* PalId_Title     */ { (const uint16_t*)Palette_Title, &dry_palette[0][0], 0x40 },
    /* PalId_LevelSel  */ { (const uint16_t*)Palette_LevelSel, &dry_palette[0][0], 0x40 },
    /* PalId_Sonic     */ { (const uint16_t*)Palette_Sonic, &dry_palette[0][0], 0x10 },
    /* PalId_GHZ       */ { (const uint16_t*)Palette_GHZ, &dry_palette[1][0], 0x30 },
    /* PalId_LZ        */ { (const uint16_t*)Palette_LZ, &dry_palette[1][0], 0x30 },
    /* PalId_MZ        */ { (const uint16_t*)Palette_MZ, &dry_palette[1][0], 0x30 },
    /* PalId_SYZ       */ { (const uint16_t*)Palette_SYZ, &dry_palette[1][0], 0x30 },
    /* PalId_SLZ       */ { (const uint16_t*)Palette_SLZ, &dry_palette[1][0], 0x30 },
    /* PalId_SBZ1      */ { (const uint16_t*)Palette_SBZ1, &dry_palette[1][0], 0x30 },
    /* PalId_Special   */ { (const uint16_t*)Palette_Special, &dry_palette[0][0], 0x40 },
    /* PalId_LZWater   */ { (const uint16_t*)Palette_LZWater, &dry_palette[0][0], 0x40 },
    /* PalId_SBZ3      */ { (const uint16_t*)Palette_SBZ3, &dry_palette[1][0], 0x30 },
    /* PalId_SBZ3Water */ { (const uint16_t*)Palette_SBZ3Water, &dry_palette[0][0], 0x40 },
    /* PalId_SBZ2      */ { (const uint16_t*)Palette_SBZ2, &dry_palette[1][0], 0x30 },
    /* PalId_SonicLZ   */ { (const uint16_t*)Palette_SonicLZ, &dry_palette[0][0], 0x10 },
    /* PalId_SonicSBZ  */ { (const uint16_t*)Palette_SonicSBZ, &dry_palette[0][0], 0x10 },
    /* PalId_SSResults */ { (const uint16_t*)Palette_SSResults, &dry_palette[0][0], 0x40 },
    /* PalId_Continue  */ { (const uint16_t*)Palette_Continue, &dry_palette[0][0], 0x20 },
    /* PalId_Ending    */ { (const uint16_t*)Palette_Ending, &dry_palette[0][0], 0x40 },
};

// Palette interface
void PalLoad1(PaletteId id)
{
    // Load given palette
    struct PalettePointer* palload = &palette_pointers[id];
    const uint16_t* inp = palload->palette;
    uint16_t* outp = &dry_palette_dup[0][0] + (palload->target - &dry_palette[0][0]);

    for (size_t i = 0; i < palload->colours; i++, inp++)
        *outp++ = LESWAP_16(*inp);
}

void PalLoad2(PaletteId id)
{
    // Load given palette
    struct PalettePointer* palload = &palette_pointers[id];
    const uint16_t* inp = palload->palette;
    uint16_t* outp = palload->target;

    for (size_t i = 0; i < palload->colours; i++, inp++)
        *outp++ = LESWAP_16(*inp);
}

void PalLoad3_Water(PaletteId id)
{
    // Load given palette
    struct PalettePointer* palload = &palette_pointers[id];
    const uint16_t* inp = palload->palette;
    uint16_t* outp = &wet_palette[0][0] + (palload->target - &dry_palette[0][0]);

    for (size_t i = 0; i < palload->colours; i++, inp++)
        *outp++ = LESWAP_16(*inp);
}

void PalLoad4_Water(PaletteId id)
{
    // Load given palette
    struct PalettePointer* palload = &palette_pointers[id];
    const uint16_t* inp = palload->palette;
    uint16_t* outp = &wet_palette_dup[0][0] + (palload->target - &dry_palette[0][0]);

    for (size_t i = 0; i < palload->colours; i++, inp++)
        *outp++ = LESWAP_16(*inp);
}

// Fade in from black
static void FadeIn_AddColour(uint16_t* col, uint16_t ref)
{
    uint16_t v = *col;
    if (v == ref)
        return;
    if ((v + 0x200) <= ref)
        v += 0x200;
    else if ((v + 0x020) <= ref)
        v += 0x020;
    else if ((v + 0x002) <= ref)
        v += 0x002;
    *col = v;
}

void FadeIn_FromBlack()
{
    uint16_t *col, *ref;

    // Fade dry palette
    col = (&dry_palette[0][0]) + palette_fade.ind;
    ref = (&dry_palette_dup[0][0]) + palette_fade.ind;
    for (int i = 0; i < palette_fade.len; i++)
        FadeIn_AddColour(col++, *ref++);

    // Fade wet palette
    col = (&wet_palette[0][0]) + palette_fade.ind;
    ref = (&wet_palette_dup[0][0]) + palette_fade.ind;
    for (int i = 0; i < palette_fade.len; i++)
        FadeIn_AddColour(col++, *ref++);
}

void PaletteFadeIn()
{
    PaletteFadeIn_At(0x00, 0x40);
}

void PaletteFadeIn_At(uint8_t ind, uint8_t len)
{
    // Initialize fade
    palette_fade.ind = ind;
    palette_fade.len = len;

    // Fill palette with black
    uint16_t* col = (&dry_palette[0][0]) + palette_fade.ind;
    for (int i = 0; i < palette_fade.len; i++)
        *col++ = 0x000;

    // Fade for 22 frames
    for (int i = 0; i < 22; i++) {
        vbla_routine = 0x12;
        WaitForVBla();
        FadeIn_FromBlack();
        RunPLC();
    }
}

// Fade out to black
static void FadeOut_DecColour(uint16_t* col)
{
    uint16_t v = *col;
    if (v == 0)
        return;
    if (v & 0x00E)
        v -= 0x002;
    else if (v & 0x0E0)
        v -= 0x020;
    else if (v & 0xE00)
        v -= 0x200;
    *col = v;
}

void FadeOut_ToBlack()
{
    uint16_t* col;

    // Fade dry palette
    col = (&dry_palette[0][0]) + palette_fade.ind;
    for (int i = 0; i < palette_fade.len; i++)
        FadeOut_DecColour(col++);

    // Fade wet palette
    col = (&wet_palette[0][0]) + palette_fade.ind;
    for (int i = 0; i < palette_fade.len; i++)
        FadeOut_DecColour(col++);
}

void PaletteFadeOut()
{
    PaletteFadeOut_At(0x00, 0x40);
}

void PaletteFadeOut_At(uint8_t ind, uint8_t len)
{
    // Initialize fade
    palette_fade.ind = ind;
    palette_fade.len = len;

    // Fade for 22 frames
    for (int i = 0; i < 22; i++) {
        vbla_routine = 0x12;
        WaitForVBla();
        FadeOut_ToBlack();
        RunPLC();
    }
}

// White in from white
static void WhiteIn_DecColour(uint16_t* col, uint16_t ref)
{
    uint16_t v = *col;
    if (v == ref)
        return;
    if ((v - 0x200) >= ref)
        v -= 0x200;
    else if ((v - 0x020) >= ref)
        v -= 0x020;
    else if ((v - 0x002) >= ref)
        v -= 0x002;
    *col = v;
}

void WhiteIn_FromWhite()
{
    uint16_t *col, *ref;

    // White dry palette
    col = (&dry_palette[0][0]) + palette_fade.ind;
    ref = (&dry_palette_dup[0][0]) + palette_fade.ind;
    for (int i = 0; i < palette_fade.len; i++)
        WhiteIn_DecColour(col++, *ref++);

    // White wet palette
    col = (&wet_palette[0][0]) + palette_fade.ind;
    ref = (&wet_palette_dup[0][0]) + palette_fade.ind;
    for (int i = 0; i < palette_fade.len; i++)
        WhiteIn_DecColour(col++, *ref++);
}

void PaletteWhiteIn()
{
    PaletteWhiteIn_At(0x00, 0x40);
}

void PaletteWhiteIn_At(uint8_t ind, uint8_t len)
{
    // Initialize fade
    palette_fade.ind = ind;
    palette_fade.len = len;

    // White for 22 frames
    for (int i = 0; i < 22; i++) {
        vbla_routine = 0x12;
        WaitForVBla();
        WhiteIn_FromWhite();
        RunPLC();
    }
}

// White out to white
static void WhiteOut_IncColour(uint16_t* col)
{
    uint16_t v = *col;
    if (v == 0xEEE)
        return;
    if ((v & 0x00E) != 0x00E)
        v += 0x002;
    else if ((v & 0x0E0) != 0x0E0)
        v += 0x020;
    else if ((v & 0xE00) != 0xE00)
        v += 0x200;
    *col = v;
}

void WhiteOut_ToWhite()
{
    uint16_t* col;

    // White dry palette
    col = (&dry_palette[0][0]) + palette_fade.ind;
    for (int i = 0; i < palette_fade.len; i++)
        WhiteOut_IncColour(col++);

    // White wet palette
    col = (&wet_palette[0][0]) + palette_fade.ind;
    for (int i = 0; i < palette_fade.len; i++)
        WhiteOut_IncColour(col++);
}

void PaletteWhiteOut()
{
    PaletteWhiteOut_At(0x00, 0x40);
}

void PaletteWhiteOut_At(uint8_t ind, uint8_t len)
{
    // Initialize fade
    palette_fade.ind = ind;
    palette_fade.len = len;

    // White for 22 frames
    for (int i = 0; i < 22; i++) {
        vbla_routine = 0x12;
        WaitForVBla();
        WhiteOut_ToWhite();
        RunPLC();
    }
}
