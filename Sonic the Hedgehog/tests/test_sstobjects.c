#include "test.h"

#include <string.h>

#include "Level.h"
#include "LevelScroll.h"
#include "Object.h"
#include "SstLoad.h"
#include "Object/Explosion.h"
#include "Object/Ring.h"
#include "Object/Sonic.h"
#include "Object/ShieldInvincibility.h"
#include "Mappings.h"

// The Sonic 1 objects that have moved to the status table, loaded from a layout by the loader of that table and run by its handler.

static const SstObjectIndex index_ = {sst_object_codes, 0x100};

#define X(v) (uint8_t)((v) >> 8), (uint8_t)(v)
#define ENTRY(x, y, id, sub) X(x), X(y), (id), (sub)

static const uint8_t layout[] = {
    ENTRY(0x100, 0x100, ObjId_Waterfall, 0), ENTRY(0x120, 0x110, ObjId_Pylon, 0), X(0xFFFF), 0, 0, 0, 0,
};

static SstLoadCamera Cam(int16_t x) {
    SstLoadCamera c = {x, 0, false, 0xFFF};
    return c;
}

static Sst *Of(int entry) {
    for (int i = 0; i < SST_COUNT; i++)
        if (sst[i].code != NULL && sst[i].respawn == entry + 1)
            return &sst[i];
    return NULL;
}

static void Start(void) {
    Sst_Reset();
    scrpos_x.v = scrpos_y.v = 0;
    SstLoadCamera c = Cam(0);
    SstLoad_Init(&index_, layout, false, &c);
}

static void SstObjects_TheLoaderMakesThemFromTheLayout(void) {
    Start();
    CHECK(Of(0) != NULL);
    CHECK(Of(0)->code == sst_object_codes[ObjId_Waterfall]);
    CHECK(Of(1) != NULL);
    CHECK(Of(1)->code == sst_object_codes[ObjId_Pylon]);
    CHECK_EQ(Of(1)->y_pos.f.u, 0x110);
}

static void SstObjects_ThePylonScrollsAgainstTheCameraAtTwiceItsSpeed(void) {
    Start();
    scrpos_x.f.u = 0x20;
    scrpos_y.f.u = 0x10;
    Sst_Process(false);                   // (its first run sets it up, and draws)
    Sst *pylon = Of(1);
    CHECK_EQ(pylon->routine, 2);
    CHECK_EQ(pylon->render_flags, 0);     // (placed on the screen)
    CHECK_EQ(pylon->x_pos.f.u, -0x40);
    CHECK_EQ(pylon->y_pos.f.u, -0x20 + 0x100);
    CHECK_EQ(Sst_QueueCount(0), 1);
    CHECK(Sst_QueueEntry(0, 0) == pylon);
}

static void SstObjects_TheWaterfallCountsSixtyFourFramesAndLeavesWhenTheCameraDoes(void) {
    Start();
    Sst_Process(false);
    Sst_Process(false);                   // (the counter ran out and started again)
    Sst *fall = Of(0);
    CHECK_EQ(fall->own.u8[0], 0x3F);
    Sst_Process(false);
    CHECK_EQ(fall->own.u8[0], 0x3E);
    SstLoadCamera c = Cam(0x2000);
    SstLoad_Update(&c);
    Sst_Process(false);
    CHECK(Of(0) == NULL);                 // deleted ...
    CHECK(!(sst_respawn_table[0] & 0x80)); // ... and its place given back: it comes again when the camera does
}

static void SstObjects_PointsRiseAndSlowAndAreDeletedWhenTheySettle(void) {
    Start();
    Sst *points = Sst_Spawn(ObjId_Points, 0x100, 0x200);
    CHECK(points != NULL);
    points->mapping_frame = 2;
    Sst_Process(false);
    CHECK_EQ(points->y_vel, -0x300 + 0x18);
    CHECK_EQ(points->y_pos.f.u, 0x200 - 3);            // (moved by the speed it had: -0x300 is -3 pixels)
    CHECK_EQ(points->render_flags & SST_RENDER_LEVEL, SST_RENDER_LEVEL);
    int frames = 1;
    while (points->code != NULL && frames < 100) {
        Sst_Process(false);
        frames++;
    }
    CHECK_EQ(frames, 33);                              // (0x300 / 0x18 = 32 slowings, deleted on the next run)
}

