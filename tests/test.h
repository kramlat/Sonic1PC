#pragma once

// Minimal, dependency-free test framework. Each test function returns void
// and uses the CHECK_* macros below; failures are recorded and printed, but
// do not stop the test (so one test function can report multiple failures).
//
// A test file defines its cases as `static void MyTest(void) { ... }` and
// registers them from a `void RegisterXTests(void)` function that calls
// RUN_TEST(MyTest) for each one. test_main.c calls each RegisterXTests().

#include <stdio.h>

extern int test_failures;
extern const char *test_current_name;

#define RUN_TEST(fn)                                                        \
    do {                                                                    \
        test_current_name = #fn;                                           \
        printf("  %s ... ", #fn);                                          \
        int before = test_failures;                                        \
        fn();                                                              \
        printf("%s\n", test_failures == before ? "ok" : "FAILED");         \
    } while (0)

#define CHECK(cond)                                                         \
    do {                                                                    \
        if (!(cond)) {                                                     \
            test_failures++;                                               \
            printf("\n    CHECK failed at %s:%d: %s", __FILE__, __LINE__,  \
                   #cond);                                                 \
        }                                                                   \
    } while (0)

#define CHECK_EQ(a, b)                                                      \
    do {                                                                    \
        long long va = (long long)(a);                                    \
        long long vb = (long long)(b);                                    \
        if (va != vb) {                                                    \
            test_failures++;                                               \
            printf("\n    CHECK_EQ failed at %s:%d: %s (%lld) != %s (%lld)", \
                   __FILE__, __LINE__, #a, va, #b, vb);                    \
        }                                                                   \
    } while (0)
