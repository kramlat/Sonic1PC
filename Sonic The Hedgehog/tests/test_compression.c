// Tests of the engine's decompressors, shared by Sonic 1's and Sonic 2's test programs: hand-built streams for the formats' tokens (so a wrong bit order or off-by-one fails by name) and, for Kosinski,
// whose encoder we have, round trips over inputs that stress its match search.
#include "test.h"

#include "Enigma.h"
#include "Kosinski.h"
#include "Nemesis.h"

#include <stdlib.h>
#include <string.h>

// --- Kosinski ---------------------------------------------------------------

// Three literals and the end token (a full match whose length byte is 0)
static void Kosinski_Literals(void) {
    static const uint8_t stream[] = { 0x17, 0x00, 'A', 'B', 'C', 0x00, 0xF0, 0x00 };
    uint8_t out[16] = { 0 };
    uint8_t *end = KosDec(stream, out);
    CHECK_EQ(end - out, 3);
    CHECK(memcmp(out, "ABC", 3) == 0);
}

// A literal, then a short match (2-bit length 3 = 4 bytes) from one byte back: a run
static void Kosinski_ShortMatchRepeatsTheLastByte(void) {
    static const uint8_t stream[] = { 0x49, 0x00, 'A', 0xFF, 0x00, 0xF0, 0x00 };
    uint8_t out[16] = { 0 };
    uint8_t *end = KosDec(stream, out);
    CHECK_EQ(end - out, 5);
    CHECK(memcmp(out, "AAAAA", 5) == 0);
}

static uint8_t *Pattern(size_t size, int kind) {
    uint8_t *data = malloc(size ? size : 1);
    uint32_t seed = 12345;
    for (size_t i = 0; i < size; i++) {
        seed = seed * 1103515245u + 12345u;
        switch (kind) {
        case 0: data[i] = 0; break;                                                  // all zero: one long match
        case 1: data[i] = (uint8_t)i; break;                                         // a ramp: matches only 256 back
        case 2: data[i] = (uint8_t)(seed >> 16); break;                              // noise: nothing to match
        case 3: data[i] = (uint8_t)("SONIC"[i % 5]); break;                          // a short period
        default: data[i] = (i / 300) % 2 ? (uint8_t)(seed >> 24) : (uint8_t)(i % 7); // runs of noise and pattern
        }
    }
    return data;
}

static void Kosinski_RoundTrip(size_t size, int kind) {
    uint8_t *data = Pattern(size, kind);
    size_t packed_size = 0;
    uint8_t *packed = KosEnc(data, size, &packed_size);
    uint8_t *out = calloc(size + 16, 1);
    CHECK(packed != NULL);
    if (packed) {
        uint8_t *end = KosDec(packed, out);
        CHECK_EQ(end - out, size);
        CHECK(memcmp(out, data, size) == 0);
    }
    free(packed);
    free(out);
    free(data);
}

static void Kosinski_RoundTripsEveryKindOfInput(void) {
    static const size_t sizes[] = { 1, 2, 3, 15, 16, 17, 255, 256, 257, 4096, 8191, 8192, 8193, 70000 };
    for (size_t s = 0; s < sizeof(sizes) / sizeof(sizes[0]); s++)
        for (int kind = 0; kind < 5; kind++)
            Kosinski_RoundTrip(sizes[s], kind);
}

static void Kosinski_EmptyInputRoundTrips(void) {
    size_t packed_size = 0;
    uint8_t *packed = KosEnc((const uint8_t *)"", 0, &packed_size);
    uint8_t out[8] = { 0xEE };
    CHECK(packed != NULL);
    if (packed)
        CHECK_EQ(KosDec(packed, out) - out, 0);
    free(packed);
}

static void Kosinski_RepetitiveDataShrinks(void) {
    uint8_t *data = Pattern(8192, 0);
    size_t packed_size = 0;
    uint8_t *packed = KosEnc(data, 8192, &packed_size);
    CHECK(packed_size < 200);
    free(packed);
    free(data);
}

// --- Nemesis ----------------------------------------------------------------

// A one-tile stream: header (tile count 1), palette byte $85 (pixel 5), code '0' = a run of 8 of it, then the data: eight '0' bits = eight rows
static const uint8_t nemesis_flat[] = { 0x00, 0x01, 0x85, 0x71, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00 };

static void Nemesis_DecodesARunToATile(void) {
    uint8_t out[40];
    memset(out, 0xEE, sizeof(out));
    NemDecToRAM(nemesis_flat, out);
    for (int i = 0; i < 32; i++)
        CHECK_EQ(out[i], 0x55);
    CHECK_EQ(out[32], 0xEE); // (one tile, and not a byte more)
}

