#include "Game.h"

#include "GM_Continue.h"
#include "GM_Ending.h"
#include "GM_Level.h"
#include "GM_Sega.h"
#include "GM_Special.h"
#include "GM_Title.h"
#ifdef SCP_COUNTDOWN
#include "GM_Countdown.h"
#endif
#ifdef SCP_SPLASH
#include "GM_SSRG.h"
#endif

// Sonic 1's screens, by game mode. The engine (Screen.h) runs whichever one `gamemode` names; anything not listed is the null screen and sends it back to the boot screen.
const ScreenFunc game_screens[] = {
    [GameMode_Sega] = GM_Sega,
    [GameMode_Title] = GM_Title,
    [GameMode_Demo] = GM_Level, // a demo is the level screen playing a recording back
    [GameMode_Level] = GM_Level,
    [GameMode_Special] = GM_Special,
    [GameMode_Continue] = GM_Continue,
    [GameMode_Ending] = GM_Ending,
    [GameMode_Credits] = GM_Credits,
#ifdef SCP_SPLASH
    [GameMode_SSRG] = GM_SSRG,
#endif
#ifdef SCP_COUNTDOWN
    [GameMode_Countdown] = GM_Countdown,
#endif
};
const int game_screen_count = (int)(sizeof(game_screens) / sizeof(game_screens[0]));
const uint8_t game_boot_screen = GameMode_Sega;
