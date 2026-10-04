#include "Settings.h"

#include "QtAudio.h"

#include <QDir>
#include <QSaveFile>
#include <QStandardPaths>
#include <QTimer>

#include <yaml-cpp/yaml.h>

#include "../Controls.h"
#include "../../GameInterface.h" // game_info

#include <stdio.h>

// Sound engine channel mutes (Sound.h is a C-only header; SOUND_MUTE_* there matches kChannelNames).
extern "C" {
void Sound_SetChannelMuted(int channel, bool muted);
bool Sound_IsChannelMuted(int channel);
}

namespace Settings {

const char *const kChannelNames[11] = {"FM 1", "FM 2", "FM 3", "FM 4", "FM 5", "FM 6",
                                       "PSG Tone 1", "PSG Tone 2", "PSG Tone 3", "PSG Noise", "DAC (samples)"};

const char *const kControlNames[CTL_COUNT] = {"up", "down", "left", "right", "a", "b", "c", "start"};

namespace {

Data g_data;
bool g_loaded = false;
const int kVersion = 1;

int Clamp(int v, int lo, int hi) {
	return v < lo ? lo : (v > hi ? hi : v);
}

// A value that is missing or has the wrong type falls back to the default instead of failing the load.
template <typename T>
T Read(const YAML::Node &node, const char *key, T fallback) {
	try {
		if (node && node[key])
			return node[key].as<T>();
	} catch (const YAML::Exception &) {
	}
	return fallback;
}

QString ReadString(const YAML::Node &node, const char *key) {
	return QString::fromStdString(Read<std::string>(node, key, ""));
}

} // namespace

Data &Get() {
	return g_data;
}

QString DataDir() {
	return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + "/" + QString::fromUtf8(game_info.data_dir);
}

QString FilePath() {
	return DataDir() + "/" + QString::fromUtf8(game_info.settings_file);
}

QString DemoDir() {
	QString dir = g_data.demo_dir.isEmpty() ? DataDir() + "/demos" : g_data.demo_dir;
	QDir().mkpath(dir);
	return dir;
}

void Load() {
	g_loaded = true;
	try {
		YAML::Node root = YAML::LoadFile(FilePath().toStdString());

		YAML::Node audio = root["audio"];
		YAML::Node music = audio["music"], sfx = audio["sfx"];
		g_data.music_enabled = Read(music, "enabled", true);
		g_data.music_volume = Clamp(Read(music, "volume", 100), 0, 100);
		g_data.sfx_enabled = Read(sfx, "enabled", true);
		g_data.sfx_volume = Clamp(Read(sfx, "volume", 100), 0, 100);
		g_data.muted_channels.clear();
		if (audio["muted_channels"] && audio["muted_channels"].IsSequence())
			for (const YAML::Node &n : audio["muted_channels"])
				g_data.muted_channels << QString::fromStdString(n.as<std::string>(""));

		YAML::Node video = root["video"];
		g_data.fullscreen = Read(video, "fullscreen", false);
		g_data.resolution = Clamp(Read(video, "resolution", 0), 0, 4);
		YAML::Node window = video["window"];
		if (window && window.IsMap()) {
			g_data.window_width = Read(window, "width", 0);
			g_data.window_height = Read(window, "height", 0);
			g_data.window_x = Read(window, "x", 0);
			g_data.window_y = Read(window, "y", 0);
			g_data.has_window_geometry = g_data.window_width >= 320 && g_data.window_height >= 224;
		}

		g_data.log_file = ReadString(root["debug"], "log_file");
		YAML::Node demo = root["demo"];
		g_data.demo_dir = ReadString(demo, "folder");
		g_data.demo_zone = Clamp(Read(demo, "last_zone", 0), 0, 6);
		g_data.demo_act = Clamp(Read(demo, "last_act", 0), 0, 3);

		YAML::Node controls = root["controls"];
		if (controls && controls.IsMap()) {
			Controls_DeadzoneLeft = Clamp(Read(controls, "deadzone_left", Controls_DeadzoneLeft), 0, CONTROLS_DEADZONE_MAX);
			Controls_DeadzoneRight = Clamp(Read(controls, "deadzone_right", Controls_DeadzoneRight), 0, CONTROLS_DEADZONE_MAX);
			Controls_DeadzoneRing = Clamp(Read(controls, "deadzone_ring", Controls_DeadzoneRing), 0, 1);
			Controls_DeadzoneLeft2 = Clamp(Read(controls, "deadzone_left2", Controls_DeadzoneLeft2), 0, CONTROLS_DEADZONE_MAX);
			Controls_DeadzoneRing2 = Clamp(Read(controls, "deadzone_ring2", Controls_DeadzoneRing2), 0, 1);
			for (int pl = 0; pl < 2; pl++) { // which gamepad is whose
				std::string key = pl ? "pad_guid2" : "pad_guid1";
				if (controls[key] && controls[key].IsScalar()) {
					snprintf(Controls_PadGuid[pl], sizeof(Controls_PadGuid[pl]), "%s", controls[key].as<std::string>().c_str());
					Controls_PadNth[pl] = Clamp(Read(controls, pl ? "pad_nth2" : "pad_nth1", 0), 0, 15);
				}
			}
		}
		if (controls && controls.IsMap())
			for (int i = 0; i < CTL_COUNT; i++) {
				YAML::Node n = controls[kControlNames[i]];
				if (!n || !n.IsMap())
					continue;
				Controls_Key[i][0] = Clamp(Read(n, "key1", Controls_Key[i][0]), 0, 511);
				Controls_Key[i][1] = Clamp(Read(n, "key2", Controls_Key[i][1]), 0, 511);
				Controls_Pad[i] = Clamp(Read(n, "pad", Controls_Pad[i]), -1, 31);
			}
		YAML::Node player2 = controls ? controls["player2"] : YAML::Node();
		if (player2 && player2.IsMap())
			for (int i = 0; i < CTL_COUNT; i++) {
				YAML::Node n = player2[kControlNames[i]];
				if (!n || !n.IsMap())
					continue;
				Controls_Key2[i][0] = Clamp(Read(n, "key1", Controls_Key2[i][0]), 0, 511);
				Controls_Key2[i][1] = Clamp(Read(n, "key2", Controls_Key2[i][1]), 0, 511);
				Controls_Pad2[i] = Clamp(Read(n, "pad", Controls_Pad2[i]), -1, 31);
			}
	} catch (const YAML::BadFile &) {
		// No settings file yet: the defaults stand (it is written on the first change or on exit).
	} catch (const YAML::Exception &e) {
		fprintf(stderr, "Settings: could not read %s (%s) -- using defaults\n", FilePath().toLocal8Bit().constData(), e.what());
	}

	// Apply the audio part to the live state, so the menus built next show it.
	QtAudio_SetMusicEnabled(g_data.music_enabled);
	QtAudio_SetMusicVolume(g_data.music_volume);
	QtAudio_SetSfxEnabled(g_data.sfx_enabled);
	QtAudio_SetSfxVolume(g_data.sfx_volume);
	for (int i = 0; i < 11; i++)
		Sound_SetChannelMuted(i, g_data.muted_channels.contains(kChannelNames[i]));
}

void Save() {
	if (!g_loaded)
		return; // never overwrite a file we haven't read

	// Audio: capture the live state.
	g_data.music_enabled = QtAudio_MusicEnabled();
	g_data.music_volume = QtAudio_MusicVolume();
	g_data.sfx_enabled = QtAudio_SfxEnabled();
	g_data.sfx_volume = QtAudio_SfxVolume();
	g_data.muted_channels.clear();
	for (int i = 0; i < 11; i++)
		if (Sound_IsChannelMuted(i))
			g_data.muted_channels << kChannelNames[i];

	YAML::Emitter out;
	out << YAML::Comment(std::string(game_info.settings_title) + " (YAML). Edited by the game; safe to edit by hand while it is not running.");
	out << YAML::BeginMap;
	out << YAML::Key << "version" << YAML::Value << kVersion;

	out << YAML::Key << "audio" << YAML::Value << YAML::BeginMap;
	out << YAML::Key << "music" << YAML::Value << YAML::Flow << YAML::BeginMap
	    << YAML::Key << "enabled" << YAML::Value << g_data.music_enabled
	    << YAML::Key << "volume" << YAML::Value << g_data.music_volume << YAML::EndMap;
	out << YAML::Key << "sfx" << YAML::Value << YAML::Flow << YAML::BeginMap
	    << YAML::Key << "enabled" << YAML::Value << g_data.sfx_enabled
	    << YAML::Key << "volume" << YAML::Value << g_data.sfx_volume << YAML::EndMap;
	out << YAML::Key << "muted_channels" << YAML::Value << YAML::Flow << YAML::BeginSeq;
	for (const QString &c : g_data.muted_channels)
		out << c.toStdString();
	out << YAML::EndSeq << YAML::EndMap;

	out << YAML::Key << "video" << YAML::Value << YAML::BeginMap;
	out << YAML::Key << "fullscreen" << YAML::Value << g_data.fullscreen;
	out << YAML::Key << "resolution" << YAML::Value << g_data.resolution;
	if (g_data.has_window_geometry)
		out << YAML::Key << "window" << YAML::Value << YAML::Flow << YAML::BeginMap
		    << YAML::Key << "x" << YAML::Value << g_data.window_x
		    << YAML::Key << "y" << YAML::Value << g_data.window_y
		    << YAML::Key << "width" << YAML::Value << g_data.window_width
		    << YAML::Key << "height" << YAML::Value << g_data.window_height << YAML::EndMap;
	out << YAML::EndMap;

	out << YAML::Key << "debug" << YAML::Value << YAML::BeginMap
	    << YAML::Key << "log_file" << YAML::Value << g_data.log_file.toStdString() << YAML::EndMap;
	out << YAML::Key << "demo" << YAML::Value << YAML::BeginMap
	    << YAML::Key << "folder" << YAML::Value << g_data.demo_dir.toStdString()
	    << YAML::Key << "last_zone" << YAML::Value << g_data.demo_zone
	    << YAML::Key << "last_act" << YAML::Value << g_data.demo_act << YAML::EndMap;
	out << YAML::Key << "controls" << YAML::Value << YAML::BeginMap;
	out << YAML::Key << "deadzone_left" << YAML::Value << Controls_DeadzoneLeft;
	out << YAML::Key << "deadzone_right" << YAML::Value << Controls_DeadzoneRight;
	out << YAML::Key << "deadzone_ring" << YAML::Value << Controls_DeadzoneRing;
	out << YAML::Key << "deadzone_left2" << YAML::Value << Controls_DeadzoneLeft2;
	out << YAML::Key << "deadzone_ring2" << YAML::Value << Controls_DeadzoneRing2;
	for (int pl = 0; pl < 2; pl++) { // which gamepad is whose (empty: automatic)
		out << YAML::Key << (pl ? "pad_guid2" : "pad_guid1") << YAML::Value << std::string(Controls_PadGuid[pl]);
		out << YAML::Key << (pl ? "pad_nth2" : "pad_nth1") << YAML::Value << Controls_PadNth[pl];
	}
	for (int i = 0; i < CTL_COUNT; i++)
		out << YAML::Key << kControlNames[i] << YAML::Value << YAML::Flow << YAML::BeginMap
		    << YAML::Key << "key1" << YAML::Value << Controls_Key[i][0]
		    << YAML::Key << "key2" << YAML::Value << Controls_Key[i][1]
		    << YAML::Key << "pad" << YAML::Value << Controls_Pad[i] << YAML::EndMap;
	out << YAML::Key << "player2" << YAML::Value << YAML::BeginMap; // (the second player's: a game with a two-player mode)
	for (int i = 0; i < CTL_COUNT; i++)
		out << YAML::Key << kControlNames[i] << YAML::Value << YAML::Flow << YAML::BeginMap
		    << YAML::Key << "key1" << YAML::Value << Controls_Key2[i][0]
		    << YAML::Key << "key2" << YAML::Value << Controls_Key2[i][1]
		    << YAML::Key << "pad" << YAML::Value << Controls_Pad2[i] << YAML::EndMap;
	out << YAML::EndMap;
	out << YAML::EndMap;
	out << YAML::EndMap;

	QDir().mkpath(DataDir());
	QSaveFile file(FilePath()); // written to a temporary file and renamed: a crash can't leave half a file
	if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
		fprintf(stderr, "Settings: could not write %s\n", FilePath().toLocal8Bit().constData());
		return;
	}
	file.write(out.c_str());
	file.write("\n");
	file.commit();
}

void SaveSoon() {
	static QTimer *timer = nullptr;
	if (timer == nullptr) {
		timer = new QTimer;
		timer->setSingleShot(true);
		timer->setInterval(500);
		QObject::connect(timer, &QTimer::timeout, [] { Save(); });
	}
	timer->start(); // restarts the countdown if it is already running
}

} // namespace Settings
