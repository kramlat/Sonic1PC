// JsonABTest: renders the same song through BOTH the byte-VM (compiled via
// libparadoxsmps from the SAME source text, in-process) and the JSON-tree-
// walking engine, and diffs the resulting PCM sample-for-sample --
// verification aid for the "SMPS runtime: walk JSON directly" plan's own
// staged rollout. Doesn't need a sound_table ID at all -- compiles the
// given .jsonc directly, so it works uniformly for any file (music or SFX,
// autodetected from its own header macros).
// Usage: JsonABTest <song.jsonc> [seconds]
#include "../src/Sound.h"
#include "../libparadoxsmps/compiler.h"
#include "../libparadoxsmps/json.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SAMPLE_RATE 44100

static int32_t *RenderFrames(int frames, uint32_t samples_per_frame) {
    int32_t *out = calloc((size_t)frames * 2 * samples_per_frame, sizeof(int32_t));
    int32_t *mix = calloc(2 * (size_t)samples_per_frame, sizeof(int32_t));
    for (int i = 0; i < frames; i++) {
        Sound_Frame();
        for (uint32_t k = 0; k < 2 * samples_per_frame; k++)
            mix[k] = 0;
        Sound_Generate(mix, samples_per_frame, SAMPLE_RATE);
        for (uint32_t k = 0; k < 2 * samples_per_frame; k++) {
            int32_t s = mix[k];
            if (s > 32767) s = 32767;
            if (s < -32768) s = -32768;
            out[(size_t)i * 2 * samples_per_frame + k] = s;
        }
    }
    free(mix);
    return out;
}

static int is_sfx_song(const PJValue *song) {
    const PJValue *header = pj_object_get(song, "header");
    for (size_t i = 0; i < pj_array_size(header); i++) {
        const char *macro = pj_object_first_key(pj_array_get(header, i));
        if (macro && strcmp(macro, "smpsHeaderTempoSFX") == 0)
            return 1;
    }
    return 0;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: JsonABTest <song.jsonc> [seconds]\n");
        return 1;
    }
    const char *jsonc_path = argv[1];
    double seconds = (argc > 2) ? atof(argv[2]) : 3.0;

    FILE *f = fopen(jsonc_path, "rb");
    if (!f) {
        perror(jsonc_path);
        return 1;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *text = malloc((size_t)size + 1);
    if (fread(text, 1, (size_t)size, f) != (size_t)size) {
        fprintf(stderr, "read error\n");
        return 1;
    }
    text[size] = '\0';
    fclose(f);

    char err[256] = {0};
    PJValue *song = pj_parse(text, err, sizeof(err));
    if (!song) {
        fprintf(stderr, "parse error: %s\n", err);
        return 1;
    }
    int is_sfx = is_sfx_song(song);
    if (!pj_object_get(song, "SMPSplaylist")) {
        fprintf(stderr, "SKIP (no SMPSplaylist -- not a compilable song, e.g. a voice bank)\n");
        return 3;
    }

    PSCompileResult compiled = ps_compile_song(song);
    if (!compiled.success) {
        fprintf(stderr, "compile error: %s\n", compiled.error_message);
        return 1;
    }

    uint32_t samples_per_frame = SAMPLE_RATE / 60;
    int frames = (int)(seconds * 60.0);

    Sound_Init();
    Sound_DebugPlayRawSong(compiled.bytes, (uint8_t)is_sfx, 0);
    int32_t *byte_out = RenderFrames(frames, samples_per_frame);
    ps_compile_result_free(&compiled);

    Sound_Init();
    if (!Sound_DebugPlayRawSongJSON(text, (uint8_t)is_sfx, 0)) {
        fprintf(stderr, "JSON parse/load failed\n");
        return 1;
    }
    int32_t *json_out = RenderFrames(frames, samples_per_frame);

    size_t total = (size_t)frames * 2 * samples_per_frame;
    size_t first_diff = (size_t)-1;
    size_t diff_count = 0;
    int64_t max_abs_diff = 0;
    for (size_t i = 0; i < total; i++) {
        int32_t d = byte_out[i] - json_out[i];
        if (d != 0) {
            if (first_diff == (size_t)-1)
                first_diff = i;
            diff_count++;
            int64_t ad = d < 0 ? -d : d;
            if (ad > max_abs_diff)
                max_abs_diff = ad;
        }
    }

    printf("%s: total=%zu diff=%zu (%.4f%%) max_abs_diff=%lld", jsonc_path, total, diff_count,
           100.0 * (double)diff_count / (double)total, (long long)max_abs_diff);
    if (first_diff != (size_t)-1)
        printf(" first_diff_frame=%zu\n", first_diff / (2 * samples_per_frame));
    else
        printf(" IDENTICAL\n");

    pj_free(song);
    free(text);
    free(byte_out);
    free(json_out);
    return diff_count == 0 ? 0 : 2;
}
