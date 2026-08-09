// YouTube-Premiere-style countdown intro for recorded showcase footage: a
// 1-minute timer with a pie-wipe progress indicator, playing one of the
// game's own music tracks -- then hands off to whatever cli_start_level/
// cli_force_demo/cli_start_special already set up (see Game.h's
// cli_countdown and countdown_target_gamemode).
//
// Deliberately "special": it uses the real game's own sound engine
// (PlayMusic/PlaySound, same Sound.c every other screen uses) but NOT the
// VDP tile renderer -- the pie wipe and the countdown number are both drawn
// directly by the SDL2 backend (Render_SetCountdownPie, Backend/VDP.h) as a
// PC-only overlay on top of the emulated frame, since real Genesis hardware
// has no vector/arc drawing to produce a smooth pie shape from.

#include "GM_Countdown.h"

#include "Backend/VDP.h"
#include "Game.h"
#include "PLC.h"
#include "Palette.h"
#include "Sound.h"
#include "Video.h"

// Default music if cli_countdown_music wasn't given one -- GHZ's theme,
// picked purely because it's the game's own opening/most recognizable track.
#define COUNTDOWN_DEFAULT_MUSIC 0x81

// $CD (Switch) -- a short, punchy click, played once per second for the
// final 10 seconds as an audible tick.
#define COUNTDOWN_TICK_SOUND 0xCD

#define COUNTDOWN_SECONDS  60
#define COUNTDOWN_TOTAL_FRAMES (COUNTDOWN_SECONDS * 60)

void GM_Countdown(void) {
    // Clear the pattern load queue and fade out whatever was on screen
    // before (title/sega/etc) -- the countdown draws nothing of its own via
    // the VDP, so a plain faded-to-black frame underneath the SDL overlay is
    // all that's needed.
    ClearPLC();
    PaletteFadeOut();
    ClearScreen();
    VDP_SetBackgroundColour(0);

    PlayMusic(cli_countdown_music >= 0 ? (uint8_t)cli_countdown_music : COUNTDOWN_DEFAULT_MUSIC);

    uint32_t frames_left = COUNTDOWN_TOTAL_FRAMES;
    int last_second_shown = -1;

    while (1) {
        vbla_routine = 0x02;
        WaitForVBla();

        int seconds_left = (int)((frames_left + 59) / 60);
        // One full pie revolution per second (resets every second), not one
        // slow revolution across the whole 60 -- matches a real YouTube
        // Premiere countdown's sweep. The digital number below is what
        // actually shows the time remaining.
        uint32_t elapsed_frames = COUNTDOWN_TOTAL_FRAMES - frames_left;
        float fraction = (float)(elapsed_frames % 60) / 60.0f;
        Render_SetCountdownPie(true, fraction, seconds_left);

        // Tick right as each of the last 10 sweeps FINISHES, not as it
        // starts -- last_second_shown is still the just-completed second's
        // count at this point (updated after the check), so 1..10 here
        // means "the sweep for that second just wrapped".
        if (seconds_left != last_second_shown) {
            if (last_second_shown >= 1 && last_second_shown <= 10)
                PlaySound(COUNTDOWN_TICK_SOUND);
            last_second_shown = seconds_left;
        }

        // START skips the rest of the countdown early.
        if ((jpad1_press1 & JPAD_START) || frames_left == 0)
            break;
        frames_left--;
    }

    // Stop drawing the overlay and hand off to whatever was queued up.
    Render_SetCountdownPie(false, 0.0f, 0);
    gamemode = countdown_target_gamemode;
}
