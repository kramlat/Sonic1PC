// jsonc2rawh: embeds a ParadoxSMPS .jsonc song/SFX file's own SOURCE TEXT
// (not compiled bytes) into a C header, as a NUL-terminated byte array
// (same "0x.., 0x.., ..." shape as jsonc2h's compiled-bytes output) -- for
// the JSON tree-walking runtime engine (Sound.c's TickChannelJSON), which
// parses and plays a song's PJValue tree directly rather than a compiled
// byte stream (see the "SMPS runtime: walk JSON directly" plan). A byte
// array rather than a string literal specifically to avoid ISO C99's
// 4095-char string-literal length limit (real .jsonc files comfortably
// exceed it once concatenated -- GCC warns -Woverlength-strings even
// though it doesn't enforce the limit itself). Same CLI shape as jsonc2h
// (input.jsonc output.h ArrayName), and parses the input once up front
// purely to fail fast on a malformed .jsonc at build time rather than at
// runtime -- the parsed tree itself isn't used for output, only the
// original source text is (pj_parse's own JSONC-comment-stripping runs
// again at load time in-game, so nothing here needs to pre-strip it).

#include "json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "jsonc2rawh: cannot open '%s': ", path);
        perror("");
        return NULL;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = (char *)malloc((size_t)size + 1);
    if (fread(buf, 1, (size_t)size, f) != (size_t)size) {
        fclose(f);
        free(buf);
        fprintf(stderr, "jsonc2rawh: read error on '%s'\n", path);
        return NULL;
    }
    buf[size] = '\0';
    fclose(f);
    return buf;
}

static void emit_byte_array(FILE *out, const char *text, size_t len) {
    for (size_t i = 0; i < len; i += 16) {
        fprintf(out, "    ");
        for (size_t j = i; j < i + 16 && j < len; j++)
            fprintf(out, "0x%02X, ", (unsigned char)text[j]);
        fprintf(out, "\n");
    }
    fprintf(out, "    0x00,\n"); // NUL terminator -- pj_parse takes a plain C string
}

int main(int argc, char **argv) {
    if (argc != 4) {
        fprintf(stderr, "Usage: jsonc2rawh input.jsonc output.h ArrayName\n");
        return 1;
    }
    const char *in_path = argv[1];
    const char *out_path = argv[2];
    const char *array_name = argv[3];

    char *text = read_file(in_path);
    if (!text)
        return 1;

    char err[256] = {0};
    PJValue *song = pj_parse(text, err, sizeof(err));
    if (!song) {
        fprintf(stderr, "jsonc2rawh: parse error in '%s': %s\n", in_path, err);
        free(text);
        return 1;
    }
    pj_free(song); // only parsed to fail fast at build time -- see the file's own top comment

    FILE *out = fopen(out_path, "w");
    if (!out) {
        fprintf(stderr, "jsonc2rawh: cannot open '%s' for writing: ", out_path);
        perror("");
        free(text);
        return 1;
    }
    fprintf(out, "#pragma once\n\nconst char %s_json[] = {\n", array_name);
    emit_byte_array(out, text, strlen(text));
    fprintf(out, "};\n");
    fclose(out);

    fprintf(stderr, "Wrote %zu bytes to %s\n", strlen(text), out_path);
    free(text);
    return 0;
}
