#pragma once

// Qt window host for the "Qt" backend (-DBACKEND=Qt): the game frame is drawn
// as a texture on a drawing widget (QOpenGLWidget) inside a QMainWindow that
// has a real menu bar. Everything else (software rendering of the frame and
// its debug overlays, audio, gamepads) stays on the SDL2 backend files; this
// only replaces the window, the keyboard and the final present.
//
// Plain C interface so the C backend files (Render.c, Input.c) can use it.

#include <stdbool.h>
#include <stdint.h>

#include "../PeekData.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	int scancode;  // SDL_Scancode value, so Input.c's existing key tables are unchanged
	bool repeat;
	char text[8];  // UTF-8 text typed by this key press ("" for non-printing keys)
} QtHost_KeyEvent;

// Creates the QApplication and the main window, shows it. The frame is
// width x height pixels; icon_rgb16 is an optional 16x16 RGB24 window icon.
// The SDL scancode a Qt key maps to (0 if the game has no use for it); the controls dialog binds keys with it.
int QtHost_ScancodeFor(int qt_key);

int QtHost_Init(const char *title, int width, int height, const uint8_t *icon_rgb16);
void QtHost_Quit(void);

// Runs pending Qt events (call once per frame). QtHost_ShouldQuit becomes true
// when the window is closed or File > Quit is chosen.
void QtHost_PumpEvents(void);
bool QtHost_ShouldQuit(void);

// Shows a new frame: pixels are width*height 32-bit, bytes R,G,B,X per pixel.
void QtHost_Present(const void *pixels, int pitch);

void QtHost_ToggleFullscreen(void);

// The picture changed size (Video > Resolution): the frames are now width x height pixels. A windowed window is resized to
// show it at that size; a fullscreen one keeps its size and just letterboxes the new aspect.
void QtHost_SetPictureSize(int width, int height);

// Held-key table indexed by SDL scancode (SDL_NUM_SCANCODES entries), and a
// queue of key-down events (for F11 and the debug console).
// Latest sound-chip snapshot for the Z80 viewer window (Render_SetZ80Peek forwards here).
void QtHost_SetZ80Peek(bool active, const Z80PeekData *data);

const uint8_t *QtHost_KeyState(void);
bool QtHost_PollKeyEvent(QtHost_KeyEvent *ev);

#ifdef __cplusplus
}
#endif
