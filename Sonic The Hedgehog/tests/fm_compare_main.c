// FMCompare: plays a song through the game's sound driver and compares the FM output of the game's
// YM2612 core (fmcore) with Nuked-OPN2 (a cycle-accurate YM2612), fed the exact same register writes on
// the same timeline. Run per FM channel (SONIC_FM_CHANNEL solos one) to see where fmcore deviates:
// loudness, pitch (zero crossings), and spectral brightness.
//
// Usage: FMCompare <song id hex> [frames] [out prefix]
//   writes <prefix>-fmcore.wav and <prefix>-nuked.wav (FM only, mono-summed stereo) and prints per-channel stats.

#include "Sound.h"
#include "Backend/YM2612.h"
#include "ym3438.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RATE 44100
#define FRAME_SAMPLES (RATE / 60)
#define NUKED_RATE (7670454.0 / 144.0)

typedef struct {
    uint32_t offset[4096];
    uint8_t data[4096];
    int n;
} WriteLog;

static void Hook(void *ctx, uint32_t offset, uint8_t data) {
    WriteLog *log = (WriteLog *)ctx;
    if (log->n < 4096) {
        log->offset[log->n] = offset;
        log->data[log->n] = data;
        log->n++;
    }
}

static void WriteWav(const char *path, const float *mono, int n) {
    FILE *f = fopen(path, "wb");
    if (!f)
        return;
    int bytes = n * 2;
    uint32_t v32;
    uint16_t v16;
    fwrite("RIFF", 1, 4, f); v32 = 36 + bytes; fwrite(&v32, 4, 1, f);
    fwrite("WAVEfmt ", 1, 8, f); v32 = 16; fwrite(&v32, 4, 1, f);
    v16 = 1; fwrite(&v16, 2, 1, f); v16 = 1; fwrite(&v16, 2, 1, f);
    v32 = RATE; fwrite(&v32, 4, 1, f); v32 = RATE * 2; fwrite(&v32, 4, 1, f);
    v16 = 2; fwrite(&v16, 2, 1, f); v16 = 16; fwrite(&v16, 2, 1, f);
    fwrite("data", 1, 4, f); v32 = bytes; fwrite(&v32, 4, 1, f);
    float peak = 1e-9f;
    for (int i = 0; i < n; i++)
        if (fabsf(mono[i]) > peak) peak = fabsf(mono[i]);
    for (int i = 0; i < n; i++) {
        int16_t s = (int16_t)(mono[i] / peak * 30000.0f);
        fwrite(&s, 2, 1, f);
    }
    fclose(f);
}

// Brightness: share of energy above ~2 kHz (first difference vs signal), and zero-crossing rate (pitch proxy).
static void Stats(const float *x, int n, double *rms, double *zcr, double *bright) {
    double e = 0, d = 0;
    int zc = 0;
    for (int i = 1; i < n; i++) {
        e += x[i] * x[i];
        double df = x[i] - x[i - 1];
        d += df * df;
        if ((x[i] >= 0) != (x[i - 1] >= 0)) zc++;
    }
    *rms = sqrt(e / n);
    *zcr = (double)zc / n * RATE / 2.0;
    *bright = e > 0 ? sqrt(d / e) : 0;
}

