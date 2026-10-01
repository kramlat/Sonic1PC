#include "Backend/MegaDrive.h"

#include "Console.h"
#include "Demo.h"
#include "Game.h"
#include "Level.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//Sonic 1 ROM header
static const MD_Header s1_header = {
	//Vectors
	/* Start of program     */ EntryPoint,
	/* Horizontal interrupt */ HBlank,
	/* Vertical interrupt   */ VBlank,
	
	//Game information
	/* Game title           */ "SONIC THE HEDGEHOG",
};

// Reads a whole demo file into a malloc'd buffer (never freed -- the process plays one game and exits).
static const uint8_t *LoadDemoFile(const char *path) {
	FILE *f = fopen(path, "rb");
	if (!f) {
		fprintf(stderr, "Sonic: couldn't open --demo file '%s'\n", path);
		exit(1);
	}
	fseek(f, 0, SEEK_END);
	long size = ftell(f);
	fseek(f, 0, SEEK_SET);
	uint8_t *buffer = malloc((size_t)size);
	if (fread(buffer, 1, (size_t)size, f) != (size_t)size) {
		fprintf(stderr, "Sonic: failed reading --demo file '%s'\n", path);
		exit(1);
	}
	fclose(f);
	return buffer;
}

// Command-line injection for testing and debugging (this is the old SonicSmoke runner, now part of the game):
//   --zone N [--act N]   skip the title screen and start straight in that level (0 GHZ, 1 LZ, 2 MZ, 3 SLZ, 4 SYZ, 5 SBZ)
//   --x N / --y N        override Sonic's start position (needs --zone)
//   --special N          start in special stage N (0-5) on the way to the level given by --zone (default GHZ act 1)
//   --demo FILE          play the level back as a demo, driven by the recorded input in FILE (needs --zone)
//   --countdown [--countdown-music HEX]   pie-wipe countdown before the injected level (SPLASH builds only)
static void ParseCommandLine(int argc, char *argv[]) {
	int zone = -1, act = 0;

	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--zone") && i + 1 < argc)
			zone = atoi(argv[++i]);
		else if (!strcmp(argv[i], "--act") && i + 1 < argc)
			act = atoi(argv[++i]);
		else if (!strcmp(argv[i], "--special") && i + 1 < argc) {
			cli_start_special = true;
			cli_special_stage = atoi(argv[++i]);
			if (zone < 0)
				zone = 0;
		} else if (!strcmp(argv[i], "--x") && i + 1 < argc)
			cli_start_x = atoi(argv[++i]);
		else if (!strcmp(argv[i], "--y") && i + 1 < argc)
			cli_start_y = atoi(argv[++i]);
		else if (!strcmp(argv[i], "--demo") && i + 1 < argc) {
			cli_demo_override = LoadDemoFile(argv[++i]);
			cli_force_demo = true;
		} else if (!strcmp(argv[i], "--countdown")) {
#ifdef SCP_SPLASH
			cli_countdown = true;
#else
			fprintf(stderr, "Sonic: --countdown needs a SPLASH build (Premier/Showcase)\n");
			exit(1);
#endif
		} else if (!strcmp(argv[i], "--countdown-music") && i + 1 < argc) {
#ifdef SCP_SPLASH
			cli_countdown_music = (int32_t)strtol(argv[++i], NULL, 16);
#else
			fprintf(stderr, "Sonic: --countdown-music needs a SPLASH build (Premier/Showcase)\n");
			exit(1);
#endif
		}
	}

	if (zone >= 0)
		cli_start_level = LEVEL_ID(zone, act);
}

//MegaDrive entry point
int main(int argc, char *argv[]) {
	ParseCommandLine(argc, argv);
	console_enabled = true; // the console drawer (Qt) only opens while debugging is available

	// stdout is fully buffered by libc whenever it's not a terminal (e.g.
	// redirected to a log file), unlike stderr, which is always unbuffered.
	// The SONIC_*_TRACE debug output (Sound.c) writes to stdout so stderr
	// stays clean for crash/error messages -- but that means a crash (which
	// goes through abort(), skipping normal stdio flush-on-exit) would
	// silently lose every buffered trace line right when it's needed most.
	// Force line buffering so each trace line hits the file immediately.
	setvbuf(stdout, NULL, _IOLBF, 0);

	//Start MegaDrive
	return MegaDrive_Start(&s1_header);
}
