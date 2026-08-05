#pragma once

#include <stddef.h>

// Writes the current local time as "YYYY.MM.DD-HH:MM:SS" into buf. Kept in
// its own translation unit, deliberately never including any game headers:
// time.h's time()/localtime() collide with the game's global `time`
// variable (Level.h) if both land in the same file.
void Timestamp_Now(char *buf, size_t len);
