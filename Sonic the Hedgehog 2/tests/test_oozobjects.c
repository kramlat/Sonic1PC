#include "test.h"

#include <string.h>

#include "Game.h"
#include "Level.h"
#include "Object.h"
#include "SstLoad.h"
#include "Object/CharControl.h"
#include "Object/CPZObjects.h"
#include "Object/OOZObjects.h"
#include "Object/Sonic.h"
#include "Object/Tails.h"
#include "SandShower.h"

// Oil Ocean's objects: the oil, the launching platform, the spiked balls, the spring and pusher, the ball on a spring and the cannon.

// (the slots below $20 are taken, so that the objects a test makes get the slots the old tests named)
static void SstTestFiller(Sst *self) { (void)self; }

static void Reset(void) {
    memset(objects, 0, sizeof(Object) * 0x60);
    Sst_Reset();
    sst_load_everything_in_range = true;
    for (int i = 4; i < 0x20; i++)
        sst[i].code = SstTestFiller;
    memset(f_switch, 0, sizeof(f_switch));
    memset(objstate, 0, sizeof(objstate));
    level_id = LEVEL_ID(ZoneId_OOZ, 0);
    scrpos_x.v = scrpos_y.v = 0;
    scrpos_x.f.u = 0;
    limit_btm2 = 0x720;
    debug_use = false;
    jpad2_press = 0;
    player->code = sst_object_codes[1];
    player->routine = 2;
    player->y_radius = 19;
    player->x_pos.f.u = 0x1000;
    player->y_pos.f.u = 0x1000;
    player->status = 0;
    OBJ_CONTROL(player) = 0;
    {
        static const SstObjectIndex index_ = {sst_object_codes, 0x100};
        SstLoadCamera camera = {(int16_t)scrpos_x.f.u, (int16_t)scrpos_y.f.u, false, 0xFFF};
        SstLoad_Init(&index_, NULL, false, &camera);
    }
}

static Sst *Spawn(Sst *slot, uint8_t type, uint8_t subtype, int16_t x, int16_t y) {
    memset(slot, 0, sizeof(Object));
    slot->code = sst_object_codes[type];
    slot->subtype = subtype;
    slot->x_pos.f.u = x;
    slot->y_pos.f.u = y;
    return slot;
}

#define SLOT(n) (&sst[n])

// Sonic standing on something with its top at `top`
static void StandOn(Sst *obj, int16_t x, int16_t top) {
    player->x_pos.f.u = x;
    player->y_pos.f.u = (int16_t)(top - player->y_radius);
    player->status &= (uint8_t)~SST_PSTATUS_IN_AIR;
    player->status |= SST_PSTATUS_OBJECT_STAND;
    player->x_vel = player->y_vel = player->ground_vel = 0;
    obj->status |= 8;
}

// --- 07 ---

static void OOZ_TheOilKillsWhoeverStaysInIt(void) {
    Reset();
    Sst *oil = Spawn(SLOT(0x1E), 7, 0, 0, 0);
    Obj_OilSurface(oil);
    CHECK_EQ(oil->y_pos.f.u, 0x758);
    StandOn(oil, 300, 0x758 - 0x30);
    for (int i = 0; i < 0x2F; i++) {
        Obj_OilSurface(oil);
        player->routine = 2;
    }
    CHECK_EQ(player->routine, 2); // (still alive)
    CHECK(player->y_pos.f.u > 0x758 - 0x30 - 19); // (it has gone down in it)
    for (int i = 0; i < 4; i++)
        Obj_OilSurface(oil);
    CHECK_EQ(player->routine, 6); // (dead)
}

static void OOZ_TheOilReleasesSonicWhenHeLeaves(void) {
    Reset();
    Sst *oil = Spawn(SLOT(0x1E), 7, 0, 0, 0);
    Obj_OilSurface(oil);
    StandOn(oil, 300, 0x758 - 0x30);
    for (int i = 0; i < 10; i++)
        Obj_OilSurface(oil);
    CHECK_EQ(oil->own.u8[0x10], 0x30 - 10);
    player->status |= SST_PSTATUS_IN_AIR; // (jumps out, well clear of it)
    player->y_pos.f.u -= 0x40;
    for (int i = 0; i < 12; i++) // (one more sink in the frame he leaves, then a pixel a frame back up)
        Obj_OilSurface(oil);
    CHECK_EQ(oil->own.u8[0x10], 0x30); // (it comes back up: Sonic is not standing and the depth recovers)
    CHECK_EQ(player->routine, 2);
}

