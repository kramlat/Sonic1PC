#include "AudioEngine.h"
#include "SongDocument.h"

#include <SDL.h>

extern "C" {
#include "../../src/Backend/YM2612.h"
#include "../../src/Sound.h"
#include "../../src/Resource/PSG/psg1.h"
#include "../../src/Resource/PSG/psg2.h"
#include "../../src/Resource/PSG/psg3.h"
#include "../../src/Resource/PSG/psg4.h"
#include "../../src/Resource/PSG/psg5.h"
#include "../../src/Resource/PSG/psg6.h"
#include "../../src/Resource/PSG/psg7.h"
#include "../../src/Resource/PSG/psg8.h"
#include "../../src/Resource/PSG/psg9.h"
}

#include <cmath>
#include <vector>

#define SAMPLE_RATE 44100
#define FM_CLOCK    (3579545 * 15 / 7) // matches Sound.c's SOUND_FM_CLOCK
#define PSG_CLOCK   3579545            // matches Sound.h's SOUND_PSG_CLOCK

// Same soft-knee limiter as Audio.c's SoftClip / voice_editor_main.cpp's own
// copy -- a hard clamp on a boosted signal pops/crackles at the ceiling.
static int16_t SoftClip(int32_t sample) {
    const double threshold = 28000.0;
    double x = (double)sample;
    if (x > threshold) {
        double range = 32767.0 - threshold;
        x = threshold + range * std::tanh((x - threshold) / range);
    } else if (x < -threshold) {
        double range = 32768.0 - threshold;
        x = -threshold + range * std::tanh((x + threshold) / range);
    }
    if (x > 32767.0)
        x = 32767.0;
    if (x < -32768.0)
        x = -32768.0;
    return (int16_t)x;
}

static const uint16_t FM_FNUM_TABLE[12] = {0x25E, 0x284, 0x2AB, 0x2D3, 0x2FE, 0x32D,
                                            0x35C, 0x38F, 0x3C5, 0x3FF, 0x43C, 0x47C};

// Same FM_SLOT_MASK/carrier logic as Sound.c's FM_IsCarrier -- only carrier
// operators get channel volume added to TL (matches real hardware: channel
// volume is a modulator-independent attenuation applied at the output stage).
static const uint8_t FM_SLOT_MASK[16] = {8, 8, 8, 8, 0xA, 0xE, 0xE, 0xF, 8, 0xC, 8, 8, 0xA, 0xF, 0xF, 0xF};
static const int FM_SLOT_BIT[4] = {0, 2, 1, 3};
static bool IsCarrier(int algorithm, int op) { return (FM_SLOT_MASK[algorithm & 0xF] >> FM_SLOT_BIT[op]) & 1; }
static int ClampTL(int tl) { return tl < 0 ? 0 : (tl > 127 ? 127 : tl); }

static void FMWrite(YM2612 *fm, uint32_t port, uint8_t reg, uint8_t data) {
    YM2612_Write(fm, port, reg);
    YM2612_Write(fm, port + 1, data);
}

static const int OP_NAMES_COUNT = 4;
static const char *OP_FIELD_NAMES[10] = {
    "smpsVcDetune",  "smpsVcCoarseFreq", "smpsVcRateScale",  "smpsVcAttackRate", "smpsVcAmpMod",
    "smpsVcDecayRate1", "smpsVcDecayRate2", "smpsVcDecayLevel", "smpsVcReleaseRate", "smpsVcTotalLevel",
};

// PSG volume envelopes (fTone_01..fTone_09) and period table -- same data
// and logic as Sound.c's own psg_envelopes/PSGStepEnvelope/PSGPeriodForNote
// (private to that file, so duplicated here for standalone preview, same
// spirit as FM_FNUM_TABLE/ClampTL/IsCarrier above).
struct PSGEnvelope {
    const uint8_t *data;
    uint32_t length;
};
static const PSGEnvelope PSG_ENVELOPES[9] = {
    {PSG_psg1, sizeof(PSG_psg1)}, {PSG_psg2, sizeof(PSG_psg2)}, {PSG_psg3, sizeof(PSG_psg3)},
    {PSG_psg4, sizeof(PSG_psg4)}, {PSG_psg5, sizeof(PSG_psg5)}, {PSG_psg6, sizeof(PSG_psg6)},
    {PSG_psg7, sizeof(PSG_psg7)}, {PSG_psg8, sizeof(PSG_psg8)}, {PSG_psg9, sizeof(PSG_psg9)},
};

