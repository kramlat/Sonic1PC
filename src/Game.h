#pragma once

#include <stdint.h>

#include "Backend/Joypad.h"
#include <stdbool.h>
//Game types
typedef enum {
	GameMode_Sega,
	GameMode_Title,
	GameMode_Demo,
	GameMode_Level,
	GameMode_Special,
	GameMode_Continue,
	GameMode_Ending,
	GameMode_Credits,
#ifdef SCP_SPLASH
	GameMode_SSRG,
	GameMode_Countdown,
#endif
} GameMode;

//Game state
extern uint8_t buffer0000[0xA400];

extern uint8_t gamemode;

extern int16_t demo;
extern uint16_t demo_length;

// CLI test hooks: only ever set by tests/smoke_main.c's argv parsing (the
// real game's Main.c never touches these), so normal boot-up is unaffected
// unless something deliberately opts in.
//
// If cli_start_level >= 0 (a LEVEL_ID(zone, act) value), EntryPoint() skips
// the Sega/title screens and jumps straight into that level, exactly as if
// "start" had been pressed on the title screen (or, if cli_force_demo is
// set, as if the title screen's attract-mode demo had triggered for that
// level instead -- see cli_demo_override in Demo.h for supplying the
// recorded input data to play back).
extern int32_t cli_start_level;
// If >= 0, overrides Sonic's start X/Y position (LevelSizeLoad()) once
// cli_start_level has picked a level -- e.g. to drop straight into a boss
// fight instead of walking there.
extern int32_t cli_start_x, cli_start_y;
// Play cli_start_level back as an attract-mode demo (recorded input driving
// Sonic, see Demo.h) instead of a real playthrough.
extern bool cli_force_demo;
// Force GameMode_Special instead of GameMode_Level once cli_start_level has
// triggered the injection (cli_start_level's actual value is ignored, but
// still must be >= 0 to opt in -- see EntryPoint()). Pick which of the 6
// special stage layouts via last_special (SpecialStage.h).
extern bool cli_start_special;
// If >= 0, overrides which of the 6 special stage layouts cli_start_special
// uses (see last_special, SpecialStage.h). -1 leaves it at the default (0).
extern int32_t cli_special_stage;
// If non-NULL, ReadJoypads() (Game.c) calls this each frame instead of
// reading the real joypad, letting an external "AI"/bot control Sonic --
// e.g. for SonicDemoRecord --ai. Must return a JPAD_* bitmask (see
// Backend/Joypad.h) of currently-held buttons.
extern uint8_t (*cli_ai_control_hook)(void);

#ifdef SCP_SPLASH
// If true, EntryPoint() routes through GameMode_Countdown (a 1-minute
// countdown screen with a pie-wipe progress indicator and music, see
// GM_Countdown.c) before whatever cli_start_level/cli_force_demo/
// cli_start_special already set up -- for recording a YouTube-Premiere-
// style countdown intro ahead of scripted showcase footage. Ignored unless
// cli_start_level is also set (nothing to count down to otherwise). Press
// START to skip the countdown early. Only exists in SPLASH builds (Premier/
// Showcase) -- same reasoning as GameMode_SSRG, this is demo-recording
// tooling, not something a normal build needs.
extern bool cli_countdown;
// Sound ID to play during the countdown (see Sound.h's SOUND_ID_* range);
// -1 uses GM_Countdown's own default.
extern int32_t cli_countdown_music;
// Set by EntryPoint() alongside cli_countdown: the gamemode (GameMode_Level/
// GameMode_Demo/GameMode_Special) the countdown should hand off to once it
// finishes (or is skipped) -- GM_Countdown.c reads this rather than
// duplicating EntryPoint()'s own cli_start_level/cli_force_demo/
// cli_start_special decision logic.
extern uint8_t countdown_target_gamemode;
#endif
extern uint16_t credits_num;

extern uint8_t credits_cheat;

extern uint8_t debug_cheat, debug_mode;

extern uint8_t jpad2_hold,  jpad2_press;
extern uint8_t jpad1_hold1, jpad1_press1;
extern uint8_t jpad1_hold2, jpad1_press2;
extern uint8_t jpad1_hold_ext, jpad1_press_ext;

extern uint32_t vbla_count;

//Global assets
extern const uint8_t Art_Text[];

extern bool VDP_PALETTE_DISPLAY;
extern uint16_t VRAMADDR;
extern uint8_t CRAMPAL;

// "Z80 Peek" debug overlay (live YM2612/SN76489 register dump) -- Left Alt
// toggles it, same edge-detected pattern as VDP_PALETTE_DISPLAY above, same
// #ifndef NDEBUG gating (see Backend/SDL2/Input.c). Game.c's main loop reads
// this each frame to decide whether to gather register state and push it to
// the render backend (Render_SetZ80Peek, Backend/VDP.h).
extern bool Z80_PEEK_DISPLAY;

//General game functions
void ReadJoypads(void);

//Entry point
void EntryPoint(void);

//Interrupt functions
void VBlank(void);
void HBlank(void);