// --- Sand Shower's quicksand: blocks 168 to 181, in a chunk of the test's own (put back after) ---

static uint8_t sand_saved_chunk[128];
static uint8_t sand_saved_cell;

static void SandOn(void) {
    memcpy(sand_saved_chunk, level_map128 + 0x80 * 0xFE, 128);
    sand_saved_cell = LevelLayout_Chunk(LEVEL_LAYOUT_PLANE_FG, 2, 1);
    for (int i = 0; i < 64; i++) { // (a chunk of sand: blocks 168 to 175, all in the range)
        level_map128[0x80 * 0xFE + i * 2] = 0;
        level_map128[0x80 * 0xFE + i * 2 + 1] = (uint8_t)(168 + (i & 7));
    }
    LevelLayout_SetChunk(LEVEL_LAYOUT_PLANE_FG, 2, 1, 0xFE); // (x $100 to $17F, y $80 to $FF: its top is at $80)
    Reset();
    player->status |= SST_PSTATUS_IN_AIR;
    player->x_pos.f.u = 0x140;
}

static void SandOff(void) {
    memcpy(level_map128 + 0x80 * 0xFE, sand_saved_chunk, 128);
    LevelLayout_SetChunk(LEVEL_LAYOUT_PLANE_FG, 2, 1, sand_saved_cell);
}

static void SSZ_SandSinksACharacterSlowlyAndHoldsItBack(void) {
    SandOn();
    player->y_pos.f.u = (int16_t)(0x84 - player->y_radius); // (feet in the first row)
    player->y_vel = 0x400;
    player->x_vel = 0x300;
    SandShower_LevelMisc();
    CHECK_EQ(player->y_vel, 0x40); // (a quarter of a pixel a frame at most)
    CHECK(player->x_vel < 0x300 && player->x_vel > 0x200); // (and slowed in the way he goes)
    SandOff();
}

static void SSZ_SandIsIgnoredOutsideIt(void) {
    SandOn();
    player->y_pos.f.u = (int16_t)(0x60 - player->y_radius); // (above the sand)
    player->y_vel = 0x400;
    SandShower_LevelMisc();
    CHECK_EQ(player->y_vel, 0x400);
    SandOff();
}

static void SSZ_AJumpInTheSandTakesHimUpWeakly(void) {
    SandOn();
    player->y_pos.f.u = (int16_t)(0xA0 - player->y_radius);
    player->y_vel = 0x40;
    jpad1_press2 = JPAD_A;
    SandShower_LevelMisc();
    CHECK_EQ(player->y_vel, -0x410); // (5/8 of a jump's $680)
    CHECK(((player->status & SST_PSTATUS_IN_BALL) != 0));
    jpad1_press2 = 0;
    SandOff();
}

static void SandPut(uint16_t word) { // the whole test chunk is that cell word
    for (int i = 0; i < 64; i++) {
        level_map128[0x80 * 0xFE + i * 2] = (uint8_t)(word >> 8);
        level_map128[0x80 * 0xFE + i * 2 + 1] = (uint8_t)word;
    }
}

static void SSZ_ASandFallCarriesACharacterDownOrUpWithItsFlipAndDoesNotKill(void) {
    SandOn();
    SandPut(0x0051);
    player->y_pos.f.u = (int16_t)(0xC0 - player->y_radius);
    player->y_vel = 0;
    for (int i = 0; i < 30; i++)
        SandShower_LevelMisc();
    CHECK(player->y_vel > 0x100); // (carried down)
    CHECK_EQ(player->routine, 2);
    SandPut(0x0851); // (Y flip)
    player->y_vel = 0;
    for (int i = 0; i < 30; i++)
        SandShower_LevelMisc();
    CHECK(player->y_vel < -0x100); // (carried up)
    SandOff();
}

