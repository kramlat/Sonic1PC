#include "test.h"

#include <string.h>

#include "Level.h"
#include "Object.h"
#include "Oscillatory Routines.h"

// The prototype's oscillators (OscillateNumInit and Oscillate_Num_Do): 16 values that swing up and down with their own rate of change and amplitude, which platforms, saws and water read. Sonic 2's differ from
// Sonic 1's in the start values of entries 9 and 15, and in the amplitudes of entries 8, 9, 14 and 15; the reference below is the prototype's own tables, and a frame of its routine written out.

// Osc_Data (the start values: control word, then a value and a rate for each) and Oscillate_Data2 (a rate of change and an amplitude each)
static const uint16_t proto_control = 0x7D;
static const uint16_t proto_start[16][2] = {
    { 0x80, 0 }, { 0x80, 0 }, { 0x80, 0 }, { 0x80, 0 }, { 0x80, 0 }, { 0x80, 0 }, { 0x80, 0 }, { 0x80, 0 },
    { 0x80, 0 }, { 0x3848, 0xEE }, { 0x2080, 0xB4 }, { 0x3080, 0x10E }, { 0x5080, 0x1C2 }, { 0x7080, 0x276 }, { 0x80, 0 }, { 0x4000, 0xFE },
};
static const uint16_t proto_step[16][2] = {
    { 2, 0x10 }, { 2, 0x18 }, { 2, 0x20 }, { 2, 0x30 }, { 4, 0x20 }, { 8, 8 }, { 8, 0x40 }, { 4, 0x40 },
    { 2, 0x38 }, { 2, 0x38 }, { 2, 0x20 }, { 3, 0x30 }, { 5, 0x50 }, { 7, 0x70 }, { 2, 0x40 }, { 2, 0x40 },
};

typedef struct {
    uint16_t control;
    uint16_t value[16], rate[16];
} Ref;

static void RefInit(Ref *r) {
    r->control = proto_control;
    for (int i = 0; i < 16; i++) {
        r->value[i] = proto_start[i][0];
        r->rate[i] = proto_start[i][1];
    }
}

// Oscillate_Num_Do: d1 counts 15 down to 0, the entry's control bit is bit d1; going up adds the rate of change to the rate and the rate to the value, and goes down when the amplitude is no longer above
// the value's high byte (cmp.b 0(a1),d4 / bhi); going down subtracts it, and goes up again when it is
static void RefDo(Ref *r) {
    for (int i = 0; i < 16; i++) {
        int bit = 15 - i;
        uint16_t add = proto_step[i][0], amp = proto_step[i][1];
        if (!(r->control & (1 << bit))) {
            r->rate[i] = (uint16_t)(r->rate[i] + add);
            r->value[i] = (uint16_t)(r->value[i] + r->rate[i]);
            if (!(amp > (r->value[i] >> 8)))
                r->control |= (uint16_t)(1 << bit);
        } else {
            r->rate[i] = (uint16_t)(r->rate[i] - add);
            r->value[i] = (uint16_t)(r->value[i] + r->rate[i]);
            if (amp > (r->value[i] >> 8))
                r->control &= (uint16_t)~(1 << bit);
        }
    }
}

static void Alive(void) {
    memset(player, 0, sizeof(Object));
    player->routine = 2;
}

static void Oscillation_StartsAsThePrototypes(void) {
    OscillateNumInit();
    CHECK_EQ(oscillatory.direction, proto_control);
    for (int i = 0; i < 16; i++) {
        CHECK_EQ(oscillatory.state[i][0], proto_start[i][0]);
        CHECK_EQ(oscillatory.state[i][1], proto_start[i][1]);
    }
}

// Entry 15 starts heading down: its rate goes from $FE to $FC on the first frame, not up to $100
static void Oscillation_EntryFifteenStartsHeadingDown(void) {
    Alive();
    OscillateNumInit();
    OscillateNumDo();
    CHECK_EQ(oscillatory.state[15][1], 0xFC);
    CHECK_EQ(oscillatory.state[15][0], (uint16_t)(0x4000 + 0xFC));
}

static void Oscillation_FollowsThePrototypeForAThousandFrames(void) {
    Alive();
    OscillateNumInit();
    Ref ref;
    RefInit(&ref);
    for (int frame = 0; frame < 1000; frame++) {
        OscillateNumDo();
        RefDo(&ref);
        CHECK_EQ(oscillatory.direction, ref.control);
        for (int i = 0; i < 16; i++) {
            if (oscillatory.state[i][0] != ref.value[i] || oscillatory.state[i][1] != ref.rate[i]) {
                CHECK_EQ(oscillatory.state[i][0], ref.value[i]);
                CHECK_EQ(oscillatory.state[i][1], ref.rate[i]);
                return;
            }
        }
    }
}

// Sonic 2's amplitudes, seen from outside: the highest the high byte of entries 8, 9, 14 and 15 reaches in 400 frames ($6F, $6F, $7E and $7E; Sonic 1's table gives $A0, $A0, $1F and $1F)
static void Oscillation_EntriesEightNineFourteenAndFifteenReachTheirOwnHeights(void) {
    Alive();
    OscillateNumInit();
    uint8_t high[16] = { 0 };
    for (int frame = 0; frame < 400; frame++) {
        OscillateNumDo();
        for (int i = 0; i < 16; i++) {
            uint8_t v = (uint8_t)(oscillatory.state[i][0] >> 8);
            if (v > high[i])
                high[i] = v;
        }
    }
    CHECK_EQ(high[8], 0x6F);
    CHECK_EQ(high[9], 0x6F);
    CHECK_EQ(high[14], 0x7E);
    CHECK_EQ(high[15], 0x7E);
    CHECK_EQ(high[0], 0x1F); // (the others are as in Sonic 1)
    CHECK_EQ(high[6], 0x80);
}

// While Sonic is dying (routine 6 and over) nothing moves
static void Oscillation_StopsWhileSonicIsDying(void) {
    Alive();
    OscillateNumInit();
    OscillateNumDo();
    uint16_t value = oscillatory.state[3][0], control = oscillatory.direction;
    player->routine = 6;
    for (int i = 0; i < 20; i++)
        OscillateNumDo();
    CHECK_EQ(oscillatory.state[3][0], value);
    CHECK_EQ(oscillatory.direction, control);
    player->routine = 5; // (alive again)
    OscillateNumDo();
    CHECK(oscillatory.state[3][0] != value);
}

// Each entry turns round: a value that goes up comes back down (and the other way)
static void Oscillation_EveryEntryTurnsRound(void) {
    Alive();
    OscillateNumInit();
    uint16_t seen_flip[16] = { 0 };
    uint16_t prev = oscillatory.direction;
    for (int frame = 0; frame < 2000; frame++) {
        OscillateNumDo();
        seen_flip[0] |= (uint16_t)(prev ^ oscillatory.direction);
        prev = oscillatory.direction;
    }
    CHECK_EQ(seen_flip[0], 0xFFFF);
}

void RegisterOscillationTests(void) {
    RUN_TEST(Oscillation_StartsAsThePrototypes);
    RUN_TEST(Oscillation_EntryFifteenStartsHeadingDown);
    RUN_TEST(Oscillation_FollowsThePrototypeForAThousandFrames);
    RUN_TEST(Oscillation_EntriesEightNineFourteenAndFifteenReachTheirOwnHeights);
    RUN_TEST(Oscillation_StopsWhileSonicIsDying);
    RUN_TEST(Oscillation_EveryEntryTurnsRound);
}
