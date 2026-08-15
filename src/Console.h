#pragma once

#include <stdbool.h>

// Quake-style in-game debug console -- SonicSmoke only (tests/smoke_main.c
// sets console_enabled = true before calling MegaDrive_Start; the real
// Sonic executable's src/Main.c never touches it, so this whole feature
// stays fully compiled-in but completely inert there).
//
// Opening the console acts like a gdb Ctrl+C (freezes gameplay + music,
// keeps rendering/input alive); closing it acts like `continue` (resumes
// exactly where it froze). See ConsoleUpdate's own comment for the
// blocking-loop shape this mirrors (GM_Level.c's existing PauseGame()).
extern bool console_enabled;

bool Console_IsOpen(void);
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
