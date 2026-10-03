#pragma once

// Persistent settings: the game settings file, under SonicPC in the data directory (YAML; the platform's data directory
// elsewhere). Holds the audio mute/volume state, per-channel mutes and the video state (fullscreen,
// window size and position), plus a few remembered choices. Loaded once at startup before the window is
// built, and saved (debounced) whenever something changes and again on exit.

#include <QString>
#include <QStringList>

namespace Settings {

// The sound chip channels the Audio menu can mute, in SOUND_MUTE_* order (see Sound.h).
extern const char *const kChannelNames[11];
// The names the controls (Backend/Controls.h) are saved under, in CTL_* order.
extern const char *const kControlNames[8];

struct Data {
	// Audio. The live values are owned by QtAudio / Sound; Load() pushes these into them and Save()
	// reads them back, so there is a single source of truth while the game runs.
	bool music_enabled = true;
	int music_volume = 100;
	bool sfx_enabled = true;
	int sfx_volume = 100;
	QStringList muted_channels;

	// Video
	bool fullscreen = false;
	int resolution = 0; // the picture size (Video.h's ResolutionMode)
	bool has_window_geometry = false;
	int window_x = 0, window_y = 0, window_width = 0, window_height = 0; // the normal (windowed) geometry

	// Remembered choices
	QString log_file;
	QString demo_dir;                 // where recordings go; default <data dir>/demos
	int demo_zone = 0, demo_act = 0;  // the Record Demo dialog's last zone and act
};

Data &Get();

QString DataDir();   // ~/.local/share/SonicPC
QString FilePath();  // DataDir()/Sonic1Settings.cfg
QString DemoDir();   // the folder demos are recorded into (created on demand)

void Load();         // reads the file (if any) and applies the audio part to the live audio state
void Save();         // captures the live audio state and writes the file now
void SaveSoon();     // the same, ~0.5 s from now (collapses bursts such as dragging a volume slider)

} // namespace Settings