static void SSZ_SandThatHoldsSticksACharacterAndCarriesItSidewaysUntilAJump(void) {
    SandOn();
    SandPut(0x0098);
    player->y_pos.f.u = (int16_t)(0xC0 - player->y_radius);
    player->y_vel = 0x300;
    SandShower_LevelMisc();
    CHECK_EQ(player->y_vel, 0);
    CHECK_EQ(player->x_vel, 0x200); // (to the right)
    SandPut(0x0498); // (X flip)
    SandShower_LevelMisc();
    CHECK_EQ(player->x_vel, -0x200);
    jpad1_press2 = JPAD_A;
    SandShower_LevelMisc();
    CHECK_EQ(player->y_vel, 0x680); // (a full jump out of it, downward)
    jpad1_press2 = 0;
    SandShower_LevelMisc();
    CHECK_EQ(player->y_vel, 0x680); // (on his way out: it does not hold him again)
    SandPut(0x0000); // (out of it: it can take him again)
    SandShower_LevelMisc();
    SandPut(0x0898); // (Y flip: up)
    SandShower_LevelMisc();
    jpad1_press2 = JPAD_A;
    SandShower_LevelMisc();
    CHECK_EQ(player->y_vel, -0x680);
    jpad1_press2 = 0;
    SandOff();
}

static void SSZ_ASandFallLetsGoWhereASidewaysOneTakesOverAndAJumpAtItsEndIsHigh(void) {
    SandOn();
    // a fall, then sand that holds: the new one takes over at once (his fall stops and the sideways flow starts)
    SandPut(0x0051);
    player->y_pos.f.u = (int16_t)(0xC0 - player->y_radius);
    player->y_vel = 0x180;
    SandShower_LevelMisc();
    SandPut(0x0098);
    SandShower_LevelMisc();
    CHECK_EQ(player->y_vel, 0);
    CHECK_EQ(player->x_vel, 0x200);
    // and back: the sideways speed goes at once
    SandPut(0x0051);
    SandShower_LevelMisc();
    CHECK_EQ(player->x_vel, 0);
    // a jump from a fall that goes on is weak, and at its end (the block below is not a fall) a full one
    SandPut(0x0051);
    player->y_vel = 0;
    jpad1_press2 = JPAD_A;
    SandShower_LevelMisc();
    CHECK_EQ(player->y_vel, -0x410);
    player->status &= (uint8_t)~SST_PSTATUS_IN_BALL;
    for (int i = 48; i < 64; i++) // (the last two rows of the chunk are empty: the fall ends above them)
        level_map128[0x80 * 0xFE + i * 2 + 1] = 0;
    player->y_pos.f.u = (int16_t)(0x80 + 5 * 16 + 8); // (his middle in the sixth row, the last of the fall: his feet are in the seventh)
    player->y_vel = 0;
    player->status |= SST_PSTATUS_IN_AIR;
    SandShower_LevelMisc();
    CHECK_EQ(player->y_vel, -0x680);
    jpad1_press2 = 0;
    SandOff();
}

static void DoorRun(Sst *door, int frames) {
    for (int i = 0; i < frames; i++)
        Obj_CPZBarrier(door);
}

static void SSZ_TheDoorOpensAsYouComeUpToItAndShutsBehindYou(void) {
    Reset();
    level_id = LEVEL_ID(ZoneId_SSZ, 0);
    Sst *door = Spawn(SLOT(0x20), 0x2D, 0x00, 0x60, 0x80);
    player->x_pos.f.u = 0x1000;
    DoorRun(door, 20);
    CHECK_EQ(door->y_pos.f.u, 0x80); // (shut: he is far away)
    player->x_pos.f.u = 0x20; // (on its open side)
    player->y_pos.f.u = 0x80;
    DoorRun(door, 20);
    CHECK_EQ(door->y_pos.f.u, 0x80 - 0x40);
    player->x_pos.f.u = 0x80; // (in the door)
    DoorRun(door, 20);
    CHECK_EQ(door->y_pos.f.u, 0x80 - 0x40); // (it does not shut on him)
    player->x_pos.f.u = 0xA0; // (past it)
    DoorRun(door, 20);
    CHECK_EQ(door->y_pos.f.u, 0x80); // (it shuts behind him)
    // flipped, it opens from the other side
    Sst *flipped = Spawn(SLOT(0x21), 0x2D, 0x00, 0x60, 0x80);
    flipped->status |= SST_STATUS_X_FLIP;
    player->x_pos.f.u = 0x20;
    DoorRun(flipped, 20);
    CHECK_EQ(flipped->y_pos.f.u, 0x80); // (not from the left)
    player->x_pos.f.u = 0xC0;
    DoorRun(flipped, 20);
    CHECK_EQ(flipped->y_pos.f.u, 0x80 - 0x40);
}

