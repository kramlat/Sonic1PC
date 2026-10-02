#pragma once

// Tools > SMPS Inspector: live per-channel activity of the sound engine (piano-key strips for FM1-6 and PSG1-3,
// squares for the noise channel) and the whole ParadoxSMPS drum kit, with a picker to play any song or effect
// through the byte driver or the JSON engine. While it is open the game is frozen like it is for the console,
// but the sound engine keeps running (see Console_SetToolPause).

class QWidget;

namespace SmpsInspector {

void Show(QWidget *parent); // create-on-first-use, then shown/raised
bool IsOpen();
void Play(int sound_id, bool json_engine); // developer hook (SONIC_QT_SMPS): play a sound as if picked

} // namespace SmpsInspector
