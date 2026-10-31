#include "test.h"

#include <stdlib.h>
#include <string.h>

#include "Kosinski.h"
#include "Level.h"

// The load block table (LevelLoadBlock.h) and the way a primary and a secondary group are unpacked.

static uint8_t *Compress(const uint8_t *data, size_t size) {
    size_t out_size;
    return KosEnc(data, size, &out_size);
}

static void LevelLoadBlock_TheSecondGroupGoesStraightAfterThePrimary(void) {
    uint8_t a[40], b[24];
    for (int i = 0; i < 40; i++) a[i] = (uint8_t)(i * 3);
    for (int i = 0; i < 24; i++) b[i] = (uint8_t)(0xA0 + i);
    uint8_t *ka = Compress(a, sizeof a), *kb = Compress(b, sizeof b);
    const uint8_t *group[2] = { ka, kb };
    uint8_t out[64] = { 0 };
    uint8_t *end = LevelLoadBlock_Unpack(group, out);
    CHECK_EQ((int)(end - out), 64);
    CHECK(memcmp(out, a, sizeof a) == 0);
    CHECK(memcmp(out + 40, b, sizeof b) == 0);
    free(ka);
    free(kb);
}

static void LevelLoadBlock_AGroupNamedTwiceIsLoadedOnce(void) {
    uint8_t a[40];
    for (int i = 0; i < 40; i++) a[i] = (uint8_t)(i + 1);
    uint8_t *ka = Compress(a, sizeof a);
    const uint8_t *group[2] = { ka, ka };
    uint8_t out[80] = { 0 };
    CHECK_EQ((int)(LevelLoadBlock_Unpack(group, out) - out), 40);
    CHECK_EQ(out[40], 0); // (the second load was skipped)
    const uint8_t *alone[2] = { ka, NULL };
    CHECK_EQ((int)(LevelLoadBlock_Unpack(alone, out) - out), 40);
    free(ka);
}

static void LevelLoadBlock_EveryLevelOfTheGameHasItsGroups(void) {
    int levels = 0;
    for (int zone = 0; zone < ZoneId_Num; zone++)
        for (int act = 0; act < 4; act++) {
            const LevelLoadBlock *llb = &level_load_block[zone][act];
            if (llb->art[0] == NULL)
                continue; // (an act the game does not have)
            CHECK(llb->art[1] != NULL);
            CHECK(llb->map16[0] != NULL && llb->map16[1] != NULL);
            CHECK(llb->map128[0] != NULL && llb->map128[1] != NULL);
            levels++;
        }
    CHECK(levels >= 4);
}

void RegisterLevelLoadBlockTests(void) {
    RUN_TEST(LevelLoadBlock_TheSecondGroupGoesStraightAfterThePrimary);
    RUN_TEST(LevelLoadBlock_AGroupNamedTwiceIsLoadedOnce);
    RUN_TEST(LevelLoadBlock_EveryLevelOfTheGameHasItsGroups);
}
