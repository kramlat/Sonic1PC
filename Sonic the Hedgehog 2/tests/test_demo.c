#include "test.h"

#include "Demo.h"
#include "Game.h"
#include "Level.h"
#include "SplitScreen.h"

// The prototype's Emerald Hill attract demo is a two player one: Tails plays pad 2 from a demo of his own (Demo_Tails_Ghz: $0046 frames of nothing, $1E of right, ...).

static void StartDemo(void) {
    level_id = LEVEL_ID(ZoneId_EHZ, 0);
    gamemode = GameMode_Demo;
    demo = 1;
    two_player_mode = 1;
    cli_demo_override = NULL;
    btn_pushtime1 = 0;
    btn_pushtime2 = (uint8_t)(intro_demo_ptr[ZoneId_EHZ][1] - 1);
    jpad1_hold1 = jpad1_press1 = jpad2_hold = jpad2_press = 0;
}

static void Demo_TailsPlaysPadTwoInTheEmeraldHillDemo(void) {
    StartDemo();
    int zero_frames = 0;
    while (zero_frames < 200) {
        MoveSonicInDemo();
        if (jpad2_hold != 0)
            break;
        zero_frames++;
    }
    CHECK_EQ(zero_frames, 0x46);          // (the first record lasts its duration byte: the counter starts one less; the others last their byte + 1)
    CHECK_EQ(jpad2_hold, 0x08);           // (then right: $081E)
    CHECK_EQ(jpad2_press, 0x08);
    int right_frames = 1;
    for (;;) {
        MoveSonicInDemo();
        if (jpad2_hold != 0x08)
            break;
        right_frames++;
    }
    CHECK_EQ(right_frames, 0x1E + 1);
    CHECK_EQ(jpad2_hold, 0x28);           // (and then right with C: $280A)
}

static void Demo_PadTwoIsLeftAloneInOtherZonesAndInOnePlayerMode(void) {
    StartDemo();
    two_player_mode = 0;
    jpad2_hold = 0x55;
    MoveSonicInDemo();
    CHECK_EQ(jpad2_hold, 0x55);

    StartDemo();
    level_id = LEVEL_ID(ZoneId_CPZ, 0);
    jpad2_hold = 0x55;
    MoveSonicInDemo();
    CHECK_EQ(jpad2_hold, 0x55);
}

void RegisterDemoTests(void) {
    RUN_TEST(Demo_TailsPlaysPadTwoInTheEmeraldHillDemo);
    RUN_TEST(Demo_PadTwoIsLeftAloneInOtherZonesAndInOnePlayerMode);
}