static void SstObjects_ABumperThrowsSonicAwayAndPaysTenPointsTenTimes(void) {
    Start();
    player->code = sst_object_codes[1];
    player->x_pos.f.u = 0x100;
    player->y_pos.f.u = 0x120;
    score = 0;
    Sst *bumper = Sst_Spawn(ObjId_Bumper, 0x100, 0x100);
    sst_respawn_table[0] = 0x80;
    bumper->respawn = 1;
    Sst_Process(false);
    CHECK_EQ(bumper->routine, 2);
    CHECK_EQ(bumper->collision_flags, 0x17 | 0xC0);
    CHECK_EQ(player->y_vel, 0);                            // (not touched yet)
    for (int hit = 1; hit <= 12; hit++) {
        bumper->collision_property = 1;                  // (what the touch response does)
        Sst_Process(false);
        CHECK_EQ(player->y_vel, 0x700);                    // (the bumper is above him: thrown down)
        CHECK_EQ(bumper->anim, 1);
        CHECK_EQ(score, hit <= 10 ? hit : 10);           // (the tenth hit is the last that pays)
        for (int i = SST_FIRST_DYNAMIC; i < SST_COUNT; i++)
            if (sst[i].code != NULL && sst[i].code != bumper->code)
                Sst_Delete(&sst[i]);                     // (the points that rose)
    }
    CHECK_EQ(sst_respawn_table[0], 0x80 + 10);
}

signed int ReactToItem(Sst *obj); // Sonic.c: the touch response

static void Touch_Setup(void) {
    Start();
    memset(player, 0, sizeof(Sst));
    player->code = sst_object_codes[ObjId_Sonic];
    player->routine = 2;
    player->y_radius = 14;
    player->x_radius = 7;
    player->x_pos.f.u = 0x100;
    player->y_pos.f.u = 0x300;
    invincibility = 0;
    score = 0;
    item_bonus = 0;
    ((Scratch_Sonic *)player->own.u8)->flash_time = 0;
}

static Sst *Touch_Chopper(void) {
    Sst *chopper = Sst_Spawn(ObjId_Chopper, 0x100, 0x300);
    Sst_Process(false);                                  // (sets it up)
    chopper->x_pos.f.u = 0x100;
    chopper->y_pos.f.u = 0x300;
    chopper->render_flags |= SST_RENDER_ON_SCREEN;       // (the renderer's)
    return chopper;
}

static void SstObjects_SonicRollingIntoABadnikOnTheTableDestroysItAndScores(void) {
    Touch_Setup();
    Sst *chopper = Touch_Chopper();
    CHECK_EQ(chopper->collision_flags, 0x09);
    player->anim = SonAnimId_Roll;
    player->y_pos.f.u = 0x2F0;                         // (landing on it from above)
    player->y_vel = 0x200;
    ReactToItem(player);
    CHECK(chopper->code == sst_object_codes[ObjId_Explosion]);   // (it became the explosion where it was)
    CHECK_EQ(chopper->routine, 0);
    CHECK_EQ(chopper->own.u16[0xB], 0);                          // (the first of the chain: 100 points)
    CHECK_EQ(score, 10);
    CHECK_EQ(item_bonus, 2);
    CHECK_EQ(player->y_vel, -0x200);                               // (bounced)
    CHECK(chopper->status & 0x80);
}

static void SstObjects_SonicWalkingIntoABadnikOnTheTableIsHurt(void) {
    Touch_Setup();
    Sst *chopper = Touch_Chopper();
    player->anim = SonAnimId_Walk;
    rings = 5;
    shield = 0;
    ReactToItem(player);
    CHECK(chopper->code == sst_object_codes[ObjId_Chopper]);     // (still a chopper)
    CHECK_EQ(player->routine, 4);                                // (hurt)
    CHECK_EQ(score, 0);
}

static void SstObjects_SonicTouchingARingOfTheLayoutCollectsIt(void) {
    Touch_Setup();
    static const uint8_t layout[] = { 0x01, 0x00, 0x03, 0x00,   0xFF, 0xFF, 0x00, 0x00 }; // one ring, where Sonic is
    Ring_StartLayout(layout);
    rings = 0;
    ReactToItem(player);
    CHECK_EQ(rings, 1);
    ReactToItem(player);
    CHECK_EQ(rings, 1); // (once)

    Ring_StartLayout(layout);
    ((Scratch_Sonic *)player->own.u8)->flash_time = 90; // (just hurt: the rings he scattered are not for him yet)
    ReactToItem(player);
    CHECK_EQ(rings, 1);
    ((Scratch_Sonic *)player->own.u8)->flash_time = 89;
    ReactToItem(player);
    CHECK_EQ(rings, 2);
    Rings_Init(NULL);
}

