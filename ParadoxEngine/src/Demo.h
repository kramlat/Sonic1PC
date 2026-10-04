#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

//Demo state
extern uint16_t btn_pushtime1;
extern uint8_t btn_pushtime2;

//Demos
extern const uint8_t *intro_demo_ptr[];
extern const uint8_t *ending_demo_ptr[];

// CLI test hook: only ever set by src/Main.c's argv parsing. If
// non-NULL, MoveSonicInDemo() plays this back instead of the built-in demo
// for the current zone -- same encoded format as intro_demo_ptr/
// ending_demo_ptr (see MoveSonicInDemo()'s use of it: a repeating [button
// state byte, ..., frame-hold-count byte at +3] pattern). Lets a custom
// recorded input sequence drive a specific test scenario (e.g. a boss
// fight) instead of only ever replaying the shipped attract-mode demos.
extern const uint8_t *cli_demo_override;
// Length in frames of the loaded demo (cli_demo_override); -1 = the usual 1800.
extern int32_t cli_demo_length;

// Demo recording state (set by Demo_RequestRecording below, i.e. the Tools menu). While cli_demo_record is
// set, RecordDemoFrame() (called from Game.c's VBlank each real-gameplay frame) records jpad1_hold1 into an
// in-memory buffer using the same run-length [button, duration] pair format MoveSonicInDemo() plays back --
// it mirrors the disassembly's unused DemoRecorder routine (MoveSonicInDemo.asm), which was "likely intended
// for a developer cartridge that used RAM instead of ROM". Stopping writes the buffer to
// cli_demo_record_path; if cli_demo_record_frames >= 0 it stops by itself after that many frames.
extern bool cli_demo_record;
extern const char *cli_demo_record_path;
extern int32_t cli_demo_record_frames;
void RecordDemoFrame(void);

// Recording from the Qt Tools menu: restarts the chosen level, records until it is stopped from the menu
// (Start stays the game's pause button) or the frame limit is reached, and then the game keeps running. A
// request is applied on the next frame, from whatever screen the game is on (it waits for a screen that
// can be left, if need be).
typedef struct {
	int zone, act;          // zone (ZoneId) and act 0-2; ignored for a special stage
	bool special;
	int special_stage;      // 0-5
	int start_x, start_y;   // override Sonic's start position; -1 = the level's own
	bool split_screen;      // record it in the split screen (a game that has one: GameInfo::split_screen)
	int frames;             // auto-stop after this many frames; -1 = until stopped
	char path[512];         // output file
} DemoRecordRequest;

void Demo_RequestRecording(const DemoRecordRequest *request);

// Plays a recorded demo file (the format above) in a level, from the start of that level. The data is
// copied. Special stages have no demo playback yet, so this is for zone levels.
typedef struct {
	int zone, act;
	int start_x, start_y;   // the same start position the demo was recorded from; -1 = the level's own
	bool split_screen;      // it was recorded in the split screen
} DemoPlayRequest;
bool Demo_RequestPlayback(const DemoPlayRequest *request, const uint8_t *data, size_t length);
void Demo_ServiceRequests(void);   // called once per frame from VBlank: applies a pending request
bool Demo_PlaybackActive(void);    // a demo file (Tools > Play Demo, or --demo) is driving Sonic
bool Demo_RecordingActive(void);   // a recording is running or about to start
int Demo_RecordedFrames(void);
void Demo_StopRecording(void);     // writes the file and stops; the game keeps running
const char *Demo_LastSavedPath(void);   // the most recent file written ("" if none), and its length
int Demo_LastSavedFrames(void);

//Demo playback
void MoveSonicInDemo(void);
