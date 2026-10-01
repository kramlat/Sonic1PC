#include "DebugLog.h"

#include "DebugPeek.h"
#include "Game.h"
#include "Level.h" // frame_count

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define LOG_CAPACITY 8192 // entries kept in memory (a ring)

bool debug_log_active = false;

static DebugLogEntry entries[LOG_CAPACITY];
static uint32_t next_seq_to_write = 1;
static FILE *log_file = NULL;
static char log_file_path[512];

bool Debug_LogStart(const char *file_path) {
	if (!Peek_DebugToolsAvailable())
		return false; // debug mode isn't available
	Debug_LogStop();
	if (file_path != NULL && file_path[0] != '\0') {
		log_file = fopen(file_path, "a");
		if (log_file == NULL)
			return false;
		snprintf(log_file_path, sizeof(log_file_path), "%s", file_path);
	}
	debug_log_active = true;
	Debug_LogPrintf("log", "logging started");
	return true;
}

void Debug_LogStop(void) {
	if (debug_log_active)
		Debug_LogPrintf("log", "logging stopped");
	debug_log_active = false;
	if (log_file != NULL) {
		fclose(log_file);
		log_file = NULL;
	}
	log_file_path[0] = '\0';
}

void Debug_LogClear(void) {
	// Forget everything written so far (readers see a gap, not stale entries).
	memset(entries, 0, sizeof(entries));
}

void Debug_LogPrintf(const char *category, const char *fmt, ...) {
	if (!debug_log_active)
		return;
	DebugLogEntry *e = &entries[next_seq_to_write % LOG_CAPACITY];
	e->seq = next_seq_to_write++;
	e->frame = frame_count;
	snprintf(e->category, sizeof(e->category), "%s", category);

	va_list args;
	va_start(args, fmt);
	vsnprintf(e->text, sizeof(e->text), fmt, args);
	va_end(args);

	if (log_file != NULL) {
		fprintf(log_file, "[%6u] %-7s %s\n", e->frame, e->category, e->text);
		fflush(log_file); // a crash must not lose the lines leading up to it
	}
}

int Debug_LogRead(uint32_t *next_seq, DebugLogEntry *out, int max) {
	uint32_t newest = next_seq_to_write; // one past the last written
	uint32_t from = *next_seq;
	if (newest - from > LOG_CAPACITY)
		from = newest - LOG_CAPACITY; // older ones were overwritten
	int n = 0;
	for (uint32_t s = from; s < newest && n < max; s++) {
		const DebugLogEntry *e = &entries[s % LOG_CAPACITY];
		if (e->seq != s)
			continue; // cleared
		out[n++] = *e;
	}
	*next_seq = newest;
	return n;
}

const char *Debug_LogFile(void) {
	return log_file != NULL ? log_file_path : NULL;
}
