#include "test.h"

#include <string.h>

#include "Level.h"
#include "LevelCollision.h"
#include "LevelDraw.h"
#include "LevelScroll.h"
#include "Object.h"
#include "Object/PushBlock.h"
#include "SstLoad.h"

// Pushable green blocks (object 33) in Marble Zone: pushed off a ledge into lava, a block drifts along it until it meets a wall,
// then sinks for 160 frames and is gone (or back at where it started when that is still on screen).

static void FollowCamera(const Sst *block) {
    scrpos_x.f.u = (int16_t)(block->x_pos.f.u - 160);
    scrpos_y.f.u = (int16_t)(block->y_pos.f.u - 112);
    SstLoadCamera camera = {scrpos_x.f.u, scrpos_y.f.u, false, 0xFFF};
    SstLoad_Update(&camera);
}

// Drops a block into the lava at (start_x, start_y) the way a push off a ledge does, with Sonic riding on top of it (the
// geyser makers only fire while he is above them), and follows it with the camera until it has sunk. Returns the number of
// frames the block spent sinking (0 if it never did).
static int RunBlock(int act, int16_t start_x, int16_t start_y, int16_t speed, int16_t orig_x, int16_t orig_y) {
    level_id = LEVEL_ID(ZoneId_MZ, act);
    LevelDataLoad();
    ColIndexLoad();
    memset(player, 0, sizeof(Sst));
    player->routine = 2;
    Sst_Reset();
    static const SstObjectIndex index_ = {sst_object_codes, 0x100};
    SstLoadCamera first = {start_x - 160, start_y - 112, false, 0xFFF};
    SstLoad_Init(&index_, NULL, false, &first);

    Sst *block = Sst_Spawn(ObjId_PushBlock, start_x, start_y);
    block->routine = 2;
    block->routine_secondary = 4; // falling
    block->y_radius = 15;
    block->x_radius = 15;
    block->width_pixels = 16;
    block->art_tile = 1;
    block->mappings = (const void *)block; // anything non-null
    Own_PushBlock *s = (Own_PushBlock *)block->own.u8;
    s->lava_speed = speed;
    s->orig_x = orig_x;
    s->orig_y = orig_y;

    int sinking = 0;
    for (int frame = 0; frame < 6000; frame++) {
        FollowCamera(block);
        player->x_pos.f.u = block->x_pos.f.u;
        player->y_pos.f.u = (int16_t)(block->y_pos.f.u - 0x28);
        player->y_radius = 19;
        player->x_radius = 9;
        ExecuteObjects();
        if (block->code == NULL || block->routine != 2)
            break; // sunk: deleted, or reset to its origin
        if (s->on_lava && block->x_vel == 0 && block->routine_secondary == 0 + (block->routine_secondary & 2))
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
