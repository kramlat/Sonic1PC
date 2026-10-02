#include "GM_Continue.h"

#include <string.h>

#include "Game.h"
#include "Level.h"
#include "LevelScroll.h"
#include "Nemesis.h"
#include "Object/ContinueScreen.h"
#include "Object/Sonic.h"
#include "PLC.h"
#include "Palette.h"
#include "Sound.h"
#include "Video.h"
#include "Backend/VDP.h"

#include "Resource/Art/ContSonic.h"
// (Defined by their resource headers elsewhere: HUD.c and PLC.c)
extern const uint8_t Art_HUDNum[], Art_MiniSonic[], Art_TitleCard[];

// The countdown starts at 10 and runs for eleven seconds in all, so that the last number is shown for its full second
#define CONTINUE_TIME ((11 * 60) - 1)

// The two digits of the countdown, written over the art of the "CONTINUE" text's number (ContScrCounter)
static void ContinueCounter(int number) {
    VDP_SeekVRAM(ART_VRAM(ArtTile_Continue_Number));
    VDP_WriteVRAM(Art_HUDNum + (number / 10) * 64, 64);
    VDP_WriteVRAM(Art_HUDNum + (number % 10) * 64, 64);
}

void GM_Continue(void) {
    PaletteFadeOut();

    // Screen setup and patterns
    ClearPLC();
    VDP_SetPlaneALocation(VRAM_FG);
    VDP_SetPlaneBLocation(VRAM_BG);
    VDP_SetSpriteLocation(VRAM_SPRITES);
    VDP_SetPlaneSize(PLANE_WIDTH, PLANE_HEIGHT);
    VDP_SetBackgroundColour(0);
    VDPDisableWaterSplit();
    ClearScreen();
    memset(objects, 0, sizeof(objects));

    VDP_SeekVRAM(ART_VRAM(ArtTile_Title_Card)); // the "CONTINUE" letters
    NemDec(Art_TitleCard);
    VDP_SeekVRAM(ART_VRAM(ArtTile_Continue_Sonic));
    NemDec(Art_ContSonic);
    VDP_SeekVRAM(ART_VRAM(ArtTile_Mini_Sonic));
    NemDec(Art_MiniSonic);
    ContinueCounter(10);

    memset(dry_palette_dup, 0, sizeof(dry_palette_dup));
    PalLoad1(PalId_Continue);
    PlayMusic(bgm_Continue);

    demo_length = CONTINUE_TIME;

    scrpos_x.v = 0;
    scrpos_y.v = 0x1000000; // the camera sits at Y $100
    sonframe_num = 0xFF;    // Sonic's graphics are always loaded afresh
    sonframe_chg = false;

    player->type = ObjId_81;
    objects[CONTINUE_SLOT_TEXT].type = ObjId_80;
    objects[CONTINUE_SLOT_LIGHT].type = ObjId_80; // the light spot on the floor Sonic lies on
    objects[CONTINUE_SLOT_LIGHT].priority = 3;
    objects[CONTINUE_SLOT_LIGHT].frame = 4;
    objects[CONTINUE_SLOT_ICON].type = ObjId_80; // the mini Sonics
    objects[CONTINUE_SLOT_ICON].routine = 4;

    ExecuteObjects();
    BuildSprites(NULL);
    PaletteFadeIn();

    for (;;) {
        vbla_routine = 0x16;
        WaitForVBla();

        if (player->routine < 6) // until a continue is used
            ContinueCounter((demo_length / 60) & 0xF);

        ExecuteObjects();
        BuildSprites(NULL);

        // Sonic has run off the screen after using a continue: back to the level
        if (player->pos.l.x.f.u >= 320 + 64 + SCREEN_WIDEADD)
            break;
        if (player->routine >= 6)
            continue; // still running off
        if (demo_length == 0) { // the countdown ran out: this is the end
            gamemode = GameMode_Sega;
            return;
        }
    }

    gamemode = GameMode_Level;
    lives = 3;
    rings = 0;
    level_time.pad = level_time.min = level_time.sec = level_time.frame = 0;
    score = 0;
    last_lamp = 0;
    continues--;
}
