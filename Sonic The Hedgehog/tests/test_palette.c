#include "test.h"

#include <string.h>

#include "Backend/VDP.h"
#include "EnginePalette.h"

// The palette loader reads colours in any of three formats: the Mega Drive's own (big-endian words, 0000bbb0ggg0rrr0), 24 bit RGB (three bytes a colour) and 00rrggbb (four, big-endian), the 24 bit ones brought
// down to the machine's 3 bits a channel for now.

static void Palette_24BitColoursBecomeTheMachinesNine(void) {
    CHECK_EQ(Palette_FromRGB24(0, 0, 0), 0x0000);
    CHECK_EQ(Palette_FromRGB24(255, 255, 255), 0x0EEE);
    CHECK_EQ(Palette_FromRGB24(255, 0, 0), 0x000E); // (red is the low channel)
    CHECK_EQ(Palette_FromRGB24(0, 255, 0), 0x00E0);
    CHECK_EQ(Palette_FromRGB24(0, 0, 255), 0x0E00);
    CHECK_EQ(Palette_FromRGB24(128, 128, 128), 0x0666); // (nearer the machine's level 3, 116, than 4, 144)
}

// The levels the machine can show come back as themselves, and no result has a bit outside 0000bbb0ggg0rrr0
static void Palette_TheMachinesLevelsRoundTrip(void) {
    static const uint8_t level[8] = { 0, 52, 87, 116, 144, 172, 206, 255 }; // (what the machine's DAC puts out, not evenly)
    for (int n = 0; n < 8; n++) {
        uint8_t v = level[n];
        uint16_t c = Palette_FromRGB24(v, v, v);
        CHECK_EQ(c, (uint16_t)((n << 9) | (n << 5) | (n << 1)));
        CHECK_EQ(c & 0xF111, 0);
    }
    for (int r = 0; r < 256; r += 5)
        for (int g = 0; g < 256; g += 7)
            for (int b = 0; b < 256; b += 11)
                CHECK_EQ(Palette_FromRGB24((uint8_t)r, (uint8_t)g, (uint8_t)b) & 0xF111, 0);
}

static void Palette_RoundingGoesToTheNearestLevel(void) {
    CHECK_EQ(Palette_FromRGB24(26, 0, 0), 0x0000); // (halfway between 0 and 52: the lower)
    CHECK_EQ(Palette_FromRGB24(27, 0, 0), 0x0002);
    CHECK_EQ(Palette_FromRGB24(0, 230, 0), 0x00C0); // (halfway between 206 and 255 is 230.5)
    CHECK_EQ(Palette_FromRGB24(0, 236, 0), 0x00E0);
}

// The way in and out of the video chip's colour RAM: its true colour is the DAC's levels, and a word of the machine's comes back from it as it went in
static void Palette_TheVDPUpscalesGenesisWords(void) {
    CHECK_EQ(VDP_Genesis2RGB(0x0000), 0x000000);
    CHECK_EQ(VDP_Genesis2RGB(0x0EEE), 0xFFFFFF);
    CHECK_EQ(VDP_Genesis2RGB(0x000E), 0xFF0000); // (red is the low channel; and RGB's high byte)
    CHECK_EQ(VDP_Genesis2RGB(0x0E00), 0x0000FF);
    CHECK_EQ(VDP_Genesis2RGB(0x0246), (uint32_t)((116 << 16) | (87 << 8) | 52)); // (red 3, green 2, blue 1)
}

// Every one of the machine's 512 colours goes up to true colour and back down as itself
static void Palette_EveryGenesisColourRoundTripsThroughTrueColour(void) {
    for (int b = 0; b < 8; b++)
        for (int g = 0; g < 8; g++)
            for (int r = 0; r < 8; r++) {
                uint16_t word = (uint16_t)((b << 9) | (g << 5) | (r << 1));
                CHECK_EQ(VDP_RGB2Genesis(VDP_Genesis2RGB(word)), word);
            }
}

// Words and true colour written to the colour RAM read back (as the machine's words)
static void Palette_ColourRAMTakesWordsAndTrueColour(void) {
    static const uint16_t words[3] = { 0x0EEE, 0x000E, 0x0246 };
    VDP_SeekCRAM(0);
    VDP_WriteCRAM(words, 3);
    CHECK_EQ(VDP_PeekCRAM(0, 0), 0x0EEE);
    CHECK_EQ(VDP_PeekCRAM(0, 1), 0x000E);
    CHECK_EQ(VDP_PeekCRAM(0, 2), 0x0246);
    CHECK_EQ(VDP_PeekColour(0, 1), 0xFF0000FFu); // (0xRRGGBBAA)

    static const uint32_t rgb[2] = { 0x00FF8000, 0x00123456 };
    VDP_SeekCRAM(3);
    VDP_WriteCRAM_RGB(rgb, 2);
    CHECK_EQ(VDP_PeekColour(0, 3), 0xFF8000FFu); // (kept as it is: true colour)
    CHECK_EQ(VDP_PeekColour(0, 4), 0x123456FFu);
    CHECK_EQ(VDP_PeekCRAM(0, 3), VDP_RGB2Genesis(0xFF8000)); // (its nearest, as the machine's word)

    VDP_SeekCRAM(0);
    VDP_FillCRAM(0x0ACE, 2);
    CHECK_EQ(VDP_PeekCRAM(0, 0), 0x0ACE);
    CHECK_EQ(VDP_PeekCRAM(0, 1), 0x0ACE);
}

