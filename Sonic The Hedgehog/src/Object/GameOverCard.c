#include "GameOverCard.h"
#include "Resource/Mappings/GameOver.h"
#include "SstLoad.h"
#include "ObjectSst.h"
#include "Object.h"
#include "Constants.h"
#include "Game.h"
#include "Level.h"
#include "PLC.h"

#include <string.h>

// Game Over card constants
#define TO_ADD SCREEN_WIDEADD2
#define FROM_ADD (TO_ADD + ((SCREEN_WIDEADD2 + 0xF) & ~0xF))
#define FROM_SUB ((0x10 - TO_ADD) & 0xF)

// Game Over card object (own.u16[0]: the frames it waits for the restart)
void Obj_GameOverCard(Sst *self) {
    switch (self->routine) {
    case 0: // Initialization
        // Wait for art to be finished loading
        if (plc_buffer[0].art != NULL)
            break;

        // Increment routine and set position
        self->routine += 2;

        self->x_pos.f.u = 0x50 - FROM_SUB;
        if (self->mapping_frame & 1)
            self->x_pos.f.u = 0x1F0 + FROM_ADD;
        self->y_pos.f.u = 0xF0 + SCREEN_TALLADD2;

        // Set object drawing information
        self->mappings = Mappings_GameOver;
        self->art_tile = TILE_MAP(1, 0, 0, 0, ArtTile_Game_Over);
        self->render_flags = 0;
        self->priority = 0 * 0x80;
        // Fallthrough
    case 2: // Moving
        if (self->x_pos.f.u == (0x120 + TO_ADD)) {
            // Start waiting for restart
            self->own.u16[0] = 720;
            self->routine += 2;
            // Doesn't call DisplaySprite or fallthrough
        } else {
            // Move
            if (self->x_pos.f.u > (0x120 + TO_ADD))
                self->x_pos.f.u -= 16;
            else if (self->x_pos.f.u < (0x120 + TO_ADD))
                self->x_pos.f.u += 16;

            // Draw
            Sst_Draw(self);
        }
        break;
    case 4:
        if (!(jpad1_hold1 & (JPAD_A | JPAD_C | JPAD_B))) {
            // Don't change handle timer if 'OVER'
            if (self->mapping_frame & 1) {
                Sst_Draw(self);
                break;
            }

            // Decrement timer
            if (self->own.u16[0]) {
                self->own.u16[0]--;
                Sst_Draw(self);
                break;
            }
        }

        // Change gamemode
        if (!time_over) {
            // Go to continue screen, or SEGA screen if we have no continues
            gamemode = continues ? GameMode_Continue : GameMode_Sega;
        } else {
// Restart level
            // REV01 clears the stored lamppost time, so a checkpoint touched late in the timer cannot soft-lock the restart
            memset(&lamp_state.time, 0, sizeof(lamp_state.time));
            restart = true;
        }

        Sst_Draw(self);
        break;
    }
}
