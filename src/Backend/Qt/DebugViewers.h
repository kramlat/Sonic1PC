#pragma once

// Debug tool windows for the Qt backend (View menu): a VDP viewer (VRAM tiles
// + palettes) and a sound-chip viewer (YM2612 / SN76489 state), both live.
// They replace the SDL2 backend's in-frame overlays (Tab / Left Alt).

#include "../PeekData.h"

class QWidget;

namespace DebugViewers {

// Create-on-first-use, then shown/raised. Both are top-level windows owned by
// `parent` (so they close with the main window).
void ShowVdpViewer(QWidget *parent);
void ShowSoundViewer(QWidget *parent);
void ShowVariableViewer(QWidget *parent); // the game's global variables, editable
void ShowObjectViewer(QWidget *parent);

// Debug event log (DebugLog.h): a window showing the log, and start/stop control.
void ShowLogViewer(QWidget *parent);
void ToggleLogging(QWidget *parent);   // start (to the chosen file, if any) or stop
void ChooseLogFile(QWidget *parent);   // pick the file used the next time logging starts
bool IsLogging(); // the game's object slots ("RAM")

// Latest sound-chip snapshot, pushed by the game each frame while the sound
// viewer is open (see QtHost_SetZ80Peek).
void SetSoundSnapshot(bool active, const Z80PeekData *data);

} // namespace DebugViewers
