#include "GM_Ending.h"
#include "Constants.h"

#include "Console.h"
#include "Demo.h"
#include "Game.h"
#include "GM_Level.h"
#include "HUD.h"
#include "Kosinski.h"
#include "Level.h"
#include "LevelCollision.h"
#include "LevelDraw.h"
#include "LevelScroll.h"
#include "Nemesis.h"
#include "Object/Ending.h"
#include "Object/Sonic.h"
#include "Oscillatory Routines.h"
#include "PLC.h"
#include "Palette.h"
#include "PaletteCycle.h"
#include "Sound.h"
#include "SpecialStage.h"
#include "Video.h"
#include "Backend/VDP.h"

#include <string.h>

extern const uint8_t Art_CreditText[];

// ---------------------------------------------------------------------------
// The ending sequence: Sonic runs onto the screen in Green Hill, stops, and either holds up the six chaos emeralds
// (they spin up, the screen flashes white and the extra flowers bloom) or doesn't; then he leaps at the screen and the
// logo slides in. Sonic himself is driven by simulated buttons and swapped for the ending-only Sonic object (87).
// ---------------------------------------------------------------------------

enum { SONIC_WAIT2_FRAME = 3 }; // fr_Wait2

static void End_MoveSonic(void) {
    switch (sonicend) {
    case 0:
        if (player->pos.l.x.f.u >= (SCREEN_WIDTH / 2) - 16) // still running in from the right
            return;
        sonicend += 2;
        lock_ctrl = true;
        jpad1_hold2 = JPAD_RIGHT; // hold right to make him skid
        jpad1_press2 = 0;
        break;
    case 2:
        if (player->pos.l.x.f.u < SCREEN_WIDTH / 2) // skidding until the middle
            return;
        sonicend += 2;
        lock_ctrl = false;
        jpad1_hold2 = 0;
        jpad1_press2 = 0;
        player->inertia = 0; // stop dead
        lock_multi = 0x81;   // controls ignored, objects not interacted with
        player->frame = SONIC_WAIT2_FRAME;
        player->anim = SonAnimId_Wait;
        player->prev_anim = SonAnimId_Wait; // so the animation isn't restarted
        player->frame_time.b = 3;
        break;
    case 4:
        sonicend += 2;
        player->pos.l.x.f.u = SCREEN_WIDTH / 2;
        player->type = ObjId_87; // the ending-only Sonic replaces the real one
        player->routine = 0;
        player->routine_sec = 0;
        break;
    }
}

// One frame of the ending sequence's loop (the part both loops share).
static void End_Frame(void) {
    PauseGame();
    ConsoleUpdate();
    vbla_routine = 0x18;
    WaitForVBla();
    frame_count++;
    End_MoveSonic();
    ExecuteObjects();
    DeformLayers();
    BuildSprites(NULL);
    ObjPosLoad();
}

