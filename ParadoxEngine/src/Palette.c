#include "EnginePalette.h"

#include "EnginePLC.h"
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

// Palette interface
uint16_t Palette_FromRGB24(uint8_t r, uint8_t g, uint8_t b) {
    unsigned r3 = (r * 7u + 127u) / 255u, g3 = (g * 7u + 127u) / 255u, b3 = (b * 7u + 127u) / 255u;
    return (uint16_t)((b3 << 9) | (g3 << 5) | (r3 << 1));
}

// The colours of an entry, brought to the machine's format, into a palette
static void LoadColours(const PalettePointer *palload, uint16_t *outp) {
    if (palload->format == PAL_FORMAT_RGB24) {
        const uint8_t *inp = (const uint8_t *)palload->palette;
        for (size_t i = 0; i < palload->colours; i++, inp += 3)
            *outp++ = Palette_FromRGB24(inp[0], inp[1], inp[2]);
    } else if (palload->format == PAL_FORMAT_0RGB32) {
        const uint8_t *inp = (const uint8_t *)palload->palette;
        for (size_t i = 0; i < palload->colours; i++, inp += 4)
            *outp++ = Palette_FromRGB24(inp[1], inp[2], inp[3]);
    } else {
        const uint16_t *inp = (const uint16_t *)palload->palette;
        for (size_t i = 0; i < palload->colours; i++, inp++)
            *outp++ = LESWAP_16(*inp);
    }
}

void PalLoad1(PaletteId id) {
    const PalettePointer *palload = &palette_pointers[id];
    LoadColours(palload, &dry_palette_dup[0][0] + (palload->target - &dry_palette[0][0]));
}

void PalLoad2(PaletteId id) {
    const PalettePointer *palload = &palette_pointers[id];
    LoadColours(palload, palload->target);
}

void PalLoad3_Water(PaletteId id) {
    const PalettePointer *palload = &palette_pointers[id];
    LoadColours(palload, &wet_palette[0][0] + (palload->target - &dry_palette[0][0]));
}

void PalLoad4_Water(PaletteId id) {
    const PalettePointer *palload = &palette_pointers[id];
    LoadColours(palload, &wet_palette_dup[0][0] + (palload->target - &dry_palette[0][0]));
}

// Fade in from black
static void FadeIn_AddColour(uint16_t* col, uint16_t ref) {
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

void FadeIn_FromBlack(void) {
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

void PaletteFadeIn(void) {
    PaletteFadeIn_At(0x00, 0x40);
}

void PaletteFadeIn_At(uint8_t ind, uint8_t len) {
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
static void FadeOut_DecColour(uint16_t* col) {
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

void FadeOut_ToBlack(void) {
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

void PaletteFadeOut(void) {
    PaletteFadeOut_At(0x00, 0x40);
}

void PaletteFadeOut_At(uint8_t ind, uint8_t len) {
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
static void WhiteIn_DecColour(uint16_t* col, uint16_t ref) {
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

void WhiteIn_FromWhite(void) {
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

void PaletteWhiteIn(void) {
    PaletteWhiteIn_At(0x00, 0x40);
}

void PaletteWhiteIn_At(uint8_t ind, uint8_t len) {
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
static void WhiteOut_IncColour(uint16_t* col) {
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

void WhiteOut_ToWhite(void) {
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

void PaletteWhiteOut(void) {
    PaletteWhiteOut_At(0x00, 0x40);
}

void PaletteWhiteOut_At(uint8_t ind, uint8_t len) {
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
