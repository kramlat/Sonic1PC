// smps2asmc: translates a SMPS2ASM-format song/SFX .asm file (SonicDriverVer=1
// only) into a small standalone C program that #includes "smps.h" and calls
// its functions in the same order, with the same arguments, as the original
// file's macro invocations. Building and running that generated program
// produces the final resource header (a "const uint8_t name[] = {...};",
// same shape as every other resource in this project).
//
// This only has to translate *syntax* -- matching macro-call names and
// argument lists to their smps.h equivalents, note names and hex literals
// to C tokens, and label lines to SMPS_Label() calls -- because smps.h/
// smps.c is what actually reimplements each macro's real behavior (this is
// only viable because SMPS2ASM song files are themselves just straight-line
// sequences of macro calls; all the driver-version-conversion logic lives
// in the shared macro package, not in individual songs -- see smps.h's own
// comment for why that's specialized to SonicDriverVer=1/SourceDriver=1).
//
// Usage: smps2asmc <input.asm> <output.c> <array_name> <output.h path>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_LINE 1024
#define MAX_ARGS 8

typedef enum { ARG_NUM, ARG_STR } ArgKind;

typedef struct {
    const char *asm_name;
    const char *c_name;
    int arg_count;
    ArgKind kinds[MAX_ARGS];
} MacroDef;

// clang-format off
static const MacroDef macros[] = {
    {"smpsHeaderStartSong",       "smpsHeaderStartSong",       1, {ARG_NUM}},
    {"smpsHeaderVoice",           "smpsHeaderVoice",           1, {ARG_STR}},
    {"smpsHeaderVoiceNull",       "smpsHeaderVoiceNull",       0, {0}},
    {"smpsHeaderChan",            "smpsHeaderChan",            2, {ARG_NUM, ARG_NUM}},
    {"smpsHeaderTempo",           "smpsHeaderTempo",           2, {ARG_NUM, ARG_NUM}},
    {"smpsHeaderFM",              "smpsHeaderFM",              3, {ARG_STR, ARG_NUM, ARG_NUM}},
    {"smpsHeaderPSG",             "smpsHeaderPSG",             5, {ARG_STR, ARG_NUM, ARG_NUM, ARG_NUM, ARG_NUM}},
    {"smpsHeaderTempoSFX",        "smpsHeaderTempoSFX",        1, {ARG_NUM}},
    {"smpsHeaderChanSFX",         "smpsHeaderChanSFX",         1, {ARG_NUM}},
    {"smpsHeaderSFXChannel",      "smpsHeaderSFXChannel",      4, {ARG_NUM, ARG_STR, ARG_NUM, ARG_NUM}},
    {"smpsPan",                   "smpsPan",                   2, {ARG_NUM, ARG_NUM}},
    {"smpsDetune",                "smpsDetune",                1, {ARG_NUM}},
    {"smpsAlterNote",             "smpsAlterNote",              1, {ARG_NUM}},
    {"smpsNop",                   "smpsNop",                    1, {ARG_NUM}},
    {"smpsReturn",                "smpsReturn",                 0, {0}},
    {"smpsFade",                  "smpsFade",                    0, {0}},
    {"smpsChanTempoDiv",          "smpsChanTempoDiv",             1, {ARG_NUM}},
    {"smpsAlterVol",              "smpsAlterVol",                  1, {ARG_NUM}},
    {"smpsNoteFill",              "smpsNoteFill",                   1, {ARG_NUM}},
    {"smpsChangeTransposition",   "smpsChangeTransposition",         1, {ARG_NUM}},
    {"smpsAlterPitch",            "smpsAlterPitch",                   1, {ARG_NUM}},
    {"smpsSetTempoMod",           "smpsSetTempoMod",                   1, {ARG_NUM}},
    {"smpsSetTempoDiv",           "smpsSetTempoDiv",                    1, {ARG_NUM}},
    {"smpsPSGAlterVol",           "smpsPSGAlterVol",                     1, {ARG_NUM}},
    {"smpsPSGAlterVolS2",         "smpsPSGAlterVolS2",                    1, {ARG_NUM}},
    {"smpsClearPush",             "smpsClearPush",                         0, {0}},
    {"smpsStopSpecial",           "smpsStopSpecial",                        0, {0}},
    {"smpsFMvoice",               "smpsFMvoice",                             1, {ARG_NUM}},
    {"smpsSetvoice",              "smpsSetvoice",                             1, {ARG_NUM}},
    {"smpsModSet",                "smpsModSet",                                4, {ARG_NUM, ARG_NUM, ARG_NUM, ARG_NUM}},
    {"smpsModOn",                 "smpsModOn",                                  0, {0}},
    {"smpsStop",                  "smpsStop",                                   0, {0}},
    {"smpsPSGform",               "smpsPSGform",                                1, {ARG_NUM}},
    {"smpsModOff",                "smpsModOff",                                 0, {0}},
    {"smpsPSGvoice",              "smpsPSGvoice",                               1, {ARG_NUM}},
    {"smpsJump",                  "smpsJump",                                   1, {ARG_STR}},
    {"smpsLoop",                  "smpsLoop",                                   3, {ARG_NUM, ARG_NUM, ARG_STR}},
    {"smpsCall",                  "smpsCall",                                   1, {ARG_STR}},
    {"smpsMaxRelRate",            "smpsMaxRelRate",                             0, {0}},
    {"smpsWeirdD1LRR",            "smpsWeirdD1LRR",                             0, {0}},
    {"smpsVcFeedback",            "smpsVcFeedback",                             1, {ARG_NUM}},
    {"smpsVcAlgorithm",           "smpsVcAlgorithm",                            1, {ARG_NUM}},
    {"smpsVcDetune",              "smpsVcDetune",                               4, {ARG_NUM, ARG_NUM, ARG_NUM, ARG_NUM}},
    {"smpsVcCoarseFreq",          "smpsVcCoarseFreq",                           4, {ARG_NUM, ARG_NUM, ARG_NUM, ARG_NUM}},
    {"smpsVcRateScale",           "smpsVcRateScale",                            4, {ARG_NUM, ARG_NUM, ARG_NUM, ARG_NUM}},
    {"smpsVcAttackRate",          "smpsVcAttackRate",                           4, {ARG_NUM, ARG_NUM, ARG_NUM, ARG_NUM}},
    {"smpsVcAmpMod",              "smpsVcAmpMod",                               4, {ARG_NUM, ARG_NUM, ARG_NUM, ARG_NUM}},
    {"smpsVcDecayRate1",          "smpsVcDecayRate1",                           4, {ARG_NUM, ARG_NUM, ARG_NUM, ARG_NUM}},
    {"smpsVcDecayRate2",          "smpsVcDecayRate2",                           4, {ARG_NUM, ARG_NUM, ARG_NUM, ARG_NUM}},
    {"smpsVcDecayLevel",          "smpsVcDecayLevel",                           4, {ARG_NUM, ARG_NUM, ARG_NUM, ARG_NUM}},
    {"smpsVcReleaseRate",         "smpsVcReleaseRate",                          4, {ARG_NUM, ARG_NUM, ARG_NUM, ARG_NUM}},
    {"smpsVcTotalLevel",          "smpsVcTotalLevel",                           4, {ARG_NUM, ARG_NUM, ARG_NUM, ARG_NUM}},
};
// clang-format on