static const uint16_t PSG_PERIOD_TABLE[70] = {
    0x356, 0x326, 0x2F9, 0x2CE, 0x2A5, 0x280, 0x25C, 0x23A, 0x21A, 0x1FB, 0x1DF, 0x1C4, 0x1AB, 0x193, 0x17D,
    0x167, 0x153, 0x140, 0x12E, 0x11D, 0x10D, 0x0FE, 0x0EF, 0x0E2, 0x0D6, 0x0C9, 0x0BE, 0x0B4, 0x0A9, 0x0A0,
    0x097, 0x08F, 0x087, 0x07F, 0x078, 0x071, 0x06B, 0x065, 0x05F, 0x05A, 0x055, 0x050, 0x04B, 0x047, 0x043,
    0x040, 0x03C, 0x039, 0x036, 0x033, 0x030, 0x02D, 0x02B, 0x028, 0x026, 0x024, 0x022, 0x020, 0x01F, 0x01D,
    0x01B, 0x01A, 0x018, 0x017, 0x016, 0x015, 0x013, 0x012, 0x011, 0x000};

static uint16_t PSGPeriodForNote(int noteIndex) {
    if (noteIndex < 0)
        noteIndex = 0;
    if (noteIndex >= (int)(sizeof(PSG_PERIOD_TABLE) / sizeof(PSG_PERIOD_TABLE[0])))
        noteIndex = (int)(sizeof(PSG_PERIOD_TABLE) / sizeof(PSG_PERIOD_TABLE[0])) - 1;
    uint16_t period = PSG_PERIOD_TABLE[noteIndex];
    return period < 1 ? 1 : period;
}

static void PSGSetAttenuation(SN76489 *chip, int channel, uint8_t atten4) {
    if (atten4 > 0x0F)
        atten4 = 0x0F;
    SN76489_Write(chip, (uint8_t)(0x80 | (channel << 5) | 0x10 | atten4));
}

static void PSGSetTonePeriod(SN76489 *chip, int channel, uint16_t period10) {
    if (period10 > 0x3FF)
        period10 = 0x3FF;
    SN76489_Write(chip, (uint8_t)(0x80 | (channel << 5) | (period10 & 0x0F)));
    SN76489_Write(chip, (uint8_t)((period10 >> 4) & 0x3F));
}

AudioEngine::AudioEngine(QObject *parent) : QObject(parent) {
    connect(&m_timer, &QTimer::timeout, this, &AudioEngine::tick);
}

AudioEngine::~AudioEngine() {
    if (m_fm)
        YM2612_Destroy(m_fm);
    if (m_audioDevice)
        SDL_CloseAudioDevice(m_audioDevice);
}

bool AudioEngine::init() {
    if (SDL_WasInit(SDL_INIT_AUDIO) == 0) {
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0)
            return false;
    }

    SDL_AudioSpec want, have;
    SDL_zero(want);
    want.freq = SAMPLE_RATE;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 1024;
    m_audioDevice = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
    if (!m_audioDevice)
        return false;
    SDL_PauseAudioDevice(m_audioDevice, 0);

    m_fm = YM2612_Create();
    SN76489_Init(&m_psg);
    Sound_Init();

    m_timer.start(1000 / 60);
    return true;
}

