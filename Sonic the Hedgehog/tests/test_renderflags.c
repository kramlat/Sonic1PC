#include "test.h"

#include "test_renderflags.h"

// Sonic 1: the objects whose original ORs the level flag in keep the layout's flips on their first run, and those whose original assigns it clear them (see test_renderflags.h).

static const uint8_t keeps[] = { 0x03, 0x08, 0x1A, 0x1C, 0x36, 0x41, 0x44, 0x4B, 0x53, 0x5D, 0x5E, 0x5F, 0x60, 0x62, 0x63, 0x65, 0x6B, 0x6C, 0x70, 0x71, 0x78, 0x7B, 0x7C, 0x7D };
static const uint8_t clears[] = { 0x01, 0x09, 0x0A, 0x0B, 0x0D, 0x11, 0x12, 0x15, 0x17, 0x18, 0x1B, 0x1D, 0x1E, 0x1F, 0x21, 0x22, 0x26, 0x27, 0x29, 0x2B, 0x2E, 0x2F, 0x32, 0x33, 0x38, 0x39, 0x3B, 0x3C, 0x3E, 0x3F, 0x40, 0x42, 0x46, 0x47, 0x49, 0x4A, 0x4C, 0x4D, 0x50, 0x51, 0x52, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5A, 0x5C, 0x61, 0x67, 0x6A, 0x73, 0x74, 0x79, 0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x89, 0x8A, 0x8C };

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