static const size_t macro_count = sizeof(macros) / sizeof(macros[0]);

// Converts one token: "$XX" hex literals become "0xXX"; everything else
// (decimal numbers, note names, named constants like panRight/cPSG1/
// fTone_03) is already valid C and passes through unchanged.
static void ConvertToken(const char *token, char *out, size_t out_size) {
    if (token[0] == '$') {
        snprintf(out, out_size, "0x%s", token + 1);
    } else {
        snprintf(out, out_size, "%s", token);
    }
}

static void Trim(char *s) {
    char *start = s;
    while (isspace((unsigned char)*start))
        start++;
    if (start != s)
        memmove(s, start, strlen(start) + 1);

    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1]))
        s[--len] = '\0';
}

// Splits `s` (a comma-separated argument list) in place into up to
// MAX_ARGS trimmed tokens. Returns the count.
static int SplitArgs(char *s, char *args[MAX_ARGS]) {
    int count = 0;
    char *tok = strtok(s, ",");
    while (tok && count < MAX_ARGS) {
        Trim(tok);
        if (*tok)
            args[count++] = tok;
        tok = strtok(NULL, ",");
    }
    return count;
}

// Splits off the first whitespace-delimited token from *s, advancing *s
// past it (to the remainder, or NULL if nothing follows). Not standard
// strsep() (that's POSIX-only, and its exact whitespace handling doesn't
// match what's needed here) -- just enough to pull the command name off
// the front of a line.
static char *strsep_local(char **s) {
    if (!*s)
        return NULL;
    char *start = *s;
    while (isspace((unsigned char)*start))
        start++;
    if (!*start) {
        *s = NULL;
        return NULL;
    }
    char *end = start;
    while (*end && !isspace((unsigned char)*end))
        end++;
    if (*end) {
        *end = '\0';
        *s = end + 1;
    } else {
        *s = NULL;
    }
    return start;
}

static const MacroDef *FindMacro(const char *name) {
    for (size_t i = 0; i < macro_count; i++)
        if (strcmp(macros[i].asm_name, name) == 0)
            return &macros[i];
    return NULL;
}

