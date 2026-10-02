#pragma once

#include <stdbool.h>
#include <stdint.h>

// Quake-style in-game debug console. The commands and variable registry live in Console.c; the window is the
// Qt drawer (src/Backend/Qt/ConsoleDrawer.cpp), which slides down over the game view and feeds lines in through
// Console_SetInput + Console_HandleKey(Enter).
//
// Opening the console acts like a gdb Ctrl+C (freezes gameplay + music,
// keeps rendering/input alive); closing it acts like `continue` (resumes
// exactly where it froze). See ConsoleUpdate's own comment for the
// blocking-loop shape this mirrors (GM_Level.c's existing PauseGame()).
extern bool console_enabled;

bool Console_IsOpen(void);

// A tool window (the SMPS Inspector) freezes gameplay the same way the open console does, but leaves the sound engine
// running: the tool plays sounds itself, and nothing from the game may start another one meanwhile.
void Console_SetToolPause(bool on);
void Console_Toggle(void);

// Called once per frame from GM_Level.c's main loop, right alongside
// PauseGame(). No-op immediately unless console_enabled AND the console is
// currently open; otherwise blocks (looping WaitForVBla, same shape as
// PauseGame's own do-while) until the console is closed again.
void ConsoleUpdate(void);

// Input routing -- called from Backend/SDL2/Input.c's Input_HandleEvents
// when console_enabled is true. Deliberately backend-agnostic (no SDL
// types here, matching Joypad.h's own JPAD_* abstraction) -- the backend
// translates SDL_TEXTINPUT/SDL_KEYDOWN into these before calling in.
void Console_HandleText(const char *text);
void Console_SetInput(const char *text); // replaces the pending input line (the drawer's line edit owns the text)
uint32_t Console_LineCount(void);        // lines ever logged: the drawer appends the ones it hasn't shown yet
typedef enum {
    ConsoleKey_Backspace,
    ConsoleKey_Enter,
    ConsoleKey_Up,
    ConsoleKey_Down,
} ConsoleKey;
void Console_HandleKey(ConsoleKey key);

// Rendering readout -- called from Backend/SDL2/Render.c's DrawConsole.
#define CONSOLE_LOG_LINES 24
#define CONSOLE_LINE_LEN  96

// index_from_bottom: 0 = most recent line, 1 = one before that, etc.
// Returns NULL once index_from_bottom goes past however many lines have
// actually been logged yet (so the renderer just stops drawing upward).
const char *Console_GetLogLine(int index_from_bottom);
const char *Console_GetInputLine(void);
int Console_GetCursor(void);
