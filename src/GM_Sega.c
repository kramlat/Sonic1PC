#include "GM_Sega.h"

#include "Game.h"
#include "Level.h"
#include "Nemesis.h"
#include "PLC.h"
#include "Palette.h"
#include "PaletteCycle.h"
#include "Video.h"

#include "Backend/VDP.h"

// SEGA art
#ifdef SCP_REV00
#include "Resource/Art/SegaREV00.h"
#include "Resource/Tilemap/SegaREV00.h"
#else
#include "Resource/Art/SegaREV01.h"
#include "Resource/Tilemap/SegaREV01.h"
#endif

// SEGA gamemode
void GM_Sega()
{
    // Stop music
    // sfx	bgm_Stop,0,1,1 //TODO

    // Clear the pattern load queue and fade out
    ClearPLC();
    PaletteFadeOut();

    // Set VDP state
    VDP_SetPlaneALocation(VRAM_FG);
    VDP_SetPlaneBLocation(VRAM_BG);
    VDP_SetBackgroundColour(0);

    wtr_state = 0;

    // Clear screen and load SEGA graphics
    ClearScreen();

    VDP_SeekVRAM(0x0000);
#ifdef SCP_REV00
    NemDec(Art_SegaREV00);
    CopyTilemap(&Tilemap_SegaREV00[0x0000], MAP_PLANE(VRAM_BG, 8, 10) + PLANE_WIDEADD + PLANE_TALLADD, 24, 8);
    CopyTilemap(&Tilemap_SegaREV00[0x0180], MAP_PLANE(VRAM_FG, 0, 0) + PLANE_WIDEADD + PLANE_TALLADD, 40, 28);
#else
    NemDec(Art_SegaREV01);
    CopyTilemap(&Tilemap_SegaREV01[0x0000], MAP_PLANE(VRAM_BG, 8, 10) + PLANE_WIDEADD + PLANE_TALLADD, 24, 8);
    CopyTilemap(&Tilemap_SegaREV01[0x0180], MAP_PLANE(VRAM_FG, 0, 0) + PLANE_WIDEADD + PLANE_TALLADD, 40, 28);
#ifdef SCP_JP
    CopyTilemap(&Tilemap_SegaREV01[0x0A40], MAP_PLANE(VRAM_FG, 29, 10) + PLANE_WIDEADD + PLANE_TALLADD, 3, 2); // Hide trademark symbol
#endif
#endif

    // Load palette and initialize cycle
    PalLoad2(PalId_SegaBG);
    pcyc_num = -10;
    pcyc_time = 0x0000;
    pcyc_buffer[6] = 0;
    pcyc_buffer[5] = 0;

    // Run palette cycle
    do {
        vbla_routine = 0x02;
        WaitForVBla();
    } while (PCycle_Sega());

    // Play "SEGA" sound

    vbla_routine = 0x14;
    WaitForVBla();

    // Wait a bit before resuming
    demo_length = 30;
    do {
        vbla_routine = 0x02;
        WaitForVBla();
        if (!demo_length)
            break;
    } while (!(jpad1_press1 & JPAD_START));

// Start title gamemode
#ifdef SCP_SPLASH
    gamemode = GameMode_SSRG;
#else
    gamemode = GameMode_Title;
#endif
}
