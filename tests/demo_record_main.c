// Demo recorder: play a level (or special stage) for real -- using the real
// joypad/keyboard, or a scripted/AI control hook -- and record the input as
// a demo file in the same format Demo.c/MoveSonicInDemo() plays back. Press
// Start to stop and write the file (or pass --frames to auto-stop after a
// fixed number of frames, e.g. for --ai runs that don't press Start
// themselves).
//
// Usage:
//   SonicDemoRecord --zone N --act N [--x N] [--y N] [--out FILE] [--ai] [--frames N]
//   SonicDemoRecord --special [--special-stage N] [--out FILE] [--ai] [--frames N]
//
//   --zone N          Zone to record in (see ZoneId in Level.h).
//   --act N           Act within that zone (default 0).
//   --special         Record a special stage instead of a zone/act.
//   --special-stage N Which of the 6 special stage layouts (default 0).
//   --x N / --y N     Override Sonic's start position.
//   --out FILE        Where to write the recording (default demo_record.bin).
//   --frames N        Auto-stop and write after N frames instead of waiting
//                      for Start to be pressed.
//   --ai              Drive Sonic with the example AI hook (see AIControl()
//                      below) instead of real input. This is a placeholder:
//                      swap AIControl()'s body out for real decision logic
//                      (or point cli_ai_control_hook at a different
//                      function entirely) to record AI-driven playthroughs.

#include "Backend/MegaDrive.h"
#include "Demo.h"
#include "Game.h"
#include "Level.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const MD_Header s1_header = {
    /* Start of program     */ EntryPoint,
    /* Horizontal interrupt */ HBlank,
    /* Vertical interrupt   */ VBlank,
    /* Game title           */ "SONIC THE HEDGEHOG",
};

// Placeholder example AI: runs right, and jumps whenever something ahead of
// Sonic is at his own height (a crude "there might be something to jump
// over/onto" heuristic) or every couple of seconds regardless, so it makes
// some forward progress instead of just walking into the first wall. This
// is deliberately simple -- the point is the hook, not the policy.
static uint8_t AIControl(void) {
    static uint32_t frame;
    frame++;

    uint8_t buttons = JPAD_RIGHT;
    if ((frame % 90) < 6) // jump briefly, roughly every 1.5s at 60fps
        buttons |= JPAD_A;
    return buttons;
}

int main(int argc, char *argv[]) {
    int zone = -1, act = 0;
    bool special = false;
    int special_stage = -1;
    const char *out_path = NULL;
    int frames = -1;
    bool use_ai = false;

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
    }

    if (!special && zone < 0) {
        fprintf(stderr, "Usage: %s --zone N [--act N] | --special [...]\n", argv[0]);
        return 1;
    }

    cli_start_level = special ? 0 : LEVEL_ID(zone, act);
    cli_start_special = special;
    cli_special_stage = special_stage;

    cli_demo_record = true;
    cli_demo_record_path = out_path;
    cli_demo_record_frames = frames;

    if (use_ai)
        cli_ai_control_hook = AIControl;

    return MegaDrive_Start(&s1_header);
}
