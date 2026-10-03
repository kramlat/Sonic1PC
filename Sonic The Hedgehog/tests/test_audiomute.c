#include "test.h"

#include <string.h>

#include "Backend/SN76489.h"

// Audio menu channel mutes: a muted PSG channel keeps running but adds nothing to the output.

static int64_t Energy(SN76489 *chip, int n) {
    int32_t out[512];
    memset(out, 0, sizeof(out));
    SN76489_Generate(chip, out, (uint32_t)n, 44100, 3579545);
    int64_t e = 0;
    for (int i = 0; i < n; i++)
        e += (int64_t)out[i] * out[i];
    return e;
}

static void PSG_MuteSilencesOnlyThatChannel(void) {
    SN76489 chip;
    SN76489_Init(&chip);
    SN76489_Write(&chip, 0x80 | 0x0E); // tone 1 period low bits
    SN76489_Write(&chip, 0x08);        // period high bits
    SN76489_Write(&chip, 0x90);        // tone 1 attenuation 0 (loudest)
    SN76489_Write(&chip, 0xA0 | 0x0E); // tone 2 period low bits
    SN76489_Write(&chip, 0x04);
    SN76489_Write(&chip, 0xB0);        // tone 2 attenuation 0

    CHECK(Energy(&chip, 512) > 0);

    chip.mute_mask = 0x3; // mute tone 1 and 2
    CHECK_EQ(Energy(&chip, 512), 0);

    chip.mute_mask = 0x1; // only tone 1 muted: tone 2 still audible
    CHECK(Energy(&chip, 512) > 0);

    chip.mute_mask = 0;
    CHECK(Energy(&chip, 512) > 0);
}

void RegisterAudioMuteTests(void) {
    RUN_TEST(PSG_MuteSilencesOnlyThatChannel);
}
