#include "test.h"

#include <string.h>

#include "Level.h"
#include "LevelScroll.h"
#include "Object.h"
#include "SstLoad.h"

// Object 52 subtype 7 (MBlock_SecretLZ1Raft): LZ1's hidden raft at
// ($9C0, $108) stays invisible until switch 2 (at $A10, $2F8) is pressed,
// then turns into type 4 (moves right when stood on, drops on wall hit).

static void Camera(int16_t x) {
    static const SstObjectIndex index_ = {sst_object_codes, 0x100};
    SstLoadCamera camera = {x, 0, false, 0xFFF};
    SstLoad_Init(&index_, NULL, false, &camera);
}

static void SpawnRaft(uint8_t poison) {
    Sst_Reset();
    for (int i = SST_FIRST_DYNAMIC; i < SST_COUNT; i++) {
        memset(&sst[i], poison, sizeof(Sst));
        sst[i].code = NULL;
    }
    level_id = LEVEL_ID(ZoneId_LZ, 0);
    memset(f_switch, 0, sizeof(f_switch));
    memset(player, 0, sizeof(Sst));
    player->routine = 2;
    player->x_pos.f.u = 0xA10;
    player->y_pos.f.u = 0x2E0;
    scrpos_x.f.u = 0x980;
    Camera(0x980);
}

static void RunRaft(Sst *raft, int frames) {
    for (int i = 0; i < frames && raft->code == sst_object_codes[ObjId_MovingBlock]; i++)
        raft->code(raft);
}

static void LZRaft_AppearsWhenSwitch2Pressed(void) {
    SpawnRaft(0);
    Sst *raft = Sst_Spawn(ObjId_MovingBlock, 0x9C0, 0x108);
    raft->subtype = 0x07;

    RunRaft(raft, 30);
    CHECK(raft->code == sst_object_codes[ObjId_MovingBlock]); // waiting, not deleted
    CHECK_EQ(raft->subtype, 7);

    f_switch[2] = 1;
    RunRaft(raft, 2);
    CHECK(raft->code == sst_object_codes[ObjId_MovingBlock]);
    CHECK_EQ(raft->subtype, 4); // became the visible raft
}

// The real spawn path: ObjPosLoad into free slots full of stale bytes (as
// left by writes after a deletion). A stale nonzero routine meant the raft
// skipped MBlock_Main and matched no routine at all -- never visible, never
// reacting to the switch.
extern uint16_t opl_routine;
static void LZRaft_LoadsCleanIntoDirtySlot(void) {
    SpawnRaft(0xAB);
    opl_routine = 0;
    ObjPosLoad();

    Sst *raft = NULL;
    for (int i = 0; i < SST_COUNT; i++)
        if (sst[i].code == sst_object_codes[ObjId_MovingBlock] && sst[i].x_pos.f.u == 0x9C0)
            raft = &sst[i];
    CHECK(raft != NULL);
    if (raft == NULL)
        return;
    CHECK_EQ(raft->routine, 0);

    RunRaft(raft, 5);
    CHECK_EQ(raft->routine, 2);
    CHECK_EQ(raft->subtype, 7);
    f_switch[2] = 1;
    RunRaft(raft, 2);
    CHECK_EQ(raft->subtype, 4);
}

void RegisterLZRaftTests(void) {
    RUN_TEST(LZRaft_AppearsWhenSwitch2Pressed);
    RUN_TEST(LZRaft_LoadsCleanIntoDirtySlot);
}