int main(int argc, char **argv) {
    if (argc != 5) {
        fprintf(stderr, "usage: %s <input.asm> <output.c> <array_name> <output.h path>\n", argv[0]);
        return 1;
    }

    const char *in_path = argv[1];
    const char *out_c_path = argv[2];
    const char *array_name = argv[3];
    const char *out_h_path = argv[4];

    FILE *in = fopen(in_path, "r");
    if (!in) {
        perror(in_path);
        return 1;
    }

    FILE *out = fopen(out_c_path, "w");
    if (!out) {
        perror(out_c_path);
        fclose(in);
        return 1;
    }

    fprintf(out, "// Generated by smps2asmc from %s -- do not edit.\n", in_path);
    fprintf(out, "#include \"smps.h\"\n\nint main(void) {\n    SMPS_Begin();\n");

    char line[MAX_LINE];
    int line_no = 0;
    while (fgets(line, sizeof(line), in)) {
        line_no++;

        // Strip comment (';' to end of line -- these files have no string
        // literals to worry about) and surrounding whitespace.
        char *comment = strchr(line, ';');
        if (comment)
            *comment = '\0';
        Trim(line);
        if (!*line)
            continue;

        // Label line: a bare identifier followed by ':' and nothing else.
        size_t len = strlen(line);
        if (line[len - 1] == ':') {
            line[len - 1] = '\0';
            int is_label = 1;
            for (char *p = line; *p; p++) {
                if (!isalnum((unsigned char)*p) && *p != '_') {
                    is_label = 0;
                    break;
                }
            }
            if (is_label) {
                fprintf(out, "    SMPS_Label(\"%s\");\n", line);
                continue;
            }
            line[len - 1] = ':'; // Not actually a label -- restore and fall through
        }

        // Otherwise: <command> [arg, arg, ...]
        char *rest = line;
        char *command = strsep_local(&rest);
        if (!command)
            continue;

        char args_buf[MAX_LINE];
        snprintf(args_buf, sizeof(args_buf), "%s", rest ? rest : "");
        char *arg_tokens[MAX_ARGS];
        int arg_count = SplitArgs(args_buf, arg_tokens);

        if (strcmp(command, "dc.b") == 0 || strcmp(command, "dc.w") == 0) {
            const char *emit = (strcmp(command, "dc.b") == 0) ? "SMPS_Byte" : "SMPS_Word";
            for (int i = 0; i < arg_count; i++) {
                char converted[128];
                ConvertToken(arg_tokens[i], converted, sizeof(converted));
                fprintf(out, "    %s(%s);\n", emit, converted);
            }
            continue;
        }

        if (strcmp(command, "smpsHeaderDAC") == 0) {
            if (arg_count == 1)
                fprintf(out, "    smpsHeaderDAC(\"%s\");\n", arg_tokens[0]);
            else if (arg_count == 3) {
                char pitch[128], vol[128];
                ConvertToken(arg_tokens[1], pitch, sizeof(pitch));
                ConvertToken(arg_tokens[2], vol, sizeof(vol));
                fprintf(out, "    smpsHeaderDAC_Full(\"%s\", %s, %s);\n", arg_tokens[0], pitch, vol);
            } else {
                fprintf(stderr, "%s:%d: smpsHeaderDAC takes 1 or 3 arguments\n", in_path, line_no);
                return 1;
            }
            continue;
        }

        if (strcmp(command, "smpsVcUnusedBits") == 0) {
            // d1r1-4 are optional in the original -- default to 0 (matching
            // its own "else: eval vcD1R1Unk,0" branch) when omitted.
            if (arg_count != 1 && arg_count != 5) {
                fprintf(stderr, "%s:%d: smpsVcUnusedBits takes 1 or 5 arguments\n", in_path, line_no);
                return 1;
            }
            char val[128];
            ConvertToken(arg_tokens[0], val, sizeof(val));
            if (arg_count == 1) {
                fprintf(out, "    smpsVcUnusedBits(%s, 0, 0, 0, 0);\n", val);
            } else {
                char d[4][128];
                for (int i = 0; i < 4; i++)
                    ConvertToken(arg_tokens[i + 1], d[i], sizeof(d[i]));
                fprintf(out, "    smpsVcUnusedBits(%s, %s, %s, %s, %s);\n", val, d[0], d[1], d[2], d[3]);
            }
            continue;
        }

        const MacroDef *def = FindMacro(command);
        if (!def) {
            fprintf(stderr, "%s:%d: unrecognized command \"%s\"\n", in_path, line_no, command);
            return 1;
        }
        if (arg_count != def->arg_count) {
            fprintf(stderr, "%s:%d: %s expects %d argument(s), got %d\n", in_path, line_no, command,
                    def->arg_count, arg_count);
            return 1;
        }

        fprintf(out, "    %s(", def->c_name);
        for (int i = 0; i < arg_count; i++) {
            if (i > 0)
                fprintf(out, ", ");
            if (def->kinds[i] == ARG_STR) {
                fprintf(out, "\"%s\"", arg_tokens[i]);
            } else {
                char converted[128];
                ConvertToken(arg_tokens[i], converted, sizeof(converted));
                fprintf(out, "%s", converted);
            }
        }
        fprintf(out, ");\n");
    }

    fprintf(out, "    SMPS_End(\"%s\", \"%s\");\n    return 0;\n}\n", out_h_path, array_name);

    fclose(in);
    fclose(out);
    return 0;
}
