#include "Audio.h"

#include "SDL.h"

#include "../../Sound.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define AUDIO_SAMPLE_RATE 44100
#define AUDIO_FRAME_HZ    60

static SDL_AudioDeviceID device;
static int32_t *mix_buffer;
static int16_t *out_buffer;
static uint32_t samples_per_frame;

void Audio_Init(void) {
    SDL_AudioSpec want, have;
    SDL_zero(want);
    want.freq = AUDIO_SAMPLE_RATE;
    want.format = AUDIO_S16SYS;
    want.channels = 1;
    want.samples = 1024;
    want.callback = NULL; // Passive -- see Audio.h

    device = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (device == 0) {
        printf("Audio_Init: %s\n", SDL_GetError());
        return;
    }

    samples_per_frame = (uint32_t)have.freq / AUDIO_FRAME_HZ;
    mix_buffer = calloc(samples_per_frame, sizeof(int32_t));
    out_buffer = calloc(samples_per_frame, sizeof(int16_t));

    Sound_Init();
    SDL_PauseAudioDevice(device, 0);
}

void Audio_Update(void) {
    if (!device)
        return;

    memset(mix_buffer, 0, samples_per_frame * sizeof(int32_t));
    Sound_Generate(mix_buffer, samples_per_frame, AUDIO_SAMPLE_RATE);

    for (uint32_t i = 0; i < samples_per_frame; i++) {
        int32_t s = mix_buffer[i];
        if (s > 32767)
            s = 32767;
        else if (s < -32768)
            s = -32768;
        out_buffer[i] = (int16_t)s;
    }

    // Don't let a stall (e.g. breakpoint, slow frame) build up an
    // ever-growing backlog of queued audio -- cap it at a few frames' worth
    // and drop the rest rather than drifting further out of sync.
    if (SDL_GetQueuedAudioSize(device) > samples_per_frame * sizeof(int16_t) * 4)
        SDL_ClearQueuedAudio(device);

    SDL_QueueAudio(device, out_buffer, samples_per_frame * sizeof(int16_t));
}

void Audio_Quit(void) {
    if (device)
        SDL_CloseAudioDevice(device);
    free(mix_buffer);
    free(out_buffer);
    mix_buffer = NULL;
    out_buffer = NULL;
}
