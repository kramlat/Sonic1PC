#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QObject>
#include <QTimer>
#include <QVector>

#include <cstdint>

extern "C" {
typedef struct YM2612 YM2612;
}
#include "../../src/Backend/SN76489.h" // plain POD struct + extern "C" functions (see that header's own C++ guard) -- safe to include directly, unlike YM2612's opaque-pointer pattern

// Two playback modes, mutually exclusive (only one is ever "live" at a
// time, matching the single SDL audio device / single QTimer this drives):
//
//  - Voice preview: hold one note on one voice, direct register writes to
//    either a standalone YM2612* (FM voices, Backend/YM2612.h -- same
//    backend the real game uses, exactly like SonicVoiceEditor did) or the
//    embedded SN76489 (PSG tones/noise, Backend/SN76489.h). For auditioning
//    a voice/tone being edited, or play-testing a track, independent of
//    any full song.
//  - Song playback: drives the REAL sequencer (SonicCore's Sound.c) via
//    Sound_DebugPlayRawSong on a just-compiled byte buffer, so what you
//    hear is the actual game's playback path, not a reimplementation.
class AudioEngine : public QObject {
    Q_OBJECT
public:
    explicit AudioEngine(QObject *parent = nullptr);
    ~AudioEngine() override;

    bool init(); // opens the SDL audio device; call once at startup

    // Voice preview. `voice` is one entry from SongDocument::voices() (the
    // schema's own shape: smpsVcAlgorithm/Feedback/UnusedBits + operators[]).
    // channelVolume defaults to 0 (full carrier level) -- matching a synth
    // plugin's own piano-key audition -- but block test-play (PianoRollPanel's
    // Play button) passes the block's real channel attenuation (its home
    // header entry's own volume arg) so play-testing a track sounds like it
    // actually will in-song, not just the raw voice in isolation.
    void previewVoice(const QJsonObject &voice, int noteIndex, int channelVolume = 0);
    // PSG tone/noise preview -- same "hold one note" model as previewVoice,
    // for a PSG channel instead of FM. toneIndex is the volume-envelope
    // selector (SongDocument's own fTone_NN convention, 1-9; 0 = no
    // envelope, flat attenuation). noise selects periodic-noise mode
    // instead of the tone channel (per your direction: an $F3-redirected
    // channel plays periodic noise here). attenuation is the channel's
    // base volume (envelope deltas add to this each tick, same as Sound.c's
    // own ch->volume/PSGStepEnvelope).
    void previewPsgTone(int noteIndex, int toneIndex, bool noise, int attenuation);
    // DAC-home block sample preview (per your direction: "actually play
    // the samples"). row matches SongDocument's own DAC row mapping:
    // 0=Kick, 1=Snare, 2=Timpani, 7-10=Hi/Mid/Low/VLow Timpani. A one-shot,
    // unlike previewVoice/previewPsgTone -- it plays through and stops on
    // its own (Sound_DebugIsDacPreviewPlaying(), polled from tick()), so
    // there's no matching stop call. Silences whichever FM/PSG preview was
    // active first, same exclusivity as switching between those two.
    void previewDacSample(int row);
    void stopPreview(); // stops whichever held preview (FM or PSG) is active
    bool isPreviewing() const { return m_previewing; }

    // Full-song playback through SonicCore's real sequencer.
    void playSong(const QByteArray &compiledBytes, bool isSfx);
    void stopSong();
    bool isPlayingSong() const { return m_playingSong; }

    // Debug/tooling passthrough for the Channels panel's mute/solo
    // checkboxes -- see Sound_DebugSetChannelMuted's own comment for
    // channel_index's meaning (PSG 0-3, then FM/DAC).
    void setChannelMuted(int channelIndex, bool muted);

signals:
    // Emitted once per 60Hz tick while a song is playing -- frameCount
    // resets to 0 on playSong(). Used by the piano roll's playhead (an
    // approximate visual sync, not a claim of frame-exact alignment with
    // the real driver's own tempo governor -- see PianoRollWidgets.cpp's
    // own comment on this).
    void frameTicked(quint64 frameCount);

    // The exact interleaved-stereo S16 buffer just queued to the audio
    // device, once per tick, whenever something is actually playing
    // (song or voice preview) -- feeds the toolbar's waveform/VU display.
    void samplesGenerated(const QVector<qint16> &interleavedStereo);

    // Per-channel activity, once per tick during song playback -- feeds the
    // Channels panel's per-strip VU bars. Approximate (key_on gate +
    // channel volume, not a true instantaneous waveform amplitude reading
    // -- getting real per-channel samples would need restructuring the FM
    // backend's mixing step, out of scope here). fmLevelsL/R sized
    // SOUND_CHANNELS_FM (7, PSG_BASE-relative), psgLevels sized
    // SOUND_CHANNELS_PSG (4). L/R split for FM/DAC only, per real
    // hardware's own pan bits (ams_fms_pan) -- PSG has no pan control in
    // this engine, hence one level per PSG channel, not a pair.
    void channelActivityUpdated(const QVector<float> &fmLevelsL, const QVector<float> &fmLevelsR,
                                 const QVector<float> &psgLevels);

private slots:
    void tick(); // 60Hz, generates one frame's audio for whichever mode is active

private:
    void sendVoiceRegisters(const QJsonObject &voice, int channelVolume);
    void stepPsgEnvelope(); // one envelope tick for the active PSG preview note, see Sound.c's own PSGStepEnvelope

    YM2612 *m_fm = nullptr;
    SN76489 m_psg;
    unsigned int m_audioDevice = 0;
    QTimer m_timer;

    bool m_previewing = false;
    bool m_previewIsPsg = false;
    // PSG preview envelope state -- mirrors the relevant subset of Sound.c's
    // SoundChannel fields for the one note this preview holds at a time.
    int m_psgToneIndex = 0;
    int m_psgVolEnvIndex = 0;
    int m_psgBaseVolume = 0;
    bool m_psgNoise = false;

    bool m_dacPreviewActive = false; // a one-shot DAC sample is currently playing (see previewDacSample())

    bool m_playingSong = false;
    bool m_songIsSfx = false;
    QByteArray m_songBytes; // kept alive for the duration of playback -- Sound.c only stores the pointer
    quint64 m_songFrameCount = 0;

    float m_outputGain = 4.0f;
};
