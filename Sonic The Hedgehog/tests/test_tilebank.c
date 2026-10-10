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
    VDP_SetReal(VDP_REAL_ALL); // (banks, 16 palette lines and 8bpp tiles are the real readings)
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
    Plane_PutEntry(plane, 0, TileEntry_FromWord(3, bank, 0));
    CHECK_EQ(TopLeft(), PIXEL_BLUE);
    Plane_Put(plane, 0, 3); // as a tile word, it is the plane's bank's (the main one)
    CHECK_EQ(TopLeft(), PIXEL_RED);
    TileBank_Free(bank);
}

static void TileBank_AFreedBanksEntriesDrawNothing(void) {
    Prepare();
    tilebank_t *bank = TileBank_Create(4);
    Solid(bank, 1, 3);
    Plane_PutEntry(&screen1p.plane_a, 0, TileEntry_FromWord(1, bank, 0));
    CHECK_EQ(TopLeft(), PIXEL_BLUE);
    TileBank_Free(bank);
    CHECK_EQ(TopLeft(), PIXEL_BACKDROP); // not read: the entry was changed to show nothing when the bank was freed
    CHECK_EQ(tilebank_stale_count, 0);   // (it is not a stale one: nothing names the bank any more)
    CHECK_EQ(TileAttr_Bank(Plane_At(&screen1p.plane_a, 0)->attr), TILEBANK_NONE);
}

static void TileBank_AnEntryWrittenAfterTheFreeStillCarriesAStaleGeneration(void) {
    Prepare();
    tilebank_t *bank = TileBank_Create(4);
    tile_entry_t late = TileEntry_FromWord(1, bank, 0); // made while it was there...
    TileBank_Free(bank);
    Plane_PutEntry(&screen1p.plane_a, 0, late);        // ... put in the plane after it was freed (nothing to purge then)
    CHECK_EQ(TopLeft(), PIXEL_BACKDROP);
    CHECK(tilebank_stale_count > 0);                   // the generation catches it
}

static void TileBank_ASlotMadeAgainIsNotTheOldBank(void) {
    Prepare();
    tilebank_t *first = TileBank_Create(4);
    Solid(first, 1, 3);
    tile_entry_t old_entry = TileEntry_FromWord(1, first, 0);
    Plane_PutEntry(&screen1p.plane_a, 0, old_entry);
    TileBank_Free(first);
    tilebank_t *second = TileBank_Create(4); // (the same slot, other art)
    Solid(second, 1, 1);
    CHECK_EQ(second->id, TileAttr_Bank(old_entry.attr));
    CHECK(second->generation != old_entry.generation);
    CHECK_EQ(TopLeft(), PIXEL_BACKDROP); // the old entry does not find the new bank's tile
    TileBank_Free(second);
}

static void TileBank_APatternPastTheBanksEndIsNotThere(void) {
    Prepare();
    tilebank_t *bank = TileBank_Create(2);
    tile_entry_t e = TileEntry_FromWord(0, bank, 0);
    e.pattern = 5;
    bool deep;
    CHECK(TileBank_PatternOf(e.attr, e.generation, e.pattern, &deep) == NULL);
    e.pattern = 1;
    CHECK(TileBank_PatternOf(e.attr, e.generation, e.pattern, &deep) != NULL);
    TileBank_Free(bank);
}

static void TileBank_APatternPastTheGenesisLimitIsReachable(void) {
    Prepare();
    tilebank_t *bank = TileBank_Create(0x3000); // more tiles than an 11 bit entry could name (and its entries use all 16 bits)
    Solid(bank, 0x2800, 3);
    tile_entry_t e = TileEntry_FromWord(0, bank, 0);
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
    CHECK_EQ(made, TILEBANK_NONE - 1); // (the main bank has a slot, and the last id is the one of an entry that shows nothing)
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
    screen1p.sprite_table[0] = (sprite_t){ 128, 0x0000, 2, 128, TileAttr_FromWord(0, bank, 0), bank->generation };
    CHECK_EQ(TopLeft(), PIXEL_BLUE);
    screen1p.sprite_table[0].attr = TileAttr_FromWord(0, TileBank_Main(), 0); // as the main bank's
    screen1p.sprite_table[0].generation = 0;
    CHECK_EQ(TopLeft(), PIXEL_RED);
    screen1p.sprite_table[0].attr = TileAttr_FromWord(0, bank, 0);
    screen1p.sprite_table[0].generation = bank->generation;
    TileBank_Free(bank);
    CHECK_EQ(TopLeft(), PIXEL_BACKDROP); // not read once the bank is gone
    memset(screen1p.sprite_table, 0, (VIEWPORT_SPRITES + 1) * sizeof(sprite_t));
}

