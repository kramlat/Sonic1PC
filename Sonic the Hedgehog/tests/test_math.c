// Tests of the engine's maths, shared by both games' test programs: the sine table (and the odd values the prototype's own table has), the angle of a vector (CalcAngle, the arctangent through a table) and the
// pseudo-random number generator, which is checked against the 68000's routine written out step by step.
#include "test.h"

#include "MathUtil.h"

static void Math_SineAtTheQuarterTurns(void) {
    CHECK_EQ(GetSin(0x00), 0);
    CHECK_EQ(GetSin(0x40), 0x100);
    CHECK_EQ(GetSin(0x80), 0);
    CHECK_EQ(GetSin(0xC0), -0x100);
    CHECK_EQ(GetCos(0x00), 0x100);
    CHECK_EQ(GetCos(0x40), 0);
    CHECK_EQ(GetCos(0x80), -0x100);
    CHECK_EQ(GetCos(0xC0), 0);
}

static void Math_SineKnownValues(void) {
    CHECK_EQ(GetSin(1), 6);
    CHECK_EQ(GetSin(0x10), 0x61);
    CHECK_EQ(GetSin(0x20), 0xB5); // (a half of the square root of two, times 256)
    CHECK_EQ(GetSin(0x30), 0xEC);
    CHECK_EQ(GetSin(0xE0), -0xB5);
}

static void Math_CalcSineGivesBothAndAcceptsNull(void) {
    int16_t s = 99, c = 99;
    CalcSine(0x20, &s, &c);
    CHECK_EQ(s, 0xB5);
    CHECK_EQ(c, 0xB5);
    CalcSine(0x40, &s, NULL);
    CHECK_EQ(s, 0x100);
    CalcSine(0x40, NULL, &c);
    CHECK_EQ(c, 0);
    CalcSine(0, NULL, NULL); // (no crash)
}

static void Math_CosineIsTheSineAQuarterTurnOn(void) {
    for (int a = 0; a < 256; a++)
        CHECK_EQ(GetCos((uint8_t)a), GetSin((uint8_t)(a + 0x40)));
}

static void Math_FirstQuarterRisesToOne(void) {
    for (int a = 0; a < 0x40; a++)
        CHECK(GetSin((uint8_t)a) <= GetSin((uint8_t)(a + 1)));
    for (int a = 0x40; a < 0x80; a++)
        CHECK(GetSin((uint8_t)a) >= GetSin((uint8_t)(a + 1)));
}

// The sine of the second half is the first's negated, except in two places where the table (Sonic 1's, and the prototype's: the same values) has a value one step off: $13 is $73 and $93 is -$75 (and $6D / $ED the same
// way). Those are the original's, so they stay
static void Math_SineIsOddExceptWhereTheOriginalIsOff(void) {
    int off = 0;
    for (int a = 0; a < 256; a++)
        if (GetSin((uint8_t)(a + 0x80)) != -GetSin((uint8_t)a)) {
            off++;
            CHECK(a == 0x13 || a == 0x93 || a == 0x6D || a == 0xED);
        }
    CHECK_EQ(off, 4);
    CHECK_EQ(GetSin(0x13), 0x73);
    CHECK_EQ(GetSin(0x93), -0x75);
}

// The sine's own square plus the cosine's is a circle of radius 256, to within the table's rounding
static void Math_SineSquaredPlusCosineSquaredIsOne(void) {
    for (int a = 0; a < 256; a++) {
        int s = GetSin((uint8_t)a), c = GetCos((uint8_t)a);
        int r2 = s * s + c * c;
        CHECK(r2 > 64900 && r2 < 66100); // (within 1% of 65536)
    }
}

// --- CalcAngle ---

static void Math_AngleOfTheOriginIsStraightUp(void) {
    CHECK_EQ(CalcAngle(0, 0), 0x40);
}

static void Math_AngleOfTheAxes(void) {
    CHECK_EQ(CalcAngle(1, 0), 0x00);
    CHECK_EQ(CalcAngle(0, 1), 0x40);
    CHECK_EQ(CalcAngle(-1, 0), 0x80);
    CHECK_EQ(CalcAngle(0, -1), 0xC0);
    CHECK_EQ(CalcAngle(1000, 0), 0x00);
    CHECK_EQ(CalcAngle(0, -1000), 0xC0);
}

static void Math_AngleOfTheDiagonals(void) {
    CHECK_EQ(CalcAngle(5, 5), 0x20);
    CHECK_EQ(CalcAngle(-5, 5), 0x60);
    CHECK_EQ(CalcAngle(-5, -5), 0xA0);
    CHECK_EQ(CalcAngle(5, -5), 0xE0);
}

