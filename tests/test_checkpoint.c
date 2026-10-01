#include "test.h"

#include <string.h>

#include "Level.h"
#include "LevelScroll.h"
#include "Object.h"
void Obj_Checkpoint_StoreInfo(Object *obj, const void *scratch); // Checkpoint.h defines its mappings array, so not included

// Regression test (2026-09): lamp posts were broken -- after dying you
// respawned at (0,0) with the camera at 0,0. The saved positions were the
// LOW word of the 16.16 fixed-point fields (the fraction, always 0) instead of
// the pixel position, and loading wrote them back into the fraction too.

static void Checkpoint_RoundTripKeepsPositions(void) {
    memset(level_objects, 0, sizeof(Object) * LEVEL_OBJECTS);
    memset(player, 0, sizeof(Object));
    Object *lamp = &level_objects[0];
    lamp->type = ObjId_Checkpoint;
    lamp->pos.l.x.f.u = 0x1290;
    lamp->pos.l.y.f.u = 0x04AC;
    lamp->scratch.u8[0] = 1;

    scrpos_x.f.u = 0x1150; scrpos_x.f.l = 0x4000;
    scrpos_y.f.u = 0x0420;
    bg_scrpos_x.f.u = 0x0300; bg_scrpos_y.f.u = 0x0100;
    bg2_scrpos_x.f.u = 0x0200; bg2_scrpos_y.f.u = 0x0080;
    bg3_scrpos_x.f.u = 0x0180; bg3_scrpos_y.f.u = 0x0040;
    life_count = 2;
    level_id = LEVEL_ID(ZoneId_LZ, 2);

    Obj_Checkpoint_StoreInfo(lamp, &lamp->scratch);
    CHECK_EQ(last_lamp, 1);

    // The player has died and the level restarts: everything is reset.
    memset(player, 0, sizeof(Object));
    scrpos_x.v = 0; scrpos_y.v = 0; bg_scrpos_x.v = 0; bg_scrpos_y.v = 0;
    bg2_scrpos_x.v = 0; bg2_scrpos_y.v = 0; bg3_scrpos_x.v = 0; bg3_scrpos_y.v = 0;
    rings = 77;

    Obj_Checkpoint_LoadInfo();

    CHECK_EQ(player->pos.l.x.f.u, 0x1290);
    CHECK_EQ(player->pos.l.y.f.u, 0x04AC);
    CHECK_EQ(scrpos_x.f.u, 0x1150);
    CHECK_EQ(scrpos_y.f.u, 0x0420);
    CHECK_EQ(bg_scrpos_x.f.u, 0x0300);
    CHECK_EQ(bg_scrpos_y.f.u, 0x0100);
    CHECK_EQ(bg2_scrpos_x.f.u, 0x0200);
    CHECK_EQ(bg2_scrpos_y.f.u, 0x0080);
    CHECK_EQ(bg3_scrpos_x.f.u, 0x0180);
    CHECK_EQ(bg3_scrpos_y.f.u, 0x0040);
    CHECK_EQ(rings, 0); // rings are reset on respawn
    CHECK_EQ(life_count, 0); // loaded then cleared, like the original
}

void RegisterCheckpointTests(void) {
    RUN_TEST(Checkpoint_RoundTripKeepsPositions);
}
