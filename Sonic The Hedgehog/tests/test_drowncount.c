#include "test.h"

#include <string.h>

#include "Level.h"
#include "LevelScroll.h"
#include "Object.h"
#include "Object/DrownCount.h"
#include "Video.h"

// LZ's drowning countdown: once the air is down to 12 seconds, number bubbles (5, 4, 3, 2, 1, 0) rise from Sonic's mouth, then stick to
// the screen and flash.

static void DrownCount_NumbersAreShownAndStayShown(void) {
    level_id = LEVEL_ID(ZoneId_LZ, 0);
    memset(objects, 0, sizeof(objects));
    memset(player, 0, sizeof(Object));
    player->type = ObjId_Sonic;
    player->routine = 2;
    player->status.p.f.underwater = true;
    player->pos.l.x.f.u = 0x1200;
    player->pos.l.y.f.u = 0x400;
    scrpos_x.f.u = 0x1200 - 160;
    scrpos_y.f.u = 0x400 - 112;
    wtr_pos1 = 0x300;
    air = 12;

    Object *master = &objects[DROWNCOUNT_SLOT];
    master->type = ObjId_DrownCount;
    master->scratch.u8[0] = DROWNCOUNT_MASTER_BIT | 1;

    int shown_frames = 0, number_objects_seen = 0, max_air_seen = air;
    for (int frame = 0; frame < 60 * 14 && air > 0 && air != 0xFFFF; frame++) {
        ExecuteObjects();
        BuildSprites(NULL);
        for (int i = 0; i < OBJECTS; i++) {
            Object *o = &objects[i];
            if (o->type != ObjId_DrownCount || (o->scratch.u8[0] & DROWNCOUNT_MASTER_BIT))
                continue;
            if (o->routine == 0xC || o->routine == 0xE) { // a number, stuck to the screen
                number_objects_seen++;
                if (o->render.f.on_screen)
                    shown_frames++;
            }
        }
    }
    CHECK(number_objects_seen > 0);
    CHECK(shown_frames > 0);
}

void RegisterDrownCountTests(void) {
    RUN_TEST(DrownCount_NumbersAreShownAndStayShown);
}
