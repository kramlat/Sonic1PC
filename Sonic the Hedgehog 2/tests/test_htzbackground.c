#include "test.h"

#include <string.h>

#include "HTZBackground.h"
#include "Level.h"
#include "LevelScroll.h"
#include "Video.h"

// Hill Top's background (the prototype's loc_2244E, loc_22584 and Bg_Scroll_HTz's loc_6108): the mountains are tiles chosen by the camera's step, and a strip made from a picture, each of its 16 rows shifted by
// its own layer's scroll.

extern const uint8_t S2Art_HTZBgStrips[];

static void HTZBackground_StepFollowsTheCamera(void) {
    CHECK_EQ(HTZBackground_Step(0), 0);
    CHECK_EQ(HTZBackground_Step(0x330), 45);
    for (int x = 0; x < 0x3000; x += 7)
        CHECK(HTZBackground_Step((int16_t)x) < 0x30);
}

static void HTZBackground_FirstStepShowsTheFirstSixChunks(void) {
    int chunks[6];
    HTZBackground_StepChunks(0, chunks);
    for (int k = 0; k < 6; k++)
        CHECK_EQ(chunks[k], k);
}

static void HTZBackground_StripRowsAreTheirPicturesWhenNothingScrolls(void) {
    int16_t layers[16] = { 0 };
    uint8_t out[0x100];
    HTZBackground_BuildStrips(0, layers, out);
    for (int i = 0; i < 16; i++)
        for (int k = 0; k < 4; k++)
            CHECK(memcmp(out + k * 0x40 + i * 4, S2Art_HTZBgStrips + i * 0x20 + k * 4, 4) == 0);
}

static void HTZBackground_AnOddPixelShiftUsesTheSecondCopy(void) {
    int16_t layers[16] = { 0 };
    uint8_t out[0x100];
    HTZBackground_BuildStrips(-8, layers, out); // (the camera's eighth: one pixel)
    for (int i = 0; i < 16; i++)
        CHECK(memcmp(out + i * 4, S2Art_HTZBgStrips + 0x200 + i * 0x20, 4) == 0);
}

static void HTZBackground_ScrollHasTheEighthThenBands(void) {
    HTZBackground_Reset();
    scrpos_x.v = 0x200 << 16;
    HTZBackground_Deform();
    CHECK_EQ(hscroll_buffer[0][0], -0x200);
    CHECK_EQ(hscroll_buffer[0][1], -0x40);        // (the top $80 lines: an eighth of the camera)
    CHECK_EQ(hscroll_buffer[0x7F][1], -0x40);
    CHECK(hscroll_buffer[0x80][1] != -0x40);     // (then the bands move on)
    CHECK_EQ(hscroll_buffer[0xDF][0], -0x200);
    CHECK_EQ(htz_layerdef[0x11], 4);              // (the drift of the layers, 4 a frame)
    HTZBackground_Deform();
    CHECK_EQ(htz_layerdef[0x11], 8);
}

// The title screen leaves its TM in VRAM at $A200, where Hill Top's background tiles go: the first frame has to put the chunks over it (no step is "current" yet), or the TM shows in the sky
static void HTZBackground_TheFirstFramePutsTheChunksOverTheTitlesTM(void) {
    HTZBackground_Reset();
    CHECK_EQ(HTZBackground_LastStep(), 0xFF);
    for (int x = 0; x < 0x3000; x += 5)
        CHECK(HTZBackground_Step((int16_t)x) != 0xFF); // (so no camera place counts as "already done")
}

void RegisterHTZBackgroundTests(void) {
    RUN_TEST(HTZBackground_TheFirstFramePutsTheChunksOverTheTitlesTM);
    RUN_TEST(HTZBackground_StepFollowsTheCamera);
    RUN_TEST(HTZBackground_FirstStepShowsTheFirstSixChunks);
    RUN_TEST(HTZBackground_StripRowsAreTheirPicturesWhenNothingScrolls);
    RUN_TEST(HTZBackground_AnOddPixelShiftUsesTheSecondCopy);
    RUN_TEST(HTZBackground_ScrollHasTheEighthThenBands);
}
