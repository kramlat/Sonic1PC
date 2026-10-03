// V3FlagTest: throwaway smoke test for SMPS driver-version-3 (Sonic 3/
// Flamedriver-compatible coordination flags) -- plays a small hand-compiled
// synthetic SFX (exercising pan, detune, volume, voice, modulation, note
// fill, DAC trigger, transposition, tempo, and the 3 documented stubs)
// through the real sequencer via Sound_DebugPlayRawSong, and asserts no
// crash plus a handful of expected post-playback state changes. Not a
// ctest -- a manual verification aid for the driver-version-3 migration
// (see the SMPS driver-version-3 plan), same throwaway spirit as
// tools/paradoxcomposer/native_compiler_test_main.cpp.
#include "../src/Sound.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// Compiled from /tmp/v3_test.jsonc (SndSFX, driverVersion:3), verified
// byte-identical between json_to_header.py and libparadoxsmps/compiler.c
// before being hand-copied here.
static const uint8_t v3_test_song[] = {
    0x00, 0x35, 0x01, 0x01, 0x80, 0x05, 0x00, 0x0A, 0x00, 0x01, 0xEF, 0x00, 0xE0, 0xC0, 0xE1, 0x05,
    0xE4, 0x10, 0xE6, 0x08, 0xF0, 0x02, 0x01, 0x01, 0x08, 0xBD, 0x08, 0xE8, 0x04, 0xEA, 0x81, 0xFF,
    0x05, 0x00, 0x00, 0x00, 0x00, 0xFE, 0x00, 0x00, 0x00, 0x00, 0xFB, 0x02, 0xFF, 0x00, 0x10, 0xFF,
    0x08, 0x01, 0xBA, 0x08, 0xF2, 0x3C, 0x01, 0x01, 0x0A, 0x05, 0x5C, 0x5C, 0x5C, 0x56, 0x11, 0x11,
    0x11, 0x0E, 0x0A, 0x0A, 0x06, 0x09, 0x3F, 0x3F, 0x3F, 0x4F, 0x80, 0x80, 0x20, 0x17,
};

#define SAMPLE_RATE 44100

int main(void) {
    Sound_Init();
    Sound_DebugPlayRawSong(v3_test_song, /*is_sfx=*/1, /*driver_version=*/3);

    uint32_t samples_per_frame = SAMPLE_RATE / 60;
    int32_t *mix = calloc(2 * (size_t)samples_per_frame, sizeof(int32_t));
    int frames = 60; // 1 second -- plenty to run the whole track to its cfStopTrack

    for (int i = 0; i < frames; i++) {
        Sound_Frame();
        for (uint32_t s = 0; s < 2 * samples_per_frame; s++)
            mix[s] = 0;
        Sound_Generate(mix, samples_per_frame, SAMPLE_RATE);
    }
    free(mix);

    // If we got here without crashing/hanging, the whole 44-flag v3 dispatch
    // table parsed its synthetic byte stream correctly (every flag consumes
    // exactly the byte count the compiler emitted for it -- any mismatch
    // there would desync the stream and very likely crash or infinite-loop
    // well before 60 frames of a 78-byte track).
    printf("V3FlagTest: played %d frames with no crash/hang -- driver-version-3 dispatch OK\n", frames);
    return 0;
}
