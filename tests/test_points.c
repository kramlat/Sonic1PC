#include "test.h"

#include "Level.h"

// The score counts in tens (the HUD draws a fixed 0 after it), so AddPoints takes points/10: a badnik is
// AddPoints(10) (100 points), a boss AddPoints(100) (1000 points), and an extra life comes every 50000 points,
// i.e. every 5000 on the counter.
static void Points_ScoreCountsInTens(void) {
    score = 0;
    score_life = 5000;
    lives = 3;
    AddPoints(100);
    CHECK_EQ(score, 100);
}

static void Points_ExtraLifeEveryFiftyThousandPoints(void) {
    score = 0;
    score_life = 5000;
    lives = 3;

    for (int i = 0; i < 49; i++)
        AddPoints(100); // 49,000 points: nothing yet
    CHECK_EQ(lives, 3);

    AddPoints(100); // 50,000
    CHECK_EQ(lives, 4);
    CHECK_EQ(score_life, 10000);

    for (int i = 0; i < 49; i++)
        AddPoints(100); // 99,000
    CHECK_EQ(lives, 4);
    AddPoints(100); // 100,000
    CHECK_EQ(lives, 5);
}

void RegisterPointsTests(void) {
    RUN_TEST(Points_ScoreCountsInTens);
    RUN_TEST(Points_ExtraLifeEveryFiftyThousandPoints);
}
