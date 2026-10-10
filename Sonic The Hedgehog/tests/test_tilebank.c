#include "test.h"

#include <string.h>

#include "Video.h"
#include "Viewport.h"
#include "TileBank.h"
#include "Backend/VDP.h"

// Tile banks (engine/TileBank.c): a name table entry names its tile by bank, generation and pattern; a freed bank's entries draw nothing, and a bank makes room for tiles the main bank has no space for.

#define PIXEL_RED 0xFF0000FFu
#define PIXEL_BLUE 0x0000FFFFu
#define PIXEL_BACKDROP 0x000000FFu

static void Prepare(void) {
    VDP_SetSplitScreen(VDP_SPLIT_NONE);
    VDP_ClearVRAM();
    Viewport_UseOwnPlanes(&screen1p);
    Plane_Clear(&screen1p.plane_a);
    Plane_Clear(&screen1p.plane_b);
    Viewport_ClearWindow(&screen1p);
    memset(screen1p.hscroll, 0, screen1p.hscroll_bytes);
    screen1p.vsram = (vsram_t){ 0, 0 };
    screen1p.sprites = NULL;
    Viewport_SetSize(SCREEN_WIDTH, SCREEN_HEIGHT, 64, 32);
    uint16_t cram[64];
    memset(cram, 0, sizeof(cram));
    cram[1] = 0x000E; cram[3] = 0x0E00; // red, blue
    VDP_SeekCRAM(0);
    VDP_WriteCRAM(cram, 64);
    VDP_SetBackgroundColour(0);
    screen1p.hint_enable = false;
    tilebank_stale_count = 0;
    tilebank_stale_fatal = false;
}

static void Solid(tilebank_t *bank, size_t tile, uint8_t colour) {
    uint8_t bytes[32];
    memset(bytes, (colour << 4) | colour, sizeof(bytes));
    TileBank_Write(bank, tile, bytes, sizeof(bytes));
}

static uint32_t TopLeft(void) {
    VDP_DrawFrame();
    int pitch, rows;
    const uint32_t *f = VDP_GetFrame(&pitch, &rows);
    return f[0];
}

static void TileBank_TheMainBankIsTheTileSpaceAndNeverGoesStale(void) {
    Prepare();
    tilebank_t *main_bank = TileBank_Main();
    CHECK_EQ(main_bank->id, 0);
    CHECK_EQ(main_bank->generation, 0);
    Solid(main_bank, 1, 1);
    Plane_Put(&screen1p.plane_a, 0, 1);
    CHECK_EQ(TopLeft(), PIXEL_RED);
    CHECK_EQ(tilebank_stale_count, 0);
}

static void TileBank_AnEntryDrawsFromItsOwnBank(void) {
    Prepare();
    tilebank_t *bank = TileBank_Create(8);
    CHECK(bank != NULL);
    Solid(bank, 3, 3);
    Solid(TileBank_Main(), 3, 1); // the same pattern in the main bank is another colour
    plane_t *plane = &screen1p.plane_a;
    Plane_PutEntry(plane, 0, TileEntry_FromWord(3, bank));
    CHECK_EQ(TopLeft(), PIXEL_BLUE);
    Plane_Put(plane, 0, 3); // as a tile word, it is the plane's bank's (the main one)
    CHECK_EQ(TopLeft(), PIXEL_RED);
    TileBank_Free(bank);
}

static void TileBank_AFreedBanksEntriesDrawNothing(void) {
    Prepare();
    tilebank_t *bank = TileBank_Create(4);
    Solid(bank, 1, 3);
    Plane_PutEntry(&screen1p.plane_a, 0, TileEntry_FromWord(1, bank));
    CHECK_EQ(TopLeft(), PIXEL_BLUE);
    TileBank_Free(bank);
    CHECK_EQ(TopLeft(), PIXEL_BACKDROP); // not read: the generation moved on
    CHECK(tilebank_stale_count > 0);
}

static void TileBank_ASlotMadeAgainIsNotTheOldBank(void) {
    Prepare();
    tilebank_t *first = TileBank_Create(4);
    Solid(first, 1, 3);
    tile_entry_t old_entry = TileEntry_FromWord(1, first);
    Plane_PutEntry(&screen1p.plane_a, 0, old_entry);
    TileBank_Free(first);
    tilebank_t *second = TileBank_Create(4); // (the same slot, other art)
    Solid(second, 1, 1);
    CHECK_EQ(second->id, old_entry.bank);
    CHECK(second->generation != old_entry.generation);
    CHECK_EQ(TopLeft(), PIXEL_BACKDROP); // the old entry does not find the new bank's tile
    TileBank_Free(second);
}

