#include <string.h>

#include "GM_Sega.h"
#include "Viewport.h"

#include "Game.h"
#include "Level.h"
#include "Enigma.h"
#include "Nemesis.h"
#include "Sound.h"
#include "PLC.h"
#include "Palette.h"
#include "PaletteCycle.h"
#include "Video.h"

#include "Backend/VDP.h"
#include "Object/SegaScreen.h"

// The SEGA logo of the Sonic 2 pre-beta (September 14, 1992), which is the final game's: its art (the logo's letters at tile 1, the block graphic at tile $80), its map (the whole of plane B) and its
// palette (four lines: the first white, the second the orange gradient the letters are drawn in). The screen as the pre-beta's Sega_Screen has it: Sonic runs across the logo (its object $B0; $B1 hides the trademark), blocks sweep over every second line of the letters
// (plane A, one way and then the other) while the two palette sweeps colour them, then the voice and the hold.
#include "Resource/S2Sega/Art.h"
#include "Resource/S2Sega/Blocks.h"
#include "Resource/S2Sega/Map.h"
#include "Resource/S2Sega/Palette.h"

#define SEGA_HOLD_FRAMES 180     // ($B4 in the pre-beta: the logo stays this long, or until Start)

// The planes the screen keeps are 128 tiles wide, 32 high (the pre-beta's), and the others' 64 wide come back on leaving
static void SegaLeave(void) {
    memset(hscroll_buffer, 0, sizeof(hscroll_buffer));
    Viewport_UploadHScroll(&screen1p, hscroll_buffer, sizeof(hscroll_buffer));
    Plane_Clear(&screen1p.plane_a);
    Plane_Clear(&screen1p.plane_b);
    Viewport_SetSize(SCREEN_WIDTH, SCREEN_HEIGHT, PLANE_WIDTH, PLANE_HEIGHT);
}

// SEGA gamemode
void GM_Sega(void) {
    // Stop music
    StopAllSound();

    // Clear the pattern load queue and fade out
    ClearPLC();
    PaletteFadeOut();

    // Set VDP state
    Viewport_UseOwnPlanes(&screen1p);
    Viewport_SetSize(SCREEN_WIDTH, SCREEN_HEIGHT, SEGA_PLANE_WIDTH, SEGA_PLANE_HEIGHT); // (planes 128 tiles wide, as the pre-beta's: the sweeps' strips are longer than the 64 of the others)
    VDP_SetBackgroundColour(0);

    wtr_state = 0;

    // Clear screen and load the graphics
    ClearScreen();
    Sst_ResetAll(); // (no object of the level before is still about)

    VDP_SeekVRAM(0x0020);
    NemDec(S2Sega_Art);
    VDP_SeekVRAM(0x1000);
    NemDec(S2Sega_Blocks);
    {
        static uint8_t map[40 * 28 * 2];
        EniDec(S2Sega_Map, map, 0);
        size_t offset = PLANE_WIDEADD + SegaPlaneRows() * SEGA_PLANE_WIDTH * 2;
        for (int row = 0; row < 28; row++, offset += SEGA_PLANE_WIDTH * 2)
            for (int col = 0; col < 40; col++)
                Plane_Put(&screen1p.plane_b, offset + col * 2, (uint16_t)((map[(row * 40 + col) * 2] << 8) | map[(row * 40 + col) * 2 + 1]));
    }
    memset(hscroll_buffer, 0, sizeof(hscroll_buffer));
    Viewport_UploadHScroll(&screen1p, hscroll_buffer, sizeof(hscroll_buffer));

    // The palette, at once (the screen is faded out: it shows with the display)
    for (int i = 0; i < 4 * 16; i++)
        dry_palette[i / 16][i % 16] = LESWAP_16(((const uint16_t *)S2Sega_Palette)[i]);

#ifndef SCP_JP
    Sst_SpawnCode(Obj_SegaTrademarkHider, 0, 0); // (the Japanese console keeps the trademark)
#endif
    sega_animation_done = false;
    Sst_SpawnCode(Obj_SegaSonic, 0, 0); // Sonic running across the logo

    // The letters start white on white (the first line of the palette is all white): the two sweeps of the pre-beta's object $B0 write the orange gradient over it

    // The animation (Sonic's run, the blinds of blocks over the letters and the palette sweeps), then the voice
    while (!sega_animation_done) {
        vbla_routine = 0x02;
        WaitForVBla();
        if ((gamemode & 0x7F) != GameMode_Sega) {
            SegaLeave();
            return; // the mode was changed from outside (e.g. an in-app demo recording request)
        }
        ExecuteObjects();
        Viewport_UploadHScroll(&screen1p, hscroll_buffer, sizeof(hscroll_buffer));
        BuildSprites(NULL);
        if (jpad1_press1 & JPAD_START)
            goto leave;
    }

    PlaySegaSound();

    vbla_routine = 0x14;
    WaitForVBla();

    // The logo stays
    demo_length = SEGA_HOLD_FRAMES;
    do {
        vbla_routine = 0x02;
        WaitForVBla();
        if ((gamemode & 0x7F) != GameMode_Sega) {
            SegaLeave();
            return; // as above
        }
        ExecuteObjects();
        BuildSprites(NULL);
        if (!demo_length)
            break;
    } while (!(jpad1_press1 & JPAD_START));

leave:
    SegaLeave();
// Start title gamemode
#ifdef SCP_SPLASH
    gamemode = GameMode_SSRG;
#else
    gamemode = GameMode_Title;
#endif
}
