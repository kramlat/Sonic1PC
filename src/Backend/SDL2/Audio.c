#include "Audio.h"

#include "SDL.h"

#include "../../Sound.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define AUDIO_SAMPLE_RATE 44100
#define AUDIO_FRAME_HZ    60

// A hard clamp on an already-loud mix (e.g. with SONIC_FM_GAIN boosting FM)
// produces audible pops/crackle every time a sample gets flattened to the
// ceiling. Soft-knee into a tanh curve above the threshold instead, so
// peaks compress smoothly rather than slam into a wall.
static int16_t SoftClip(int32_t sample) {
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

static SDL_AudioDeviceID device;
static int32_t *mix_buffer;
static int16_t *out_buffer;
static uint32_t samples_per_frame;

void Audio_Init(void) {
    SDL_AudioSpec want, have;
    SDL_zero(want);
    want.freq = AUDIO_SAMPLE_RATE;
    want.format = AUDIO_S16SYS;
    want.channels = 2; // Stereo -- FM/DAC panning, PSG duplicated equally into both (see Sound_Generate)
    want.samples = 1024;
    want.callback = NULL; // Passive -- see Audio.h

    device = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (device == 0) {
        printf("Audio_Init: %s\n", SDL_GetError());
        return;
    }

    samples_per_frame = (uint32_t)have.freq / AUDIO_FRAME_HZ;
    mix_buffer = calloc(2 * (size_t)samples_per_frame, sizeof(int32_t));
    out_buffer = calloc(2 * (size_t)samples_per_frame, sizeof(int16_t));

    Sound_Init();
    SDL_PauseAudioDevice(device, 0);
}

void Audio_Update(void) {
    if (!device)
        return;

    Sound_Frame();

    memset(mix_buffer, 0, 2 * (size_t)samples_per_frame * sizeof(int32_t));
    Sound_Generate(mix_buffer, samples_per_frame, AUDIO_SAMPLE_RATE);

    for (uint32_t i = 0; i < 2 * samples_per_frame; i++)
        out_buffer[i] = SoftClip(mix_buffer[i]);

    // Don't let a stall (e.g. breakpoint, slow frame) build up an
    // ever-growing backlog of queued audio -- but SDL_ClearQueuedAudio
    // abruptly discards whatever's queued, creating a hard silence-then-jump
    // discontinuity exactly when it fires (audible as a pop/click, and if
    // this trips under routine frame-timing jitter rather than only genuine
    // stalls, that's a recurring artifact, not a rare one). Skip queueing
    // this frame's audio instead when there's already a backlog -- lets it
    // drain naturally with no discontinuity, at the cost of that frame's
    // audio simply not being added (inaudible on its own, unlike a clear).
    if (SDL_GetQueuedAudioSize(device) <= 2 * samples_per_frame * sizeof(int16_t) * 4)
        SDL_QueueAudio(device, out_buffer, 2 * samples_per_frame * sizeof(int16_t));
}

void Audio_Quit(void) {
    if (device)
        SDL_CloseAudioDevice(device);
    free(mix_buffer);
    free(out_buffer);
    mix_buffer = NULL;
    out_buffer = NULL;
}