static void SstObjects_AChopperJumpsInItsPlaceAndTurnsBackAtTheWater(void) {
    Start();
    Sst *chopper = Sst_Spawn(ObjId_Chopper, 0x100, 0x200);
    Sst_Process(false);
    CHECK_EQ(chopper->y_vel, -0x700);
    int min_y = 0x200, max_y = 0x200;
    for (int i = 0; i < 100; i++) {
        Sst_Process(false);
        if (chopper->y_pos.f.u < min_y)
            min_y = chopper->y_pos.f.u;
        if (chopper->y_pos.f.u > max_y)
            max_y = chopper->y_pos.f.u;
    }
    CHECK(min_y < 0x200 - 0x70);                       // (it jumped)
    CHECK(max_y <= 0x200);                             // (and never went lower than the water it came from)
}

static void SstObjects_JawsTurnAfterTheirDelay(void) {
    Start();
    Sst *jaws = Sst_Spawn(ObjId_Jaws, 0x100, 0x200);
    jaws->subtype = 2;                                 // (a delay of 2 * 64 frames)
    Sst_Process(false);
    CHECK_EQ(jaws->x_vel, -0x40);
    for (int i = 0; i < 126; i++)
        Sst_Process(false);
    CHECK_EQ(jaws->x_vel, -0x40);
    for (int i = 0; i < 4; i++)
        Sst_Process(false);
    CHECK_EQ(jaws->x_vel, 0x40);
    CHECK(jaws->status & SST_STATUS_X_FLIP);
}

static void SstObjects_AHelixMakesItsSpikesAndTakesThemWhenItLeaves(void) {
    Start();
    Sst *helix = Sst_Spawn(ObjId_Helix, 0x100, 0x200);
    helix->subtype = 8;                                  // eight spikes in all
    Sst_Process(false);
    int made = 0;
    for (int i = SST_FIRST_DYNAMIC; i < SST_COUNT; i++)
        made += sst[i].code == helix->code;
    CHECK_EQ(made, 8);                                   // (the parent and seven others: the parent is one of the row)
    SstLoadCamera far = {0x3000, 0, false, 0xFFF};
    SstLoad_Update(&far);
    Sst_Process(false);
    made = 0;
    for (int i = SST_FIRST_DYNAMIC; i < SST_COUNT; i++)
        made += sst[i].code != NULL;
    CHECK_EQ(made, 1);                                   // (the children go with the parent: the pylon, fixed to the screen, is all that stays)
}

static void SstObjects_AnOrbinautCarriesFourSpikeballsAndTakesThemWhenItLeaves(void) {
    Start();
    memset(player, 0, sizeof(Sst));
    player->x_pos.f.u = 0x2000;
    player->y_pos.f.u = 0x2000;
    Sst *orb = Sst_Spawn(ObjId_Orbinaut, 0x100, 0x200);
    orb->subtype = 0;                                    // (LZ's: waits for Sonic)
    Sst_Process(false);
    int balls = 0;
    for (int i = SST_FIRST_DYNAMIC; i < SST_COUNT; i++)
        balls += sst[i].code == orb->code && &sst[i] != orb;
    CHECK_EQ(balls, 4);
    SstLoadCamera far = {0x3000, 0, false, 0xFFF};
    SstLoad_Update(&far);
    Sst_Process(false);
    int left = 0;
    for (int i = SST_FIRST_DYNAMIC; i < SST_COUNT; i++)
        left += sst[i].code != NULL;
    CHECK_EQ(left, 1);                                   // (the pylon of the layout stays; the orbinaut and its balls are gone)
}

