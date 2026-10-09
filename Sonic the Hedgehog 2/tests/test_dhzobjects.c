#include "test.h"

#include <string.h>

#include "Level.h"
#include "Object.h"
#include "Object/DHZObjects.h"
#include "Object/Sonic.h"
#include "Object/Tails.h"

// Dust Hill's objects (and the collapsing platform it shares with Oil Ocean): the platform that breaks, the stomper, the switch and the drawbridge it lowers, the boxes on their path, the chain of spiked balls and
// the platform with spikes at its sides.

void Obj_CollapsingPlatform(Object *obj);

static void Reset(int zone) {
    memset(objects, 0, sizeof(Object) * 0x60);
    memset(f_switch, 0, sizeof(f_switch));
    level_id = LEVEL_ID(zone, 0);
    scrpos_x.v = scrpos_y.v = 0;
    scrpos_x.f.u = 0;
    limit_btm2 = 0x720;
    player->routine = 2;
    player->y_rad = 19;
    player->pos.l.x.f.u = 0x1000;
    player->pos.l.y.f.u = 0x1000;
}

static Object *Spawn(uint8_t type, uint8_t subtype, int16_t x, int16_t y) {
    Object *obj = &objects[0x20];
    memset(obj, 0, sizeof(Object));
    obj->type = type;
    obj->scratch.u8[0] = subtype;
    obj->pos.l.x.f.u = x;
    obj->pos.l.y.f.u = y;
    return obj;
}

// Sonic standing on a flat top at `top`
static void StandOn(int16_t x, int16_t top) {
    player->pos.l.x.f.u = x;
    player->pos.l.y.f.u = (int16_t)(top - player->y_rad);
    player->status.p.f.in_air = false;
    player->status.p.f.object_stand = true;
}

static int CountOf(uint8_t type, uint8_t routine) {
    int n = 0;
    for (int i = 0x20; i < 0x60; i++)
        n += objects[i].type == type && objects[i].routine == routine;
    return n;
}

// --- 1F ---

static void DHZ_TheCollapsingPlatformBreaksIntoSixPiecesOnceStoodOn(void) {
    Reset(ZoneId_MCZ);
    Object *obj = Spawn(0x1F, 0, 200, 400);
    Obj_CollapsingPlatform(obj);
    CHECK_EQ(obj->width_pixels, 0x20);
    CHECK_EQ(obj->tile & 0x7FF, 0x3F4);
    for (int i = 0; i < 20; i++) { // (nobody on it: it stays)
        Obj_CollapsingPlatform(obj);
        CHECK_EQ(obj->routine, 2);
    }
    for (int i = 0; i < 30 && obj->routine != 6; i++) {
        StandOn(200, 400 - 0x10);
        Obj_CollapsingPlatform(obj);
    }
    CHECK_EQ(obj->routine, 6); // (it has let go)
    CHECK_EQ(obj->frame, 1);
    CHECK_EQ(CountOf(0x1F, 6), 6); // (the platform itself is the first piece)
    CHECK_EQ(obj->scratch.u8[0x10] <= 0x1A, true);
}

static void DHZ_TheCollapsingPlatformOfOilOceanIsWiderAndHasSevenPieces(void) {
    Reset(ZoneId_OOZ);
    Object *obj = Spawn(0x1F, 0, 200, 400);
    Obj_CollapsingPlatform(obj);
    CHECK_EQ(obj->width_pixels, 0x40);
    CHECK_EQ(obj->tile & 0x7FF, 0x39D);
    for (int i = 0; i < 30 && obj->routine != 6; i++) {
        StandOn(200, 400 - 0x10);
        Obj_CollapsingPlatform(obj);
    }
    CHECK_EQ(CountOf(0x1F, 6), 7);
}

// A piece falls when its delay has gone: the others wait
static void DHZ_PiecesFallOneAfterAnother(void) {
    Reset(ZoneId_MCZ);
    Object *obj = Spawn(0x1F, 0, 200, 400);
    Obj_CollapsingPlatform(obj);
    for (int i = 0; i < 30 && obj->routine != 6; i++) {
        StandOn(200, 400 - 0x10);
        Obj_CollapsingPlatform(obj);
    }
    int16_t y[0x60];
    for (int i = 0x20; i < 0x60; i++)
        y[i] = objects[i].pos.l.y.f.u;
    for (int frame = 0; frame < 14; frame++)
        for (int i = 0x20; i < 0x60; i++)
            if (objects[i].type == 0x1F && objects[i].routine == 6) {
                objects[i].render.f.on_screen = true; // (a piece that has fallen out of sight is gone: they are in sight here)
                Obj_CollapsingPlatform(&objects[i]);
            }
    int fallen = 0;
    for (int i = 0x21; i < 0x60; i++)
        fallen += objects[i].type == 0x1F && objects[i].pos.l.y.f.u > y[i];
    CHECK(fallen >= 1 && fallen < 5); // (only the piece with the shortest delay, $02, has begun to fall by now: $0A has not got going)
}

