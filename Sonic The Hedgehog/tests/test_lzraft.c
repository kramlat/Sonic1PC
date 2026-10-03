#include "test.h"

#include <string.h>

#include "Level.h"
#include "LevelScroll.h"
#include "Object.h"
void Obj_MovingBlock(Object *obj); // MovingBlock.h defines its mappings arrays, so not included

// Object 52 subtype 7 (MBlock_SecretLZ1Raft): LZ1's hidden raft at
// ($9C0, $108) stays invisible until switch 2 (at $A10, $2F8) is pressed,
// then turns into type 4 (moves right when stood on, drops on wall hit).

static Object *SpawnRaft(uint8_t poison) {
    for (int i = 0; i < LEVEL_OBJECTS; i++) {
        memset(&level_objects[i], poison, sizeof(Object));
        level_objects[i].type = ObjId_Null;
    }
    level_id = LEVEL_ID(ZoneId_LZ, 0);
    memset(f_switch, 0, sizeof(f_switch));
    memset(player, 0, sizeof(Object));
    player->routine = 2;
    player->pos.l.x.f.u = 0xA10;
    player->pos.l.y.f.u = 0x2E0;
    scrpos_x.f.u = 0x980;
    return &level_objects[5];
}

static void RunRaft(Object *raft, int frames) {
    for (int i = 0; i < frames && raft->type == ObjId_MovingBlock; i++)
        Obj_MovingBlock(raft);
}

static void LZRaft_AppearsWhenSwitch2Pressed(void) {
    Object *raft = SpawnRaft(0);
    raft->type = ObjId_MovingBlock;
    raft->pos.l.x.f.u = 0x9C0;
    raft->pos.l.y.f.u = 0x108;
    raft->scratch.u8[0] = 0x07;

    RunRaft(raft, 30);
    CHECK_EQ(raft->type, ObjId_MovingBlock); // waiting, not deleted
    CHECK_EQ(raft->scratch.u8[0], 7);

    f_switch[2] = 1;
    RunRaft(raft, 2);
    CHECK_EQ(raft->type, ObjId_MovingBlock);
    CHECK_EQ(raft->scratch.u8[0], 4); // became the visible raft
}

// The real spawn path: ObjPosLoad into free slots full of stale bytes (as
// left by writes after ObjectDelete). A stale nonzero routine meant the raft
// skipped MBlock_Main and matched no routine at all -- never visible, never
// reacting to the switch.
extern uint16_t opl_routine;
static void LZRaft_LoadsCleanIntoDirtySlot(void) {
    SpawnRaft(0xAB);
    opl_routine = 0;
    ObjPosLoad();

    Object *raft = NULL;
    for (int i = 0; i < LEVEL_OBJECTS; i++)
        if (level_objects[i].type == ObjId_MovingBlock && level_objects[i].pos.l.x.f.u == 0x9C0)
            raft = &level_objects[i];
    CHECK(raft != NULL);
    if (raft == NULL)
        return;
    CHECK_EQ(raft->routine, 0);

    RunRaft(raft, 5);
    CHECK_EQ(raft->routine, 2);
    CHECK_EQ(raft->scratch.u8[0], 7);
    f_switch[2] = 1;
    RunRaft(raft, 2);
    CHECK_EQ(raft->scratch.u8[0], 4);
}

void RegisterLZRaftTests(void) {
    RUN_TEST(LZRaft_AppearsWhenSwitch2Pressed);
    RUN_TEST(LZRaft_LoadsCleanIntoDirtySlot);
}
