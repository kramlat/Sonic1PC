#pragma once

#include <stdbool.h>
#include <stdint.h>

//Demo state
extern uint16_t btn_pushtime1;
extern uint8_t btn_pushtime2;

//Demos
extern const uint8_t *intro_demo_ptr[];
extern const uint8_t *ending_demo_ptr[];

// CLI test hook: only ever set by tests/smoke_main.c's argv parsing. If
// non-NULL, MoveSonicInDemo() plays this back instead of the built-in demo
// for the current zone -- same encoded format as intro_demo_ptr/
// ending_demo_ptr (see MoveSonicInDemo()'s use of it: a repeating [button
// state byte, ..., frame-hold-count byte at +3] pattern). Lets a custom
// recorded input sequence drive a specific test scenario (e.g. a boss
// fight) instead of only ever replaying the shipped attract-mode demos.
extern const uint8_t *cli_demo_override;

// CLI test hooks: only ever set by tests/demo_record_main.c. If
// cli_demo_record is set, RecordDemoFrame() (called from Game.c's VBlank
// each real-gameplay frame) records jpad1_hold1 into an in-memory buffer
// using the same run-length [button, duration] pair format
// MoveSonicInDemo() plays back -- mirrors the disassembly's unused
// DemoRecorder routine (MoveSonicInDemo.asm), which was "likely intended
// for a developer cartridge that used RAM instead of ROM". Pressing Start
// dumps that buffer to cli_demo_record_path and exits; if
// cli_demo_record_frames >= 0, recording instead dumps and exits
// automatically once that many frames have been recorded (for scripted/AI
// runs that don't press Start themselves).
extern bool cli_demo_record;
extern const char *cli_demo_record_path;
extern int32_t cli_demo_record_frames;
void RecordDemoFrame(void);

//Demo playback
void MoveSonicInDemo(void);
