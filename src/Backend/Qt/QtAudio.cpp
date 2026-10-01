#include "QtAudio.h"

#include <QAudioDevice>
#include <QAudioFormat>
#include <QAudioSink>
#include <QByteArray>
#include <QIODevice>
#include <QMediaDevices>

#include <algorithm>
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <vector>

// The sound engine (Sound.h is a C-only header, so the few calls used are declared here).
extern "C" {
void Sound_Init(void);
void Sound_Frame(void);
void Sound_GenerateMusic(int32_t *out, uint32_t count, uint32_t sample_rate);
void Sound_GenerateSfx(int32_t *out, uint32_t count, uint32_t sample_rate);
}

namespace {

const int kSampleRate = 44100;
const int kFrameHz = 60;
const int kChannels = 2; // stereo: FM/DAC panning, PSG duplicated equally into both (see Sound_Generate)
const int kBufferFrames = 8; // sink buffer, in game frames (~133 ms)
const int kPrimeFrames = 2;  // silence queued at start so a late frame doesn't underrun

// A hard clamp on an already-loud mix (e.g. with SONIC_FM_GAIN boosting FM) produces audible
// pops/crackle every time a sample gets flattened to the ceiling. Soft-knee into a tanh curve
// above the threshold instead, so peaks compress smoothly rather than slam into a wall.
int16_t SoftClip(int32_t sample) {
	const double threshold = 28000.0;
	double x = (double)sample;
	if (x > threshold) {
		double range = 32767.0 - threshold;
		x = threshold + range * tanh((x - threshold) / range);
	} else if (x < -threshold) {
		double range = 32768.0 - threshold;
		x = -threshold + range * tanh((x + threshold) / range);
	}
	if (x > 32767.0)
		x = 32767.0;
	if (x < -32768.0)
		x = -32768.0;
	return (int16_t)x;
}

// One output: a chip-set generator feeding a sink, with its own mute and volume.
struct Output {
	void (*generate)(int32_t *, uint32_t, uint32_t);
	QAudioSink *sink = nullptr;
	QIODevice *device = nullptr;
	bool enabled = true;
	int volume = 100;
};

Output g_music = {Sound_GenerateMusic};
Output g_sfx = {Sound_GenerateSfx};
uint32_t g_samples_per_frame = kSampleRate / kFrameHz;
std::vector<int32_t> g_mix;
std::vector<int16_t> g_out;
bool g_sinks_tried = false;

// Sinks are created on the first frame, not in Audio_Init: that runs before the Qt
// application (created with the window) exists.
void CreateSinks() {
	g_sinks_tried = true;
	QAudioFormat format;
	format.setSampleRate(kSampleRate);
	format.setChannelCount(kChannels);
	format.setSampleFormat(QAudioFormat::Int16);

	QAudioDevice device = QMediaDevices::defaultAudioOutput();
	if (device.isNull() || !device.isFormatSupported(format)) {
		fprintf(stderr, "Audio: no usable audio output (%s) -- running silent\n",
		        device.isNull() ? "no device" : "format not supported");
		return;
	}

	const int frame_bytes = (int)g_samples_per_frame * kChannels * (int)sizeof(int16_t);
	for (Output *o : {&g_music, &g_sfx}) {
		o->sink = new QAudioSink(device, format);
		o->sink->setBufferSize(frame_bytes * kBufferFrames);
		o->device = o->sink->start(); // push mode: we write PCM into this device
		if (o->device != nullptr) {
			QByteArray silence(frame_bytes * kPrimeFrames, 0);
			o->device->write(silence);
		}
	}
}

void Feed(Output &o) {
	const uint32_t n = g_samples_per_frame * kChannels;
	std::fill(g_mix.begin(), g_mix.end(), 0);
	o.generate(g_mix.data(), g_samples_per_frame, kSampleRate); // always: keeps the chips advancing while muted

	for (uint32_t i = 0; i < n; i++)
		g_out[i] = o.enabled ? SoftClip((int32_t)((int64_t)g_mix[i] * o.volume / 100)) : 0;

	if (o.sink == nullptr || o.device == nullptr)
		return;
	// Don't let a stall (breakpoint, slow frame) build an ever-growing backlog -- but discarding
	// queued audio abruptly would pop. Skip adding this frame's audio when there is already a
	// backlog instead; it drains naturally with no discontinuity.
	const qint64 bytes = (qint64)n * sizeof(int16_t);
	if (o.sink->bytesFree() >= bytes)
		o.device->write(reinterpret_cast<const char *>(g_out.data()), bytes);
}

} // namespace

extern "C" {

void Audio_Init(void) {
	g_mix.assign((size_t)g_samples_per_frame * kChannels, 0);
	g_out.assign((size_t)g_samples_per_frame * kChannels, 0);
	Sound_Init();
}

void Audio_Update(void) {
	if (g_mix.empty())
		return;
	if (!g_sinks_tried)
		CreateSinks();

	Sound_Frame();
	Feed(g_music);
	Feed(g_sfx);
}

void Audio_Quit(void) {
	for (Output *o : {&g_music, &g_sfx}) {
		if (o->sink != nullptr) {
			o->sink->stop();
			delete o->sink;
			o->sink = nullptr;
			o->device = nullptr;
		}
	}
}

void QtAudio_SetMusicEnabled(bool enabled) { g_music.enabled = enabled; }
bool QtAudio_MusicEnabled(void) { return g_music.enabled; }
void QtAudio_SetMusicVolume(int percent) { g_music.volume = percent < 0 ? 0 : (percent > 100 ? 100 : percent); }
int QtAudio_MusicVolume(void) { return g_music.volume; }
void QtAudio_SetSfxEnabled(bool enabled) { g_sfx.enabled = enabled; }
bool QtAudio_SfxEnabled(void) { return g_sfx.enabled; }
void QtAudio_SetSfxVolume(int percent) { g_sfx.volume = percent < 0 ? 0 : (percent > 100 ? 100 : percent); }
int QtAudio_SfxVolume(void) { return g_sfx.volume; }

} // extern "C"