// --- 2A ---

static void DHZ_TheStomperRisesAPixelAFrameAndDropsEight(void) {
    Reset(ZoneId_MCZ);
    Object *obj = Spawn(0x2A, 0, 200, 500);
    Obj_DHZStomper(obj);
    CHECK_EQ(obj->pos.l.y.f.u, 499);
    for (int i = 1; i < 0x60; i++)
        Obj_DHZStomper(obj);
    CHECK_EQ(obj->pos.l.y.f.u, 500 - 0x60); // (at the top)
    Obj_DHZStomper(obj);
    CHECK_EQ(obj->pos.l.y.f.u, 500 - 0x58); // (and dropping)
    for (int i = 0; i < 11; i++)
        Obj_DHZStomper(obj);
    CHECK_EQ(obj->pos.l.y.f.u, 500); // (12 frames to the bottom)
    Obj_DHZStomper(obj);
    CHECK_EQ(obj->pos.l.y.f.u, 499); // (rising again)
}

// --- 47 and 77 ---

static void DHZ_TheSwitchIsDownWhileSomeoneStandsOnIt(void) {
    Reset(ZoneId_MCZ);
    Object *obj = Spawn(0x47, 3, 200, 400);
    Obj_Switch(obj);
    CHECK_EQ(obj->pos.l.y.f.u, 404); // (4 lower than where it is placed)
    obj->render.f.on_screen = true;
    StandOn(200, 404 - 5);
    Obj_Switch(obj);
    CHECK_EQ(f_switch[3], 1);
    CHECK_EQ(obj->frame, 1);
    player->pos.l.x.f.u = 0x1000; // (steps off)
    player->status.p.f.in_air = true;
    player->status.p.f.object_stand = false;
    Obj_Switch(obj);
    CHECK_EQ(f_switch[3], 0);
    CHECK_EQ(obj->frame, 0);
}

static void DHZ_ASwitchWithBitSixSetsTheTopBit(void) {
    Reset(ZoneId_MCZ);
    Object *obj = Spawn(0x47, 0x42, 200, 400);
    Obj_Switch(obj);
    obj->render.f.on_screen = true;
    StandOn(200, 404 - 5);
    Obj_Switch(obj);
    CHECK_EQ(f_switch[2], 0x80);
}

static void DHZ_TheDrawbridgeLowersAtStartAndRaisesOnceItsSwitchIsDown(void) {
    Reset(ZoneId_MCZ);
    Object *obj = Spawn(0x77, 5, 200, 400);
    for (int i = 0; i < 20; i++)
        Obj_DHZGate(obj);
    CHECK_EQ(obj->frame, 0); // (flat)
    f_switch[5] = 1;
    for (int i = 0; i < 20; i++)
        Obj_DHZGate(obj);
    CHECK_EQ(obj->anim, 1);
    CHECK_EQ(obj->frame, 2); // (up)
    f_switch[5] = 0;
    for (int i = 0; i < 20; i++)
        Obj_DHZGate(obj);
    CHECK_EQ(obj->frame, 2); // (only once)
}

static void DHZ_ARaisedDrawbridgeLetsGoOfWhoeverStandsOnIt(void) {
    Reset(ZoneId_MCZ);
    Object *obj = Spawn(0x77, 5, 200, 400);
    f_switch[5] = 1;
    for (int i = 0; i < 20; i++)
        Obj_DHZGate(obj);
    obj->status.b |= 1 << 3;
    player->status.p.f.object_stand = true;
    Obj_DHZGate(obj);
    CHECK(!player->status.p.f.object_stand);
    CHECK_EQ(obj->status.b & 0x18, 0);
}

// --- 6A ---

