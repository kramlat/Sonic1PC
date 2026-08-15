#include "Backend/MegaDrive.h"

#include "Game.h"

#include <stdio.h>

//Sonic 1 ROM header
static const MD_Header s1_header = {
	//Vectors
	/* Start of program     */ EntryPoint,
	/* Horizontal interrupt */ HBlank,
	/* Vertical interrupt   */ VBlank,
	
	//Game information
	/* Game title           */ "SONIC THE HEDGEHOG",
};

//MegaDrive entry point
int main(int argc, char *argv[]) {
	(void)argc;
	(void)argv;

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