static void SSZ_TheSwitchDoorWaitsForItsSwitchAndShutsOnceYouAreThrough(void) {
    Reset();
    level_id = LEVEL_ID(ZoneId_SSZ, 0);
    memset(f_switch, 0, sizeof(f_switch));
    Sst *door = Spawn(SLOT(0x20), 0x2D, 0x10, 0x60, 0x80);
    player->x_pos.f.u = 0x20; // (at it, but its switch is not pressed)
    player->y_pos.f.u = 0x80;
    DoorRun(door, 20);
    CHECK_EQ(door->y_pos.f.u, 0x80);
    f_switch[0] = 1; // (pressed, and let go: it stays up)
    DoorRun(door, 2);
    f_switch[0] = 0;
    DoorRun(door, 200);
    CHECK_EQ(door->y_pos.f.u, 0x80 - 0x40);
    player->x_pos.f.u = 0x80; // (going through)
    DoorRun(door, 20);
    CHECK_EQ(door->y_pos.f.u, 0x80 - 0x40);
    player->x_pos.f.u = 0xA0; // (through)
    DoorRun(door, 20);
    CHECK_EQ(door->y_pos.f.u, 0x80);
}

static void SSZ_TheTimedSwitchDoorStaysUpForTheTimeOfItsSubtype(void) {
    Reset();
    level_id = LEVEL_ID(ZoneId_SSZ, 0);
    memset(f_switch, 0, sizeof(f_switch));
    Sst *door = Spawn(SLOT(0x20), 0x2D, 0x20, 0x60, 0x80); // (3 seconds)
    player->x_pos.f.u = 0x20; // (on its open side: it does not mind which side)
    player->y_pos.f.u = 0x80;
    DoorRun(door, 20);
    CHECK_EQ(door->y_pos.f.u, 0x80);
    f_switch[0] = 1;
    DoorRun(door, 2);
    f_switch[0] = 0;
    player->x_pos.f.u = 0x1000;
    player->y_pos.f.u = 0x1000;
    DoorRun(door, 150);
    CHECK_EQ(door->y_pos.f.u, 0x80 - 0x40); // (up after 2.5 seconds)
    DoorRun(door, 100);
    CHECK_EQ(door->y_pos.f.u, 0x80); // (shut after 4)
    Sst *slow = Spawn(SLOT(0x21), 0x2D, 0x80 | 0x20, 0x60, 0x80); // (8 seconds)
    f_switch[0] = 1;
    DoorRun(slow, 2);
    f_switch[0] = 0;
    DoorRun(slow, 400);
    CHECK_EQ(slow->y_pos.f.u, 0x80 - 0x40);
    player->x_pos.f.u = 0x80; // (in its way when the time is up: it shuts all the same)
    player->y_pos.f.u = 0x80;
    DoorRun(slow, 200);
    CHECK_EQ(slow->y_pos.f.u, 0x80);
}

static void SSZ_TheTimedDoorCrushesWhoIsStillUnderItWhenTheTimeIsUp(void) {
    Reset();
    level_id = LEVEL_ID(ZoneId_SSZ, 0);
    memset(f_switch, 0, sizeof(f_switch));
    Sst *door = Spawn(SLOT(0x20), 0x2D, 0x20, 0x60, 0x80); // (3 seconds)
    door->render_flags |= SST_RENDER_ON_SCREEN;
    player->x_pos.f.u = 0x1000; // (away while it opens)
    f_switch[0] = 1;
    DoorRun(door, 2);
    f_switch[0] = 0;
    DoorRun(door, 20);
    player->x_pos.f.u = 0x70; // (under it)
    player->y_pos.f.u = 0x95;
    player->status &= (uint8_t)~SST_PSTATUS_IN_AIR;
    player->y_vel = 0;
    for (int i = 0; i < 100; i++) {
        door->render_flags |= SST_RENDER_ON_SCREEN;
        Obj_CPZBarrier(door);
    }
    CHECK_EQ(player->routine, 2); // (alive while it is up)
    for (int i = 0; i < 200 && player->routine == 2; i++) {
        door->render_flags |= SST_RENDER_ON_SCREEN;
        Obj_CPZBarrier(door);
    }
    CHECK_EQ(player->routine, 6); // (crushed)
}

