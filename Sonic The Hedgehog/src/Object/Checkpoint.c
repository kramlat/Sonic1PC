#include "Checkpoint.h"
#include "Resource/Mappings/Checkpoint.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"
#include "Constants.h"
#include "Game.h"
#include "DebugLog.h"
#include "Sound.h"
#include <stdio.h>
#include <string.h>

/* ---------------------------------------------------------------------------
; Object 79 - lamppost (Checkpoint)
; --------------------------------------------------------------------------- */

void Obj_Checkpoint(Sst *self) {
    Own_Checkpoint *own = (Own_Checkpoint *)self->own.u8;

    switch (self->routine) {
        case 0: { /* Lamp_Main / Constructor */
            self->routine += 2;
            self->mappings = Mappings_Checkpoint;
            self->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_Lamppost); // Art_Lamppost lives at VRAM 0xD800 (tile 0x6C0) -- moved from 0xF400/tile 0x7A0 to make room for the Spin Dash dust (see PLC.c's PLC_Main), but this reference was never updated to match
            self->render_flags = 4;
            self->width_pixels = 8;
            self->priority = 5 * 0x80;
            (*SstLoad_State(self)) &= 0x7F;
            bool already_hit = ((*SstLoad_State(self)) & 1);
            bool newer_checkpoint = ((last_lamp & 0x7F) >= (self->subtype & 0x7F));

            if (already_hit || newer_checkpoint) {
                (*SstLoad_State(self)) |= 1;
                self->routine = 4; // Lamp_Finish
                self->mapping_frame = 3;   // Red lamppost frame
            }
            break;
        }

        case 2: { /* Lamp_Blue / Active Idle */
            if (debug_use != 0 || (lock_multi & 0x80)) break; // f_playerctrl bit 7: object interaction disabled
            /* If we've already passed a newer checkpoint, turn red and stop checking */
            if ((last_lamp & 0x7F) >= (self->subtype & 0x7F)) {
                (*SstLoad_State(self)) |= 1;
                self->routine = 4;
                self->mapping_frame = 3;
                break;
            }

            /* .chkhit: Collision check with player */
            int16_t dx = player->x_pos.f.u - self->x_pos.f.u + 8;
            int16_t dy = player->y_pos.f.u - self->y_pos.f.u + 0x40;

            if ((uint16_t)dx < 0x10 && (uint16_t)dy < 0x68) {
                PlaySound(sfx_Lamppost);
                self->routine += 2;

                /* Spawn the twirling ball object */
                Sst *ball = Sst_Allocate();
                if (ball) {
                    Own_Checkpoint *ball_scratch = (Own_Checkpoint *)ball->own.u8;
                    ball->code = Obj_Checkpoint;
                    ball->routine = 6; /* Lamp_Twirl */

                    /* Store original center position for rotation */
                    ball_scratch->pos.x.v = self->x_pos.v;
                    ball_scratch->pos.y.v = self->y_pos.v - (0x18 << 16);

                    ball->mappings = Mappings_Checkpoint;
                    ball->art_tile = TILE_MAP(0, 0, 0, 0, ArtTile_Lamppost); // Art_Lamppost lives at VRAM 0xD800 (tile 0x6C0) -- moved from 0xF400/tile 0x7A0 to make room for the Spin Dash dust (see PLC.c's PLC_Main), but this reference was never updated to match
                    ball->render_flags = 4;
                    ball->width_pixels = 8;
                    ball->priority = 4 * 0x80;
                    ball->mapping_frame = 2; /* "ball only" frame */
                    ball_scratch->time = 0x20;
                }

                self->mapping_frame = 1; /* "post only" frame */
                Obj_Checkpoint_StoreInfo(self, own);
                (*SstLoad_State(self)) |= 1;
            }
            break;
        }

        case 4: /* Lamp_Finish */
           break;

        case 6: { /* Lamp_Twirl */
            if ((int16_t)--own->time < 0)
                self->routine = 4; // like the original, still twirls once more this frame

            uint8_t angle = self->angle;
            self->angle -= 0x10;
            angle -= 0x40;

            int16_t sine, cosine;
            CalcSine(angle, &sine, &cosine);

            /* Rotation logic: Cosine affects X, Sine affects Y */
            self->x_pos.f.u = own->pos.x.f.u + ((cosine * 0xC00) >> 16);
            self->y_pos.f.u = own->pos.y.f.u + ((sine * 0xC00) >> 16);
            break;
        }
    }

    SstOnScreenTest(self);
}

// ===========================================================================
// Subroutine to store information when you hit a lamppost
// ===========================================================================

void Obj_Checkpoint_StoreInfo(Sst *self, const Own_Checkpoint *own)
{
    // Store the ID of the current lamppost
    // Assembly: move.b obSubtype(a0),(v_lastlamp).w
    last_lamp = self->subtype;
    prev_lamp = last_lamp;
    DEBUG_LOG("level", "checkpoint %u stored at (%d,%d)", last_lamp & 0x7F, self->x_pos.f.u, self->y_pos.f.u);

    // Store Player position for respawn
    // The position words are 16.16 fixed point: the PIXEL position is the high word
    // (f.u). Truncating .v to 16 bits kept the fractional low word instead, so
    // the saved respawn position and camera were all 0.
    lamp_state.spawn.x = (uint16_t)self->x_pos.f.u;
    lamp_state.spawn.y = (uint16_t)self->y_pos.f.u;

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