static void DHZ_TheBoxesMakeTwoMoreAndMoveOnAPath(void) {
    Reset(ZoneId_MCZ);
    Object *obj = Spawn(0x6A, 0x18, 200, 400);
    Obj_RotatingBoxes(obj);
    CHECK_EQ(CountOf(0x6A, 0), 2); // (the two others wait to start)
    CHECK_EQ(objects[0x21].pos.l.x.f.u, 200 + 0x40);
    CHECK_EQ(objects[0x21].pos.l.y.f.u, 400 + 0x40);
    CHECK_EQ(objects[0x22].pos.l.x.f.u, 200 - 0x40);
    CHECK_EQ(objects[0x21].scratch.u8[0], 6);
    CHECK_EQ(objects[0x22].scratch.u8[0], 0xC);
    CHECK_EQ(obj->xsp, 0x100); // (the parent starts on the step after the cycle's last: to the right)
    for (int i = 0; i < 0x40; i++)
        Obj_RotatingBoxes(obj);
    CHECK_EQ(obj->pos.l.x.f.u, 200 + 0x40); // (a pixel a frame for the step's $40 frames)
    CHECK_EQ(obj->pos.l.y.f.u, 400);
    CHECK_EQ(obj->ysp, 0x100); // (the first step: down)
    CHECK_EQ(obj->xsp, 0);
}

static void DHZ_FlippedBoxesTurnTheOtherWay(void) {
    Reset(ZoneId_MCZ);
    Object *obj = Spawn(0x6A, 0x18, 200, 400);
    obj->status.o.f.x_flip = true;
    Obj_RotatingBoxes(obj);
    CHECK_EQ(obj->xsp, -0x100);
    CHECK_EQ(objects[0x21].scratch.u8[0], 0xC); // (the one on the right swaps with the left)
    CHECK_EQ(objects[0x22].scratch.u8[0], 6);
}

// --- 75 ---

static void DHZ_TheSpikedBallIsAtTheEndOfItsChain(void) {
    Reset(ZoneId_MCZ);
    Object *obj = Spawn(0x75, 0x16, 200, 400); // 6 links, speed $80
    Obj_SpikeballChain(obj);
    CHECK_EQ(obj->col_type, 0x9A);
    CHECK_EQ(objects[0x21].child_count, 6);
    CHECK(objects[0x21].render.f.multi_sprite);
    CHECK_EQ(obj->pos.l.x.f.u, 200 + 6 * 16); // (the angle starts at 0: to the right)
    CHECK_EQ(obj->pos.l.y.f.u, 400);
    CHECK_EQ(objects[0x21].children[0].x, 200);
    CHECK_EQ(objects[0x21].children[5].x, 200 + 5 * 16);
    CHECK_EQ(objects[0x21].children[5].frame, 1);
}

static void DHZ_TheFlipOfTheObjectGivesTheStartingAngle(void) {
    Reset(ZoneId_MCZ);
    Object *obj = Spawn(0x75, 0x16, 200, 400);
    obj->status.b = 1; // x flip: ror #2 puts bit 0 at bit 6: a quarter turn
    Obj_SpikeballChain(obj);
    CHECK_EQ(obj->pos.l.x.f.u, 200); // (sine at $40: straight down)
    CHECK_EQ(obj->pos.l.y.f.u, 400 + 6 * 16);
}

static void DHZ_TheChainTurnsBySpeedTimesEightAFrame(void) {
    Reset(ZoneId_MCZ);
    Object *obj = Spawn(0x75, 0x16, 200, 400);
    Obj_SpikeballChain(obj);
    for (int i = 0; i < 128; i++) // 128 frames of $80 is a quarter of the angle's range, and the first frame's: straight down
        Obj_SpikeballChain(obj);
    CHECK_EQ(obj->pos.l.x.f.u, 200);
    CHECK_EQ(obj->pos.l.y.f.u, 400 + 6 * 16);
    for (int i = 0; i < 128; i++)
        Obj_SpikeballChain(obj);
    CHECK_EQ(obj->pos.l.x.f.u, 200 - 6 * 16); // (and then to the left)
    CHECK_EQ(obj->pos.l.y.f.u, 400);
}

static void DHZ_ASubtypeOfLowNibbleFIsASolidBlock(void) {
    Reset(ZoneId_MCZ);
    Object *obj = Spawn(0x75, 0x0F, 200, 400);
    Obj_SpikeballChain(obj);
    CHECK_EQ(obj->frame, 2);
    CHECK_EQ(obj->routine, 4);
    CHECK_EQ(obj->col_type, 0);
    CHECK_EQ(objects[0x21].type, 0); // (no chain)
}

