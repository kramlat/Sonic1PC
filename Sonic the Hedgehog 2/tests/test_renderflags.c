#include "test.h"

#include "test_renderflags.h"

// (Object 08, the dust and splash, is in the second list since the alpha: it animates on its first run, which sets the flips from its status.)
// Sonic 2: the objects whose original ORs the level flag in keep the layout's flips on their first run, and those whose original assigns it clear them (see test_renderflags.h).

static const uint8_t keeps[] = { 0x03, 0x0B, 0x0C, 0x14, 0x16, 0x1A, 0x1B, 0x1C, 0x1F, 0x2A, 0x2D, 0x36, 0x41, 0x45, 0x4B, 0x4C, 0x53, 0x54, 0x55, 0x56, 0x6A, 0x74, 0x76, 0x7D, 0x9D };
static const uint8_t clears[] = { 0x01, 0x02, 0x04, 0x05, 0x08, 0x09, 0x0A, 0x0D, 0x11, 0x12, 0x13, 0x15, 0x17, 0x18, 0x19, 0x21, 0x26, 0x27, 0x29, 0x2E, 0x2F, 0x31, 0x32, 0x33, 0x38, 0x39, 0x3B, 0x3C, 0x3E, 0x3F, 0x46, 0x47, 0x49, 0x4F, 0x5C, 0x6B, 0x75, 0x79, 0x8A };

static void RenderFlags_ObjectsThatOrTheLevelFlagInKeepTheLayoutsFlips(void) {
    for (size_t i = 0; i < sizeof(keeps); i++) {
        const uint8_t after = RenderFlags_AfterFirstRun(keeps[i]);
        if (after != 3) {
            printf("\n    object %02X: flips %02X after its first run, kept expected", keeps[i], after);
            CHECK_EQ(after, 3);
        }
    }
}

static void RenderFlags_ObjectsThatAssignTheLevelFlagClearTheLayoutsFlips(void) {
    for (size_t i = 0; i < sizeof(clears); i++) {
        const uint8_t after = RenderFlags_AfterFirstRun(clears[i]);
        if (after != 0 && after != 0xFF) {
            printf("\n    object %02X: flips %02X after its first run, cleared expected", clears[i], after);
            CHECK_EQ(after, 0);
        }
    }
}

void RegisterRenderFlagsTests(void) {
    RUN_TEST(RenderFlags_ObjectsThatOrTheLevelFlagInKeepTheLayoutsFlips);
    RUN_TEST(RenderFlags_ObjectsThatAssignTheLevelFlagClearTheLayoutsFlips);
}
