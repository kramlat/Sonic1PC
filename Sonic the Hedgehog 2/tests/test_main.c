// Sonic 2's unit tests: the same dependency-free harness as Sonic 1's (its test.h), linked against Sonic2Core, so they exercise the real production code of the prototype port (the Simon Wai
// prototype's tables, objects and level events) rather than reimplementing it.
#include "test.h"

#include <stdlib.h>

int test_failures = 0;
const char *test_current_name = NULL;

void RegisterCompressionTests(void);
void RegisterLevelDataTests(void);
void RegisterObjectCoverageTests(void);
void RegisterSolidTests(void);
void RegisterHTZQuakeTests(void);
void RegisterHTZFloorTests(void);
void RegisterHTZFireballTests(void);
void RegisterHTZBackgroundTests(void);
void RegisterWaterTests(void);
void RegisterSignpostTests(void);
void RegisterDemoTests(void);
void RegisterCPZObjectTests(void);
void RegisterNGHZObjectTests(void);
void RegisterHTZObjectTests(void);
void RegisterCameraTests(void);

int main(void) {
    printf("Compression tests:\n");
    RegisterCompressionTests();
    printf("Level data tests:\n");
    RegisterLevelDataTests();
    printf("Object coverage tests:\n");
    RegisterObjectCoverageTests();
    printf("Solid object tests:\n");
    RegisterSolidTests();
    printf("Hill Top earthquake tests:\n");
    RegisterHTZQuakeTests();
    printf("Hill Top breakable floor tests:\n");
    RegisterHTZFloorTests();
    printf("Hill Top fireball tests:\n");
    RegisterHTZFireballTests();
    printf("Hill Top background tests:\n");
    RegisterHTZBackgroundTests();
    printf("Water tests:\n");
    RegisterWaterTests();
    printf("Signpost tests:\n");
    RegisterSignpostTests();
    printf("Chemical Plant object tests:\n");
    RegisterCPZObjectTests();
    printf("Neo Green Hill object tests:\n");
    RegisterNGHZObjectTests();
    printf("Hill Top object tests:\n");
    RegisterHTZObjectTests();
    printf("Camera tests:\n");
    RegisterCameraTests();
    printf("Demo tests:\n");
    RegisterDemoTests();

    if (test_failures) {
        printf("\n%d check(s) FAILED\n", test_failures);
        return EXIT_FAILURE;
    }
    printf("\nAll tests passed.\n");
    return EXIT_SUCCESS;
}