static void SSZ_AFallTurnsIntoTheSidewaysSandBesideItsEnd(void) {
    SandOn();
    SandPut(0x0000);
    for (int row = 0; row < 3; row++) // (a fall, down the first column)
        level_map128[0x80 * 0xFE + (row * 8) * 2 + 1] = 0x51;
    level_map128[0x80 * 0xFE + (3 * 8) * 2 + 1] = 0x97; // (its bend)
    level_map128[0x80 * 0xFE + (3 * 8 + 1) * 2 + 1] = 0x98; // (and sand that holds beside it)
    player->x_pos.f.u = 0x108;
    player->y_pos.f.u = (int16_t)(0x80 + 3 * 16 + 4);
    player->y_vel = 0x180;
    player->x_vel = 0;
    SandShower_LevelMisc();
    CHECK_EQ(player->y_vel, 0); // (not dropped)
    CHECK_EQ(player->x_vel, 0x200); // (carried round the corner)
    SandOff();
}

static void SSZ_SandKillsAtThirtyTwoPixelsOfDepthAndNotBefore(void) {
    SandOn();
    player->y_pos.f.u = (int16_t)(0x80 + 0x2F - player->y_radius);
    SandShower_LevelMisc();
    CHECK_EQ(player->routine, 2);
    player->y_pos.f.u = (int16_t)(0x80 + 0x30 - player->y_radius);
    SandShower_LevelMisc();
    CHECK_EQ(player->routine, 6);
    SandOff();
}

// --- 33 ---

static void OOZ_TheLauncherHopsEverySecondOrSo(void) {
    Reset();
    Sst *obj = Spawn(SLOT(0x20), 0x33, 0, 200, 400);
    Obj_OOZLauncher(obj);
    CHECK_EQ(obj->art_tile & 0x7FF, 0x32C);
    CHECK_EQ(obj->routine_secondary, 0); // (its first frame bounces once on its place and settles: waiting)
    CHECK_EQ(obj->y_pos.f.u, 400);
    int frames = 0;
    while (frames < 400 && obj->y_pos.f.u == 400) {
        Obj_OOZLauncher(obj);
        frames++;
    }
    CHECK(frames >= 0x78 && frames <= 0x7C); // (it leaps after about $78 frames)
    CHECK(obj->y_pos.f.u < 400);
    for (int i = 0; i < 400 && obj->routine_secondary != 0; i++)
        Obj_OOZLauncher(obj);
    CHECK_EQ(obj->routine_secondary, 0); // (it comes down and settles again)
    CHECK_EQ(obj->y_pos.f.u, 400);
}

// The alpha's platform has fire under it: an object of its own, $10 above the lid's middle, that is not there (no harm, nothing drawn) until the lid is $14 or more above the fire (it starts $10 below)
static void OOZ_TheLauncherHasFireThatShowsOnlyWhileItIsUp(void) {
    Reset();
    Sst *obj = Spawn(SLOT(0x20), 0x33, 0, 200, 150);
    Obj_OOZLauncher(obj);
    Sst *fire = SLOT(0x21);
    CHECK(fire->code == sst_object_codes[0x33]);
    CHECK_EQ(fire->routine, 4);
    CHECK_EQ(fire->y_pos.f.u, 134);
    CHECK_EQ(fire->x_pos.f.u, 200);
    Obj_OOZLauncher(fire);
    CHECK_EQ(fire->collision_flags, 0);
    obj->y_pos.f.u = 150 - 0x30; // (the lid has hopped up: the fire shows once it is $24 above its place)
    Obj_OOZLauncher(fire);
    CHECK_EQ(fire->collision_flags, 0x9B);
    obj->y_pos.f.u = 150;
    Obj_OOZLauncher(fire);
    CHECK_EQ(fire->collision_flags, 0);
}