// The same with a second code ('1' = four pixels of 3): the stream "1 1 0 0 0 0 0 0 0" is a row of 3s and seven rows of 5s
static void Nemesis_TwoCodes(void) {
    static const uint8_t stream[] = { 0x00, 0x01, 0x85, 0x71, 0x00, 0x83, 0x31, 0x01, 0xFF, 0xC0, 0x00, 0x00, 0x00 };
    uint8_t out[32];
    NemDecToRAM(stream, out);
    for (int i = 0; i < 4; i++)
        CHECK_EQ(out[i], 0x33);
    for (int i = 4; i < 32; i++)
        CHECK_EQ(out[i], 0x55);
}

// Header bit 15 is the XOR mode: each row is XORed with the one before, so equal rows alternate with zero
static void Nemesis_XorMode(void) {
    uint8_t stream[sizeof(nemesis_flat)];
    memcpy(stream, nemesis_flat, sizeof(stream));
    stream[0] = 0x80;
    uint8_t out[32];
    NemDecToRAM(stream, out);
    for (int row = 0; row < 8; row++)
        for (int i = 0; i < 4; i++)
            CHECK_EQ(out[row * 4 + i], row % 2 ? 0x00 : 0x55);
}

// The count in the header is in tiles: two tiles are sixteen rows
static void Nemesis_TileCountSetsTheLength(void) {
    uint8_t stream[sizeof(nemesis_flat) + 2];
    memcpy(stream, nemesis_flat, sizeof(nemesis_flat));
    memset(stream + sizeof(nemesis_flat), 0, 2);
    stream[1] = 0x02;
    uint8_t out[72];
    memset(out, 0xEE, sizeof(out));
    NemDecToRAM(stream, out);
    for (int i = 0; i < 64; i++)
        CHECK_EQ(out[i], 0x55);
    CHECK_EQ(out[64], 0xEE);
}

// --- Enigma -----------------------------------------------------------------

static uint16_t Word(const uint8_t *p) {
    return (uint16_t)((p[0] << 8) | p[1]);
}

// An incremental run of 3 from $10, then a literal run of 2 ($99), then the end token; the output is big-endian words
static void Enigma_IncrementalAndLiteralRuns(void) {
    static const uint8_t stream[] = { 0x01, 0x00, 0x00, 0x10, 0x00, 0x99, 0x09, 0x1F, 0xE0, 0x00 };
    uint8_t out[16] = { 0 };
    EniDec(stream, out, 0);
    CHECK_EQ(Word(out), 0x10);
    CHECK_EQ(Word(out + 2), 0x11);
    CHECK_EQ(Word(out + 4), 0x12);
    CHECK_EQ(Word(out + 6), 0x99);
    CHECK_EQ(Word(out + 8), 0x99);
    CHECK_EQ(Word(out + 10), 0); // (nothing after the end token)
}

// The start tile is added to every value
static void Enigma_StartTileIsAdded(void) {
    static const uint8_t stream[] = { 0x01, 0x00, 0x00, 0x10, 0x00, 0x99, 0x09, 0x1F, 0xE0, 0x00 };
    uint8_t out[16] = { 0 };
    EniDec(stream, out, 0x100);
    CHECK_EQ(Word(out), 0x110);
    CHECK_EQ(Word(out + 4), 0x112);
    CHECK_EQ(Word(out + 6), 0x199);
}

// A repeated inline value: 4 bits wide, $A, three times
static void Enigma_InlineValueRepeats(void) {
    static const uint8_t stream[] = { 0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x85, 0x5F, 0xC0, 0x00, 0x00 };
    uint8_t out[16] = { 0 };
    EniDec(stream, out, 0);
    CHECK_EQ(Word(out), 0xA);
    CHECK_EQ(Word(out + 2), 0xA);
    CHECK_EQ(Word(out + 4), 0xA);
    CHECK_EQ(Word(out + 6), 0);
}

void RegisterCompressionTests(void) {
    RUN_TEST(Kosinski_Literals);
    RUN_TEST(Kosinski_ShortMatchRepeatsTheLastByte);
    RUN_TEST(Kosinski_RoundTripsEveryKindOfInput);
    RUN_TEST(Kosinski_EmptyInputRoundTrips);
    RUN_TEST(Kosinski_RepetitiveDataShrinks);
    RUN_TEST(Nemesis_DecodesARunToATile);
    RUN_TEST(Nemesis_TwoCodes);
    RUN_TEST(Nemesis_XorMode);
    RUN_TEST(Nemesis_TileCountSetsTheLength);
    RUN_TEST(Enigma_IncrementalAndLiteralRuns);
    RUN_TEST(Enigma_StartTileIsAdded);
    RUN_TEST(Enigma_InlineValueRepeats);
}
