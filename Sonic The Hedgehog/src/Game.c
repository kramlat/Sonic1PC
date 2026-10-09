#include "Game.h"
#include "DebugLog.h"

#include "HUD.h"
#include "Backend/VDP.h"
#include "Backend/YM2612.h"
#include "Sound.h"
#include <SDL.h>
#include "Demo.h"
#include "Level.h"
#include "LevelDraw.h"
#include "LevelScroll.h"
#include "Object/Sonic.h"
#include "Object/Splash.h"
#include "PLC.h"
#include "Palette.h"
#include "PaletteCycle.h"
#include "SpecialStage.h"
#include "Video.h"

#include <string.h>

#include "GM_Level.h"
#include "GM_Sega.h"
#include "GM_Special.h"
#include "GM_Ending.h"
#include "GM_Continue.h"
#include "GM_Title.h"
#ifdef SCP_COUNTDOWN
#include "GM_Countdown.h"
#endif
#ifdef SCP_SPLASH
#include "GM_SSRG.h"
#endif

// Game


int16_t demo;
uint16_t demo_length;
int32_t cli_start_ending = -1; // --ending N: start the ending sequence with N emeralds
int32_t cli_start_credits = -1; // --credits N: start the credits at page N
int32_t cli_ending_ship = 0;     // --ship: with --ending, Eggman's ship was wrecked in the Final Zone
int32_t cli_emeralds = -1;      // --emeralds N: the emeralds held when starting at the credits
int32_t cli_start_continue = -1; // --continue N: start on the continue screen with N continues
int32_t cli_start_level = -1;
bool cli_no_debug = false;      // --no-debug: a debug build without its debug mode (for screenshots)
int32_t cli_resolution = -1;    // --resolution N: the picture size (Video.h's ResolutionMode), not saved
int32_t cli_start_x = -1, cli_start_y = -1;
bool cli_force_demo = false;
bool cli_start_special = false;
int32_t cli_special_stage = -1;
uint8_t (*cli_ai_control_hook)(void) = NULL;
#ifdef SCP_COUNTDOWN
bool cli_countdown = false;
int32_t cli_countdown_music = -1;
uint8_t countdown_target_gamemode;
#endif
uint16_t credits_num;

uint8_t credits_cheat;

uint8_t debug_cheat, debug_mode;

uint8_t jpad2_hold, jpad2_press; // Joypad 2 state (jpad2_press is what a game may overwrite, as Sonic 2 does with the pad a CPU Tails copies)
uint8_t jpad2_press_raw; // pad 2 as it was pressed, left alone
uint8_t pause_pad2;      // pad 2's Start pauses too (a game with a second player sets it)
uint8_t jpad1_hold1, jpad1_press1; // Joypad 1 state
uint8_t jpad1_hold2, jpad1_press2; // Sonic controls
uint8_t jpad1_hold_ext, jpad1_press_ext; // Extended (non-Genesis) bindings -- see Backend/Joypad.h

uint32_t vbla_count;


// General game functions
void ReadJoypads(void) {
    uint8_t state;

    // Read joypad 1
    state = cli_ai_control_hook ? cli_ai_control_hook() : Joypad_GetState1();
    jpad1_press1 = state & ~jpad1_hold1;
    jpad1_hold1 = state;

    // Read joypad 2
    state = Joypad_GetState2();
    jpad2_press = state & ~jpad2_hold;
    jpad2_press_raw = jpad2_press;
    jpad2_hold = state;

    // Read extended (non-Genesis) bindings
    state = Joypad_GetExtState1();
    jpad1_press_ext = state & ~jpad1_hold_ext;
    jpad1_hold_ext = state;
}