// --- 16 palette lines, and 8 bits a pixel tiles ---

static void SetLine(int line, uint32_t rgb) {
    uint32_t colours[16];
    for (int i = 0; i < 16; i++)
        colours[i] = rgb;
    VDP_SeekCRAM((size_t)line * 16);
    VDP_WriteCRAM_RGB(colours, 16);
}

#define PIXEL_RGB(rrggbb) (((rrggbb) << 8) | 0xFF)

static void TileBank_APaletteGroupReachesTheLinesBeyondTheFirstFour(void) {
    Prepare();
    SetLine(9, 0x123456);
    Solid(TileBank_Main(), 1, 7); // colour 7 of whatever line
    screen1p.plane_a.palette_group = 2; // the tile word's own two bits (1) plus 4 times this: line 9
    Plane_Put(&screen1p.plane_a, 0, TILE_MAP(0, 1, 0, 0, 1));
    CHECK_EQ(TopLeft(), PIXEL_RGB(0x123456u));
    screen1p.plane_a.palette_group = 0;
}

static void TileBank_ASpritesPaletteGroupReachesThemToo(void) {
    Prepare();
    SetLine(13, 0xABCDEF);
    Solid(TileBank_Main(), 2, 4);
    screen1p.sprites = screen1p.sprite_table;
    memset(screen1p.sprite_table, 0, (VIEWPORT_SPRITES + 1) * sizeof(sprite_t));
    screen1p.sprite_table[0] = (sprite_t){ 128, 0x0000, 2, 128, TileAttr_FromWord((uint16_t)TILE_MAP(0, 1, 0, 0, 2), TileBank_Main(), 3), 0 }; // line 1 + 4 * 3 = 13
    CHECK_EQ(TopLeft(), PIXEL_RGB(0xABCDEFu));
    memset(screen1p.sprite_table, 0, (VIEWPORT_SPRITES + 1) * sizeof(sprite_t));
}

// An 8bpp tile: 64 bytes, a byte a pixel, every pixel the colour index 0x95 (line 9, colour 5)
static void Deep(tilebank_t *bank, size_t slot, uint8_t index) {
    uint8_t bytes[64];
    memset(bytes, index, sizeof(bytes));
    TileBank_Write(bank, slot, bytes, sizeof(bytes));
}

static void TileBank_TheDepthTableSaysWhichSlotsAreEightBitTiles(void) {
    Prepare();
    tilebank_t *bank = TileBank_Create(8);
    CHECK(!TileBank_IsDeep(bank, 2));
    TileBank_SetDepth(bank, 2, 4, true); // two 8bpp tiles: slots 2-3 and 4-5
    CHECK(TileBank_IsDeep(bank, 2));
    CHECK(!TileBank_IsDeep(bank, 3)); // (the tail is no tile of its own)
    CHECK(TileBank_IsDeep(bank, 4));
    CHECK(!TileBank_IsDeep(bank, 6));
    CHECK(bank->depth[3] == TILE_SLOT_8BPP_TAIL);
    TileBank_SetDepth(bank, 2, 4, false);
    CHECK(!TileBank_IsDeep(bank, 2));
    CHECK(!TileBank_IsDeep(TileBank_Main(), 5));
    TileBank_Free(bank);
    CHECK(!TileBank_IsDeep(bank, 2)); // (a freed bank has none)
}

static void TileBank_AnEightBitTileDrawsItsBytesAsColoursOfTheWholePalette(void) {
    Prepare();
    SetLine(9, 0x654321);
    SetLine(1, 0x111111);
    tilebank_t *bank = TileBank_Create(8);
    Deep(bank, 2, 0x95);
    TileBank_SetDepth(bank, 2, 2, true);
    Plane_PutEntry(&screen1p.plane_a, 0, TileEntry_FromWord(2, bank, 0)); // (its own palette bits do not matter: palette 0)
    CHECK_EQ(TopLeft(), PIXEL_RGB(0x654321u));
    TileBank_SetDepth(bank, 2, 2, false); // as a 4bpp tile the same bytes are two colours: index 9 and 5, in line 0
    CHECK(TopLeft() != PIXEL_RGB(0x654321u));
    TileBank_Free(bank);
}

