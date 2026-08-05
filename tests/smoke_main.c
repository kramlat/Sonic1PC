// Headless-friendly game runner for testing: same entry point as the real
// Sonic executable, but with CLI injection so a smoke test (or a human
// debugging a specific scenario, e.g. a boss fight) can skip straight to it
// instead of navigating there by hand. This is what tests/RunSmoke.cmake
// drives, and what earlier debugging sessions were doing by hand-editing
// fprintf() calls into GM_Level.c/PLC.c and remembering to revert them --
// this tool replaces that.
//
// Usage: SonicSmoke [--zone N] [--act N] [--x N] [--y N] [--demo FILE]
//   --zone N    Jump straight into zone N (see ZoneId in Level.h). If
//               omitted, boots normally through the Sega/title screens (and
//               every other flag below is ignored).
//   --act N     Act within that zone (default 0).
//   --x N       Override Sonic's start X position.
//   --y N       Override Sonic's start Y position.
//   --demo FILE Play the level back as an attract-mode demo (recorded input
//               driving Sonic, not the real joypad) using FILE as the
//               recorded input data, instead of playing it for real. FILE
//               must already be in the game's demo-encoding (see
//               cli_demo_override in Demo.h).

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

// Reads a whole file into a malloc'd buffer. Never freed -- this process
// runs the game once and exits.
static const uint8_t *LoadFile(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "SonicSmoke: couldn't open --demo file '%s'\n", path);
        exit(1);
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    uint8_t *buffer = malloc((size_t)size);
    if (fread(buffer, 1, (size_t)size, f) != (size_t)size) {
        fprintf(stderr, "SonicSmoke: failed reading --demo file '%s'\n", path);
        exit(1);
    }
    fclose(f);

    return buffer;
}

int main(int argc, char *argv[]) {
    int zone = -1, act = 0;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--zone") && i + 1 < argc)
            zone = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--act") && i + 1 < argc)
            act = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--x") && i + 1 < argc)
            cli_start_x = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--y") && i + 1 < argc)
            cli_start_y = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--demo") && i + 1 < argc) {
            cli_demo_override = LoadFile(argv[++i]);
            cli_force_demo = true;
        }
    }

    if (zone >= 0)
        cli_start_level = LEVEL_ID(zone, act);

    return MegaDrive_Start(&s1_header);
}
