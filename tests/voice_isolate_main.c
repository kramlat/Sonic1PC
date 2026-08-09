// SonicVoiceIsolate: plays a real song at real speed/timing through the
// actual gameplay driver (PlayMusic/Sound_Frame/Sound_Generate), but mutes
// every FM channel except the one currently loaded with a chosen voice
// index (and mutes PSG/DAC entirely) -- so you can hear exactly one voice
// in its real musical context, in isolation, and tell which voice index in
// the bank is "the correct one" by ear.
//
// Usage: SonicVoiceIsolate [song_id_hex]
// While running, type a voice index (decimal or 0xHEX) and press Enter to
// switch which voice is isolated; type "a" to hear everything (isolation
// off); "q" to quit.

#include "../src/Sound.h"

#include <SDL.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>

#define SAMPLE_RATE 44100
#define FRAME_HZ    60

int main(int argc, char **argv) {
    uint8_t song_id = (argc > 1) ? (uint8_t)strtol(argv[1], NULL, 16) : 0x81;

    if (SDL_Init(SDL_INIT_AUDIO) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }

    SDL_AudioSpec want, have;
    SDL_zero(want);
    want.freq = SAMPLE_RATE;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = 1024;
    SDL_AudioDeviceID dev = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
    if (dev == 0) {
        fprintf(stderr, "SDL_OpenAudioDevice: %s\n", SDL_GetError());
        return 1;
    }
    SDL_PauseAudioDevice(dev, 0);

    Sound_Init();
    PlayMusic(song_id);

    int voice_index = -1; // start with everything audible until the user picks one
    Sound_DebugIsolateVoice(voice_index);

    printf("Playing song $%02X. Type a voice index (decimal or 0xHEX) + Enter to isolate it,\n", song_id);
    printf("\"a\" + Enter to hear everything again, \"q\" + Enter to quit.\n");
    printf("> ");
    fflush(stdout);

    uint32_t samples_per_frame = SAMPLE_RATE / FRAME_HZ;
    int32_t *mix = calloc(2 * (size_t)samples_per_frame, sizeof(int32_t));
    int16_t *out = calloc(2 * (size_t)samples_per_frame, sizeof(int16_t));

    int running = 1;
    while (running) {
        // Non-blocking-ish stdin read: SDL_GetTicks-paced frame loop, check
        // for a full line each frame via select-free polling isn't
        // available portably here, so just do a blocking readline in a
        // separate rhythm -- simplest robust approach: peek with a short
        // poll via fgets is blocking, so instead check stdin readiness with
        // a plain non-blocking flag set once at startup.
        static char line[64];
        fd_set fds;
        struct timeval tv = {0, 0};
        FD_ZERO(&fds);
        FD_SET(fileno(stdin), &fds);
        if (select(fileno(stdin) + 1, &fds, NULL, NULL, &tv) > 0) {
            if (fgets(line, sizeof(line), stdin)) {
                size_t len = strlen(line);
                while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
                    line[--len] = '\0';
                if (len == 0) {
                    // just Enter -- ignore
                } else if (line[0] == 'q' || line[0] == 'Q') {
                    running = 0;
                } else if (line[0] == 'a' || line[0] == 'A') {
                    voice_index = -1;
                    Sound_DebugIsolateVoice(voice_index);
                    printf("-- isolation off, everything audible\n> ");
                    fflush(stdout);
                } else {
                    voice_index = (int)strtol(line, NULL, 0);
                    Sound_DebugIsolateVoice(voice_index);
                    printf("-- isolating voice %d (0x%02X)\n> ", voice_index, voice_index);
                    fflush(stdout);
                }
            }
        }

        Sound_Frame();
        memset(mix, 0, 2 * (size_t)samples_per_frame * sizeof(int32_t));
        Sound_Generate(mix, samples_per_frame, SAMPLE_RATE);
        for (uint32_t i = 0; i < 2 * samples_per_frame; i++) {
            int32_t s = mix[i];
            if (s > 32767)
                s = 32767;
            else if (s < -32768)
                s = -32768;
            out[i] = (int16_t)s;
        }
        if (SDL_GetQueuedAudioSize(dev) <= 2 * samples_per_frame * sizeof(int16_t) * 4)
            SDL_QueueAudio(dev, out, 2 * samples_per_frame * sizeof(int16_t));

        SDL_Delay(1000 / FRAME_HZ);
    }

    free(mix);
    free(out);
    SDL_CloseAudioDevice(dev);
    SDL_Quit();
    return 0;
}