void GM_Ending(void) {
    StopAllSound();
    ClearPLC();
    PaletteFadeOut();

    // Screen setup and patterns
    Level_ClearState();
    ClearScreen();
    VDP_SetPlaneALocation(VRAM_FG);
    VDP_SetPlaneBLocation(VRAM_BG);
    VDP_SetSpriteLocation(VRAM_SPRITES);
    VDP_SetPlaneSize(PLANE_WIDTH, PLANE_HEIGHT);
    VDP_SetBackgroundColour(0x20); // line 2, entry 0
    VDPDisableWaterSplit();
    air = 30;

    // The good ending (every emerald) has the extra flowers: level 0600, otherwise 0601
    level_id = LEVEL_ID(ZoneId_EndZ, emeralds == 6 ? 0 : 1);

    QuickPLC(PlcId_Ending);
    HUD_Base();
    LevelSizeLoad();
    DeformLayers();
    fg_plane.flags |= LEVEL_SCROLL_LEFT;
    LevelDataLoad();
    LoadTilesFromStart();
    ColIndexLoad(); // Green Hill's collision
    Ending_LoadFlowerArt();
    PalLoad1(PalId_Sonic);
    PlayMusic(bgm_Ending);

    // Debug mode only with the cheat (the original lets A alone enable it)
    if (debug_cheat && (jpad1_hold1 & JPAD_A))
        debug_mode = true;

    // Sonic runs in from the right, controls locked on a simulated left press
    player->type = ObjId_Sonic;
    player->status.p.f.x_flip = true;
    lock_ctrl = true;
    jpad1_hold2 = JPAD_LEFT;
    jpad1_press2 = 0;
    player->inertia = -0x800; // capped to -0x600 straight away
    objects[1].type = ObjId_HUD;
    if (ending_eggmobile_exploding) // Eggman's ship was wrecked in the Final Zone: it falls in the distance
        objects[ENDING_SLOT_EGGMOBILE].type = ObjId_8D;
    ObjPosLoad();
    ExecuteObjects();
    BuildSprites(NULL);

    rings = 0;
    level_time.pad = level_time.min = level_time.sec = level_time.frame = 0;
    life_num = 0;
    shield = false;
    invincibility = false;
    shoes = false;
    debug_use = false;
    restart = 0;
    frame_count = 0;
    OscillateNumInit();
    score_count = true;
    ring_count = true;
    time_count = false; // the timer stays stopped
    demo_length = 1800;

    vbla_routine = 0x18;
    WaitForVBla();
    PaletteFadeIn();

    // Main loop
    for (;;) {
        End_Frame();
        PaletteCycle();
        OscillateNumDo();
        SynchroAnimate();

        if ((gamemode & 0x7F) != GameMode_Ending)
            break;
        if (!restart)
            continue;

        // The emeralds are spinning at full radius: a slow white-in while they keep spinning
        restart = 0;
        palette_fade.ind = 0;
        palette_fade.len = 0x40;
        pal_chgspeed = 0;
        bool died = false;
        for (;;) {
            End_Frame();
            OscillateNumDo();
            SynchroAnimate();
            if (--pal_chgspeed < 0) {
                pal_chgspeed = 2;
                WhiteOut_ToWhite();
            }
            if (player->routine >= 6) { // Sonic died somehow: don't soft-lock, go straight to the credits
                died = true;
                break;
            }
            if (restart) // the emeralds have disappeared
                break;
        }
        if (died)
            break;

        // Fully white and the emeralds are gone: add the flowers to the layout and fade back in
        restart = 0;
        LEVEL_LAYOUT_FG(2)[0] = 0xAA; LEVEL_LAYOUT_FG(2)[1] = 0xAB; LEVEL_LAYOUT_FG(2)[2] = 0xAE; LEVEL_LAYOUT_FG(2)[3] = 0x9A;
        LEVEL_LAYOUT_FG(3)[0] = 0xAC; LEVEL_LAYOUT_FG(3)[1] = 0xAD; LEVEL_LAYOUT_FG(3)[2] = 0xAF; LEVEL_LAYOUT_FG(3)[3] = 0xB0;
        DrawChunks(scrpos_x.f.u, scrpos_y.f.u, LEVEL_LAYOUT_FG(0), VRAM_FG);
        PalLoad1(PalId_Ending);
        PaletteWhiteIn();
    }

    // To the credits
    gamemode = GameMode_Credits;
    PlayMusic(bgm_Credits);
    credits_num = 0;
}

// ---------------------------------------------------------------------------
// The credits: a text page for two seconds, then an attract mode demo of the level that comes next (the "ending demos",
// run by GM_Level with demo < 0); after the last page comes the "TRY AGAIN" / "END" screen.
// ---------------------------------------------------------------------------

// The levels of the ending demos, in the order they are shown.
static const uint16_t ending_demo_levels[8] = {
    LEVEL_ID(ZoneId_GHZ, 0), LEVEL_ID(ZoneId_MZ, 1), LEVEL_ID(ZoneId_SYZ, 2), LEVEL_ID(ZoneId_LZ, 2),
    LEVEL_ID(ZoneId_SLZ, 2), LEVEL_ID(ZoneId_SBZ, 0), LEVEL_ID(ZoneId_SBZ, 1), LEVEL_ID(ZoneId_GHZ, 0),
};

