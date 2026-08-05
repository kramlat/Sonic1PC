// Demo recorder: play a level (or special stage) for real -- using the real
// joypad/keyboard, a scripted example AI, or an external controller driving
// Sonic live over a named pipe -- and record the input as a demo file in
// the same format Demo.c/MoveSonicInDemo() plays back. Press Start to stop
// and write the file (or pass --frames to auto-stop after a fixed number of
// frames, e.g. for unattended --ai/--ai-pipe runs).
//
// Usage:
//   SonicDemoRecord --zone N --act N [--x N] [--y N] [--out FILE] [--frames N] [--ai | --ai-pipe PATH]
//   SonicDemoRecord --special [--special-stage N] [--out FILE] [--frames N] [--ai | --ai-pipe PATH]
//
//   --zone N          Zone to record in (see ZoneId in Level.h).
//   --act N           Act within that zone (default 0).
//   --special         Record a special stage instead of a zone/act.
//   --special-stage N Which of the 6 special stage layouts (default 0).
//   --x N / --y N     Override Sonic's start position.
//   --out FILE        Where to write the recording. Defaults to
//                      "bindir/demos/Zone N Act N timestamp.bin", e.g.
//                      ".../demos/Zone 0 Act 1 2026.07.05-02:12:00.bin", or
//                      ".../demos/Special Stage N timestamp.bin" for
//                      --special (N is --special-stage's default, 0, if not
//                      given).
//   --frames N        Auto-stop and write after N frames instead of waiting
//                      for Start to be pressed.
//   --ai              Drive Sonic with the example AI hook (see AIControl()
//                      below) instead of real input. A placeholder -- swap
//                      its body out for real decision logic to record
//                      AI-driven playthroughs without external plumbing.
//   --ai-pipe PATH    Drive Sonic from an external controller (a script,
//                      another process, an actual LLM agent, ...) instead
//                      of real input -- see tests/ai_pipe.h for the wire
//                      format: newline-terminated lines of one-letter
//                      button tokens (U D L R A B C S) written to a FIFO
//                      created at PATH. E.g. from a shell:
//                      `echo "R A" > PATH` to hold right and jump,
//                      `echo > PATH` to let go of everything. The most
//                      recently received line stays held until the next
//                      one arrives, so the writer doesn't need to keep
//                      pace with 60fps.

#include "Backend/MegaDrive.h"
#include "Demo.h"
#include "Game.h"
#include "Level.h"

#include "ai_pipe.h"
#include "timestamp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const MD_Header s1_header = {
    /* Start of program     */ EntryPoint,
    /* Horizontal interrupt */ HBlank,
    /* Vertical interrupt   */ VBlank,
    /* Game title           */ "SONIC THE HEDGEHOG",
};

// Placeholder example AI: runs right, and jumps briefly every couple of
// seconds, so it makes some forward progress instead of just walking into
// the first wall. Deliberately simple -- the point is the hook, not the
// policy. Swap this out (or use --ai-pipe instead) for real decision logic.
static uint8_t AIControl(void) {
    static uint32_t frame;
    frame++;

    uint8_t buttons = JPAD_RIGHT;
    if ((frame % 90) < 6) // jump briefly, roughly every 1.5s at 60fps
        buttons |= JPAD_A;
    return buttons;
}

// Builds e.g. "DEMO_OUTPUT_DIR/Zone 0 Act 1 2026.07.05-02:12:00.bin" (or
// ".../Special Stage 0 2026.07.05-02:12:00.bin" for --special, where 0 is
// --special-stage's default when it isn't given) into a static buffer, used
// as the default --out when none is given. DEMO_OUTPUT_DIR (bindir/demos,
// created by CMakeLists.txt) is only defined in test builds (BUILD_TESTS) --
// this file is only ever compiled as part of the SonicDemoRecord test tool,
// never the real game, so it's always available here.
static const char *DefaultOutPath(bool special, int zone, int act, int special_stage) {
    static char path[128];

    char timestamp[32];
    Timestamp_Now(timestamp, sizeof(timestamp));

    if (special)
        snprintf(path, sizeof(path), DEMO_OUTPUT_DIR "/Special Stage %d %s.bin",
                 special_stage >= 0 ? special_stage : 0, timestamp);
    else
        snprintf(path, sizeof(path), DEMO_OUTPUT_DIR "/Zone %d Act %d %s.bin", zone, act + 1, timestamp);

    return path;
}

int main(int argc, char *argv[]) {
    int zone = -1, act = 0;
    bool special = false;
    int special_stage = -1;
    const char *out_path = NULL;
    int frames = -1;
    bool use_ai = false;
    const char *ai_pipe_path = NULL;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--zone") && i + 1 < argc)
            zone = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--act") && i + 1 < argc)
            act = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--special"))
            special = true;
        else if (!strcmp(argv[i], "--special-stage") && i + 1 < argc)
            special_stage = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--x") && i + 1 < argc)
            cli_start_x = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--y") && i + 1 < argc)
            cli_start_y = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--out") && i + 1 < argc)
            out_path = argv[++i];
        else if (!strcmp(argv[i], "--frames") && i + 1 < argc)
            frames = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--ai"))
            use_ai = true;
        else if (!strcmp(argv[i], "--ai-pipe") && i + 1 < argc)
            ai_pipe_path = argv[++i];
    }

    if (!special && zone < 0) {
        fprintf(stderr, "Usage: %s --zone N [--act N] | --special [...]\n", argv[0]);
        return 1;
    }

    cli_start_level = special ? 0 : LEVEL_ID(zone, act);
    cli_start_special = special;
    cli_special_stage = special_stage;

    cli_demo_record = true;
    cli_demo_record_path = out_path ? out_path : DefaultOutPath(special, zone, act, special_stage);
    cli_demo_record_frames = frames;

    if (ai_pipe_path) {
        AIPipe_Open(ai_pipe_path);
        cli_ai_control_hook = AIPipe_Poll;
    } else if (use_ai) {
        cli_ai_control_hook = AIControl;
    }

    return MegaDrive_Start(&s1_header);
}
