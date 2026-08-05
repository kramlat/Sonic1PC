#include "Signpost.h"

#include "Game.h"
#include "Level.h"
#include "LevelScroll.h"

#include "Macros.h"

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

// Signpost object
static const int8_t sparkle_pos[8][2] = {
    { -24, -16 },
    { 8, 8 },
    { -16, 0 },
    { -24, -8 },
    { 0, -8 },
    { -16, 0 },
    { -24, 8 },
    { 24, 16 },
};

void Obj_Signpost(Object* obj) {
    Scratch_Signpost* scratch = (Scratch_Signpost*)&obj->scratch;

    switch (obj->routine) {
    case 0: // Initialization
        // Increment routine
        obj->routine += 2;

        // Set object drawing information
        obj->mappings = Mappings_Signpost;
        obj->tile = TILE_MAP(0, 0, 0, 0, 0x680);
        obj->render.b = 0;
        obj->render.f.align_fg = true;
        obj->width_pixels = 24;
        obj->priority = 4;
        // Fallthrough
    case 2: // Wait for contact
        // Check if player is touching signpost
        if ((uint16_t)(player->pos.l.x.f.u - obj->pos.l.x.f.u) < 0x20) {
            // Start spinning
            // music	sfx_Signpost,0,0,0	; play signpost sound TODO
            time_count = false;
            limit_left2 = limit_right2;
            obj->routine += 2;
        }
        break;
    case 4: // Spinning
        // Handle spinning animation
        if (--scratch->spin_time < 0) {
            // Do next animation and increment routine when stopped spinning
            scratch->spin_time = 60;
            if (++obj->anim == 3)
                obj->routine += 2;
        }

        // Handle sparkling
        if (--scratch->sparkle_time < 0) {
            // Update sparkle time and ID
            scratch->sparkle_time = 11;
            uint8_t sparkle_id = scratch->sparkle_id;
            scratch->sparkle_id = (scratch->sparkle_id + 2) & 0xE;

            // Spawn sparkle
            const int8_t* spawn_pos = sparkle_pos[sparkle_id >> 1];
            Object* sparkle = FindFreeObj();
            if (sparkle != NULL) {
                // Set sparkle type and routine
                sparkle->type = ObjId_Ring;
                sparkle->routine = 6;

                // Set sparkle position
                sparkle->pos.l.x.f.u = obj->pos.l.x.f.u + *spawn_pos++;
                sparkle->pos.l.y.f.u = obj->pos.l.y.f.u + *spawn_pos++;

                // Set sparkle drawing information
#ifdef SCP_REV00
                sparkle->mappings = Mappings_RingREV00;
#else
                sparkle->mappings = Mappings_RingREV01;
#endif
                sparkle->tile = TILE_MAP(0, 1, 0, 0, 0x7B2);
                sparkle->render.b = 0;
                sparkle->render.f.align_fg = true;
                sparkle->priority = 2;
                sparkle->width_pixels = 8;
            }
        }
        break;
    case 6: // Player run
        // Lock player controls
        if (debug_use)
            break;
        if (!player->status.p.f.in_air) {
            lock_ctrl = true;
            jpad1_hold2 = JPAD_RIGHT;
            jpad1_press2 = 0;
        }

        // Check if level end sequence should play
        if (player->type != ObjId_Null && (uint16_t)player->pos.l.x.f.u < (limit_right2 + 296 + SCREEN_WIDEADD))
            break;

        // Start level end sequence
        obj->routine += 2;
        if (objects[23].type != ObjId_Null)
            break;

        // Reset game state
        limit_left2 = limit_right2;
        invincibility = false;
        time_count = false;

        // Load "Got through" card
        objects[23].type = ObjId_GotThroughCard;
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

        //music bgm_GotThrough,0,1,0 ; play "Sonic got through" music TODO
        break;
    case 8: // Level end
        break;
    }

    // Animate, draw, and delete signpost(??) when off-screen
    AnimateSprite(obj, Animation_Signpost);
    DisplaySprite(obj);
    if (IS_OFFSCREEN(obj->pos.l.x.f.u))
        ObjectDelete(obj);
}
