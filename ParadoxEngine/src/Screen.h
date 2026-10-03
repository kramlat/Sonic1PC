#pragma once

#include <stdint.h>

// Screens. A game is a set of screens -- the SEGA logo, the title, a level, a special stage, the credits -- and the engine runs whichever one `gamemode`
// names, over and over, until that screen changes `gamemode` (a screen function runs its own per-frame loop and returns when it hands over).
//
// The game supplies the table (game_screens); the engine never knows what is in it. A screen the game does not have (a NULL entry, or a mode past the
// end) is the NULL SCREEN: the engine resets the video and goes back to the boot screen, so a game can leave out any screen it does not need -- or
// replace one with Screen_Null while it is being written -- without breaking the loop. Only the boot screen has to exist.

typedef void (*ScreenFunc)(void);

// The running screen's index in game_screens. The top bit (SCREEN_FLAG) is the game's own: Sonic 1 uses it for "the title card is showing".
extern uint8_t gamemode;
#define SCREEN_FLAG 0x80
#define SCREEN_MASK 0x7F

// Provided by the game (see GameInterface.h)
extern const ScreenFunc game_screens[];
extern const int game_screen_count;
extern const uint8_t game_boot_screen; // the screen the engine falls back to (and the one a game starts on)

// The null screen: does nothing (the engine then falls back to the boot screen). Usable as a table entry, though a NULL entry means the same.
void Screen_Null(void);

// Runs the screens for ever: once per pass it applies a picture size chosen from the menu, then runs gamemode's screen. Does not return.
void Screens_Run(void);