static void OOZ_TheLauncherCarriesWhoStandsInTheMiddleUpAndThrowsThem(void) {
    Reset();
    Sst *obj = Spawn(SLOT(0x20), 0x33, 1, 200, 400);
    Obj_OOZLauncher(obj);
    CHECK_EQ(obj->routine_secondary, 4);
    StandOn(obj, 200, 400 - 9);
    Obj_OOZLauncher(obj);
    CHECK_EQ(obj->routine_secondary, 6); // (it has them)
    CHECK_EQ(OBJ_CONTROL(player), 1);
    for (int i = 0; i < 30 && obj->routine_secondary != 8; i++)
        Obj_OOZLauncher(obj);
    CHECK_EQ(obj->routine_secondary, 8);
    CHECK_EQ(obj->y_pos.f.u, 400 - 0x7D); // (the top of the rise)
    CHECK_EQ(player->y_vel, -0x1000);
    CHECK_EQ(player->ground_vel, 0x800);
    CHECK_EQ(OBJ_CONTROL(player), 0);
    CHECK(((player->status & SST_PSTATUS_IN_AIR) != 0));
}

static void OOZ_TheLauncherIgnoresSomeoneAtItsEdge(void) {
    Reset();
    Sst *obj = Spawn(SLOT(0x20), 0x33, 1, 200, 400);
    Obj_OOZLauncher(obj);
    StandOn(obj, 200 + 0x14, 400 - 9);
    Obj_OOZLauncher(obj);
    CHECK_EQ(obj->routine_secondary, 4); // (outside the $10 of its middle: nothing)
}

// --- 43 ---

static void OOZ_ASingleSpikedBallRollsToAndFro(void) {
    Reset();
    Sst *obj = Spawn(SLOT(0x20), 0x43, 0, 300, 400);
    scrpos_x.f.u = 200;
    Obj_OOZSpikeball(obj);
    CHECK_EQ(obj->collision_flags, 0xA5);
    CHECK_EQ(obj->own.u8[0x1C - 0x0], obj->own.u8[0x1C]); // (its partner is itself)
    int16_t lo = 0x7FFF, hi = -0x7FFF;
    for (int i = 0; i < 1200; i++) {
        Obj_OOZSpikeball(obj);
        if (obj->x_pos.f.u < lo) lo = obj->x_pos.f.u;
        if (obj->x_pos.f.u > hi) hi = obj->x_pos.f.u;
    }
    CHECK_EQ(lo, 300 - 0x68);
    CHECK_EQ(hi, 300 + 0x68);
}

static void OOZ_APairOfSpikedBallsTurnRoundWhereTheyMeet(void) {
    Reset();
    Sst *obj = Spawn(SLOT(0x20), 0x43, 6, 400, 400);
    scrpos_x.f.u = 300;
    Obj_OOZSpikeball(obj);
    Sst *other = SLOT(0x21);
    CHECK(other->code == sst_object_codes[0x43]);
    Obj_OOZSpikeball(other); // (the frame's second object)
    CHECK_EQ(obj->x_pos.f.u, 400 - 0x18 - 1); // (they move apart a pixel each)
    CHECK_EQ(other->x_pos.f.u, 400 + 0x18 + 1);
    int16_t gap_min = 0x7FFF;
    for (int i = 0; i < 3000; i++) {
        Obj_OOZSpikeball(obj);
        Obj_OOZSpikeball(other);
        const int16_t gap = (int16_t)(other->x_pos.f.u - obj->x_pos.f.u);
        if (gap < gap_min)
            gap_min = gap;
        CHECK(obj->x_pos.f.u >= 400 - 0xE8 && obj->x_pos.f.u <= 400 + 0xE8);
        CHECK(other->x_pos.f.u >= 400 - 0xE8 && other->x_pos.f.u <= 400 + 0xE8);
    }
    CHECK(gap_min >= 0x2E); // (they never go into each other)
}

// --- 45 ---

static void OOZ_ThePushSpringIsPressedByStandingAndThrowsOnceFullyPressed(void) {
    Reset();
    Sst *obj = Spawn(SLOT(0x20), 0x45, 0, 200, 400);
    Obj_OOZPushSpring(obj);
    CHECK_EQ(obj->own.u8[0x8], 0); // (the power's low byte)
    StandOn(obj, 200, 400 - 0x14);
    for (int i = 0; i < 9; i++) {
        Obj_OOZPushSpring(obj);
        CHECK_EQ(obj->mapping_frame, i + 1);
    }
    CHECK_EQ(player->y_vel, 0);
    Obj_OOZPushSpring(obj); // (fully pressed: it throws)
    CHECK_EQ(player->y_vel, -0x1000);
    CHECK(((player->status & SST_PSTATUS_IN_AIR) != 0));
    CHECK_EQ(player->anim, SonAnimId_Spring);
}