void AudioEngine::sendVoiceRegisters(const QJsonObject &voice, int channelVolume) {
    const int port = 0, ch = 0; // always preview on FM1
    const QJsonArray ops = voice.value("operators").toArray();

    int algorithm = hexFieldToInt(voice.value("smpsVcAlgorithm"));
    int feedback = hexFieldToInt(voice.value("smpsVcFeedback"));
    int unusedBits = hexFieldToInt(voice.value("smpsVcUnusedBits"));

    uint8_t alg_ext_bit = (uint8_t)((algorithm >> 3) & 1);
    uint8_t alg_low3 = (uint8_t)(algorithm & 7);
    FMWrite(m_fm, port, (uint8_t)(0xB0 + ch),
            (uint8_t)((alg_ext_bit << 7) | ((unusedBits & 1) << 6) | ((feedback & 7) << 3) | alg_low3));

    for (int op = 0; op < OP_NAMES_COUNT; op++) {
        QJsonObject o = (op < ops.size()) ? ops.at(op).toObject() : QJsonObject();
        int dt = hexFieldToInt(o.value(OP_FIELD_NAMES[0]));
        int mul = hexFieldToInt(o.value(OP_FIELD_NAMES[1]));
        int rs = hexFieldToInt(o.value(OP_FIELD_NAMES[2]));
        int ar = hexFieldToInt(o.value(OP_FIELD_NAMES[3]));
        int am = hexFieldToInt(o.value(OP_FIELD_NAMES[4]));
        int d1r = hexFieldToInt(o.value(OP_FIELD_NAMES[5]));
        int d2r = hexFieldToInt(o.value(OP_FIELD_NAMES[6]));
        int d1l = hexFieldToInt(o.value(OP_FIELD_NAMES[7]));
        int rr = hexFieldToInt(o.value(OP_FIELD_NAMES[8]));
        int tl = hexFieldToInt(o.value(OP_FIELD_NAMES[9]));

        int slot = op * 4;
        FMWrite(m_fm, port, (uint8_t)(0x30 + slot + ch), (uint8_t)((dt << 4) | (mul & 0xF)));
        FMWrite(m_fm, port, (uint8_t)(0x50 + slot + ch), (uint8_t)((rs << 6) | (ar & 0x1F)));
        FMWrite(m_fm, port, (uint8_t)(0x60 + slot + ch), (uint8_t)((am << 5) | (d1r & 0x1F)));
        FMWrite(m_fm, port, (uint8_t)(0x70 + slot + ch), (uint8_t)(d2r & 0x1F));
        FMWrite(m_fm, port, (uint8_t)(0x80 + slot + ch), (uint8_t)((d1l << 4) | (rr & 0xF)));
        int add = IsCarrier(algorithm, op) ? channelVolume : 0;
        FMWrite(m_fm, port, (uint8_t)(0x40 + slot + ch), (uint8_t)ClampTL(tl + add));
    }
}

void AudioEngine::previewVoice(const QJsonObject &voice, int noteIndex, int channelVolume) {
    if (m_previewIsPsg) {
        // Switching from a PSG preview -- silence both PSG channels it
        // might be using before handing off to FM.
        PSGSetAttenuation(&m_psg, 0, 0x0F);
        PSGSetAttenuation(&m_psg, 3, 0x0F);
        m_previewIsPsg = false;
        m_previewing = false;
    }

    sendVoiceRegisters(voice, channelVolume);

    int shifted = noteIndex + 1;
    int octave = shifted / 12;
    if (octave > 7)
        octave = 7;
    int fnum = FM_FNUM_TABLE[shifted % 12];
    FMWrite(m_fm, 0, (uint8_t)0xA4, (uint8_t)((octave << 3) | (fnum >> 8)));
    FMWrite(m_fm, 0, (uint8_t)0xA0, (uint8_t)(fnum & 0xFF));

    if (!m_previewing)
        FMWrite(m_fm, 0, 0x28, 0xF0); // key-on, chan_code 0 (FM1)
    m_previewing = true;
}

