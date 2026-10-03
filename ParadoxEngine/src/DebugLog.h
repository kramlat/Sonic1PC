#pragma once

// Debug event log: can be started and stopped at runtime (the Qt backend's
// View > Logging menu), keeps the recent entries in memory for the Log window
// and can also write them to a file. Starting is refused unless debug mode is
// available (a debug build, or the debug code entered), and while logging is
// off DEBUG_LOG costs one flag test.

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern bool debug_log_active;

// DEBUG_LOG("category", "printf format", args...) -- categories are short
// lowercase tags ("game", "level", "player", "sound"...), used for filtering.
#define DEBUG_LOG(category, ...) \
	do { if (debug_log_active) Debug_LogPrintf(category, __VA_ARGS__); } while (0)

void Debug_LogPrintf(const char *category, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

// Starts logging; file_path may be NULL (in-memory only, for the Log window).
// Returns false if debug mode isn't available or the file can't be opened.
bool Debug_LogStart(const char *file_path);
void Debug_LogStop(void);
void Debug_LogClear(void);

typedef struct {
	uint32_t seq;        // 1, 2, 3... never reused
	uint32_t frame;      // the game's frame_count when it was logged
	char category[16];
	char text[200];
} DebugLogEntry;

// Copies entries with seq >= *next_seq into out (at most max) and advances
// *next_seq past them. Entries that have already been overwritten are skipped.
int Debug_LogRead(uint32_t *next_seq, DebugLogEntry *out, int max);

// The file currently being written to, or NULL.
const char *Debug_LogFile(void);

#ifdef __cplusplus
}
#endif