static void SstObjects_ABuzzBomberFiresAMissileThatGoesWhenItsParentExplodes(void) {
    Start();
    memset(player, 0, sizeof(Sst));
    player->x_pos.f.u = 0x100 - 0x20;                  // (close to it)
    player->y_pos.f.u = 0x200;
    Sst *buzz = Sst_Spawn(ObjId_BuzzBomber, 0x100, 0x200);
    Sst_Process(false);
    buzz->render_flags |= SST_RENDER_ON_SCREEN;
    Sst *missile = NULL;
    for (int f = 0; f < 400 && missile == NULL; f++) {
        buzz->render_flags |= SST_RENDER_ON_SCREEN;
        Sst_Process(false);
        for (int i = SST_FIRST_DYNAMIC; i < SST_COUNT; i++)
            if (sst[i].code == sst_object_codes[ObjId_BuzzMissile])
                missile = &sst[i];
    }
    CHECK(missile != NULL);
    for (int f = 0; f < 16; f++)
        Sst_Process(false);                              // (its 14 frames of waiting)
    CHECK_EQ(missile->routine, 2);                       // (charging, on the Buzz Bomber's nose)
    buzz->code = sst_object_codes[ObjId_Explosion];      // (it was hit)
    buzz->routine = 0;
    Sst_Process(false);
    CHECK(missile->code == NULL);                        // (the missile goes with it)
}

static int CountPowerups(void) {
    int n = 0;
    for (int i = 0; i < SST_COUNT; i++)
        if (sst[i].code == Obj_ShieldInvincibility)
            n++;
    return n;
}

static void SstObjects_TheShieldFollowsSonicAndGoesWhenHeLosesIt(void) {
    Start();
    player->x_pos.f.u = 0x180;
    player->y_pos.f.u = 0x90;
    shield = true;
    invincibility = false;
    ShieldInvincibility_Make(0);
    ShieldInvincibility_Make(0);                       // (he takes a second shield monitor: there is still the one)
    CHECK_EQ(CountPowerups(), 1);
    Sst_Process(false);
    Sst_Process(false);
    Sst *obj = NULL;
    for (int i = 0; i < SST_COUNT; i++)
        if (sst[i].code == Obj_ShieldInvincibility)
            obj = &sst[i];
    CHECK(obj != NULL);
    CHECK_EQ(obj->x_pos.f.u, 0x180);
    CHECK_EQ(obj->y_pos.f.u, 0x90);
    shield = false;
    Sst_Process(false);
    CHECK_EQ(CountPowerups(), 0);
}

static void SstObjects_TheFourStarsTrailSonicAndGoWhenInvincibilityEnds(void) {
    Start();
    invincibility = true;
    for (uint8_t a = 1; a <= 4; a++)
        ShieldInvincibility_Make(a);
    CHECK_EQ(CountPowerups(), 4);
    for (int i = 0; i < 4; i++)
        Sst_Process(false);
    CHECK_EQ(CountPowerups(), 4);
    invincibility = false;
    Sst_Process(false);
    CHECK_EQ(CountPowerups(), 0);
}

// Every mapping that the status table draws is in its format: 6-byte pieces whose size byte has only its low four bits (the build had stale headers once: the arrays were the old 8-byte data)
static void CheckMappings(const uint8_t *m, int frames) {
    for (int f = 0; f < frames; f++) {
        const uint8_t *pieces;
        uint16_t count = Mappings_FramePieces(m, (unsigned)f, &pieces);
        CHECK(count < 64);
        for (uint16_t i = 0; i < count; i++)
            CHECK((pieces[i * SPRITE_PIECE_SIZE_S3 + 1] & 0xF0) == 0);
    }
}

extern const uint8_t Mappings_Sonic[];
extern const uint8_t Mappings_Pylon[];
extern const uint8_t Mappings_Points[];
extern const uint8_t Mappings_ShieldInvincibility[];
extern const uint8_t Mappings_Checkpoint[];
extern const uint8_t Mappings_RingREV01[];
extern const uint8_t Mappings_SSRotate[];
extern const uint8_t Mappings_SSGlass[];
extern const uint8_t Mappings_SSWall[];
extern const uint8_t Mappings_SSUp[];
extern const uint8_t Mappings_SSDown[];
extern const uint8_t Mappings_VanishSonic[];
extern const uint8_t Mappings_TitleSonic[];
extern const uint8_t Mappings_PSB[];
extern const uint8_t Mappings_Credits[];
extern const uint8_t Mappings_GameOver[];
extern const uint8_t Mappings_GiantRing[];
extern const uint8_t Mappings_RingFlash[];
extern const uint8_t Mappings_Button[];
extern const uint8_t Mappings_PathSwapper[];
extern const uint8_t Mappings_WaterSurface[];
extern const uint8_t Mappings_HiddenBonuses[];

