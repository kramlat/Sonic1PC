// SonicSoundWav: renders a real song/SFX to a raw 16-bit stereo PCM file for
// direct offline inspection (waveform stats, FFT, etc), independent of any
// live audio backend. Usage: SonicSoundWav <sound_id_hex> <seconds> <out.pcm>
#include "../src/Sound.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define SAMPLE_RATE 44100

int main(int argc, char **argv) {
    uint8_t id = (argc > 1) ? (uint8_t)strtol(argv[1], NULL, 16) : 0x81;
    double seconds = (argc > 2) ? atof(argv[2]) : 2.0;
    const char *out_path = (argc > 3) ? argv[3] : "/tmp/out.pcm";

    Sound_Init();
    PlayMusic(id);

    FILE *f = fopen(out_path, "wb");
    if (!f) {
        perror(out_path);
        return 1;
    }

    uint32_t samples_per_frame = SAMPLE_RATE / 60;
    int frames = (int)(seconds * 60.0);
    int32_t *mix = calloc(2 * (size_t)samples_per_frame, sizeof(int32_t));
    int16_t *out = calloc(2 * (size_t)samples_per_frame, sizeof(int16_t));

    for (int i = 0; i < frames; i++) {
        Sound_Frame();
        for (uint32_t k = 0; k < 2 * samples_per_frame; k++)
            mix[k] = 0;
        Sound_Generate(mix, samples_per_frame, SAMPLE_RATE);
        for (uint32_t k = 0; k < 2 * samples_per_frame; k++) {
            int32_t s = mix[k];
            if (s > 32767) s = 32767;
            if (s < -32768) s = -32768;
            out[k] = (int16_t)s;
        }
        fwrite(out, sizeof(int16_t), 2 * samples_per_frame, f);
    }

    fclose(f);
    free(mix);
    free(out);
    return 0;
}