int main(int argc, char **argv) {
    int id = argc > 1 ? (int)strtol(argv[1], NULL, 16) : 0x01;
    int frames = argc > 2 ? atoi(argv[2]) : 600;
    const char *prefix = argc > 3 ? argv[3] : "fmcompare";
    const char *solo = getenv("SONIC_FM_CHANNEL");

    Sound_Init();
    memset(sound_music.queue, 0, sizeof(sound_music.queue));
    memset(sound_sfx.queue, 0, sizeof(sound_sfx.queue));

    static WriteLog log;
    YM2612_SetWriteHook(sound_music.fm, Hook, &log);

    static ym3438_t nuked;
    OPN2_SetChipType(ym3438_mode_ym2612);
    OPN2_Reset(&nuked);

    int total = frames * FRAME_SAMPLES;
    float *a = calloc(total, sizeof(float)), *b = calloc(total, sizeof(float));
    static int32_t buf[2 * FRAME_SAMPLES];

    PlayMusic((uint8_t)id);
    for (int f = 0; f < frames; f++) {
        log.n = 0;
        Sound_Frame();

        if (getenv("FMCOMPARE_DUMP") && f == atoi(getenv("FMCOMPARE_DUMP"))) {
            int ch = solo ? atoi(solo) : 0, port = ch / 3, c = ch % 3;
            printf("frame %d FM%d: B0=%02X B4=%02X A4=%02X A0=%02X\n", f, ch + 1, YM2612_PeekReg(sound_music.fm, port, 0xB0 + c),
                   YM2612_PeekReg(sound_music.fm, port, 0xB4 + c), YM2612_PeekReg(sound_music.fm, port, 0xA4 + c), YM2612_PeekReg(sound_music.fm, port, 0xA0 + c));
            const char *names[] = {"DT/MUL", "TL", "RS/AR", "AM/D1R", "D2R", "D1L/RR", "SSG"};
            for (int r = 0; r < 7; r++) {
                printf("  %-7s", names[r]);
                for (int row = 0; row < 4; row++)
                    printf(" %02X", YM2612_PeekReg(sound_music.fm, port, (uint8_t)(0x30 + r * 0x10 + row * 4 + c)));
                printf("   (rows +0 +4 +8 +C)\n");
            }
        }

        // fmcore
        memset(buf, 0, sizeof(buf));
        YM2612_Generate(sound_music.fm, buf, FRAME_SAMPLES, RATE, 7670454);
        for (int i = 0; i < FRAME_SAMPLES; i++)
            a[f * FRAME_SAMPLES + i] = (float)(buf[2 * i] + buf[2 * i + 1]);

        // Nuked: the frame's native-rate samples, with this frame's writes spread over its first samples
        // (one write per native sample = 24 clocks, plus a few spare samples, so the chip's 32-clock
        // busy time after a data write is respected). Output is recorded for every native sample -- the
        // timeline has no gaps -- then resampled to 44.1 kHz by linear interpolation.
        static float native[2048];
        int native_n = (int)(NUKED_RATE / 60.0);
        int w = 0, wait = 0;
        for (int i = 0; i < native_n; i++) {
            if (w < log.n && wait == 0) {
                OPN2_Write(&nuked, log.offset[w], log.data[w]);
                wait = (log.offset[w] & 1) ? 2 : 1; // data writes keep the chip busy for 32 clocks
                w++;
            }
            if (wait > 0)
                wait--;
            long l = 0, r = 0;
            for (int c = 0; c < 24; c++) {
                Bit16s out[2];
                OPN2_Clock(&nuked, out);
                l += out[0];
                r += out[1];
            }
            native[i] = (float)(l + r);
        }
        for (int i = 0; i < FRAME_SAMPLES; i++) {
            double pos = (double)i * native_n / FRAME_SAMPLES;
            int i0 = (int)pos;
            double t = pos - i0;
            float v0 = native[i0], v1 = native[i0 + 1 < native_n ? i0 + 1 : i0];
            b[f * FRAME_SAMPLES + i] = (float)(v0 + (v1 - v0) * t);
        }
    }

    // Remove DC (the real chip's DAC output sits on an offset) with a gentle high-pass on both.
    for (int pass = 0; pass < 2; pass++) {
        float *x = pass ? b : a;
        double prev_in = 0, prev_out = 0;
        for (int i = 0; i < total; i++) {
            double y = x[i] - prev_in + 0.995 * prev_out;
            prev_in = x[i];
            prev_out = y;
            x[i] = (float)y;
        }
    }

    // Raw dump of both signals (float32) for plotting/analysis: <prefix>-fmcore.f32 / -nuked.f32
    {
        char raw[512];
        snprintf(raw, sizeof(raw), "%s-fmcore.f32", prefix);
        FILE *rf = fopen(raw, "wb"); if (rf) { fwrite(a, sizeof(float), total, rf); fclose(rf); }
        snprintf(raw, sizeof(raw), "%s-nuked.f32", prefix);
        rf = fopen(raw, "wb"); if (rf) { fwrite(b, sizeof(float), total, rf); fclose(rf); }
    }

    char path[512];
    snprintf(path, sizeof(path), "%s-fmcore.wav", prefix);
    WriteWav(path, a, total);
    snprintf(path, sizeof(path), "%s-nuked.wav", prefix);
    WriteWav(path, b, total);

    // Normalise each to its own RMS, then compare shape: envelope correlation per frame, pitch, brightness.
    float pa = 0, pb = 0;
    for (int i = 0; i < total; i++) { if (fabsf(a[i]) > pa) pa = fabsf(a[i]); if (fabsf(b[i]) > pb) pb = fabsf(b[i]); }
    fprintf(stderr, "peaks: fmcore %.0f nuked %.0f\n", pa, pb);
    double ra, za, ba, rb, zb, bb;
    Stats(a, total, &ra, &za, &ba);
    Stats(b, total, &rb, &zb, &bb);
    double env_dot = 0, ea = 0, eb = 0;
    for (int f = 0; f < frames; f++) {
        double sa = 0, sb = 0;
        for (int i = 0; i < FRAME_SAMPLES; i++) {
            sa += a[f * FRAME_SAMPLES + i] * a[f * FRAME_SAMPLES + i];
            sb += b[f * FRAME_SAMPLES + i] * b[f * FRAME_SAMPLES + i];
        }
        sa = sqrt(sa / FRAME_SAMPLES) / (ra + 1e-9);
        sb = sqrt(sb / FRAME_SAMPLES) / (rb + 1e-9);
        env_dot += sa * sb; ea += sa * sa; eb += sb * sb;
    }
    printf("song $%02X ch %s: envelope corr %.3f | zero-cross Hz fmcore %.0f nuked %.0f | brightness fmcore %.3f nuked %.3f\n",
           id, solo ? solo : "all", env_dot / sqrt(ea * eb + 1e-12), za, zb, ba, bb);
    return 0;
}
