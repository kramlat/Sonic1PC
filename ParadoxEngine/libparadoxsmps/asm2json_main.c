// asm2json: converts a real SMPS2ASM-format .asm song/SFX file to ParadoxSMPS
// JSONC via libparadoxsmps (converter.c) -- same CLI shape and stdout/stderr
// behavior as tools/paradoxsmps/asm_to_json.py, for direct byte/text-level
// cross-checking against the original migration script's own output.
//
// Usage: asm2json input.asm > output.jsonc

#include "converter.h"
#include "json.h"

#include <stdio.h>
#include <stdlib.h>

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "asm2json: cannot open '%s': ", path);
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
        fprintf(stderr, "asm2json: read error on '%s'\n", path);
        return NULL;
    }
    buf[size] = '\0';
    fclose(f);
    return buf;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: asm2json input.asm > output.jsonc\n");
        return 1;
    }
    char *text = read_file(argv[1]);
    if (!text) return 1;

    PSConvertResult result = ps_convert_asm(text);
    free(text);
    if (!result.success) {
        fprintf(stderr, "asm2json: convert error: %s\n", result.error_message);
        return 1;
    }

    char *out = pj_serialize(result.result, true);
    fputs(out, stdout);
    fputc('\n', stdout);
    free(out);

    ps_convert_result_free(&result);
    return 0;
}
