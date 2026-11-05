#include "SstLoad.h"
#include "Object.h"
#include "Object/TitleCard.h"

#include "Level.h"
#include "LevelScroll.h"

#include "Game.h"
#include "PLC.h"
#include "Sound.h"

// Time bonus lookup, indexed by (total seconds / 15), clamped to the last
// entry (0 points) for times of 5 minutes or more.
#define TIME_BONUSES_NUM 20
static const uint16_t time_bonuses[TIME_BONUSES_NUM] = {
    5000, 5000, 1000, 500,  // 0:00 - 0:59
    400,  400,  300,  300,  // 1:00 - 1:59
    200,  200,  200,  200,  // 2:00 - 2:59
    100,  100,  100,  100,  // 3:00 - 3:59
    50,   50,   50,   50,   // 4:00 - 4:59
};

// Loads the end-of-act "GOT THROUGH" title card sequence -- shared by
// Signpost (routine 6, level's own goal) and Prison (routine $E, boss
// capsule's own animal release). Idempotent: real hardware's own first
// check is "already loaded? then don't do it again" (its card is the
// status table's one, as v_endcard was), so it's
// safe to call from multiple objects/frames.
void GotThroughAct(void) {
    for (int i = 0; i < SST_COUNT; i++)
        if (sst[i].code != NULL && sst[i].code == Obj_GotThroughCard)
            return;

    // Reset game state
    limit_left2 = limit_right2;
    invincibility = false;
    time_count = false;

    // Load "Got through" card
    Sst_SpawnCode(Obj_GotThroughCard, 0, 0);
    NewPLC(PlcId_TitleCard);
    endact_bonus = true;

    // Time Bonus
#ifdef SCP_FIX_BUGS
    // Time doesn't update while Debug Mode is enabled, which always
    // results in an annoying, unskippable 50,000 point time bonus
    // with it enabled.
    if (!debug_mode)
#endif
    {
        uint16_t total_sec = (uint16_t)level_time.min * 60 + level_time.sec;
        uint16_t index = total_sec / 15;
        if (index >= TIME_BONUSES_NUM)
            index = TIME_BONUSES_NUM - 1;
        time_bonus = time_bonuses[index];
    }

    // Ring Bonus
    ring_bonus = rings * 10;

    PlayMusic(bgm_GotThrough);
}
