#include "Checkpoint.h"
#include "Constants.h"
#include "Game.h"
#include "DebugLog.h"
#include "Sound.h"
#include <stdio.h>
#include <string.h>

/* ---------------------------------------------------------------------------
; Object 79 - lamppost (Checkpoint)
; --------------------------------------------------------------------------- */

void Obj_Checkpoint(Object *obj) {
    Scratch_Checkpoint *scratch = (Scratch_Checkpoint*)&obj->scratch;

    switch (obj->routine) {
        case 0: { /* Lamp_Main / Constructor */
            obj->routine += 2;
            obj->mappings = Mappings_Checkpoint;
            obj->tile = TILE_MAP(0, 0, 0, 0, ArtTile_Lamppost); // Art_Lamppost lives at VRAM 0xD800 (tile 0x6C0) -- moved from 0xF400/tile 0x7A0 to make room for the Spin Dash dust (see PLC.c's PLC_Main), but this reference was never updated to match
            obj->render.b = 4;
            obj->width_pixels = 8;
            obj->priority = 5;
            objstate[obj->respawn_index] &= 0x7F;
            bool already_hit = (objstate[obj->respawn_index] & 1);
            bool newer_checkpoint = ((last_lamp & 0x7F) >= (scratch->subtype & 0x7F));

            if (already_hit || newer_checkpoint) {
                objstate[obj->respawn_index] |= 1;
                obj->routine = 4; // Lamp_Finish
                obj->frame = 3;   // Red lamppost frame
            }
            break;
        }

        case 2: { /* Lamp_Blue / Active Idle */
            if (debug_use != 0 || (lock_multi & 0x80)) break; // f_playerctrl bit 7: object interaction disabled
            /* If we've already passed a newer checkpoint, turn red and stop checking */
            if ((last_lamp & 0x7F) >= (scratch->subtype & 0x7F)) {
                objstate[obj->respawn_index] |= 1;
                obj->routine = 4;
                obj->frame = 3;
                break;
            }

            /* .chkhit: Collision check with player */
            int16_t dx = player->pos.l.x.f.u - obj->pos.l.x.f.u + 8;
            int16_t dy = player->pos.l.y.f.u - obj->pos.l.y.f.u + 0x40;

            if ((uint16_t)dx < 0x10 && (uint16_t)dy < 0x68) {
                PlaySound(sfx_Lamppost);
                obj->routine += 2;

                /* Spawn the twirling ball object */
                Object* ball = FindFreeObj();
                if (ball) {
                    // FindFreeObj() slots can hold stale bytes (see BossSpringYard.c): a
                    // leftover angle changes where the twirl starts, and a leftover respawn
                    // index makes RememberState corrupt another object's respawn flags.
                    memset(ball, 0, sizeof(Object));
                    ball->mappings = NULL;
                    Scratch_Checkpoint* ball_scratch = (Scratch_Checkpoint*)&ball->scratch;
                    ball->type = ObjId_Checkpoint;
                    ball->routine = 6; /* Lamp_Twirl */

                    /* Store original center position for rotation */
                    ball_scratch->pos.x.v = obj->pos.l.x.v;
                    ball_scratch->pos.y.v = obj->pos.l.y.v - (0x18 << 16);

                    ball->mappings = Mappings_Checkpoint;
                    ball->tile = TILE_MAP(0, 0, 0, 0, ArtTile_Lamppost); // Art_Lamppost lives at VRAM 0xD800 (tile 0x6C0) -- moved from 0xF400/tile 0x7A0 to make room for the Spin Dash dust (see PLC.c's PLC_Main), but this reference was never updated to match
                    ball->render.b = 4;
                    ball->width_pixels = 8;
                    ball->priority = 4;
                    ball->frame = 2; /* "ball only" frame */
                    ball_scratch->time = 0x20;
                }

                obj->frame = 1; /* "post only" frame */
                Obj_Checkpoint_StoreInfo(obj, scratch);
                objstate[obj->respawn_index] |= 1;
            }
            break;
        }

        case 4: /* Lamp_Finish */
           break;

        case 6: { /* Lamp_Twirl */
            if ((int16_t)--scratch->time < 0)
                obj->routine = 4; // like the original, still twirls once more this frame

            uint8_t angle = obj->angle;
            obj->angle -= 0x10;
            angle -= 0x40;

            int16_t sine, cosine;
            CalcSine(angle, &sine, &cosine);

            /* Rotation logic: Cosine affects X, Sine affects Y */
            obj->pos.l.x.f.u = scratch->pos.x.f.u + ((cosine * 0xC00) >> 16);
            obj->pos.l.y.f.u = scratch->pos.y.f.u + ((sine * 0xC00) >> 16);
            break;
        }
    }

    RememberState(obj);
}

// ===========================================================================
// Subroutine to store information when you hit a lamppost
// ===========================================================================

void Obj_Checkpoint_StoreInfo(Object *obj, const Scratch_Checkpoint *scratch)
{
    // Store the ID of the current lamppost
    // Assembly: move.b obSubtype(a0),(v_lastlamp).w
    last_lamp = scratch->subtype;
    prev_lamp = last_lamp;
    DEBUG_LOG("level", "checkpoint %u stored at (%d,%d)", last_lamp & 0x7F, obj->pos.l.x.f.u, obj->pos.l.y.f.u);

    // Store Player position for respawn
    // The position words are 16.16 fixed point: the PIXEL position is the high word
    // (f.u). Truncating .v to 16 bits kept the fractional low word instead, so
    // the saved respawn position and camera were all 0.
    lamp_state.spawn.x = (uint16_t)obj->pos.l.x.f.u;
    lamp_state.spawn.y = (uint16_t)obj->pos.l.y.f.u;

    // Store Status variables
    lamp_state.rings    = rings;
    lamp_state.lives    = life_count; // the original stores v_lifecount (not the lives number)
    lamp_state.time     = level_time; // Struct copy (LevelTime)
    lamp_state.dle      = dle_routine;
    lamp_state.limitbtm = limit_btm2;

    // Store Camera/Scroll positions
    lamp_state.foreground.x  = (uint16_t)scrpos_x.f.u;
    lamp_state.foreground.y  = (uint16_t)scrpos_y.f.u;

    lamp_state.background.x  = (uint16_t)bg_scrpos_x.f.u;
    lamp_state.background.y  = (uint16_t)bg_scrpos_y.f.u;

    lamp_state.background2.x = (uint16_t)bg2_scrpos_x.f.u;
    lamp_state.background2.y = (uint16_t)bg2_scrpos_y.f.u;

    lamp_state.background3.x = (uint16_t)bg3_scrpos_x.f.u;
    lamp_state.background3.y = (uint16_t)bg3_scrpos_y.f.u;

    // Store Water level data
    lamp_state.water_level.pos     = (uint16_t)wtr_pos2;
    lamp_state.water_level.routine = wtr_routine;
    lamp_state.water_level.state   = wtr_state;
}
