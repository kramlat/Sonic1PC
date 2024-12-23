#include "GM_Title.h"

#include "Game.h"
#include "Level.h"
#include "LevelDraw.h"
#include "LevelScroll.h"
#include "Nemesis.h"
#include "PLC.h"
#include "Palette.h"
#include "PaletteCycle.h"
#include "SpecialStage.h"
#include "Video.h"

#include "Backend/VDP.h"

#include <string.h>

// Title screen state
uint8_t demo_num;

// Title screen demo list
static const uint16_t title_demos[] = {
    LEVEL_ID(ZoneId_GHZ, 0),
    LEVEL_ID(ZoneId_MZ, 0),
    LEVEL_ID(ZoneId_SYZ, 0),
    LEVEL_ID(6, 0), // Special stage
};

// Japanese credits
#include "Resource/Art/JapaneseCredits.h"
#include "Resource/Tilemap/JapaneseCredits.h"

// Credits font
#include "Resource/Art/CreditsFont.h"

// Title
#include "Resource/Art/TitleFG.h"
#include "Resource/Art/TitleSonic.h"
#include "Resource/Art/TitleTM.h"
#include "Resource/Tilemap/TitleFG.h"

// Level select
/*
static void LevelSelect()
{
        do
        {
                //Run frame
                vbla_routine = 0x04;
                WaitForVBla();

                //Run level select selection and PLCs

                RunPLC();
        } while (plc_buffer[0].art != NULL || !(jpad1_press1 & (JPAD_A | JPAD_B | JPAD_C | JPAD_START)));
}
*/

// Level stuff
static void PlayLevel()
{
    gamemode = (jpad1_hold1 & JPAD_A) ? GameMode_Special : GameMode_Level;
    lives = 3;
    rings = 0;
    time.pad = time.min = time.sec = time.frame = 0;
    score = 0;
    last_special = 0;
    emeralds = 0;
    memset(emerald_list, 0, sizeof(emerald_list));
    continues = 0;
#ifndef SCP_REV00
    score_life = 5000;
#endif
    // sfx	bgm_Fade,0,1,1 ; fade out music //TODO
}

static void Tit_ChkLevSel()
{
    PlayLevel();

    /*
    //Load level select palette
    PalLoad2(PalId_LevelSel);

    //Clear scroll buffer
    memset(hscroll_buffer, 0, sizeof(hscroll_buffer));
    VDP_SeekVRAM(VRAM_HSCROLL);
    VDP_FillVRAM(0, sizeof(hscroll_buffer));

    //Enter level select loop
    LevelSelect();
    */
}