static void TileBank_APatternPastTheBanksEndIsNotThere(void) {
    Prepare();
    tilebank_t *bank = TileBank_Create(2);
    tile_entry_t e = TileEntry_FromWord(0, bank);
    e.pattern = 5;
    CHECK(TileBank_Pattern(&e) == NULL);
    e.pattern = 1;
    CHECK(TileBank_Pattern(&e) != NULL);
    TileBank_Free(bank);
}

static void TileBank_APatternPastTheGenesisLimitIsReachable(void) {
    Prepare();
    tilebank_t *bank = TileBank_Create(0x3000); // more tiles than an 11 bit entry could name
    Solid(bank, 0x2800, 3);
    tile_entry_t e = TileEntry_FromWord(0, bank);
    e.pattern = 0x2800;
    Plane_PutEntry(&screen1p.plane_a, 0, e);
    CHECK_EQ(TopLeft(), PIXEL_BLUE);
    TileBank_Free(bank);
}

static void TileBank_TheTableRunsOutAndMakesRoomAgain(void) {
    tilebank_t *held[TILEBANKS];
    int made = 0;
    for (; made < TILEBANKS; made++) {
        held[made] = TileBank_Create(1);
        if (held[made] == NULL)
            break;
    }
    CHECK_EQ(made, TILEBANKS - 1); // (the main bank has a slot)
    CHECK(TileBank_Create(1) == NULL);
    for (int i = 0; i < made; i++)
        TileBank_Free(held[i]);
    tilebank_t *again = TileBank_Create(1);
    CHECK(again != NULL);
    TileBank_Free(again);
}

static void TileBank_APlaneWordKeepsItsFlagsAndPattern(void) {
    Prepare();
    Plane_Put(&screen1p.plane_a, 4, 0xE80F); // priority, palette 3, x flip, pattern $0F
    CHECK_EQ(Plane_Word(&screen1p.plane_a, 4), 0xE80F);
    CHECK_EQ(Plane_Word(&screen1p.plane_a, 0), 0);
    CHECK(Plane_At(&screen1p.plane_a, 0x2000 * 2) == NULL); // past its memory
}

// A sprite drawn from a bank of its own, and from a bank that is gone
static void TileBank_ASpriteDrawsFromItsBankAndNothingFromAFreedOne(void) {
    Prepare();
    tilebank_t *bank = TileBank_Create(8);
    Solid(bank, 2, 3);
    Solid(TileBank_Main(), 2, 1);
    screen1p.sprites = screen1p.sprite_table;
    memset(screen1p.sprite_table, 0, (VIEWPORT_SPRITES + 1) * sizeof(sprite_t));
    // one 8x8 cell (width 0, height 0) at the picture's top left, pattern 2 of the bank
    screen1p.sprite_table[0] = (sprite_t){ 128, 0x0000, 2, 128, bank->id, bank->generation };
    CHECK_EQ(TopLeft(), PIXEL_BLUE);
    screen1p.sprite_table[0].bank = 0; // as the main bank's
    screen1p.sprite_table[0].generation = 0;
    CHECK_EQ(TopLeft(), PIXEL_RED);
    screen1p.sprite_table[0].bank = bank->id;
    screen1p.sprite_table[0].generation = bank->generation;
    TileBank_Free(bank);
    CHECK_EQ(TopLeft(), PIXEL_BACKDROP); // not read once the bank is gone
    memset(screen1p.sprite_table, 0, (VIEWPORT_SPRITES + 1) * sizeof(sprite_t));
}

void RegisterTileBankTests(void) {
    RUN_TEST(TileBank_ASpriteDrawsFromItsBankAndNothingFromAFreedOne);
    RUN_TEST(TileBank_TheMainBankIsTheTileSpaceAndNeverGoesStale);
    RUN_TEST(TileBank_AnEntryDrawsFromItsOwnBank);
    RUN_TEST(TileBank_AFreedBanksEntriesDrawNothing);
    RUN_TEST(TileBank_ASlotMadeAgainIsNotTheOldBank);
    RUN_TEST(TileBank_APatternPastTheBanksEndIsNotThere);
    RUN_TEST(TileBank_APatternPastTheGenesisLimitIsReachable);
    RUN_TEST(TileBank_TheTableRunsOutAndMakesRoomAgain);
    RUN_TEST(TileBank_APlaneWordKeepsItsFlagsAndPattern);
}
