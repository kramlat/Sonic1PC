#include "test.h"

#include <stdlib.h>

int test_failures = 0;
const char *test_current_name = NULL;

void RegisterPLCTests(void);
void RegisterLevelDrawTests(void);
void RegisterLevelCollisionTests(void);

int main(void) {
    printf("PLC tests:\n");
    RegisterPLCTests();

    printf("LevelDraw tests:\n");
    RegisterLevelDrawTests();

    printf("LevelCollision tests:\n");
    RegisterLevelCollisionTests();

    printf("\n%s\n", test_failures == 0 ? "All tests passed." : "Some tests FAILED.");
    return test_failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