static uint16_t aligned_genesis[4];

// An entry swapped in for the first, and put back
static PalettePointer saved_entry;
static void UseEntry(PalettePointer entry) {
    saved_entry = palette_pointers[0];
    palette_pointers[0] = entry;
}
static void PutBack(void) {
    palette_pointers[0] = saved_entry;
}

// The colour RAM has 16 lines, all drawn with (the entries and sprites carry a palette group of four lines each on top of the Genesis' two bits): each can be written and read back, its own, and the first four are untouched by the rest
static void Palette_ColourRAMHasSixteenLines(void) {
    CHECK_EQ(VDP_PALETTES, 16);
    CHECK_EQ(VDP_PALETTES_ACTIVE, 16);
    CHECK_EQ(COLOURS, 256);
    VDP_SeekCRAM(0);
    VDP_FillCRAM(0x0222, 64);
    for (int line = 4; line < 16; line++) {
        VDP_SeekCRAM((size_t)(line * 16));
        VDP_FillCRAM((uint16_t)(((line & 7) << 1) | ((line >> 3) << 5)), 16); // (each line its own colour: the line's low bits in red, its high bit in green)
    }
    for (int line = 4; line < 16; line++)
        for (int i = 0; i < 16; i++)
            CHECK_EQ(VDP_PeekCRAM(line, i), (uint16_t)(((line & 7) << 1) | ((line >> 3) << 5)));
    for (int line = 0; line < 4; line++)
        CHECK_EQ(VDP_PeekCRAM(line, 5), 0x0222);
    static const uint32_t last[1] = { 0x00ABCDEF };
    VDP_SeekCRAM(255);
    VDP_WriteCRAM_RGB(last, 1); // (the very last colour)
    CHECK_EQ(VDP_PeekColour(15, 15), 0xABCDEFFFu);
    VDP_SeekCRAM(0);
    VDP_FillCRAM(0, COLOURS);
}

static void Palette_GenesisColoursLoadAsTheyWere(void) {
    static const uint8_t file[8] = { 0x0E, 0xEE, 0x00, 0x0E, 0x02, 0x46, 0x00, 0x00 }; // (big-endian, as the files are)
    memcpy(aligned_genesis, file, sizeof(file));
    memset(dry_palette, 0xFF, sizeof(dry_palette));
    UseEntry((PalettePointer){ aligned_genesis, &dry_palette[0][0], 4, PAL_FORMAT_GENESIS });
    PalLoad2(0);
    PutBack();
    CHECK_EQ(dry_palette[0][0], 0x0EEE);
    CHECK_EQ(dry_palette[0][1], 0x000E);
    CHECK_EQ(dry_palette[0][2], 0x0246);
    CHECK_EQ(dry_palette[0][3], 0x0000);
    CHECK_EQ(dry_palette[0][4], 0xFFFF); // (only the colours named)
}

// An entry that does not say its format is a Mega Drive one
static void Palette_AnEntryWithNoFormatIsGenesis(void) {
    static const uint8_t file[2] = { 0x0A, 0xCE };
    memcpy(aligned_genesis, file, sizeof(file));
    UseEntry((PalettePointer){ aligned_genesis, &dry_palette[0][0], 1 });
    PalLoad2(0);
    PutBack();
    CHECK_EQ(dry_palette[0][0], 0x0ACE);
}

static void Palette_RGB24ColoursLoadConverted(void) {
    static const uint8_t rgb[3 * 3] = { 255, 255, 255, 255, 0, 0, 0, 128, 255 };
    memset(dry_palette, 0xFF, sizeof(dry_palette));
    UseEntry((PalettePointer){ rgb, &dry_palette[0][0], 3, PAL_FORMAT_RGB24 });
    PalLoad2(0);
    PutBack();
    CHECK_EQ(dry_palette[0][0], 0x0EEE);
    CHECK_EQ(dry_palette[0][1], 0x000E);
    CHECK_EQ(dry_palette[0][2], 0x0E60); // (green 128 is nearer 116 than 144)
    CHECK_EQ(dry_palette[0][3], 0xFFFF);
}

