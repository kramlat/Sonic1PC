#include "Screen.h"

#include "DebugLog.h"
#include "Video.h"

#include <SDL.h>

uint8_t gamemode;

void Screen_Null(void) {
}

void Screens_Run(void) {
    while (1) {
        SDL_Delay(1000 / 60);
        {
            static uint8_t logged_mode = 0xFF;
            if ((gamemode & SCREEN_MASK) != logged_mode) {
                DEBUG_LOG("game", "game mode %u -> %u", logged_mode, (unsigned)(gamemode & SCREEN_MASK));
                logged_mode = (uint8_t)(gamemode & SCREEN_MASK);
            }
        }
        // A picture size chosen from the menu takes hold as each game mode starts (a level also checks as it runs)
        Video_ApplyPendingResolution();

        unsigned mode = gamemode & SCREEN_MASK;
        ScreenFunc screen = (mode < (unsigned)game_screen_count) ? game_screens[mode] : NULL;
        if (screen == NULL || screen == Screen_Null) {
            // The null screen: nothing here, so start over from the boot screen
            VDPSetupGame();
            gamemode = game_boot_screen;
        } else {
            screen();
        }
    }
}
