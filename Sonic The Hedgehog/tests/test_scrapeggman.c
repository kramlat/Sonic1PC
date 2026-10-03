#include "test.h"

#include <string.h>

#include "Level.h"
#include "LevelScroll.h"
#include "Object.h"
#include "PLC.h"
#include "Sound.h"
#include "Object/ScrapEggman.h"

// SBZ2's end cutscene (objects 82/83) and the dynamic level events that start it and chain SBZ3 -> Final Zone,
// from s1disasm's "_inc/DynamicLevelEvents.asm" and "_incObj/82, 83 SBZ Eggman Cutscene and Crumbling Floor.asm".
// boss_sbz2_x = 0x2050, boss_sbz2_y = 0x510.

#define SBZ2_X 0x2050
#define SBZ2_Y 0x510

static int Count(uint8_t type) {
    int n = 0;
    for (int i = 0; i < LEVEL_OBJECTS; i++)
        if (level_objects[i].type == type)
            n++;
    return n;
}

static void RunObjects(void) {
    for (int i = 0; i < LEVEL_OBJECTS; i++) {
        Object *o = &level_objects[i];
        if (o->type == ObjId_ScrapEggman)
            Obj_ScrapEggman(o);
        else if (o->type == ObjId_FalseFloor) {
            o->render.f.on_screen = true; // BuildSprites would set this for anything drawn on screen
            Obj_FalseFloor(o);
        }
    }
    frame_count++;
}

static void Reset(uint16_t id) {
    memset(level_objects, 0, sizeof(Object) * LEVEL_OBJECTS);
    memset(player, 0, sizeof(Object));
    player->routine = 2;
    level_id = id;
    dle_routine = 0;
    lock_screen = false;
    restart = false;
    last_lamp = 1;
    lock_multi = 0;
    limit_btm1 = limit_btm2 = 0x800;
    limit_left2 = 0;
    player->pos.l.y.f.u = SBZ2_Y;
}

static void SBZ2_EventsSpawnTheCutscene(void) {
    Reset(LEVEL_ID(ZoneId_SBZ, 1));
    scrpos_x.f.u = 0x1000;
    DynamicLevelEvents();
    CHECK_EQ(limit_btm1 & 0xFFF, 0x800 & 0xFFF); // boundary open at the start of the act
    CHECK_EQ(dle_routine, 0);

    scrpos_x.f.u = 0x1E00; // main -> blocks
    DynamicLevelEvents();
    CHECK_EQ(dle_routine, 2);
    CHECK_EQ(Count(ObjId_FalseFloor), 0);

    scrpos_x.f.u = SBZ2_X - 0x1A0; // blocks: spawns the false floor manager
    DynamicLevelEvents();
    CHECK_EQ(dle_routine, 4);
    CHECK_EQ(Count(ObjId_FalseFloor), 1);

    scrpos_x.f.u = SBZ2_X - 0xF0; // Eggman: spawns him and locks the screen
    DynamicLevelEvents();
    CHECK_EQ(dle_routine, 6);
    CHECK_EQ(Count(ObjId_ScrapEggman), 1);
    CHECK(lock_screen);
}

static void SBZ2_FloorBuildsAndCollapses(void) {
    Reset(LEVEL_ID(ZoneId_SBZ, 1));
    scrpos_x.f.u = SBZ2_X - 0xF0;
    Object *manager = &level_objects[0];
    manager->type = ObjId_FalseFloor;
    RunObjects();
    CHECK_EQ(Count(ObjId_FalseFloor), 9); // manager + 8 blocks
    CHECK_EQ(manager->routine, 2);

    // Nothing happens until Eggman sends "GO".
    for (int i = 0; i < 100; i++)
        RunObjects();
    CHECK_EQ(Count(ObjId_FalseFloor), 9);

    ((Scratch_FalseFloor *)&manager->scratch)->cmd = 0x474F; // "GO"
    for (int i = 0; i < 400; i++)
        RunObjects();
    // Every block has become 4 fragments (the manager is gone).
    CHECK_EQ(manager->type, 0);
    CHECK_EQ(Count(ObjId_FalseFloor), 8 * 4);
}

static void SBZ2_EggmanPressesTheSwitch(void) {
    Reset(LEVEL_ID(ZoneId_SBZ, 1));
    scrpos_x.f.u = SBZ2_X - 0xF0;
    level_objects[0].type = ObjId_FalseFloor;
    level_objects[20].type = ObjId_ScrapEggman;
    player->pos.l.x.f.u = SBZ2_X + 0x80; // far from Eggman (0x2160)
    RunObjects();
    CHECK_EQ(Count(ObjId_ScrapEggman), 2); // Eggman + his switch
    Object *eggman = &level_objects[20];
    CHECK_EQ(eggman->pos.l.x.f.u, SBZ2_X + 0x110);
    CHECK_EQ(eggman->routine_sec, 0);

    for (int i = 0; i < 60; i++) // he waits for Sonic
        RunObjects();
    CHECK_EQ(eggman->routine_sec, 0);

    player->pos.l.x.f.u = SBZ2_X + 0xC0; // within 128px
    for (int i = 0; i < 400 && eggman->routine_sec != 6; i++)
        RunObjects();
    CHECK_EQ(eggman->routine_sec, 6);                 // leapt, landed and set the floor off
    CHECK_EQ(eggman->pos.l.y.f.u, SBZ2_Y + 0x8B);     // snapped to the floor
    CHECK_EQ(((Scratch_FalseFloor *)&level_objects[0].scratch)->cmd, 0x474F);
}

