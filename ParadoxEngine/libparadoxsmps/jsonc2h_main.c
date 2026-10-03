// jsonc2h: compiles a ParadoxSMPS .jsonc song/SFX file straight to a C
// header, via libparadoxsmps (json.c + compiler.c) -- the same CLI shape as
// tools/paradoxsmps/json_to_header.py (input.jsonc output.h ArrayName), but
// this is now the ONE canonical compiler: both the real CMake build
// (add_paradoxsmps_song(), CMakeLists.txt) and ParadoxComposer link the
// same library directly, so there is no second hand-ported implementation
// left to drift out of sync.

#include "compiler.h"
#include "json.h"

#include <stdio.h>
#include <stdlib.h>

static char *read_file(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "jsonc2h: cannot open '%s': ", path);
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
        fprintf(stderr, "jsonc2h: read error on '%s'\n", path);
        return NULL;
    }
    buf[size] = '\0';
    fclose(f);
    return buf;
}

int main(int argc, char **argv) {
    if (argc != 4) {
        fprintf(stderr, "Usage: jsonc2h input.jsonc output.h ArrayName\n");
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
    free(text);
    if (!song) {
        fprintf(stderr, "jsonc2h: parse error in '%s': %s\n", in_path, err);
        return 1;
    }

    PSCompileResult result = ps_compile_song(song);
    pj_free(song);
    if (!result.success) {
        fprintf(stderr, "jsonc2h: compile error in '%s': %s\n", in_path, result.error_message);
        return 1;
    }

    FILE *out = fopen(out_path, "w");
    if (!out) {
        fprintf(stderr, "jsonc2h: cannot open '%s' for writing: ", out_path);
        perror("");
        ps_compile_result_free(&result);
        return 1;
    }
    // Matches smps2asmc's own generated-header convention exactly (#pragma
    // once, uint8_t) -- interchangeable at the include site with what the
    // pipeline this replaces used to produce.
    fprintf(out, "#pragma once\n\n#include <stdint.h>\n\nconst uint8_t %s[] = {\n", array_name);
    for (size_t i = 0; i < result.byte_count; i += 16) {
        fprintf(out, "    ");
        for (size_t j = i; j < i + 16 && j < result.byte_count; j++)
            fprintf(out, "0x%02X, ", result.bytes[j]);
        fprintf(out, "\n");
    }
    fprintf(out, "};\n");
    // Sound.c's sound_table_driver_ver[] references this by name (e.g.
    // `[bgm_GHZ] = Mus81_GHZ_DRIVERVER,`) so the JSON's own "driverVersion"
    // field stays the single source of truth all the way to the runtime.
    fprintf(out, "#define %s_DRIVERVER %d\n", array_name, result.driver_version);
    fclose(out);

    fprintf(stderr, "Wrote %zu bytes to %s\n", result.byte_count, out_path);
    ps_compile_result_free(&result);
    return 0;
}
