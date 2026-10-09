#include "test.h"

#include <string.h>

#include "EnginePalette.h"

// The palette loader reads colours in any of three formats: the Mega Drive's own (big-endian words, 0000bbb0ggg0rrr0), 24 bit RGB (three bytes a colour) and 00rrggbb (four, big-endian), the 24 bit ones brought
// down to the machine's 3 bits a channel for now.

static void Palette_24BitColoursBecomeTheMachinesNine(void) {
    CHECK_EQ(Palette_FromRGB24(0, 0, 0), 0x0000);
    CHECK_EQ(Palette_FromRGB24(255, 255, 255), 0x0EEE);
    CHECK_EQ(Palette_FromRGB24(255, 0, 0), 0x000E); // (red is the low channel)
    CHECK_EQ(Palette_FromRGB24(0, 255, 0), 0x00E0);
    CHECK_EQ(Palette_FromRGB24(0, 0, 255), 0x0E00);
    CHECK_EQ(Palette_FromRGB24(128, 128, 128), 0x0888); // (4 of 7, doubled)
}

// The levels the machine can show (n/7 of full, for n of 0 to 7) come back as themselves, and no result has a bit outside 0000bbb0ggg0rrr0
static void Palette_TheMachinesLevelsRoundTrip(void) {
    for (int n = 0; n < 8; n++) {
        uint8_t v = (uint8_t)(n * 255 / 7);
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
    CHECK_EQ(Palette_FromRGB24(18, 0, 0), 0x0000); // (a 7th is 36.4: 18 is below half of it)
    CHECK_EQ(Palette_FromRGB24(19, 0, 0), 0x0002);
    CHECK_EQ(Palette_FromRGB24(0, 236, 0), 0x00C0); // (6 of 7 is 218.6, 7 of 7 is 255: 236 is nearer to 6.5 and over: 7)
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
    CHECK_EQ(dry_palette[0][2], 0x0E80);
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
    CHECK_EQ(dry_palette[0][1], 0x0E80);
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
    RUN_TEST(Palette_GenesisColoursLoadAsTheyWere);
    RUN_TEST(Palette_AnEntryWithNoFormatIsGenesis);
    RUN_TEST(Palette_RGB24ColoursLoadConverted);
    RUN_TEST(Palette_0RGB32ColoursLoadConverted);
    RUN_TEST(Palette_TheThreeFormatsAgree);
    RUN_TEST(Palette_EveryLoaderTakesBothFormats);
}