void AudioEngine::previewPsgTone(int noteIndex, int toneIndex, bool noise, int attenuation) {
    if (m_previewing && !m_previewIsPsg)
        FMWrite(m_fm, 0, 0x28, 0x00); // switching from an FM preview -- key it off first

    m_previewIsPsg = true;
    m_previewing = true;

    m_psgToneIndex = toneIndex;
    m_psgVolEnvIndex = 0; // every new note restarts its envelope from the top, matching Sound.c's own note-on
    m_psgBaseVolume = qBound(0, attenuation, 0x0F);
    m_psgNoise = noise;

    const uint16_t period = PSGPeriodForNote(noteIndex);
    if (noise) {
        // Noise "sync" mode reads its period from tone channel 2's own
        // register -- there's no separate noise-pitch register on real
        // hardware (see Sound.c's own TickChannel comment on this same
        // trick). $E3 = periodic (not white) noise, sync rate -- per your
        // direction, an $F3-redirected channel previews as periodic noise.
        PSGSetTonePeriod(&m_psg, 2, period);
        SN76489_Write(&m_psg, 0xE3);
        PSGSetAttenuation(&m_psg, 0, 0x0F); // silence the plain tone channel so it doesn't leak through too
        PSGSetAttenuation(&m_psg, 3, (uint8_t)m_psgBaseVolume);
    } else {
        PSGSetTonePeriod(&m_psg, 0, period);
        PSGSetAttenuation(&m_psg, 3, 0x0F); // silence noise in case a previous preview left it sounding
        PSGSetAttenuation(&m_psg, 0, (uint8_t)m_psgBaseVolume);
    }
}

void AudioEngine::stopPreview() {
    if (m_previewing) {
        if (m_previewIsPsg) {
            PSGSetAttenuation(&m_psg, 0, 0x0F);
            PSGSetAttenuation(&m_psg, 3, 0x0F);
        } else {
            FMWrite(m_fm, 0, 0x28, 0x00); // key-off
        }
    }
    m_previewing = false;
    m_previewIsPsg = false;
}

void AudioEngine::previewDacSample(int row) {
    if (m_previewing) {
        if (m_previewIsPsg) {
            PSGSetAttenuation(&m_psg, 0, 0x0F);
            PSGSetAttenuation(&m_psg, 3, 0x0F);
        } else {
            FMWrite(m_fm, 0, 0x28, 0x00);
        }
        m_previewing = false;
        m_previewIsPsg = false;
    }
    // Row numbering matches SongDocument::dacSampleNamesForBlock's own
    // scheme exactly -- see its comment for the full table. 0-6 are the 7
    // base samples (matches DAC_SAMPLE_* order exactly); 7+ are pitch
    // variants of Timpani/Tom/Bongo.
    if (row >= 0 && row <= 6)
        Sound_DebugPreviewDacSample(row);
    else if (row >= 7 && row <= 10)
        Sound_DebugPreviewDacVariant(DAC_SAMPLE_TIMPANI, row - 7); // 0=Hi,1=Mid,2=Low,3=Floor
    else if (row >= 11 && row <= 13)
        Sound_DebugPreviewDacVariant(DAC_SAMPLE_TOM, row - 11); // 0=Mid,1=Low,2=Floor
    else if (row >= 14 && row <= 16)
        Sound_DebugPreviewDacVariant(DAC_SAMPLE_BONGO, row - 14); // 0=Hi,1=Mid,2=Low
    else
        return; // not a real DAC row -- nothing to trigger
    m_dacPreviewActive = true;
}

void AudioEngine::stepPsgEnvelope() {
    if (m_psgToneIndex <= 0 || m_psgToneIndex > 9)
        return;
    const PSGEnvelope &env = PSG_ENVELOPES[m_psgToneIndex - 1];
    if (m_psgVolEnvIndex >= (int)env.length)
        return;
    int8_t delta = (int8_t)env.data[m_psgVolEnvIndex];
    if ((uint8_t)delta == 0x80)
        return; // terminator -- hold (index does not advance)
    m_psgVolEnvIndex++;
    int vol = m_psgBaseVolume + delta;
    if (vol < 0 || vol > 0x0F)
        vol = 0x0F; // real driver clamps both overflow AND underflow to silence, not to loudest
    PSGSetAttenuation(&m_psg, m_psgNoise ? 3 : 0, (uint8_t)vol);
}

void AudioEngine::playSong(const QByteArray &compiledBytes, bool isSfx) {
    m_songBytes = compiledBytes; // keep the buffer alive -- Sound.c only stores the pointer
    m_songIsSfx = isSfx;
    m_songFrameCount = 0;
    Sound_DebugPlayRawSong(reinterpret_cast<const uint8_t *>(m_songBytes.constData()), isSfx ? 1 : 0);
    m_playingSong = true;
}

void AudioEngine::stopSong() {
    StopAllSound();
    m_playingSong = false;
}