static void SstObjects_TheMappingsOfTheTableAreInItsFormat(void) {
    CheckMappings(Mappings_Sonic, 0x40);
    CheckMappings(Mappings_Pylon, 1);
    CheckMappings(Mappings_Points, 7);
    CheckMappings(Mappings_ShieldInvincibility, 8);
    CheckMappings(Mappings_Checkpoint, 3);
    CheckMappings(Mappings_RingREV01, (Mappings_RingREV01[0] << 8 | Mappings_RingREV01[1]) / 2);
    CheckMappings(Mappings_SSRotate, (Mappings_SSRotate[0] << 8 | Mappings_SSRotate[1]) / 2);
    CheckMappings(Mappings_SSGlass, (Mappings_SSGlass[0] << 8 | Mappings_SSGlass[1]) / 2);
    CheckMappings(Mappings_SSWall, (Mappings_SSWall[0] << 8 | Mappings_SSWall[1]) / 2);
    CheckMappings(Mappings_SSUp, (Mappings_SSUp[0] << 8 | Mappings_SSUp[1]) / 2);
    CheckMappings(Mappings_SSDown, (Mappings_SSDown[0] << 8 | Mappings_SSDown[1]) / 2);
    CheckMappings(Mappings_VanishSonic, (Mappings_VanishSonic[0] << 8 | Mappings_VanishSonic[1]) / 2);
    CheckMappings(Mappings_TitleSonic, (Mappings_TitleSonic[0] << 8 | Mappings_TitleSonic[1]) / 2);
    CheckMappings(Mappings_PSB, (Mappings_PSB[0] << 8 | Mappings_PSB[1]) / 2);
    CheckMappings(Mappings_Credits, (Mappings_Credits[0] << 8 | Mappings_Credits[1]) / 2);
    CheckMappings(Mappings_GameOver, (Mappings_GameOver[0] << 8 | Mappings_GameOver[1]) / 2);
    CheckMappings(Mappings_GiantRing, (Mappings_GiantRing[0] << 8 | Mappings_GiantRing[1]) / 2);
    CheckMappings(Mappings_RingFlash, (Mappings_RingFlash[0] << 8 | Mappings_RingFlash[1]) / 2);
    CheckMappings(Mappings_Button, (Mappings_Button[0] << 8 | Mappings_Button[1]) / 2);
    CheckMappings(Mappings_PathSwapper, 8);
    CheckMappings(Mappings_WaterSurface, 2);
    CheckMappings(Mappings_HiddenBonuses, 2);
}

void RegisterSstObjectsTests(void) {
    RUN_TEST(SstObjects_TheMappingsOfTheTableAreInItsFormat);
    RUN_TEST(SstObjects_TheShieldFollowsSonicAndGoesWhenHeLosesIt);
    RUN_TEST(SstObjects_TheFourStarsTrailSonicAndGoWhenInvincibilityEnds);
    RUN_TEST(SstObjects_ABuzzBomberFiresAMissileThatGoesWhenItsParentExplodes);
    RUN_TEST(SstObjects_AnOrbinautCarriesFourSpikeballsAndTakesThemWhenItLeaves);
    RUN_TEST(SstObjects_AHelixMakesItsSpikesAndTakesThemWhenItLeaves);
    RUN_TEST(SstObjects_SonicRollingIntoABadnikOnTheTableDestroysItAndScores);
    RUN_TEST(SstObjects_SonicWalkingIntoABadnikOnTheTableIsHurt);
    RUN_TEST(SstObjects_SonicTouchingARingOfTheLayoutCollectsIt);
    RUN_TEST(SstObjects_AChopperJumpsInItsPlaceAndTurnsBackAtTheWater);
    RUN_TEST(SstObjects_JawsTurnAfterTheirDelay);
    RUN_TEST(SstObjects_ABumperThrowsSonicAwayAndPaysTenPointsTenTimes);
    RUN_TEST(SstObjects_PointsRiseAndSlowAndAreDeletedWhenTheySettle);
    RUN_TEST(SstObjects_TheLoaderMakesThemFromTheLayout);
    RUN_TEST(SstObjects_ThePylonScrollsAgainstTheCameraAtTwiceItsSpeed);
    RUN_TEST(SstObjects_TheWaterfallCountsSixtyFourFramesAndLeavesWhenTheCameraDoes);
}
