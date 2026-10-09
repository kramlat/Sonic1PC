#include "Game.h"

#include "GM_Continue.h"
#ifdef SCP_COUNTDOWN
#include "GM_Countdown.h"
#endif
#include "GM_Ending.h"
#include "GM_Level.h"
#include "GM_Sega.h"
#include "GM_Special.h"
#include "GM_Title.h"

#include "Backend/VDP.h"
#include "Camera.h"
#include "HTZQuake.h"
#include "Sprites.h"

// Sonic 2's screens, by game mode (the engine's Screen.h). They start as Sonic 1's -- the SEGA logo, the title, the level, the special stage, the continue screen, the ending
// and the credits -- and each is replaced by Sonic 2's own as it is written (the title first: Sonic 2's emblem over an Emerald Hill background).
// A level is where the shadow/highlight mode may be on (a level started with C held, see Game_LevelObjects): it ends with the level, and the split screen too
static void Screen_Level(void) {
    GM_Level();
    HTZQuake_Reset(); // (the sprites follow the camera again)
    VDP_SetShadowHighlight(false);
    VDP_SetSplitScreen(VDP_SPLIT_NONE, NULL);
    VDP_SetSplitWater(NULL, NULL, 0, 0);
    camera_split = false;
    sprite_split_screen = SPRITE_SPLIT_NONE;
}

const ScreenFunc game_screens[] = {
    [GameMode_Sega] = GM_Sega,
    [GameMode_Title] = GM_Title,
    [GameMode_Demo] = Screen_Level,
    [GameMode_Level] = Screen_Level,
    [GameMode_Special] = GM_Special,
    [GameMode_Continue] = GM_Continue,
    [GameMode_Ending] = GM_Ending,
    [GameMode_Credits] = GM_Credits,
#ifdef SCP_COUNTDOWN
    [GameMode_Countdown] = GM_Countdown,
#endif
};
const int game_screen_count = (int)(sizeof(game_screens) / sizeof(game_screens[0]));
const uint8_t game_boot_screen = GameMode_Sega;
