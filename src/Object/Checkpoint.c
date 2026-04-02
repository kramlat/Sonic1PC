#include "Checkpoint.h"
#include "Game.h"
#include <stdio.h>

/* ---------------------------------------------------------------------------
; Object 79 - lamppost (Checkpoint)
; --------------------------------------------------------------------------- */

void Obj_Checkpoint(Object* obj) {
    Scratch_Checkpoint* scratch = (Scratch_Checkpoint*)&obj->scratch;

    switch (obj->routine) {
        case 0: { /* Lamp_Main / Constructor */
            obj->routine += 2;
            obj->mappings = Mappings_Checkpoint;
            obj->tile = TILE_MAP(0, 0, 0, 0, 0x7A0);
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
            if (debug_use != 0 || lock_ctrl < 0) break;
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
                // QueueSound(sfx_Lamppost);
                obj->routine += 2;

                /* Spawn the twirling ball object */
                Object* ball = FindFreeObj();
                if (ball) {
                    Scratch_Checkpoint* ball_scratch = (Scratch_Checkpoint*)&ball->scratch;
                    ball->type = ObjId_Checkpoint;
                    ball->routine = 6; /* Lamp_Twirl */

                    /* Store original center position for rotation */
                    ball_scratch->pos.x.v = obj->pos.l.x.v;
                    ball_scratch->pos.y.v = obj->pos.l.y.v - (0x18 << 16);

                    ball->mappings = Mappings_Checkpoint;
                    ball->tile = TILE_MAP(0, 0, 0, 0, 0x7A0);
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
            if ((int16_t)--scratch->time < 0) {
                obj->routine = 4;
                break;
            }

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

void Obj_Checkpoint_StoreInfo(Object* obj, Scratch_Checkpoint* scratch)
{
    // Store the ID of the current lamppost
    // Assembly: move.b obSubtype(a0),(v_lastlamp).w
    last_lamp = scratch->subtype;
    prev_lamp = last_lamp;

    // Store Player position for respawn
    // Note: Truncating the 32-bit fixed point position to 16-bit integer pixels
    lamp_state.spawn.x = (uint16_t)obj->pos.l.x.v;
    lamp_state.spawn.y = (uint16_t)obj->pos.l.y.v;

    // Store Status variables
    lamp_state.rings    = rings;
    lamp_state.lives    = lives;
    lamp_state.time     = time; // Struct copy (LevelTime)
    lamp_state.dle      = dle_routine;
    lamp_state.limitbtm = limit_btm2;

    // Store Camera/Scroll positions
    lamp_state.foreground.x  = (uint16_t)scrpos_x.v;
    lamp_state.foreground.y  = (uint16_t)scrpos_y.v;

    lamp_state.background.x  = (uint16_t)bg_scrpos_x.v;
    lamp_state.background.y  = (uint16_t)bg_scrpos_y.v;

    lamp_state.background2.x = (uint16_t)bg2_scrpos_x.f.u;
    lamp_state.background2.y = (uint16_t)bg2_scrpos_y.f.u;

    lamp_state.background3.x = (uint16_t)bg3_scrpos_x.f.u;
    lamp_state.background3.y = (uint16_t)bg3_scrpos_y.f.u;

    // Store Water level data
    lamp_state.water_level.pos     = (uint16_t)wtr_pos2;
    lamp_state.water_level.routine = wtr_routine;
    lamp_state.water_level.state   = wtr_state;
}
