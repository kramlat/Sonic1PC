#include "test.h"

#include <string.h>

#include "Level.h"
#include "LevelCollision.h"
#include "LevelDraw.h"
#include "LevelScroll.h"
#include "Object.h"
#include "Object/PushBlock.h"

// Pushable green blocks (object 33) in Marble Zone: pushed off a ledge into lava, a block drifts along it until it meets a wall,
// then sinks for 160 frames and is gone (or back at where it started when that is still on screen).

static void FollowCamera(const Object *block) {
    scrpos_x.f.u = (int16_t)(block->pos.l.x.f.u - 160);
    scrpos_y.f.u = (int16_t)(block->pos.l.y.f.u - 112);
}

// Drops a block into the lava at (start_x, start_y) the way a push off a ledge does, with Sonic riding on top of it (the
// geyser makers only fire while he is above them), and follows it with the camera until it has sunk. Returns the number of
// frames the block spent sinking (0 if it never did).
static int RunBlock(int act, int16_t start_x, int16_t start_y, int16_t speed, int16_t orig_x, int16_t orig_y) {
    level_id = LEVEL_ID(ZoneId_MZ, act);
    LevelDataLoad();
    ColIndexLoad();
    memset(objects, 0, sizeof(objects));
    memset(player, 0, sizeof(Object));
    player->routine = 2;

    Object *block = &objects[40];
    block->type = ObjId_PushBlock;
    block->routine = 2;
    block->routine_sec = 4; // falling
    block->pos.l.x.f.u = start_x;
    block->pos.l.y.f.u = start_y;
    block->y_rad = 15;
    block->x_rad = 15;
    block->width_pixels = 16;
    block->tile = 1;
    block->mappings = (const void *)block; // anything non-null
    Scratch_PushBlock *s = (Scratch_PushBlock *)&block->scratch;
    s->lava_speed = speed;
    s->orig_x = orig_x;
    s->orig_y = orig_y;

    int sinking = 0;
    for (int frame = 0; frame < 6000; frame++) {
        FollowCamera(block);
        player->pos.l.x.f.u = block->pos.l.x.f.u;
        player->pos.l.y.f.u = (int16_t)(block->pos.l.y.f.u - 0x28);
        player->y_rad = 19;
        player->x_rad = 9;
        ExecuteObjects();
        if (block->type != ObjId_PushBlock || block->routine != 2)
            break; // sunk: deleted, or reset to its origin
        if (s->on_lava && block->xsp == 0 && block->routine_sec == 0 + (block->routine_sec & 2))
            sinking++;
    }
    return sinking;
}

static void PushBlock_MZ2DriftsLeftToTheWallThenSinks(void) {
    int frames = RunBlock(1, 0xE00, 0x450, -0x400, 0xE30, 0x450);
    CHECK(frames >= 150); // the block sinks for 160 frames
}

void RegisterPushBlockTests(void) {
    RUN_TEST(PushBlock_MZ2DriftsLeftToTheWallThenSinks);
}