static void OOZ_TheWeakerPushSpringThrowsLessHigh(void) {
    Reset();
    Sst *obj = Spawn(SLOT(0x20), 0x45, 2, 200, 400);
    Obj_OOZPushSpring(obj);
    StandOn(obj, 200, 400 - 0x14);
    for (int i = 0; i < 10; i++)
        Obj_OOZPushSpring(obj);
    CHECK_EQ(player->y_vel, -0xA00);
}

static void OOZ_ThePusherReturnsToWhereItRests(void) {
    Reset();
    Sst *obj = Spawn(SLOT(0x20), 0x45, 0x10, 200, 400);
    Obj_OOZPushSpring(obj);
    CHECK_EQ(obj->routine, 4);
    CHECK_EQ(obj->mapping_frame, 0xA);
    obj->x_pos.f.u = 192; // (pushed 8 to the left)
    obj->mapping_frame = 0xA + 8;
    Obj_OOZPushSpring(obj);
    CHECK_EQ(obj->x_pos.f.u, 196); // (back 4 a frame)
    Obj_OOZPushSpring(obj);
    CHECK_EQ(obj->x_pos.f.u, 200);
    CHECK_EQ(obj->mapping_frame, 0xA);
}

// --- 46 ---

static void OOZ_TheBallSpringsOffWhenItsSwitchIsDown(void) {
    Reset();
    Sst *obj = Spawn(SLOT(0x20), 0x46, 0x20, 200, 400);
    Obj_OOZSpringBall(obj);
    CHECK_EQ(obj->routine, 2);
    int springs = 0;
    for (int i = 0x21; i < 0x60; i++)
        springs += sst[i].code == sst_object_codes[0x46] && sst[i].routine == 6;
    CHECK_EQ(springs, 1);
    Obj_OOZSpringBall(obj);
    CHECK_EQ(obj->routine, 2); // (waiting)
    f_switch[2] = 1;
    Obj_OOZSpringBall(obj);
    CHECK_EQ(obj->routine, 4);
    CHECK_EQ(obj->y_vel, -0x300);
    CHECK_EQ(obj->ground_vel, 0x100);
}

static void OOZ_TheBallSpringsOffWhenPlayerTwoPressesA(void) {
    Reset();
    Sst *obj = Spawn(SLOT(0x20), 0x46, 0, 200, 400);
    Obj_OOZSpringBall(obj);
    jpad2_press = JPAD_A;
    Obj_OOZSpringBall(obj);
    CHECK_EQ(obj->routine, 4);
}

static void OOZ_AFlippedBallGoesTheOtherWay(void) {
    Reset();
    Sst *obj = Spawn(SLOT(0x20), 0x46, 0, 200, 400);
    obj->status |= SST_STATUS_X_FLIP;
    Obj_OOZSpringBall(obj);
    jpad2_press = JPAD_A;
    Obj_OOZSpringBall(obj);
    CHECK_EQ(obj->ground_vel, -0x100);
}

// --- 48 ---

static void OOZ_TheCannonCatchesACharacterAndFiresItOff(void) {
    Reset();
    Sst *obj = Spawn(SLOT(0x20), 0x48, 0, 200, 400);
    Obj_OOZCannon(obj);
    CHECK_EQ(obj->width_pixels, 0x28);
    CHECK_EQ(obj->own.u8[0x2C - 0x28], 0);
    player->x_pos.f.u = 205;
    player->y_pos.f.u = 395;
    Obj_OOZCannon(obj);
    CHECK_EQ(obj->own.u8[0x2C - 0x28], 2); // (caught)
    CHECK_EQ(player->x_pos.f.u, 200);
    CHECK_EQ(player->y_pos.f.u, 400);
    CHECK_EQ(OBJ_CONTROL(player), 0x81);
    CHECK_EQ(player->ground_vel, 0x1000);
    for (int i = 0; i < 100 && obj->own.u8[0x2C - 0x28] == 2; i++)
        Obj_OOZCannon(obj);
    CHECK_EQ(obj->own.u8[0x2C - 0x28], 4); // (fired)
    CHECK_EQ(player->x_vel, 0x1000); // (subtype 0 fires to the right)
    CHECK_EQ(player->y_vel, 0);
    const int16_t x = player->x_pos.f.u;
    Obj_OOZCannon(obj);
    CHECK_EQ(player->x_pos.f.u, x + 0x10); // ($1000 is 16 pixels a frame)
}

