#include "test.h"

#include <string.h>

#include "../fmcore/fm_voice.h"

// FM algorithms 8-F are fmcore's own routings, only defaults: a game may replace them (FMVoice_SetCustomAlgorithm), the chip's real 0-7 never change.

static void FMAlgorithm_DefaultsAreInForce(void) {
    FMVoice_ResetCustomAlgorithms();
    for (int a = 0; a < FM_VOICE_ALGORITHM_COUNT; a++)
        for (int from = 0; from < FM_VOICE_OP_COUNT; from++)
            for (int to = 0; to <= FM_VOICE_OP_COUNT; to++)
                CHECK_EQ(FMVoice_AlgorithmRoutes(a, from, to), FM_VOICE_ALGORITHM_DEFAULT[a][from][to]);
}

static void FMAlgorithm_HardwareOnesCannotBeReplaced(void) {
    FMAlgorithmRoute silent;
    memset(silent, 0, sizeof(silent));
    for (int a = 0; a < FM_VOICE_HARDWARE_ALGORITHMS; a++) {
        CHECK(!FMVoice_SetCustomAlgorithm(a, silent));
        CHECK(FMVoice_AlgorithmRoutes(a, 3, FM_VOICE_OUT) || FMVoice_AlgorithmRoutes(a, 1, FM_VOICE_OUT) || FMVoice_AlgorithmRoutes(a, 0, FM_VOICE_OUT));
    }
    CHECK(!FMVoice_SetCustomAlgorithm(FM_VOICE_ALGORITHM_COUNT, silent));
    CHECK(!FMVoice_SetCustomAlgorithm(-1, silent));
}

static void FMAlgorithm_OverrideReachesVoiceAndCarriers(void) {
    // Operator 1 alone audible, feeding nothing.
    FMAlgorithmRoute only_first;
    memset(only_first, 0, sizeof(only_first));
    only_first[0][FM_VOICE_OUT] = true;

    CHECK(FMVoice_SetCustomAlgorithm(9, only_first));
    CHECK(FMVoice_AlgorithmCarrier(9, 0));
    CHECK(!FMVoice_AlgorithmCarrier(9, 1));
    CHECK(!FMVoice_AlgorithmCarrier(9, 3));

    FMVoice voice;
    FMVoice_Init(&voice);
    FMVoice_LoadAlgorithm(&voice, 9);
    CHECK_EQ(memcmp(voice.connect, only_first, sizeof(only_first)), 0);

    // Another algorithm is untouched, and a reset puts the default back.
    CHECK_EQ(FMVoice_AlgorithmRoutes(10, 0, 3), FM_VOICE_ALGORITHM_DEFAULT[10][0][3]);
    FMVoice_ResetCustomAlgorithm(9);
    for (int from = 0; from < FM_VOICE_OP_COUNT; from++)
        for (int to = 0; to <= FM_VOICE_OP_COUNT; to++)
            CHECK_EQ(FMVoice_AlgorithmRoutes(9, from, to), FM_VOICE_ALGORITHM_DEFAULT[9][from][to]);
}

static void FMAlgorithm_CarriersMatchTheRealFMSlotMask(void) {
    // The original driver's FMSlotMask for algorithms 0-7 (bit k = operator k+1)
    static const uint8_t real[8] = {8, 8, 8, 8, 0xA, 0xE, 0xE, 0xF};
    for (int a = 0; a < 8; a++) {
        int mask = 0;
        for (int op = 0; op < FM_VOICE_OP_COUNT; op++)
            mask |= FMVoice_AlgorithmCarrier(a, op) << op;
        CHECK_EQ(mask, real[a]);
    }
}

void RegisterFMAlgorithmTests(void) {
    RUN_TEST(FMAlgorithm_DefaultsAreInForce);
    RUN_TEST(FMAlgorithm_HardwareOnesCannotBeReplaced);
    RUN_TEST(FMAlgorithm_OverrideReachesVoiceAndCarriers);
    RUN_TEST(FMAlgorithm_CarriersMatchTheRealFMSlotMask);
}