// Title gamemode
void GM_Title()
{
    // Stop music
    // sfx	bgm_Stop,0,1,1 //TODO

    // Clear the pattern load queue and fade out
    ClearPLC();
    PaletteFadeOut();

    // Set VDP state
    VDP_SetPlaneALocation(VRAM_FG);
    VDP_SetPlaneBLocation(VRAM_BG);
    VDP_SetBackgroundColour(0x20); // Line 2, entry 0

    wtr_state = 0;

    // Clear screen
    ClearScreen();

    // Clear object memory
    memset(objects, 0, sizeof(objects));

    // Load Japanese credits
    VDP_SeekVRAM(0x0000);
    NemDec(Art_JapaneseCredits);
    VDP_SeekVRAM(0x14C0);
    NemDec(Art_CreditsFont);

    CopyTilemap(Tilemap_JapaneseCredits, VRAM_FG + PLANE_WIDEADD + PLANE_TALLADD, 40, 24);

    // Clear palette
    memset(dry_palette_dup, 0, sizeof(dry_palette_dup));
    PalLoad1(PalId_Sonic);

    // Load "SONIC TEAM PRESENTS" object
    objects[2].type = ObjId_Credits;

    ExecuteObjects();
    BuildSprites(NULL);

    // Fade in
    PaletteFadeIn();

    // Load title art
    VDP_SeekVRAM(0x4000);
    NemDec(Art_TitleFG);
    VDP_SeekVRAM(0x6000);
    NemDec(Art_TitleSonic);
    VDP_SeekVRAM(0xA200);
    NemDec(Art_TitleTM);

    // Reset game state
    last_lamp = 0;
    debug_use = false;
    demo = 0;

    // Load GHZ
    level_id = LEVEL_ID(ZoneId_GHZ, 0);
    pcyc_time = 0;

    LevelSizeLoad();
    DeformLayers();
    LoadLevelMaps();
    LoadLevelLayout();
    player->pos.l.x.f.u += SCREEN_WIDEADD2; // For widescreen so the title starts scrolling at the correct time

    // Fade out
    PaletteFadeOut();

    // Draw background
    ClearScreen();
    DrawChunks(bg_scrpos_x.f.u, bg_scrpos_y.f.u, level_layout[0][1], VRAM_BG);

    // Load title mappings
    CopyTilemap(&Tilemap_TitleFG[0x0000], MAP_PLANE(VRAM_FG, 3, 4) + PLANE_WIDEADD + PLANE_TALLADD, 34, 22);

    // Load GHZ art and title palette
    VDP_SeekVRAM(0x0000);
    NemDec(Art_GHZ1);
    PalLoad1(PalId_Title);

    // Run title screen for 376 frames
    demo_length = 376;

// Clear objects
#ifdef SCP_FIX_BUGS
    memset(&objects[2], 0, sizeof(Object));
#else
    memset(&objects[2], 0, 0x20); // 0x20 instead of the object structure size?
                                  // This why the "PRESS START BUTTON" text is missing.
#endif

    // Load title objects
    objects[1].type = ObjId_TitleSonic;
    objects[2].type = ObjId_PSB;
#ifndef SCP_JP
    objects[3].type = ObjId_PSB;
    objects[3].frame = 3;
#endif
    objects[4].type = ObjId_PSB;
    objects[4].frame = 2;

    ExecuteObjects();
    DeformLayers();
    BuildSprites(NULL);

    NewPLC(PlcId_Main);

    // Fade in
    PaletteFadeIn();

    // Loop
    do {
        // Render frame
        vbla_routine = 0x04;
        WaitForVBla();

        // Run game and load PLCs
        ExecuteObjects();
        DeformLayers();
        BuildSprites(NULL);
        RunPLC();

        // Run palette cycle
        PCycle_Title();

        // Move Sonic object (yep, this is how they scroll the camera)
        //...and return to the Sega screen after a minute?
        if ((player->pos.l.x.f.u += 2) >= 0x1C00) {
            gamemode = GameMode_Sega;
            return;
        }

        // TODO: Check for level select cheat

        // Check if the title's over
        if (!demo_length) {
            // Run the title screen but with reduced code for 30 frames
            demo_length = 30;

            do {
                // Render frame
                vbla_routine = 0x04;
                WaitForVBla();

                // Run game and load PLCs
                DeformLayers();
                PaletteCycle(); // Wrong palette cycle
                RunPLC();

                // Move Sonic object
                if ((player->pos.l.x.f.u += 2) >= 0x1C00) {
                    gamemode = GameMode_Sega;
                    return;
                }

                // Check if start is pressed
                if (jpad1_press1 &= JPAD_START) {
                    Tit_ChkLevSel();
                    return;
                }
            } while (demo_length);

            // Load demo
            // sfx	bgm_Fade,0,1,1 ; fade out music //TODO

            level_id = title_demos[demo_num & 7];
            if (++demo_num >= 4)
                demo_num = 0;

            // Enter demo gamemode
            demo = 1;
            if (level_id != 0x600) {
                // Regular level
                gamemode = GameMode_Demo;
            } else {
                // Special stage
                gamemode = GameMode_Special;
                level_id = 0;
                last_special = 0;
            }

            // Set game state
            lives = 3;
            rings = 0;
            time.pad = time.min = time.sec = time.frame = 0;
            score = 0;
#ifndef SCP_REV00
            score_life = 5000;
#endif
            return;
        }
    } while (!(jpad1_press1 &= JPAD_START));

    Tit_ChkLevSel();
}
