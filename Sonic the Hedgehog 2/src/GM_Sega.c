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
// palette (four lines: the first white, the second the orange gradient the letters are drawn in). Stage 1 of the screen: the logo stands where it ends, with the hold and the voice of the original (the
// pre-beta's Sega_Screen); Sonic running across it, the letters pushed aside line by line and the palette sweeps come after (the pre-beta's objects $B0 and $B1).
#include "Resource/S2Sega/Art.h"
#include "Resource/S2Sega/Blocks.h"
#include "Resource/S2Sega/Map.h"
#include "Resource/S2Sega/Palette.h"

#define SEGA_ANIMATION_FRAMES 80 // (what the pre-beta's two runs and their palette sweeps take, until its objects have the voice played)
#define SEGA_HOLD_FRAMES 180     // ($B4 in the pre-beta: the logo stays this long, or until Start)

// SEGA gamemode
void GM_Sega(void) {
    // Stop music
    StopAllSound();

    // Clear the pattern load queue and fade out
    ClearPLC();
    PaletteFadeOut();

    // Set VDP state
    Viewport_UseOwnPlanes(&screen1p);
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
        CopyTilemap(map, &screen1p.plane_b, PLANE_WIDEADD + PLANE_TALLADD, 40, 28);
    }

    // The palette, at once (the screen is faded out: it shows with the display)
    for (int i = 0; i < 4 * 16; i++)
        dry_palette[i / 16][i % 16] = LESWAP_16(((const uint16_t *)S2Sega_Palette)[i]);

#ifndef SCP_JP
    Sst_SpawnCode(Obj_SegaTrademarkHider, 0, 0); // (the Japanese console keeps the trademark)
#endif

    // The letters start white on white (the first line of the palette is all white) and the two sweeps of the pre-beta's object $B0 write the orange gradient over it; where they end is what stays
    // (the last rows of its two lists, at $FB00 and $FB10)
    static const uint16_t sweep_end[16] = {
        0x0EEE, 0x0EEE, 0x0EEC, 0x0EC8, 0x0EA6, 0x0E84, 0x0E62, 0x0E60, // (the second sweep: colours 0 to 7)
        0x0EEE, 0x0EEC, 0x0EC8, 0x0EA6, 0x0E84, 0x0E62, 0x0E60, 0x0EEE, // (the first: 8 to 15)
    };
    for (int i = 0; i < 16; i++)
        dry_palette[0][i] = sweep_end[i];

    // The animation (stage 2: Sonic's run and the letters' lines), then the voice
    for (int frame = 0; frame < SEGA_ANIMATION_FRAMES; frame++) {
        vbla_routine = 0x02;
        WaitForVBla();
        if ((gamemode & 0x7F) != GameMode_Sega)
            return; // the mode was changed from outside (e.g. an in-app demo recording request)
        ExecuteObjects();
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
        if ((gamemode & 0x7F) != GameMode_Sega)
            return; // as above
        ExecuteObjects();
        BuildSprites(NULL);
        if (!demo_length)
            break;
    } while (!(jpad1_press1 & JPAD_START));

leave:
// Start title gamemode
#ifdef SCP_SPLASH
    gamemode = GameMode_SSRG;
#else
    gamemode = GameMode_Title;
#endif
}