// Game entry point
void EntryPoint(void) {
    // Initialize game system
    VDPSetupGame();

    // Initialize game state
    gamemode = GameMode_Sega;

    // CLI test hook: skip straight to a level, bypassing Sega/title screens.
    // Mirrors GM_Title.c's PlayLevel().
    if (cli_start_level >= 0) {
#ifndef NDEBUG
        debug_cheat = !cli_no_debug; // debug builds have it on from the title screen; level injection skips that
#endif
        level_id = (uint16_t)cli_start_level;
        lives = 3;
        rings = 0;
        level_time.pad = level_time.min = level_time.sec = level_time.frame = 0;
        score = 0;
        last_special = (cli_special_stage >= 0) ? (uint8_t)cli_special_stage : 0;
        emeralds = 0;
        memset(emerald_list, 0, sizeof(emerald_list));
        continues = 0;
        score_life = 5000;
        if (cli_start_special) {
            demo = cli_force_demo ? 1 : 0; // with --demo, the special stage plays it back too
            gamemode = GameMode_Special;
        } else {
            // demo > 0 makes MoveSonicInDemo() (Demo.c) drive Sonic from
            // recorded input instead of the real joypad -- see
            // cli_demo_override there for supplying that input data.
            demo = cli_force_demo ? 1 : 0;
            gamemode = cli_force_demo ? GameMode_Demo : GameMode_Level;
        }
    }

    // CLI test hook for the continue screen
    if (cli_start_continue >= 0) {
        lives = 0;
        continues = (uint8_t)cli_start_continue;
        level_id = LEVEL_ID(ZoneId_GHZ, 0);
        gamemode = GameMode_Continue;
    }

    // CLI test hooks for the ending and the credits
    if (cli_start_ending >= 0 || cli_start_credits >= 0) {
#ifndef NDEBUG
        debug_cheat = !cli_no_debug;
#endif
        lives = 3;
        rings = 0;
        score = 0;
        emeralds = (uint8_t)(cli_start_ending >= 0 ? cli_start_ending : (cli_emeralds >= 0 ? cli_emeralds : 0));
        for (int i = 0; i < 6; i++)
            emerald_list[i] = (uint8_t)(i < emeralds ? i : 0);
        if (cli_start_ending >= 0) {
            ending_eggmobile_exploding = (uint8_t)cli_ending_ship;
            gamemode = GameMode_Ending;
        } else {
            credits_num = (uint16_t)cli_start_credits;
            gamemode = GameMode_Credits;
        }
    }

#ifdef SCP_COUNTDOWN
    // Countdown intro: capture whatever gamemode was just decided above --
    // GameMode_Sega (normal boot, if --zone wasn't given) or Level/Demo/
    // Special (if it was) -- as the hand-off target, and show the countdown
    // first instead. Deliberately outside the cli_start_level block above:
    // a countdown before the game's own normal Sega/title/attract-mode
    // sequence is just as valid a use as one before an injected level.
    if (cli_countdown) {
        countdown_target_gamemode = gamemode;
        gamemode = GameMode_Countdown;
    }
#endif

    // Run the screens (Sonic1Screens.c lists them)
    Screens_Run();
}

// Interrupts
// Matches "move.w (v_hblank_hreg).w,(a5)" in the original VBlank routines:
// re-arm the LZ water palette-swap scanline every frame. HBlank resets the
// counter to 223 after each swap, and LZWaterFeatures (which computes the
// line) does not run while paused, so without this the split was lost after
// the first paused frame and the whole screen drew with the dry palette.
static void WriteHBlankLine(void) {
    VDP_SetHIntCounter(hbla_counter);
}

void WriteVRAMBuffers(void) {
    // Read joypad state
    ReadJoypads();

    // Copy palette
    VDP_SeekCRAM(0);
    if (wtr_state)
        VDP_WriteCRAM(&wet_palette[0][0], 0x40);
    else
        VDP_WriteCRAM(&dry_palette[0][0], 0x40);

    // Copy buffers -- sprite_buffer no longer gets copied into VRAM; VDP_Render
    // reads it directly via VDP_SetSpriteBuffer (see Video.c's VDPSetupGame).
    VDP_SeekVRAM(VRAM_HSCROLL);
    VDP_WriteVRAM((const uint8_t*)hscroll_buffer, sizeof(hscroll_buffer));
}

// Matches VBlank_UpdateScreen in the original: level tile scrolling,
// animated tiles, HUD, PLC processing (3 tiles/frame here -- slower than
// VBlank's own 9 tiles/frame elsewhere, since this competes for time with
// everything else this handler does), and the demo length countdown.
// Called directly from VBlank's case 0x08 when there's enough time, or
// deferred to HBlank when the LZ water surface is too close to the top of
// the screen for it to safely run during VBlank itself.
static void VBlank_UpdateScreen(void) {
    // Scroll camera
    LoadTilesAsYouMove();

    // Update level animations and HUD
    AnimateLevelGfx();
    HUD_Update();

    // Process PLCs
    ProcessDPLC2();

    // Decrement demo timer
    if (demo_length)
        demo_length--;
}

