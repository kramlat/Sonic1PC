#include "Game.h"

#include "GM_Continue.h"
#include "GM_Ending.h"
#include "GM_Level.h"
#include "GM_Sega.h"
#include "GM_Special.h"
#include "GM_Title.h"

// Sonic 2's screens, by game mode (the engine's Screen.h). They start as Sonic 1's -- the SEGA logo, the title, the level, the special stage, the continue screen, the ending
// and the credits -- and each is replaced by Sonic 2's own as it is written (the title first: Sonic 2's emblem over an Emerald Hill background).
const ScreenFunc game_screens[] = {
    [GameMode_Sega] = GM_Sega,
    [GameMode_Title] = GM_Title,
    [GameMode_Demo] = GM_Level,
    [GameMode_Level] = GM_Level,
    [GameMode_Special] = GM_Special,
    [GameMode_Continue] = GM_Continue,
    [GameMode_Ending] = GM_Ending,
    [GameMode_Credits] = GM_Credits,
};
const int game_screen_count = (int)(sizeof(game_screens) / sizeof(game_screens[0]));
const uint8_t game_boot_screen = GameMode_Sega;
