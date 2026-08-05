#pragma once

#include <stdint.h>

// External-controller gamepad pipe for SonicDemoRecord --ai-pipe. Kept in
// its own translation unit, deliberately never including any game headers:
// unistd.h's pause() collides with the game's global `pause` variable
// (Level.h) if both land in the same file.

// Creates (if needed) and opens the FIFO at `path` for non-blocking
// reading. Exits the process with an error message on failure.
void AIPipe_Open(const char *path);

// Drains everything currently waiting in the pipe and returns the JPAD_*
// bitmask (Backend/Joypad.h) from the most recently completed line, or the
// previous state if nothing new has arrived. Non-blocking either way.
uint8_t AIPipe_Poll(void);