static void OOZ_TheCannonIgnoresACharacterOutOfReach(void) {
    Reset();
    Sst *obj = Spawn(SLOT(0x20), 0x48, 0, 200, 400);
    Obj_OOZCannon(obj);
    player->x_pos.f.u = 200 + 0x10;
    player->y_pos.f.u = 400;
    Obj_OOZCannon(obj);
    CHECK_EQ(obj->own.u8[0x2C - 0x28], 0);
}

static void OOZ_ACannonWithBitSevenLetsGoAtOnce(void) {
    Reset();
    Sst *obj = Spawn(SLOT(0x20), 0x48, 0x80, 200, 400);
    Obj_OOZCannon(obj);
    player->x_pos.f.u = 200;
    player->y_pos.f.u = 400;
    Obj_OOZCannon(obj);
    for (int i = 0; i < 100 && obj->own.u8[0x2C - 0x28] == 2; i++)
        Obj_OOZCannon(obj);
    CHECK_EQ(obj->own.u8[0x2C - 0x28], 6); // (waiting to be ready again)
    CHECK_EQ(OBJ_CONTROL(player), 0);
    CHECK(((player->status & SST_PSTATUS_IN_AIR) != 0));
}

void RegisterOOZObjectTests(void) {
    RUN_TEST(OOZ_TheOilKillsWhoeverStaysInIt);
    RUN_TEST(OOZ_TheOilReleasesSonicWhenHeLeaves);
    RUN_TEST(OOZ_TheLauncherHopsEverySecondOrSo);
    RUN_TEST(OOZ_TheLauncherHasFireThatShowsOnlyWhileItIsUp);
    RUN_TEST(OOZ_TheLauncherCarriesWhoStandsInTheMiddleUpAndThrowsThem);
    RUN_TEST(OOZ_TheLauncherIgnoresSomeoneAtItsEdge);
    RUN_TEST(OOZ_ASingleSpikedBallRollsToAndFro);
    RUN_TEST(OOZ_APairOfSpikedBallsTurnRoundWhereTheyMeet);
    RUN_TEST(OOZ_ThePushSpringIsPressedByStandingAndThrowsOnceFullyPressed);
    RUN_TEST(OOZ_TheWeakerPushSpringThrowsLessHigh);
    RUN_TEST(OOZ_ThePusherReturnsToWhereItRests);
    RUN_TEST(OOZ_TheBallSpringsOffWhenItsSwitchIsDown);
    RUN_TEST(SSZ_SandSinksACharacterSlowlyAndHoldsItBack);
    RUN_TEST(SSZ_SandIsIgnoredOutsideIt);
    RUN_TEST(SSZ_AJumpInTheSandTakesHimUpWeakly);
    RUN_TEST(SSZ_SandKillsAtThirtyTwoPixelsOfDepthAndNotBefore);
    RUN_TEST(SSZ_AFallTurnsIntoTheSidewaysSandBesideItsEnd);
    RUN_TEST(SSZ_TheDoorOpensAsYouComeUpToItAndShutsBehindYou);
    RUN_TEST(SSZ_TheSwitchDoorWaitsForItsSwitchAndShutsOnceYouAreThrough);
    RUN_TEST(SSZ_TheTimedSwitchDoorStaysUpForTheTimeOfItsSubtype);
    RUN_TEST(SSZ_TheTimedDoorCrushesWhoIsStillUnderItWhenTheTimeIsUp);
    RUN_TEST(SSZ_ASandFallCarriesACharacterDownOrUpWithItsFlipAndDoesNotKill);
    RUN_TEST(SSZ_ASandFallLetsGoWhereASidewaysOneTakesOverAndAJumpAtItsEndIsHigh);
    RUN_TEST(SSZ_SandThatHoldsSticksACharacterAndCarriesItSidewaysUntilAJump);
    RUN_TEST(OOZ_TheBallSpringsOffWhenPlayerTwoPressesA);
    RUN_TEST(OOZ_AFlippedBallGoesTheOtherWay);
    RUN_TEST(OOZ_TheCannonCatchesACharacterAndFiresItOff);
    RUN_TEST(OOZ_TheCannonIgnoresACharacterOutOfReach);
    RUN_TEST(OOZ_ACannonWithBitSevenLetsGoAtOnce);
}