// Always built: the Sound Viewer is also available in release builds once the debug code is entered
// Z80 Peek: gathers live FM/PSG register state and pushes it to the render
// backend once per real frame. VBlank() (not EntryPoint()'s while(1), which
// only iterates once per gamemode change -- every GM_*() function runs its
// own internal per-frame loop) is the one place that's genuinely called
// every frame regardless of which gamemode is active, matching
// VDP_PALETTE_DISPLAY's own reasoning for living here too.
static void UpdateZ80Peek(void) {
    if (!Z80_PEEK_DISPLAY) {
        Render_SetZ80Peek(false, NULL);
        return;
    }

    Z80PeekData peek;
    static const int kTLRow[4] = {0, 2, 1, 3}; // operators 1-4 live in register rows +0, +8, +4, +C
    for (int port = 0; port < 2; port++) {
        for (int ch = 0; ch < 3; ch++) {
            peek.fm_alg_fb[port][ch] = YM2612_PeekReg(sound_music.fm, port, (uint8_t)(0xB0 + ch));
            peek.fm_pan[port][ch] = YM2612_PeekReg(sound_music.fm, port, (uint8_t)(0xB4 + ch));
            peek.fm_freq[port][ch] = (uint16_t)((YM2612_PeekReg(sound_music.fm, port, (uint8_t)(0xA4 + ch)) & 0x3F) << 8 |
                                                YM2612_PeekReg(sound_music.fm, port, (uint8_t)(0xA0 + ch)));
            for (int op = 0; op < 4; op++)
                peek.fm_tl[port][ch][op] = YM2612_PeekReg(sound_music.fm, port, (uint8_t)(0x40 + kTLRow[op] * 4 + ch)); // operator op+1
        }
    }
    peek.fm_keyon = YM2612_PeekKeyOn(sound_music.fm);
    for (int c = 0; c < 3; c++) {
        peek.psg_tone_period[c] = sound_music.psg.tone_period[c];
        peek.psg_tone_atten[c] = sound_music.psg.tone_atten[c];
    }
    peek.psg_noise_atten = sound_music.psg.noise_atten;
    peek.psg_noise_shift_rate = sound_music.psg.noise_shift_rate;
    peek.psg_noise_fb_white = sound_music.psg.noise_fb_white;
    Render_SetZ80Peek(true, &peek);
}