static void TileBank_AnEightBitSpriteStepsTwoSlotsACell(void) {
    Prepare();
    SetLine(9, 0x654321);
    SetLine(10, 0x0000FF);
    tilebank_t *bank = TileBank_Create(8);
    Deep(bank, 0, 0x95); // the first cell
    Deep(bank, 2, 0xA3); // the second, of a sprite two cells high (height 1)
    TileBank_SetDepth(bank, 0, 4, true);
    screen1p.sprites = screen1p.sprite_table;
    memset(screen1p.sprite_table, 0, (VIEWPORT_SPRITES + 1) * sizeof(sprite_t));
    screen1p.sprite_table[0] = (sprite_t){ 128, 0x0100, 0, 128, TileAttr_FromWord(0, bank, 0), bank->generation }; // height 1: two cells
    VDP_DrawFrame();
    int pitch, rows;
    const uint32_t *f = VDP_GetFrame(&pitch, &rows);
    CHECK_EQ(f[0], PIXEL_RGB(0x654321u));
    CHECK_EQ(f[8 * pitch], PIXEL_RGB(0x0000FFu)); // the next cell down is the next tile (two slots on), colour 0xA3
    memset(screen1p.sprite_table, 0, (VIEWPORT_SPRITES + 1) * sizeof(sprite_t));
    TileBank_Free(bank);
}

// --- the two words of an entry, and the compat and real readings ---

static void TileBank_AGenesisWordBecomesAPatternAndAnAttributeWord(void) {
    Prepare();
    const uint16_t word = (uint16_t)TILE_MAP(1, 2, 1, 1, 0x123); // priority, palette 2, both flips, pattern $123
    tile_entry_t e = TileEntry_FromWord(word, TileBank_Main(), 1); // palette group 1: line 4 + 2 = 6
    CHECK_EQ(e.pattern, 0x123);
    CHECK(e.attr & TILE_ATTR_PRIORITY);
    CHECK(e.attr & TILE_ATTR_X_FLIP);
    CHECK(e.attr & TILE_ATTR_Y_FLIP);
    CHECK_EQ(TileAttr_Palette(e.attr), 6);
    CHECK_EQ(TileAttr_Bank(e.attr), 0);
    CHECK_EQ(TileEntry_Word(&e), (uint16_t)TILE_MAP(1, 2, 1, 1, 0x123)); // back to the Genesis' word: the group is not in it
}

static void TileBank_TheCompatReadingIgnoresTheBankAndTheHigherPalettes(void) {
    Prepare();
    SetLine(1, 0x00FF00);
    SetLine(5, 0xFF0000);
    tilebank_t *bank = TileBank_Create(8);
    Solid(bank, 3, 7);
    Solid(TileBank_Main(), 3, 7);
    Plane_PutEntry(&screen1p.plane_a, 0, TileEntry_FromWord(TILE_MAP(0, 1, 0, 0, 3), bank, 1)); // line 5, in the bank
    VDP_SetReal(VDP_REAL_ALL);
    CHECK_EQ(TopLeft(), PIXEL_RGB(0xFF0000u)); // real: line 5 (its colour 7), from the bank
    VDP_SetReal(0);
    CHECK_EQ(TopLeft(), PIXEL_RGB(0x00FF00u)); // compat: line 5 is line 1 (the two low bits), and the pattern is the main bank's
    VDP_SetReal(VDP_REAL_ALL);
    TileBank_Free(bank);
}

static void TileBank_EachPartCanBeRealOnItsOwn(void) {
    Prepare();
    SetLine(5, 0xFF0000);
    SetLine(1, 0x00FF00);
    Solid(TileBank_Main(), 3, 7);
    Plane_PutEntry(&screen1p.plane_a, 0, TileEntry_FromWord(TILE_MAP(0, 1, 0, 0, 3), TileBank_Main(), 1));
    VDP_SetReal(VDP_REAL_PLANES); // real tiles, compat palettes
    CHECK_EQ(TopLeft(), PIXEL_RGB(0x00FF00u));
    VDP_SetReal(VDP_REAL_PALETTES);
    CHECK_EQ(TopLeft(), PIXEL_RGB(0xFF0000u));
    CHECK_EQ(VDP_GetReal(), VDP_REAL_PALETTES);
    VDP_SetReal(VDP_REAL_ALL);
}

void RegisterTileBankTests(void) {
    RUN_TEST(TileBank_AnEntryWrittenAfterTheFreeStillCarriesAStaleGeneration);
    RUN_TEST(TileBank_AGenesisWordBecomesAPatternAndAnAttributeWord);
    RUN_TEST(TileBank_TheCompatReadingIgnoresTheBankAndTheHigherPalettes);
    RUN_TEST(TileBank_EachPartCanBeRealOnItsOwn);
    RUN_TEST(TileBank_APaletteGroupReachesTheLinesBeyondTheFirstFour);
    RUN_TEST(TileBank_ASpritesPaletteGroupReachesThemToo);
    RUN_TEST(TileBank_TheDepthTableSaysWhichSlotsAreEightBitTiles);
    RUN_TEST(TileBank_AnEightBitTileDrawsItsBytesAsColoursOfTheWholePalette);
    RUN_TEST(TileBank_AnEightBitSpriteStepsTwoSlotsACell);
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