void AudioEngine::setChannelMuted(int channelIndex, bool muted) {
    Sound_DebugSetChannelMuted(channelIndex, muted ? 1 : 0);
}

void AudioEngine::tick() {
    const uint32_t samplesPerFrame = SAMPLE_RATE / 60;
    std::vector<int32_t> mix(2 * samplesPerFrame, 0);
    std::vector<int16_t> out(2 * samplesPerFrame);
    bool producedAnything = false;

    if (m_playingSong) {
        Sound_Frame();
        Sound_Generate(mix.data(), samplesPerFrame, SAMPLE_RATE);
        emit frameTicked(m_songFrameCount++);

        QVector<float> fmL(SOUND_CHANNELS_FM, 0.0f), fmR(SOUND_CHANNELS_FM, 0.0f), psg(SOUND_CHANNELS_PSG, 0.0f);
        for (int i = 0; i < SOUND_CHANNELS_PSG; i++) {
            const SoundChannel &ch = sound_music.channels[i];
            psg[i] = ch.key_on ? qBound(0.0f, 1.0f - ch.volume / 127.0f, 1.0f) : 0.0f;
        }
        for (int i = 0; i < SOUND_CHANNELS_FM; i++) {
            const SoundChannel &ch = sound_music.channels[SOUND_CHANNEL_FM_BASE + i];
            const float level = ch.key_on ? qBound(0.0f, 1.0f - ch.volume / 127.0f, 1.0f) : 0.0f;
            const bool panLeft = (ch.ams_fms_pan & 0x80) != 0;
            const bool panRight = (ch.ams_fms_pan & 0x40) != 0;
            fmL[i] = panLeft ? level : 0.0f;
            fmR[i] = panRight ? level : 0.0f;
        }
        emit channelActivityUpdated(fmL, fmR, psg);
        producedAnything = true;
    } else if (m_previewing && m_previewIsPsg) {
        stepPsgEnvelope();
        std::vector<int32_t> psgBuf(samplesPerFrame, 0); // SN76489_Generate is mono, unlike YM2612_Generate
        SN76489_Generate(&m_psg, psgBuf.data(), samplesPerFrame, SAMPLE_RATE, PSG_CLOCK);
        for (uint32_t i = 0; i < samplesPerFrame; i++) {
            const int32_t s = (int32_t)(psgBuf[i] * m_outputGain);
            mix[2 * i + 0] += s;
            mix[2 * i + 1] += s;
        }
        producedAnything = true;
    } else if (m_previewing) {
        std::vector<int32_t> fmBuf(2 * samplesPerFrame, 0);
        YM2612_Generate(m_fm, fmBuf.data(), samplesPerFrame, SAMPLE_RATE, FM_CLOCK);
        for (uint32_t i = 0; i < 2 * samplesPerFrame; i++)
            mix[i] += (int32_t)(fmBuf[i] * m_outputGain);
        producedAnything = true;
    }

    // DAC sample preview mixes in additively alongside whatever else is
    // happening above (per your direction, it's a one-shot -- see
    // previewDacSample()'s own comment -- not part of the mutually
    // exclusive FM/PSG/song modes), and stops itself once the sample has
    // naturally finished playing.
    if (m_dacPreviewActive) {
        std::vector<int32_t> dacBuf(2 * samplesPerFrame, 0);
        Sound_DebugGenerateDacPreview(dacBuf.data(), samplesPerFrame, SAMPLE_RATE);
        for (uint32_t i = 0; i < 2 * samplesPerFrame; i++)
            mix[i] += dacBuf[i];
        producedAnything = true;
        if (!Sound_DebugIsDacPreviewPlaying())
            m_dacPreviewActive = false;
    }

    if (!producedAnything)
        return; // nothing to play -- don't queue silence forever

    for (uint32_t i = 0; i < 2 * samplesPerFrame; i++)
        out[i] = SoftClip(mix[i]);
    if (SDL_GetQueuedAudioSize(m_audioDevice) < 2 * samplesPerFrame * sizeof(int16_t) * 4)
        SDL_QueueAudio(m_audioDevice, out.data(), (uint32_t)(2 * samplesPerFrame * sizeof(int16_t)));

    emit samplesGenerated(QVector<qint16>(out.begin(), out.end()));
}