void VBlank(void) {
    Demo_ServiceRequests(); // in-app demo recording requests (Demo.h)
#ifdef SCP_COUNTDOWN
    Countdown_ServiceRequest(); // Tools > Countdown (Demo.h)
#endif
    UpdateZ80Peek();

    uint8_t routine = vbla_routine;
    bool skip_music = false; // set by case 0x08 when deferring to HBlank

    // Paused (0x10) shares VBlank_SpecialStage or VBlank_Levels in the
    // original, picked by game mode -- not a plain case-label fallthrough.
    if (routine == 0x10)
        routine = (gamemode == GameMode_Special) ? 0x0A : 0x08;

    if (vbla_routine != 0x00) {
        // Set VDP state
        // The title screen's foreground is a fixed picture: its scroll value is the (clamped) camera's, which only happens to be a
        // multiple of the plane's height while the plane is 32 rows.
        VDP_SetVScroll(routine == 0x04 && (gamemode & 0x7F) == GameMode_Title ? 0 : vid_scrpos_y_dup, vid_bg_scrpos_y_dup);

        // Set screen state
        vbla_routine = 0x00;
    }

    // Tell HBlank() to swap CRAM to the water palette next time it fires.
    // Only meaningful in LZ (the only zone with h-int actually enabled),
    // but set unconditionally every frame either way, matching the original.
    hblank_pal = true;

    // Run VBlank routine
    switch (routine) {
    case 0x00:
        // Lag frame. Only LZ does anything here in the original: rewrite
        // the palette and the water palette-swap line so a lag frame keeps
        // the split intact.
        if (((gamemode & 0x7F) == GameMode_Level) && Level_HasWater()) {
            VDP_SeekCRAM(0);
            if (wtr_state)
                VDP_WriteCRAM(&wet_palette[0][0], 0x40);
            else
                VDP_WriteCRAM(&dry_palette[0][0], 0x40);
            WriteHBlankLine();
        }
        break;
    case 0x02:
        WriteVRAMBuffers();
        // Fallthrough
    case 0x14:
        if (demo_length)
            demo_length--;
        break;
    case 0x04:
        WriteVRAMBuffers();
        LoadTilesAsYouMove_BGOnly();
        ProcessDPLC();
        if (demo_length)
            demo_length--;
        break;
    case 0x08:
        // Read joypad state
        ReadJoypads();
        RecordDemoFrame();

        // Copy palette
        VDP_SeekCRAM(0);
        if (wtr_state)
            VDP_WriteCRAM(&wet_palette[0][0], 0x40);
        else
            VDP_WriteCRAM(&dry_palette[0][0], 0x40);
        WriteHBlankLine();

        // Copy buffers -- sprite_buffer no longer gets copied into VRAM; VDP_Render
        // reads it directly via VDP_SetSpriteBuffer (see Video.c's VDPSetupGame).
        VDP_SeekVRAM(VRAM_HSCROLL);
        VDP_WriteVRAM((const uint8_t*)hscroll_buffer, sizeof(hscroll_buffer));

        // Update Sonic's art
        if (sonframe_chg) {
            VDP_SeekVRAM(0xF000);
            VDP_WriteVRAM(sgfx_buffer, SONIC_DPLC_SIZE);
            sonframe_chg = false;
        }

        // Update the splash/dust companion's art
        if (splashdust_frame_chg) {
            VDP_SeekVRAM(ArtTile_SplashDust * 0x20);
            VDP_WriteVRAM(splashdust_gfx_buffer, SPLASHDUST_GFX_SIZE);
            splashdust_frame_chg = false;
        }

        // Copy duplicate plane positions and flags
        scrpos_x_dup.v = scrpos_x.v;
        scrpos_y_dup.v = scrpos_y.v;
        bg_scrpos_x_dup.v = bg_scrpos_x.v;
        bg_scrpos_y_dup.v = bg_scrpos_y.v;
        bg2_scrpos_x_dup.v = bg2_scrpos_x.v;
        bg2_scrpos_y_dup.v = bg2_scrpos_y.v;
        bg3_scrpos_x_dup.v = bg3_scrpos_x.v;
        bg3_scrpos_y_dup.v = bg3_scrpos_y.v;

        LevelPlane_Snapshot(&fg_plane);
        bg1_scroll_flags_dup = bg1_scroll_flags;
        bg2_scroll_flags_dup = bg2_scroll_flags;
        bg3_scroll_flags_dup = bg3_scroll_flags;

        // If the LZ water surface's HBlank trigger line is near the top of the
        // screen, there isn't enough time before HBlank fires to safely do these
        // updates -- real hardware defers them to HBlank instead. For every
        // other zone hbla_counter stays at its dormant position (223), so this
        // always runs immediately.
        if (hbla_counter >= 96) {
            VBlank_UpdateScreen();
        } else {
            // Not enough time -- defer to HBlank instead. Matches the
            // original exactly: addq.l #4,sp / bra.w VBlank_Exit skips both
            // the demo timer decrement above (VBlank_UpdateScreen's rts
            // never runs) and the sound driver update below (the return
            // address back to that call site gets popped off unused).
            doupdatesinhblank = true;
            skip_music = true;
        }
        break;
    case 0x0A:
        // Read joypad state
        ReadJoypads();
        RecordDemoFrame();

        // Copy palette
        VDP_SeekCRAM(0);
        VDP_WriteCRAM(&dry_palette[0][0], 0x40);

        // Copy buffers -- sprite_buffer no longer gets copied into VRAM; VDP_Render
        // reads it directly via VDP_SetSpriteBuffer (see Video.c's VDPSetupGame).
        VDP_SeekVRAM(VRAM_HSCROLL);
        VDP_WriteVRAM((const uint8_t*)hscroll_buffer, sizeof(hscroll_buffer));

        // Run palette cycle
        PCycle_SS();

        // Update Sonic's art
        if (sonframe_chg) {
            VDP_SeekVRAM(0xF000);
            VDP_WriteVRAM(sgfx_buffer, SONIC_DPLC_SIZE);
            sonframe_chg = false;
        }

        // Update the splash/dust companion's art
        if (splashdust_frame_chg) {
            VDP_SeekVRAM(ArtTile_SplashDust * 0x20);
            VDP_WriteVRAM(splashdust_gfx_buffer, SPLASHDUST_GFX_SIZE);
            splashdust_frame_chg = false;
        }

        // Decrement demo timer
        if (demo_length)
            demo_length--;
        break;
    case 0x0C: // Title Cards
    case 0x18: // Ending Sequence (shares the exact same routine in the original)
        // Read joypad state
        ReadJoypads();

        // Copy palette
        VDP_SeekCRAM(0);
        if (wtr_state)
            VDP_WriteCRAM(&wet_palette[0][0], 0x40);
        else
            VDP_WriteCRAM(&dry_palette[0][0], 0x40);
        WriteHBlankLine();

        // Copy buffers -- sprite_buffer no longer gets copied into VRAM; VDP_Render
        // reads it directly via VDP_SetSpriteBuffer (see Video.c's VDPSetupGame).
        VDP_SeekVRAM(VRAM_HSCROLL);
        VDP_WriteVRAM((const uint8_t*)hscroll_buffer, sizeof(hscroll_buffer));

        // Update Sonic's art
        if (sonframe_chg) {
            VDP_SeekVRAM(0xF000);
            VDP_WriteVRAM(sgfx_buffer, SONIC_DPLC_SIZE);
            sonframe_chg = false;
        }

        // Update the splash/dust companion's art
        if (splashdust_frame_chg) {
            VDP_SeekVRAM(ArtTile_SplashDust * 0x20);
            VDP_WriteVRAM(splashdust_gfx_buffer, SPLASHDUST_GFX_SIZE);
            splashdust_frame_chg = false;
        }

        // Copy duplicate plane positions and flags
        scrpos_x_dup.v = scrpos_x.v;
        scrpos_y_dup.v = scrpos_y.v;
        bg_scrpos_x_dup.v = bg_scrpos_x.v;
        bg_scrpos_y_dup.v = bg_scrpos_y.v;
        bg2_scrpos_x_dup.v = bg2_scrpos_x.v;
        bg2_scrpos_y_dup.v = bg2_scrpos_y.v;
        bg3_scrpos_x_dup.v = bg3_scrpos_x.v;
        bg3_scrpos_y_dup.v = bg3_scrpos_y.v;

        LevelPlane_Snapshot(&fg_plane);
        bg1_scroll_flags_dup = bg1_scroll_flags;
        bg2_scroll_flags_dup = bg2_scroll_flags;
        bg3_scroll_flags_dup = bg3_scroll_flags;

        // Scroll camera
        LoadTilesAsYouMove();

        // Update level animations and HUD
        AnimateLevelGfx();
        HUD_Update();

        // Process PLCs
        ProcessDPLC();
        break;
    case 0x16: // Continue screen; also the special stage's white fade-out (the original shares it)
        ReadJoypads();
        VDP_SeekCRAM(0);
        VDP_WriteCRAM(&dry_palette[0][0], 0x40);
        VDP_SeekVRAM(VRAM_HSCROLL);
        VDP_WriteVRAM((const uint8_t*)hscroll_buffer, sizeof(hscroll_buffer));
        if (sonframe_chg) {
            VDP_SeekVRAM(0xF000);
            VDP_WriteVRAM(sgfx_buffer, SONIC_DPLC_SIZE);
            sonframe_chg = false;
        }
        if (demo_length)
            demo_length--;
        break;
    case 0x12:
        WriteVRAMBuffers();
        WriteHBlankLine();
        ProcessDPLC();
        break;
    }

    // Update music (skipped this frame if case 0x08 deferred to HBlank --
    // matches the original popping its return address unused in that case)
    if (!skip_music) {
        // (The sound driver is not stepped here: it runs on the audio device's own callback, see QtAudio.cpp.)
    }

    // Increment VBlank counter
    vbla_count++;
}

void HBlank(void) {
    if (!hblank_pal)
        return;
    hblank_pal = false;

    // Write the water palette to CRAM. Unconditional -- unlike VBlank's own
    // palette write (which picks dry or wet for the *whole* frame based on
    // wtr_state), this one always writes wet, since its purpose is to
    // override the bottom portion of an otherwise-dry frame once the
    // h-int counter is repositioned to the water surface's scanline
    // (LZWaterFeatures.c moves it to the water surface's scanline).
    VDP_SeekCRAM(0);
    VDP_WriteCRAM(&wet_palette[0][0], 0x40);

    // Reset the h-int counter back to its dormant, once-per-frame position
    VDP_SetHIntCounter(SCREEN_HEIGHT - 1);

    if (doupdatesinhblank) {
        doupdatesinhblank = false;

        // VBlank ran out of time this frame (LZ water surface too close to
        // the top of the screen) and deferred its standard updates to here.
        // hbla_counter only leaves its dormant 223 in LZ (LZWaterFeatures.c), so
        // this is reached only there, when the water surface is near the top of the screen.
        VBlank_UpdateScreen();

        // (The sound driver is not stepped here: it runs on the audio device's own callback, see QtAudio.cpp.)
    }
}