// 00rrggbb: four bytes a colour, the first ignored, the rest red, green and blue
static void Palette_0RGB32ColoursLoadConverted(void) {
    static const uint8_t words[4 * 3] = { 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x80, 0xFF, 0xAA, 0xFF, 0xFF, 0xFF }; // red; (0, 128, 255); and a stray top byte over white
    memset(dry_palette, 0xFF, sizeof(dry_palette));
    UseEntry((PalettePointer){ words, &dry_palette[0][0], 3, PAL_FORMAT_0RGB32 });
    PalLoad2(0);
    PutBack();
    CHECK_EQ(dry_palette[0][0], 0x000E);
    CHECK_EQ(dry_palette[0][1], 0x0E60);
    CHECK_EQ(dry_palette[0][2], 0x0EEE); // (the top byte is not a channel)
    CHECK_EQ(dry_palette[0][3], 0xFFFF);
}

// The same colour in each of the three formats loads the same
static void Palette_TheThreeFormatsAgree(void) {
    static const uint8_t rgb[3] = { 200, 100, 30 };
    static const uint8_t words[4] = { 0x00, 200, 100, 30 };
    uint16_t c = Palette_FromRGB24(200, 100, 30);
    uint8_t file[2] = { (uint8_t)(c >> 8), (uint8_t)c };
    memcpy(aligned_genesis, file, 2);
    uint16_t out[3];
    PalettePointer entries[3] = { { aligned_genesis, &dry_palette[0][0], 1, PAL_FORMAT_GENESIS }, { rgb, &dry_palette[0][0], 1, PAL_FORMAT_RGB24 }, { words, &dry_palette[0][0], 1, PAL_FORMAT_0RGB32 } };
    for (int i = 0; i < 3; i++) {
        UseEntry(entries[i]);
        PalLoad2(0);
        PutBack();
        out[i] = dry_palette[0][0];
    }
    CHECK_EQ(out[0], c);
    CHECK_EQ(out[1], c);
    CHECK_EQ(out[2], c);
}

// All four loaders take both formats, into their own palette: the live one (2), the reference the fades work towards (1), and the water's pair (3 and 4)
static void Palette_EveryLoaderTakesBothFormats(void) {
    static const uint8_t rgb[3] = { 255, 0, 255 };
    static const uint8_t file[2] = { 0x0E, 0x0E };
    memcpy(aligned_genesis, file, sizeof(file));
    memset(dry_palette, 0, sizeof(dry_palette));
    memset(dry_palette_dup, 0, sizeof(dry_palette_dup));
    memset(wet_palette, 0, sizeof(wet_palette));
    memset(wet_palette_dup, 0, sizeof(wet_palette_dup));

    UseEntry((PalettePointer){ rgb, &dry_palette[1][0], 1, PAL_FORMAT_RGB24 });
    PalLoad1(0);
    PalLoad2(0);
    PalLoad3_Water(0);
    PalLoad4_Water(0);
    PutBack();
    CHECK_EQ(dry_palette_dup[1][0], 0x0E0E);
    CHECK_EQ(dry_palette[1][0], 0x0E0E);
    CHECK_EQ(wet_palette[1][0], 0x0E0E);
    CHECK_EQ(wet_palette_dup[1][0], 0x0E0E);
    CHECK_EQ(dry_palette[0][0], 0);

    memset(dry_palette_dup, 0, sizeof(dry_palette_dup));
    memset(wet_palette, 0, sizeof(wet_palette));
    UseEntry((PalettePointer){ aligned_genesis, &dry_palette[2][0], 1, PAL_FORMAT_GENESIS });
    PalLoad1(0);
    PalLoad3_Water(0);
    PutBack();
    CHECK_EQ(dry_palette_dup[2][0], 0x0E0E);
    CHECK_EQ(wet_palette[2][0], 0x0E0E);
}

void RegisterPaletteTests(void) {
    RUN_TEST(Palette_24BitColoursBecomeTheMachinesNine);
    RUN_TEST(Palette_TheMachinesLevelsRoundTrip);
    RUN_TEST(Palette_RoundingGoesToTheNearestLevel);
    RUN_TEST(Palette_TheVDPUpscalesGenesisWords);
    RUN_TEST(Palette_EveryGenesisColourRoundTripsThroughTrueColour);
    RUN_TEST(Palette_ColourRAMTakesWordsAndTrueColour);
    RUN_TEST(Palette_ColourRAMHasSixteenLines);
    RUN_TEST(Palette_GenesisColoursLoadAsTheyWere);
    RUN_TEST(Palette_AnEntryWithNoFormatIsGenesis);
    RUN_TEST(Palette_RGB24ColoursLoadConverted);
    RUN_TEST(Palette_0RGB32ColoursLoadConverted);
    RUN_TEST(Palette_TheThreeFormatsAgree);
    RUN_TEST(Palette_EveryLoaderTakesBothFormats);
}