// --- 76 ---

static void DHZ_ThePlatformWithSpikesSlidesAwayWhenSomeoneComesNear(void) {
    Reset(ZoneId_MCZ);
    Object *obj = Spawn(0x76, 0, 300, 400);
    Obj_PlatformSpikes(obj);
    CHECK_EQ(obj->width_pixels, 0x40);
    for (int i = 0; i < 5; i++)
        Obj_PlatformSpikes(obj);
    CHECK_EQ(obj->pos.l.x.f.u, 300); // (nobody near)
    player->pos.l.x.f.u = 300 - 0x80; // within the strip $C0 to $41 to the left, at its height, on the ground
    player->pos.l.y.f.u = 400;
    player->status.p.f.in_air = false;
    Obj_PlatformSpikes(obj);
    Obj_PlatformSpikes(obj);
    CHECK_EQ(obj->scratch.u8[0], 2);
    for (int i = 0; i < 0x7F; i++)
        Obj_PlatformSpikes(obj);
    CHECK_EQ(obj->pos.l.x.f.u, 300 - 0x80); // (it has gone $80 to the left and stops)
    Obj_PlatformSpikes(obj);
    CHECK_EQ(obj->pos.l.x.f.u, 300 - 0x80);
}

static void DHZ_ACharacterInTheAirDoesNotSetItOff(void) {
    Reset(ZoneId_MCZ);
    Object *obj = Spawn(0x76, 0, 300, 400);
    Obj_PlatformSpikes(obj);
    player->pos.l.x.f.u = 300 - 0x80;
    player->pos.l.y.f.u = 400;
    player->status.p.f.in_air = true;
    for (int i = 0; i < 10; i++)
        Obj_PlatformSpikes(obj);
    CHECK_EQ(obj->scratch.u8[0], 0);
}

static void DHZ_AFlippedPlatformWithSpikesSlidesRight(void) {
    Reset(ZoneId_MCZ);
    Object *obj = Spawn(0x76, 0, 300, 400);
    obj->status.o.f.x_flip = true;
    Obj_PlatformSpikes(obj);
    player->pos.l.x.f.u = 300 + 0x80 - 0x100 + 0x80 - 0x40; // (the strip is the same one turned round: $40 to $C0 to the right)
    player->pos.l.x.f.u = 300 + 0x80;
    player->pos.l.y.f.u = 400;
    player->status.p.f.in_air = false;
    for (int i = 0; i < 12; i++)
        Obj_PlatformSpikes(obj);
    CHECK(obj->pos.l.x.f.u > 300);
}

void RegisterDHZObjectTests(void) {
    RUN_TEST(DHZ_TheCollapsingPlatformBreaksIntoSixPiecesOnceStoodOn);
    RUN_TEST(DHZ_TheCollapsingPlatformOfOilOceanIsWiderAndHasSevenPieces);
    RUN_TEST(DHZ_PiecesFallOneAfterAnother);
    RUN_TEST(DHZ_TheStomperRisesAPixelAFrameAndDropsEight);
    RUN_TEST(DHZ_TheSwitchIsDownWhileSomeoneStandsOnIt);
    RUN_TEST(DHZ_ASwitchWithBitSixSetsTheTopBit);
    RUN_TEST(DHZ_TheDrawbridgeLowersAtStartAndRaisesOnceItsSwitchIsDown);
    RUN_TEST(DHZ_ARaisedDrawbridgeLetsGoOfWhoeverStandsOnIt);
    RUN_TEST(DHZ_TheBoxesMakeTwoMoreAndMoveOnAPath);
    RUN_TEST(DHZ_FlippedBoxesTurnTheOtherWay);
    RUN_TEST(DHZ_TheSpikedBallIsAtTheEndOfItsChain);
    RUN_TEST(DHZ_TheFlipOfTheObjectGivesTheStartingAngle);
    RUN_TEST(DHZ_TheChainTurnsBySpeedTimesEightAFrame);
    RUN_TEST(DHZ_ASubtypeOfLowNibbleFIsASolidBlock);
    RUN_TEST(DHZ_ThePlatformWithSpikesSlidesAwayWhenSomeoneComesNear);
    RUN_TEST(DHZ_ACharacterInTheAirDoesNotSetItOff);
    RUN_TEST(DHZ_AFlippedPlatformWithSpikesSlidesRight);
}
