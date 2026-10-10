#include "test.h"

#include <stdlib.h>

int test_failures = 0;
const char *test_current_name = NULL;

void RegisterCompressionTests(void);
void RegisterPaletteTests(void);
void RegisterRenderFlagsTests(void);
void RegisterVdp8bppTests(void);
void RegisterTileBankTests(void);
void RegisterMathTests(void);
void RegisterPLCTests(void);
void RegisterLevelDrawTests(void);
void RegisterLevelCollisionTests(void);
void RegisterPrisonTests(void);
void RegisterLZ3WallTests(void);
void RegisterFMAlgorithmTests(void);
void RegisterObjectsManagerTests(void);
void RegisterRingsManagerTests(void);
void RegisterSpritesTests(void);
void RegisterVDPSplitTests(void);
void RegisterLevelPlaneTests(void);
void RegisterBossLZTests(void);
void RegisterBossSLZTests(void);
void RegisterScrapEggmanTests(void);
void RegisterBossFinalTests(void);
void RegisterSpecialStageTests(void);
void RegisterEndingTests(void);
void RegisterPointsTests(void);
void RegisterLZConveyorTests(void);
void RegisterLZRaftTests(void);
void RegisterPushBlockTests(void);
void RegisterLZBlocksTests(void);
void RegisterDrownCountTests(void);
void RegisterRollerTests(void);
void RegisterSpikeBallTests(void);
void RegisterStaircaseTests(void);
void RegisterOscillatorTests(void);
void RegisterSceneryTests(void);
void RegisterAudioMuteTests(void);
void RegisterDemoRecordTests(void);
void RegisterSoundDriverTests(void);
void RegisterObjCollisionTests(void);
void RegisterWaterSplitTests(void);
void RegisterGiantRingTests(void);
void RegisterCheckpointTests(void);
void RegisterSpringTests(void);
void RegisterSpikesTests(void);

int main(void) {
    printf("Compression tests:\n");
    RegisterCompressionTests();
    printf("Palette tests:\n");
    RegisterPaletteTests();
    printf("Render flag tests:\n");
    RegisterRenderFlagsTests();
    printf("VDP 8bpp tile tests:\n");
    RegisterVdp8bppTests();
    RegisterTileBankTests();
    printf("Math tests:\n");
    RegisterMathTests();
    printf("PLC tests:\n");
    RegisterPLCTests();

    printf("LevelDraw tests:\n");
    RegisterLevelDrawTests();

    printf("LevelCollision tests:\n");
    RegisterLevelCollisionTests();

    printf("Prison capsule tests:\n");
    RegisterPrisonTests();

    printf("LZ3 wall switch tests:\n");
    RegisterLZ3WallTests();
    RegisterFMAlgorithmTests();
    RegisterObjectsManagerTests();
    RegisterRingsManagerTests();
    RegisterSpritesTests();
    RegisterVDPSplitTests();
    RegisterLevelPlaneTests();

    printf("LZ boss tests:\n");
    RegisterBossLZTests();
    RegisterBossSLZTests();
    RegisterScrapEggmanTests();
    RegisterBossFinalTests();
    RegisterSpecialStageTests();
    RegisterEndingTests();
    RegisterPointsTests();

    printf("LZ conveyor tests:\n");
    RegisterLZConveyorTests();

    printf("LZ raft tests:\n");
    RegisterLZRaftTests();
    RegisterPushBlockTests();
    RegisterLZBlocksTests();
    RegisterDrownCountTests();
    RegisterRollerTests();
    RegisterSpikeBallTests();
    RegisterStaircaseTests();
    RegisterOscillatorTests();
    RegisterSceneryTests();
    RegisterAudioMuteTests();
    RegisterDemoRecordTests();
    RegisterSoundDriverTests();

    printf("Object collision tests:\n");
    RegisterObjCollisionTests();
    RegisterWaterSplitTests();

    printf("Giant ring tests:\n");
    RegisterGiantRingTests();

    printf("Checkpoint tests:\n");
    RegisterCheckpointTests();

    printf("Spring tests:\n");
    RegisterSpringTests();

    printf("Spikes tests:\n");
    RegisterSpikesTests();

    printf("\n%s\n", test_failures == 0 ? "All tests passed." : "Some tests FAILED.");
    return test_failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
