#include "test.h"

#include <stdlib.h>

int test_failures = 0;
const char *test_current_name = NULL;

void RegisterPLCTests(void);
void RegisterLevelDrawTests(void);
void RegisterLevelCollisionTests(void);
void RegisterPrisonTests(void);
void RegisterLZ3WallTests(void);
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

int main(void) {
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

    printf("\n%s\n", test_failures == 0 ? "All tests passed." : "Some tests FAILED.");
    return test_failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
