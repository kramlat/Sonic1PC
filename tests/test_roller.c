#include "test.h"

#include <string.h>

#include "Level.h"
#include "LevelCollision.h"
#include "LevelDraw.h"
#include "LevelScroll.h"
#include "Object.h"

// Roller (object 43, SYZ): rolling along the floor it meets a ledge, then jumps (the second time) and lands again. It must come down
// through the air, not snap to the floor the moment it starts falling.

static void Roller_JumpComesDownSmoothlyAndLands(void) {
    level_id = LEVEL_ID(ZoneId_SYZ, 0);
    LevelDataLoad();
    ColIndexLoad();
    memset(objects, 0, sizeof(objects));
    memset(player, 0, sizeof(Object));
    player->routine = 2;
    player->pos.l.x.f.u = 0x100; // far away
    player->pos.l.y.f.u = 0x100;
    scrpos_x.f.u = 0x710 - 160;
    scrpos_y.f.u = 0x250 - 112;

    Object *roller = &objects[40];
    roller->type = ObjId_Roller;
    roller->pos.l.x.f.u = 0x710;
    roller->pos.l.y.f.u = 0x250;
    for (int f = 0; f < 60 && roller->routine != 2; f++) // lands on the floor (invisible until then)
        ExecuteObjects();
    CHECK_EQ(roller->routine, 2);
    int16_t floor_y = roller->pos.l.y.f.u;

    roller->routine_sec = 6; // jumping
    roller->ysp = -0x600;
    int16_t prev_y = roller->pos.l.y.f.u;
    int max_drop = 0, frames_in_air = 0;
    for (int f = 0; f < 200 && roller->routine_sec == 6; f++) {
        ExecuteObjects();
        int drop = roller->pos.l.y.f.u - prev_y;
        if (drop > max_drop)
            max_drop = drop;
        prev_y = roller->pos.l.y.f.u;
        frames_in_air++;
    }
    CHECK(frames_in_air > 20);   // a real jump takes a while
    CHECK(max_drop <= 12);       // falls at gravity's pace, no snapping down
    CHECK_EQ(roller->routine_sec, 4); // landed: rolling again
    CHECK_EQ(roller->pos.l.y.f.u, floor_y);
}

void RegisterRollerTests(void) {
    RUN_TEST(Roller_JumpComesDownSmoothlyAndLands);
}