// The angle depends on the ratio only
static void Math_AngleIgnoresTheScale(void) {
    CHECK_EQ(CalcAngle(3, 4), CalcAngle(30, 40));
    CHECK_EQ(CalcAngle(-3, 4), CalcAngle(-300, 400));
}

// CalcAngle of the sine and cosine of an angle gives the angle back, to within a step or two (the table is coarse where the curve is flat)
static void Math_AngleOfTheSineAndCosineGivesTheAngleBack(void) {
    for (int a = 0; a < 256; a++) {
        uint16_t got = CalcAngle(GetCos((uint8_t)a), GetSin((uint8_t)a));
        int diff = (int)(uint8_t)(got - a);
        if (diff > 128)
            diff -= 256;
        CHECK(diff >= -2 && diff <= 2);
    }
}

// --- RandomNumber ---

// The prototype's PseudoRandomNumber (loc_31E4), a 68000 routine, step by step: d1 = seed (or $2A6D365A if 0); d0 = d1; d1 <<= 2; d1 += d0; d1 <<= 3; d1 += d0; d0.w = d1.w; swap d1; d0.w += d1.w;
// d1.w = d0.w; swap d1; seed = d1; the result is d0
static uint32_t ref_seed;
static uint32_t Ref_Random(void) {
    uint32_t d1 = ref_seed;
    if (d1 == 0)
        d1 = 0x2A6D365A;
    uint32_t d0 = d1;
    d1 <<= 2;
    d1 += d0;
    d1 <<= 3;
    d1 += d0;
    d0 = (d0 & 0xFFFF0000u) | (d1 & 0xFFFF);
    d1 = (d1 << 16) | (d1 >> 16);
    d0 = (d0 & 0xFFFF0000u) | (uint16_t)((d0 & 0xFFFF) + (d1 & 0xFFFF));
    d1 = (d1 & 0xFFFF0000u) | (d0 & 0xFFFF);
    d1 = (d1 << 16) | (d1 >> 16);
    ref_seed = d1;
    return d0;
}

static void Math_RandomMatchesTheOriginalsRoutine(void) {
    random_seed.v = ref_seed = 0x12345678;
    for (int i = 0; i < 200; i++) {
        uint32_t want = Ref_Random();
        uint32_t got = RandomNumber();
        CHECK_EQ(got, want);
        CHECK_EQ(random_seed.v, ref_seed);
    }
}

static void Math_RandomSeedsItselfWhenTheSeedIsZero(void) {
    random_seed.v = ref_seed = 0;
    uint32_t want = Ref_Random();
    CHECK_EQ(RandomNumber(), want);
    CHECK(random_seed.v != 0);
}

static void Math_RandomIsRepeatableFromTheSameSeed(void) {
    random_seed.v = 0xCAFE;
    uint32_t a[8], b[8];
    for (int i = 0; i < 8; i++)
        a[i] = RandomNumber();
    random_seed.v = 0xCAFE;
    for (int i = 0; i < 8; i++)
        b[i] = RandomNumber();
    for (int i = 0; i < 8; i++)
        CHECK_EQ(a[i], b[i]);
}

static void Math_RandomIsNotStuck(void) {
    random_seed.v = 1;
    uint32_t first = RandomNumber();
    int different = 0;
    for (int i = 0; i < 50; i++)
        different += RandomNumber() != first;
    CHECK(different > 45);
}

void RegisterMathTests(void) {
    RUN_TEST(Math_SineAtTheQuarterTurns);
    RUN_TEST(Math_SineKnownValues);
    RUN_TEST(Math_CalcSineGivesBothAndAcceptsNull);
    RUN_TEST(Math_CosineIsTheSineAQuarterTurnOn);
    RUN_TEST(Math_FirstQuarterRisesToOne);
    RUN_TEST(Math_SineIsOddExceptWhereTheOriginalIsOff);
    RUN_TEST(Math_SineSquaredPlusCosineSquaredIsOne);
    RUN_TEST(Math_AngleOfTheOriginIsStraightUp);
    RUN_TEST(Math_AngleOfTheAxes);
    RUN_TEST(Math_AngleOfTheDiagonals);
    RUN_TEST(Math_AngleIgnoresTheScale);
    RUN_TEST(Math_AngleOfTheSineAndCosineGivesTheAngleBack);
    RUN_TEST(Math_RandomMatchesTheOriginalsRoutine);
    RUN_TEST(Math_RandomSeedsItselfWhenTheSeedIsZero);
    RUN_TEST(Math_RandomIsRepeatableFromTheSameSeed);
    RUN_TEST(Math_RandomIsNotStuck);
}