// Prepares the next ending demo (the one after the credits page being shown); credits_num moves on to the next page.
static void EndingDemoLoad(void) {
    level_id = ending_demo_levels[credits_num & 7];
    credits_num++;
    if (credits_num >= 9) // past the last page: no more demos
        return;

    demo = (int16_t)0x8001; // the credits variant of demo mode
    gamemode = GameMode_Demo;
    lives = 3;
    rings = 0;
    level_time.pad = level_time.min = level_time.sec = level_time.frame = 0;
    score = 0;
    last_lamp = 0;

    if (credits_num == 4) { // the Labyrinth demo starts from a lamppost, in the water
        last_lamp = 1;
        prev_lamp = 1;
        lamp_state.spawn.x = 0xA00;
        lamp_state.spawn.y = 0x62C;
        lamp_state.rings = 13;
        memset(&lamp_state.time, 0, sizeof(lamp_state.time));
        lamp_state.dle = 0;
        lamp_state.pad0 = 0;
        lamp_state.limitbtm = 0x800;
        lamp_state.foreground.x = 0x957;
        lamp_state.foreground.y = 0x5CC;
        lamp_state.background.x = 0x4AB;
        lamp_state.background.y = 0x3A6;
        lamp_state.background2.x = 0;
        lamp_state.background2.y = 0x28C;
        lamp_state.background3.x = 0;
        lamp_state.background3.y = 0;
        lamp_state.water_level.pos = 0x308;
        lamp_state.water_level.routine = 1;
        lamp_state.water_level.state = 1;
    }
}

// The fixed screen set-up of the credits and "TRY AGAIN" screens (8 colours, no water, black backdrop).
static void CreditsScreenSetup(void) {
    ClearPLC();
    PaletteFadeOut();
    VDP_SetPlaneALocation(VRAM_FG);
    VDP_SetPlaneBLocation(VRAM_BG);
    VDP_SetPlaneSize(PLANE_WIDTH, PLANE_HEIGHT);
    VDP_SetBackgroundColour(0x20);
    VDPDisableWaterSplit();
    ClearScreen();
    memset(objects, 0, sizeof(objects));
}

// The "TRY AGAIN" (not every emerald) / "END" (all of them) screen: Eggman, juggling the emeralds Sonic missed or
// throwing a tantrum. Start, or 30 seconds, goes back to the Sega screen.
static void TryAgainEnd(void) {
    CreditsScreenSetup();
    QuickPLC(PlcId_TryAgain);

    memset(dry_palette_dup, 0, sizeof(dry_palette_dup));
    PalLoad1(PalId_Ending);
    dry_palette_dup[2][0] = 0; // keep the backdrop black

    objects[CREDITS_SLOT_TEXT].type = ObjId_8B;
    ExecuteObjects();
    BuildSprites(NULL);

    demo_length = 1800;
    PaletteFadeIn();

    for (;;) {
        PauseGame();
        vbla_routine = 0x04;
        WaitForVBla();
        ExecuteObjects();
        BuildSprites(NULL);

        if (jpad1_press1 & JPAD_START)
            break;
        if (demo_length == 0)
            break;
        if ((gamemode & 0x7F) != GameMode_Credits)
            break;
    }
    gamemode = GameMode_Sega;
}

void GM_Credits(void) {
    CreditsScreenSetup();

    VDP_SeekVRAM(ART_VRAM(ArtTile_Credits_Font));
    NemDec(Art_CreditText);

    memset(dry_palette_dup, 0, sizeof(dry_palette_dup));
    PalLoad1(PalId_Sonic);

    objects[CREDITS_SLOT_TEXT].type = ObjId_Credits;
    ExecuteObjects();
    BuildSprites(NULL);

    EndingDemoLoad();

    // The next demo's level art, loaded while the page is up
    if (level_header[LEVEL_ZONE(level_id)].plc1 != 0)
        AddPLC(level_header[LEVEL_ZONE(level_id)].plc1);
    AddPLC(PlcId_Main2);

    demo_length = 120; // a page shows for two seconds
    PaletteFadeIn();

    // Until both the two seconds are up and the level art has finished loading
    do {
        vbla_routine = 0x04;
        WaitForVBla();
        RunPLC();
    } while (demo_length != 0 || plc_buffer[0].art != NULL);

    if (credits_num == 9) // past the last page
        TryAgainEnd();
}