static void SBZ3_TopOfTheLevelLeadsToFinalZone(void) {
    Reset(LEVEL_ID(ZoneId_LZ, 3));
    scrpos_x.f.u = 0xD00;
    player->pos.l.y.f.u = 0x100; // not at the top yet
    DynamicLevelEvents();
    CHECK(!restart);

    scrpos_x.f.u = 0xCF0; // not far enough right
    player->pos.l.y.f.u = 0x10;
    DynamicLevelEvents();
    CHECK(!restart);

    scrpos_x.f.u = 0xD00;
    DynamicLevelEvents();
    CHECK(restart);
    CHECK_EQ(level_id, LEVEL_ID(ZoneId_SBZ, 2));
    CHECK_EQ(last_lamp, 0);
    CHECK_EQ(lock_multi, 1);
}

void Obj_GotThroughCard(Object *obj);

// The end-of-act card in SBZ2 doesn't lead to the next act: after the tally it slides out, hands control back,
// starts the Final Zone music and opens the right boundary up to the cutscene room (Got_SBZ2_MoveOut/Boundary).
static void SBZ2_GotThroughCardLeadsIntoTheCutscene(void) {
    Reset(LEVEL_ID(ZoneId_SBZ, 1));
    limit_right2 = SBZ2_X - 0x100;
    time_bonus = ring_bonus = 0; // nothing to tally down
    lock_ctrl = true;
    plc_buffer[0].art = NULL; // the card waits for its art to finish loading
    sound_music.queue[SOUND_QUEUE_NORMAL] = 0; // nothing queued yet
    level_objects[0].type = ObjId_GotThroughCard;

    Object *ring = NULL;
    for (int f = 0; f < 3000; f++) {
        for (int i = 0; i < LEVEL_OBJECTS; i++)
            if (level_objects[i].type == ObjId_GotThroughCard)
                Obj_GotThroughCard(&level_objects[i]);
        for (int i = 0; i < LEVEL_OBJECTS; i++)
            if (level_objects[i].type == ObjId_GotThroughCard && level_objects[i].frame == 4)
                ring = &level_objects[i];
        if (f == 600) { // well into the 3s waits and tally, but not done
            CHECK(ring != NULL && ring->routine != 0x10);
            CHECK_EQ(sound_music.queue[SOUND_QUEUE_NORMAL], 0); // no music change before the slide-out
        }
        if (ring != NULL && ring->routine == 0x10)
            break;
    }
    CHECK(ring != NULL);
    if (ring == NULL)
        return;
    CHECK_EQ(ring->routine, 0x10);
    CHECK(!lock_ctrl);       // controls handed back
    CHECK_EQ(sound_music.queue[SOUND_QUEUE_NORMAL], bgm_FZ); // and the Final Zone music queued for the cutscene
    CHECK(!restart);         // and no jump to a next act
    CHECK_EQ(Count(ObjId_GotThroughCard), 1); // only the Ring Bonus element is left, holding the boundary open

    for (int f = 0; f < 400 && Count(ObjId_GotThroughCard) > 0; f++)
        Obj_GotThroughCard(ring);
    CHECK_EQ(limit_right2, SBZ2_X + 0xB0);
    CHECK_EQ(Count(ObjId_GotThroughCard), 0);
}

// Falling out of Eggman's room (the false floor gone) loads stage 0x0103 -- SBZ3 -- from the cutscene's own
// trigger, only once the cutscene is under way, and never kills Sonic on the way.
static void SBZ2_FallingThroughTheFloorLoadsSBZ3(void) {
    Reset(LEVEL_ID(ZoneId_SBZ, 1));
    player->pos.l.x.f.u = SBZ2_X + 0x40;

    scrpos_x.f.u = SBZ2_X - 0x200; // camera still far away: Eggman isn't spawned and the cutscene hasn't started
    player->pos.l.y.f.u = SBZ2_Y + 0x300; // below the room anyway
    dle_routine = 4;
    DynamicLevelEvents();
    CHECK_EQ(dle_routine, 4);
    CHECK(!restart);

    scrpos_x.f.u = SBZ2_X;

    dle_routine = 6; // cutscene running
    player->pos.l.y.f.u = SBZ2_Y + 0xA0; // standing on the floor
    DynamicLevelEvents();
    CHECK(!restart);

    player->pos.l.x.f.u = 0x1F00; // below the floor, but not in the cutscene room
    player->pos.l.y.f.u = SBZ2_Y + 0x300;
    DynamicLevelEvents();
    CHECK(!restart);

    player->pos.l.x.f.u = SBZ2_X + 0x40;
    DynamicLevelEvents(); // fell through
    CHECK(restart);
    CHECK_EQ(level_id, 0x0103);
    CHECK_EQ(last_lamp, 0);
}

void RegisterScrapEggmanTests(void) {
    RUN_TEST(SBZ2_EventsSpawnTheCutscene);
    RUN_TEST(SBZ2_FloorBuildsAndCollapses);
    RUN_TEST(SBZ2_EggmanPressesTheSwitch);
    RUN_TEST(SBZ2_FallingThroughTheFloorLoadsSBZ3);
    RUN_TEST(SBZ3_TopOfTheLevelLeadsToFinalZone);
    RUN_TEST(SBZ2_GotThroughCardLeadsIntoTheCutscene);
}
