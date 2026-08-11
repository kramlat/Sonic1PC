#define _POSIX_C_SOURCE 200809L

#include "compiler.h"

#include <ctype.h>
#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Error handling: setjmp/longjmp mirrors the Python/C++ versions' exception-
// based early-exit from deep inside recursive emit_event() calls, without
// threading an error code through every call site.
// ---------------------------------------------------------------------------

typedef struct {
    jmp_buf jmp;
    char message[256];
} ErrorContext;

static void ps_fail(ErrorContext *err, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(err->message, sizeof(err->message), fmt, args);
    va_end(args);
    longjmp(err->jmp, 1);
}

// ---------------------------------------------------------------------------
// unhex()/CHANNEL_ID/DAC_NAMES/note_value() -- json_to_header.py:68-128
// ---------------------------------------------------------------------------

static int channel_id_lookup(const char *s) {
    static const struct {
        const char *name;
        int value;
    } table[] = {
        {"cFM1", 0x00}, {"cFM2", 0x01}, {"cFM3", 0x02}, {"cFM4", 0x04}, {"cFM5", 0x05},
        {"cFM6", 0x06}, {"cPSG1", 0x80}, {"cPSG2", 0xA0}, {"cPSG3", 0xC0}, {"cNoise", 0xE0},
    };
    for (size_t i = 0; i < sizeof(table) / sizeof(table[0]); i++)
        if (strcmp(s, table[i].name) == 0)
            return table[i].value;
    return -1;
}

static int dac_name_lookup(const char *s) {
    static const struct {
        const char *name;
        int value;
    } table[] = {
        {"Kick", 0x81}, {"Snare", 0x82}, {"Timpani", 0x83}, {"HiTimpani", 0x88},
        {"MidTimpani", 0x89}, {"LowTimpani", 0x8A}, {"VLowTimpani", 0x8B},
    };
    for (size_t i = 0; i < sizeof(table) / sizeof(table[0]); i++)
        if (strcmp(s, table[i].name) == 0)
            return table[i].value;
    return -1;
}

static int is_hex_string(const char *s) { // ^-?0x[0-9A-Fa-f]+$
    if (*s == '-')
        s++;
    if (s[0] != '0' || (s[1] != 'x' && s[1] != 'X'))
        return 0;
    s += 2;
    if (!*s)
        return 0;
    for (; *s; s++)
        if (!isxdigit((unsigned char)*s))
            return 0;
    return 1;
}

static int is_dec_string(const char *s) { // ^-?\d+$
    if (*s == '-')
        s++;
    if (!*s)
        return 0;
    for (; *s; s++)
        if (!isdigit((unsigned char)*s))
            return 0;
    return 1;
}

static int is_ftone_string(const char *s) { // ^fTone_[0-9A-Fa-f]+$
    if (strncmp(s, "fTone_", 6) != 0)
        return 0;
    s += 6;
    if (!*s)
        return 0;
    for (; *s; s++)
        if (!isxdigit((unsigned char)*s))
            return 0;
    return 1;
}

static int unhex_int(const PJValue *v, int fallback) {
    if (pj_type(v) == PJ_NUMBER)
        return (int)pj_get_number(v, (double)fallback);
    if (pj_type(v) != PJ_STRING)
        return fallback;
    const char *s = pj_get_string(v, "");
    if (is_hex_string(s)) {
        int neg = (s[0] == '-');
        const char *digits = neg ? s + 3 : s + 2;
        long mag = strtol(digits, NULL, 16);
        return neg ? -(int)mag : (int)mag;
    }
    if (is_dec_string(s))
        return (int)strtol(s, NULL, 10);
    int cid = channel_id_lookup(s);
    if (cid >= 0)
        return cid;
    if (is_ftone_string(s)) {
        const char *underscore = strrchr(s, '_');
        return (int)strtol(underscore + 1, NULL, 16);
    }
    return fallback;
}

static const char *as_block_name(const PJValue *v) { return pj_get_string(v, ""); }

static int note_base_for_letter(char letter) {
    switch (letter) {
    case 'C':
        return 0;
    case 'D':
        return 2;
    case 'E':
        return 4;
    case 'F':
        return 5;
    case 'G':
        return 7;
    case 'A':
        return 9;
    case 'B':
        return 11;
    default:
        return -1;
    }
}

// note_value_internal: throws (via ErrorContext) on an unrecognized name --
// used by the compiler itself, where an unrecognized name is a real error.
// ps_note_value() (public, non-throwing) wraps this for the DAW's display
// use, matching NativeCompiler.cpp's own two-tier noteValue()/
// noteValueInternal() split.
static int note_value_internal(ErrorContext *err, const char *name) {
    if (strcmp(name, "Rst") == 0)
        return 0x80;
    if (strcmp(name, "MaxPSG") == 0)
        return note_value_internal(err, "A5");
    int dac = dac_name_lookup(name);
    if (dac >= 0)
        return dac;

    size_t len = strlen(name);
    if (len < 2)
        ps_fail(err, "unrecognized note name: %s", name);
    char letter = name[0];
    if (note_base_for_letter(letter) < 0)
        ps_fail(err, "unrecognized note name: %s", name);
    size_t i = 1;
    int accidental = 0;
    if (name[i] == 's') {
        accidental = 1;
        i++;
    } else if (name[i] == 'b') {
        accidental = -1;
        i++;
    }
    if (i >= len)
        ps_fail(err, "unrecognized note name: %s", name);
    for (size_t j = i; j < len; j++)
        if (!isdigit((unsigned char)name[j]))
            ps_fail(err, "unrecognized note name: %s", name);
    int octave = atoi(name + i);
    int semitone = note_base_for_letter(letter) + accidental;
    return 0x81 + octave * 12 + semitone;
}

int ps_note_value(const char *name) {
    ErrorContext err;
    if (setjmp(err.jmp) != 0)
        return -1; // unrecognized name -- caller treats this as "can't place this row"
    return note_value_internal(&err, name);
}

// ---------------------------------------------------------------------------
// Emitter -- json_to_header.py:143-181. Dynamic byte buffer + a two-pass
// patch/resolve system for forward references (smpsJump/smpsLoop/smpsCall
// can target labels anywhere, including blocks not yet emitted).
// ---------------------------------------------------------------------------

typedef struct {
    char *target;
    size_t pos;
    int kind; // 0 = "rel-1", 1 = "abs"
} Patch;

typedef struct {
    char *name;
    int addr;
} BlockAddr;

typedef struct {
    uint8_t *buf;
    size_t len, cap;

    BlockAddr *blocks;
    size_t block_count, block_cap;

    Patch *patches;
    size_t patch_count, patch_cap;

    ErrorContext *err;
    int synthetic_label_counter;
    int driver_version; // 1 (default) or 3 -- see SMPS driver-version-3 plan; selects emit_event vs emit_event_v3
} Emitter;

static void em_init(Emitter *em, ErrorContext *err) {
    memset(em, 0, sizeof(*em));
    em->err = err;
    em->driver_version = 1;
}

static void em_free(Emitter *em) {
    free(em->buf);
    for (size_t i = 0; i < em->block_count; i++)
        free(em->blocks[i].name);
    free(em->blocks);
    for (size_t i = 0; i < em->patch_count; i++)
        free(em->patches[i].target);
    free(em->patches);
}

static void em_byte(Emitter *em, int v) {
    if (em->len == em->cap) {
        em->cap = em->cap ? em->cap * 2 : 64;
        em->buf = (uint8_t *)realloc(em->buf, em->cap);
    }
    em->buf[em->len++] = (uint8_t)(v & 0xFF);
}

static void em_word_placeholder(Emitter *em, const char *target, int kind) {
    if (em->patch_count == em->patch_cap) {
        em->patch_cap = em->patch_cap ? em->patch_cap * 2 : 8;
        em->patches = (Patch *)realloc(em->patches, em->patch_cap * sizeof(Patch));
    }
    em->patches[em->patch_count].target = strdup(target ? target : "");
    em->patches[em->patch_count].pos = em->len;
    em->patches[em->patch_count].kind = kind;
    em->patch_count++;
    em_byte(em, 0);
    em_byte(em, 0);
}

static void em_word_abs(Emitter *em, int v) {
    em_byte(em, (v >> 8) & 0xFF);
    em_byte(em, v & 0xFF);
}

static void em_mark_block(Emitter *em, const char *name) {
    // Matches Python/C++'s dict-assignment semantics: re-marking an
    // existing name updates its address in place rather than duplicating.
    for (size_t i = 0; i < em->block_count; i++) {
        if (strcmp(em->blocks[i].name, name) == 0) {
            em->blocks[i].addr = (int)em->len;
            return;
        }
    }
    if (em->block_count == em->block_cap) {
        em->block_cap = em->block_cap ? em->block_cap * 2 : 16;
        em->blocks = (BlockAddr *)realloc(em->blocks, em->block_cap * sizeof(BlockAddr));
    }
    em->blocks[em->block_count].name = strdup(name);
    em->blocks[em->block_count].addr = (int)em->len;
    em->block_count++;
}

static int em_find_block(Emitter *em, const char *name, int *out_addr) {
    for (size_t i = 0; i < em->block_count; i++) {
        if (strcmp(em->blocks[i].name, name) == 0) {
            *out_addr = em->blocks[i].addr;
            return 1;
        }
    }
    return 0;
}

static void em_apply_patches(Emitter *em) {
    for (size_t i = 0; i < em->patch_count; i++) {
        const Patch *p = &em->patches[i];
        int target_addr;
        if (!em_find_block(em, p->target, &target_addr))
            ps_fail(em->err, "unresolved reference to block '%s' -- never defined in SMPSplaylist", p->target);
        int value;
        if (p->kind == 0) // rel-1
            value = (target_addr - (int)p->pos - 1) & 0xFFFF;
        else // abs
            value = target_addr & 0xFFFF;
        em->buf[p->pos] = (uint8_t)((value >> 8) & 0xFF);
        em->buf[p->pos + 1] = (uint8_t)(value & 0xFF);
    }
}

static char *new_synthetic_label(Emitter *em) {
    char buf[32];
    snprintf(buf, sizeof(buf), "__synthetic_%d", ++em->synthetic_label_counter);
    return strdup(buf);
}

// ---------------------------------------------------------------------------
// emit_simple/ZERO_ARG_OPS -- json_to_header.py:187-211
// ---------------------------------------------------------------------------

static int simple_op_code(const char *mnemonic) {
    static const struct {
        const char *name;
        int code;
    } table[] = {
        {"smpsChanTempoDiv", 0xE5}, {"smpsAlterVol", 0xE6}, {"smpsNoteFill", 0xE8}, {"smpsAlterPitch", 0xE9},
        {"smpsAlterNote", 0xE1},    {"smpsNop", 0xE2},      {"smpsPSGform", 0xF3},  {"smpsPSGvoice", 0xF5},
        {"smpsSetvoice", 0xEF},
    };
    for (size_t i = 0; i < sizeof(table) / sizeof(table[0]); i++)
        if (strcmp(mnemonic, table[i].name) == 0)
            return table[i].code;
    return -1;
}

static void emit_simple(Emitter *em, const char *mnemonic, const PJValue *value) {
    int op = simple_op_code(mnemonic);
    if (op < 0)
        ps_fail(em->err, "%s: byte encoding not yet implemented in the compiler", mnemonic);
    em_byte(em, op);
    em_byte(em, unhex_int(value, 0) & 0xFF);
}

static int zero_arg_op_code(const char *event) {
    static const struct {
        const char *name;
        int code;
    } table[] = {
        {"smpsStop", 0xF2}, {"smpsClearPush", 0xED}, {"smpsStopSpecial", 0xEE},
        {"smpsWeirdD1LRR", 0xF9}, {"smpsFade", 0xE4}, {"smpsNoAttack", 0xE7},
    };
    for (size_t i = 0; i < sizeof(table) / sizeof(table[0]); i++)
        if (strcmp(event, table[i].name) == 0)
            return table[i].code;
    return -1;
}

// ---------------------------------------------------------------------------
// emit_event -- json_to_header.py:214-319
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// emit_event_v3/emit_simple_v3 -- SMPS driver-version-3 (Sonic 3/Flamedriver-
// compatible) coordination flags. C port of json_to_header.py's own
// emit_event_v3/emit_simple_v3/ZERO_ARG_OPS_V3/SIMPLE_OPS_V3 -- keep these
// two byte-for-byte in sync, same convention the driver-version-1 pair
// above already follows. See the SMPS driver-version-3 plan for the full
// researched flag table this mirrors.
// ---------------------------------------------------------------------------

static int zero_arg_op_code_v3(const char *event, int *out_sub) {
    static const struct {
        const char *name;
        int code;
        int sub; // -1 = not an extended ($FF-prefixed) flag
    } table[] = {
        {"cfPreventAttack", 0xE7, -1}, {"cfSilenceStopTrack", 0xE3, -1}, {"cfStopTrack", 0xF2, -1},
        {"cfJumpReturn", 0xF9, -1},    {"cfDisableModulation", 0xFA, -1}, {"cfResetSpindashRev", 0xFF, 0x07},
    };
    for (size_t i = 0; i < sizeof(table) / sizeof(table[0]); i++)
        if (strcmp(event, table[i].name) == 0) {
            *out_sub = table[i].sub;
            return table[i].code;
        }
    return -1;
}

static int simple_op_code_v3(const char *mnemonic, int *out_sub, int *out_argc) {
    static const struct {
        const char *name;
        int code, sub, argc;
    } table[] = {
        {"cfDetune", 0xE1, -1, 1},          {"cfFadeInToPrevious", 0xE2, -1, 1},
        {"cfSetVolume", 0xE4, -1, 1},       {"cfChangeVolume2", 0xE5, -1, 2},
        {"cfChangeVolume", 0xE6, -1, 1},    {"cfNoteFill", 0xE8, -1, 1},
        {"cfPlayDACSample", 0xEA, -1, 1},   {"cfChangePSGVolume", 0xEC, -1, 1},
        {"cfSetKey", 0xED, -1, 1},          {"cfSendFMI", 0xEE, -1, 2},
        {"cfAlterModulation", 0xF1, -1, 2}, {"cfSetPSGNoise", 0xF3, -1, 1},
        {"cfSetModulation", 0xF4, -1, 1},   {"cfSetPSGVolEnv", 0xF5, -1, 1},
        {"cfChangeTransposition", 0xFB, -1, 1}, {"cfToggleAltFreqMode", 0xFD, -1, 1},
        {"cfFM3SpecialMode", 0xFE, -1, 4},
        {"cfSetTempo", 0xFF, 0x00, 1}, {"cfPlaySFXByIndex", 0xFF, 0x01, 1}, {"cfHaltSound", 0xFF, 0x02, 1},
        {"cfSetTempoDivider", 0xFF, 0x04, 1}, {"cfSetSSGEG", 0xFF, 0x05, 4}, {"cfFMVolEnv", 0xFF, 0x06, 2},
        {"cfChanSetTempoDivider", 0xFF, 0x08, 1}, {"cfChanFMCommand", 0xFF, 0x09, 2},
        {"cfNoteFillSet", 0xFF, 0x0A, 1}, {"cfPitchSlide", 0xFF, 0x0B, 1}, {"cfSetLFO", 0xFF, 0x0C, 2},
        {"cfPlayMusicByIndex", 0xFF, 0x0D, 1},
    };
    for (size_t i = 0; i < sizeof(table) / sizeof(table[0]); i++)
        if (strcmp(mnemonic, table[i].name) == 0) {
            *out_sub = table[i].sub;
            *out_argc = table[i].argc;
            return table[i].code;
        }
    return -1;
}

static void emit_simple_v3(Emitter *em, const char *mnemonic, const PJValue *value) {
    int sub, argc;
    int op = simple_op_code_v3(mnemonic, &sub, &argc);
    if (op < 0)
        ps_fail(em->err, "%s: byte encoding not yet implemented in the driverVersion-3 compiler", mnemonic);
    int is_array = pj_type(value) == PJ_ARRAY;
    int n = is_array ? (int)pj_array_size(value) : 1;
    if (n != argc)
        ps_fail(em->err, "%s: expected %d arg(s), got %d", mnemonic, argc, n);
    em_byte(em, op);
    if (sub >= 0)
        em_byte(em, sub);
    for (int i = 0; i < n; i++)
        em_byte(em, unhex_int(is_array ? pj_array_get(value, (size_t)i) : value, 0) & 0xFF);
}

static void emit_event_v3(Emitter *em, const PJValue *event) {
    if (pj_type(event) == PJ_STRING) {
        const char *s = pj_get_string(event, "");
        int sub;
        int op = zero_arg_op_code_v3(s, &sub);
        if (op < 0)
            ps_fail(em->err, "bare event '%s': not implemented (driverVersion 3)", s);
        em_byte(em, op);
        if (sub >= 0)
            em_byte(em, sub);
        return;
    }

    PJValue *v;

    if ((v = pj_object_get(event, "note")) != NULL) {
        em_byte(em, note_value_internal(em->err, pj_get_string(v, "")));
        PJValue *dur = pj_object_get(event, "duration");
        if (dur)
            em_byte(em, unhex_int(dur, 0) & 0xFF);
        return;
    }
    if (pj_object_has(event, "tie") || pj_object_has(event, "inheritedNote")) {
        em_byte(em, unhex_int(pj_object_get(event, "duration"), 0) & 0xFF);
        return;
    }
    if ((v = pj_object_get(event, "cfPanningAMSFMS")) != NULL) {
        const char *direction = pj_get_string(pj_array_get(v, 0), "");
        static const struct {
            const char *name;
            int value;
        } pan_values[] = {
            {"panLeft", 0x80}, {"panRight", 0x40}, {"panCentre", 0xC0}, {"panCenter", 0xC0}, {"panNone", 0x00},
        };
        int base = 0;
        for (size_t i = 0; i < sizeof(pan_values) / sizeof(pan_values[0]); i++)
            if (strcmp(direction, pan_values[i].name) == 0)
                base = pan_values[i].value;
        em_byte(em, 0xE0);
        em_byte(em, (base + unhex_int(pj_array_get(v, 1), 0)) & 0xFF);
        return;
    }
    if ((v = pj_object_get(event, "cfModulation")) != NULL) {
        em_byte(em, 0xF0);
        for (size_t i = 0; i < pj_array_size(v); i++)
            em_byte(em, unhex_int(pj_array_get(v, i), 0) & 0xFF);
        return;
    }
    if ((v = pj_object_get(event, "cfSetVoice")) != NULL) {
        em_byte(em, 0xEF);
        if (pj_type(v) == PJ_ARRAY) {
            for (size_t i = 0; i < pj_array_size(v); i++)
                em_byte(em, unhex_int(pj_array_get(v, i), 0) & 0xFF);
        } else {
            em_byte(em, unhex_int(v, 0) & 0xFF);
        }
        return;
    }
    if ((v = pj_object_get(event, "cfConditionalJump")) != NULL) {
        em_byte(em, 0xEB);
        em_byte(em, unhex_int(pj_array_get(v, 0), 0) & 0xFF);
        em_word_placeholder(em, as_block_name(pj_array_get(v, 1)), 0); // rel-1
        return;
    }
    if ((v = pj_object_get(event, "cfRepeatAtPos")) != NULL) {
        const PJValue *idx = pj_array_get(v, 0);
        const PJValue *cnt = pj_array_get(v, 1);
        char *label = new_synthetic_label(em);
        em_mark_block(em, label);
        for (size_t i = 2; i < pj_array_size(v); i++)
            emit_event_v3(em, pj_array_get(v, i));
        em_byte(em, 0xF7);
        em_byte(em, unhex_int(idx, 0) & 0xFF);
        em_byte(em, unhex_int(cnt, 0) & 0xFF);
        em_word_placeholder(em, label, 0); // rel-1
        free(label);
        return;
    }
    if ((v = pj_object_get(event, "cfJumpTo")) != NULL) {
        em_byte(em, 0xF6);
        em_word_placeholder(em, as_block_name(v), 0); // rel-1
        return;
    }
    if ((v = pj_object_get(event, "cfJumpToGosub")) != NULL) {
        em_byte(em, 0xF8);
        em_word_placeholder(em, as_block_name(v), 1); // abs
        return;
    }
    if ((v = pj_object_get(event, "cfLoopContinuousSFX")) != NULL) {
        em_byte(em, 0xFC);
        em_word_placeholder(em, as_block_name(v), 0); // rel-1
        return;
    }
    if ((v = pj_object_get(event, "cfCopyData")) != NULL) {
        em_byte(em, 0xFF);
        em_byte(em, 0x03);
        em_word_placeholder(em, as_block_name(pj_array_get(v, 0)), 1); // abs
        em_byte(em, unhex_int(pj_array_get(v, 1), 0) & 0xFF);
        return;
    }

    static const char *simple_mnemonics_v3[] = {
        "cfDetune", "cfFadeInToPrevious", "cfSetVolume", "cfChangeVolume2", "cfChangeVolume", "cfNoteFill",
        "cfPlayDACSample", "cfChangePSGVolume", "cfSetKey", "cfSendFMI", "cfAlterModulation", "cfSetPSGNoise",
        "cfSetModulation", "cfSetPSGVolEnv", "cfChangeTransposition", "cfToggleAltFreqMode", "cfFM3SpecialMode",
        "cfSetTempo", "cfPlaySFXByIndex", "cfHaltSound", "cfSetTempoDivider", "cfSetSSGEG", "cfFMVolEnv",
        "cfChanSetTempoDivider", "cfChanFMCommand", "cfNoteFillSet", "cfPitchSlide", "cfSetLFO", "cfPlayMusicByIndex",
    };
    for (size_t i = 0; i < sizeof(simple_mnemonics_v3) / sizeof(simple_mnemonics_v3[0]); i++) {
        if (pj_object_has(event, simple_mnemonics_v3[i])) {
            emit_simple_v3(em, simple_mnemonics_v3[i], pj_object_get(event, simple_mnemonics_v3[i]));
            return;
        }
    }

    ps_fail(em->err, "unrecognized event (driverVersion 3, no matching mnemonic found)");
}

// ---------------------------------------------------------------------------
// smpsCall target tracking -- see the main compile loop's own comment for
// why this exists (auto-appending an explicit smpsReturn for blocks that
// rely on the JSON schema's "block end IS the implicit return" convention,
// which this byte compiler has no native concept of).
// ---------------------------------------------------------------------------

#define MAX_CALL_TARGETS 256

typedef struct {
    const char *names[MAX_CALL_TARGETS];
    size_t count;
} CallTargetSet;

static void call_target_add(CallTargetSet *set, const char *name) {
    for (size_t i = 0; i < set->count; i++)
        if (strcmp(set->names[i], name) == 0)
            return;
    if (set->count < MAX_CALL_TARGETS)
        set->names[set->count++] = name;
}

static int call_target_has(const CallTargetSet *set, const char *name) {
    for (size_t i = 0; i < set->count; i++)
        if (strcmp(set->names[i], name) == 0)
            return 1;
    return 0;
}

// Recurses into inline smpsLoop/smpsJump bodies too (a smpsCall can appear
// nested inside one), harmlessly skipping the idx/cnt string entries at a
// smpsLoop body's own start (pj_object_get(event,"smpsCall") on a raw
// PJ_STRING node is never true, so those are just no-ops here).
static void scan_events_for_calls(const PJValue *events, CallTargetSet *set) {
    for (size_t i = 0; i < pj_array_size(events); i++) {
        const PJValue *event = pj_array_get(events, i);
        if (pj_type(event) != PJ_OBJECT)
            continue;
        const PJValue *v;
        if ((v = pj_object_get(event, "smpsCall")) != NULL)
            call_target_add(set, as_block_name(v));
        if ((v = pj_object_get(event, "smpsLoop")) != NULL)
            scan_events_for_calls(v, set);
        if ((v = pj_object_get(event, "smpsJump")) != NULL)
            scan_events_for_calls(v, set);
    }
}

// A block's own last JSON event genuinely ends control flow through it --
// stop/return outright, or an unconditional diversion (jumpTo/inline
// smpsJump, which per the schema is always "no count = forever", never
// falls through). smpsLoop deliberately doesn't count: it CAN fall
// through once its own repeat count is exhausted, and already compiles
// its own real $F7 either way.
static int event_is_terminal(const PJValue *event) {
    if (pj_type(event) == PJ_STRING) {
        const char *s = pj_get_string(event, "");
        return strcmp(s, "smpsStop") == 0 || strcmp(s, "smpsStopSpecial") == 0 || strcmp(s, "smpsFade") == 0 ||
               strcmp(s, "smpsReturn") == 0;
    }
    return pj_object_has(event, "jumpTo") || pj_object_has(event, "smpsJump");
}

static void emit_event(Emitter *em, const PJValue *event) {
    if (em->driver_version >= 3) {
        emit_event_v3(em, event);
        return;
    }
    if (pj_type(event) == PJ_STRING) {
        const char *s = pj_get_string(event, "");
        int op = zero_arg_op_code(s);
        if (op < 0)
            ps_fail(em->err, "bare event '%s': not implemented", s);
        em_byte(em, op);
        return;
    }

    PJValue *v;

    if ((v = pj_object_get(event, "note")) != NULL) {
        em_byte(em, note_value_internal(em->err, pj_get_string(v, "")));
        PJValue *dur = pj_object_get(event, "duration");
        if (dur)
            em_byte(em, unhex_int(dur, 0) & 0xFF);
        return;
    }
    if (pj_object_has(event, "tie") || pj_object_has(event, "inheritedNote")) {
        em_byte(em, unhex_int(pj_object_get(event, "duration"), 0) & 0xFF);
        return;
    }
    if ((v = pj_object_get(event, "smpsMod")) != NULL) {
        em_byte(em, pj_get_bool(v, false) ? 0xF1 : 0xF4);
        return;
    }
    if ((v = pj_object_get(event, "smpsPan")) != NULL) {
        const char *direction = pj_get_string(pj_array_get(v, 0), "");
        static const struct {
            const char *name;
            int value;
        } pan_values[] = {
            {"panLeft", 0x80}, {"panRight", 0x40}, {"panCentre", 0xC0}, {"panCenter", 0xC0}, {"panNone", 0x00},
        };
        int base = 0;
        for (size_t i = 0; i < sizeof(pan_values) / sizeof(pan_values[0]); i++)
            if (strcmp(direction, pan_values[i].name) == 0)
                base = pan_values[i].value;
        em_byte(em, 0xE0);
        em_byte(em, (base + unhex_int(pj_array_get(v, 1), 0)) & 0xFF);
        return;
    }
    if ((v = pj_object_get(event, "smpsModSet")) != NULL) {
        em_byte(em, 0xF0);
        for (size_t i = 0; i < pj_array_size(v); i++)
            em_byte(em, unhex_int(pj_array_get(v, i), 0) & 0xFF);
        return;
    }
    if ((v = pj_object_get(event, "smpsPSGvoice")) != NULL) {
        // Value is either "fTone_XX" (parse the suffix after '_') or a bare
        // hex value like "0x1" (no underscore -- parse the whole string).
        // strtol with base 16 accepts an optional "0x" prefix either way,
        // so both cases share one parse call once the right substring is
        // selected -- matches json_to_header.py's own
        // `.rsplit("_", 1)[-1]` exactly (which is a no-op when there's no
        // underscore, not a failure).
        const char *name = pj_get_string(v, "");
        const char *underscore = strrchr(name, '_');
        const char *num_part = underscore ? underscore + 1 : name;
        em_byte(em, 0xF5);
        em_byte(em, (int)strtol(num_part, NULL, 16) & 0xFF);
        return;
    }
    {
        int is_loop = pj_object_has(event, "smpsLoop");
        if (is_loop || pj_object_has(event, "smpsJump")) {
            const PJValue *value = pj_object_get(event, is_loop ? "smpsLoop" : "smpsJump");
            const PJValue *idx = NULL, *cnt = NULL;
            size_t body_start = 0;
            if (is_loop) {
                idx = pj_array_get(value, 0);
                cnt = pj_array_get(value, 1);
                body_start = 2;
            }
            char *label = new_synthetic_label(em);
            em_mark_block(em, label);
            for (size_t i = body_start; i < pj_array_size(value); i++)
                emit_event(em, pj_array_get(value, i));
            if (is_loop) {
                em_byte(em, 0xF7);
                em_byte(em, unhex_int(idx, 0) & 0xFF);
                em_byte(em, unhex_int(cnt, 0) & 0xFF);
            } else {
                em_byte(em, 0xF6);
            }
            em_word_placeholder(em, label, 0); // rel-1
            free(label);
            return;
        }
    }

    static const char *simple_mnemonics[] = {
        "smpsChanTempoDiv", "smpsAlterVol", "smpsNoteFill", "smpsAlterPitch",
        "smpsAlterNote",    "smpsNop",      "smpsPSGform",  "smpsSetvoice",
    };
    for (size_t i = 0; i < sizeof(simple_mnemonics) / sizeof(simple_mnemonics[0]); i++) {
        if (pj_object_has(event, simple_mnemonics[i])) {
            emit_simple(em, simple_mnemonics[i], pj_object_get(event, simple_mnemonics[i]));
            return;
        }
    }
    if ((v = pj_object_get(event, "smpsPSGAlterVol")) != NULL) {
        em_byte(em, 0xEC);
        em_byte(em, unhex_int(v, 0) & 0xFF);
        return;
    }
    if ((v = pj_object_get(event, "smpsSetTempoDiv")) != NULL) {
        em_byte(em, 0xEB);
        em_byte(em, unhex_int(v, 0) & 0xFF);
        return;
    }
    if ((v = pj_object_get(event, "smpsSetTempoMod")) != NULL) {
        em_byte(em, 0xEA);
        em_byte(em, unhex_int(v, 0) & 0xFF);
        return;
    }
    if ((v = pj_object_get(event, "smpsCall")) != NULL) {
        em_byte(em, 0xF8);
        em_word_placeholder(em, as_block_name(v), 1); // abs
        return;
    }
    if ((v = pj_object_get(event, "jumpTo")) != NULL) {
        const PJValue *loop_args = pj_object_get(event, "smpsLoopArgs");
        if (loop_args) {
            em_byte(em, 0xF7);
            em_byte(em, unhex_int(pj_array_get(loop_args, 0), 0) & 0xFF);
            em_byte(em, unhex_int(pj_array_get(loop_args, 1), 0) & 0xFF);
        } else {
            em_byte(em, 0xF6);
        }
        em_word_placeholder(em, as_block_name(v), 0); // rel-1
        return;
    }

    ps_fail(em->err, "unrecognized event (no matching mnemonic found)");
}

// ---------------------------------------------------------------------------
// emit_voice -- json_to_header.py:331-372
// ---------------------------------------------------------------------------

static const int kOpWriteOrder[4] = {0, 2, 1, 3};

static void op_field(const PJValue *ops, const char *key, int out[4]) {
    for (int i = 0; i < 4; i++) {
        const PJValue *op = (size_t)i < pj_array_size(ops) ? pj_array_get(ops, i) : NULL;
        out[i] = unhex_int(op ? pj_object_get(op, key) : NULL, 0);
    }
}

static void emit_voice(Emitter *em, const PJValue *voice) {
    int alg = unhex_int(pj_object_get(voice, "smpsVcAlgorithm"), 0);
    int fb = unhex_int(pj_object_get(voice, "smpsVcFeedback"), 0);
    int ub = unhex_int(pj_object_get(voice, "smpsVcUnusedBits"), 0);
    int alg_ext = (alg >> 3) & 1;
    em_byte(em, (alg_ext << 7) | ((ub & 1) << 6) | ((fb & 7) << 3) | (alg & 7));

    const PJValue *ops = pj_object_get(voice, "operators");
    int dt[4], mul[4], rs[4], ar[4], am[4], d1r[4], d2r[4], d1l[4], rr[4], tl[4];
    op_field(ops, "smpsVcDetune", dt);
    op_field(ops, "smpsVcCoarseFreq", mul);
    op_field(ops, "smpsVcRateScale", rs);
    op_field(ops, "smpsVcAttackRate", ar);
    op_field(ops, "smpsVcAmpMod", am);
    op_field(ops, "smpsVcDecayRate1", d1r);
    op_field(ops, "smpsVcDecayRate2", d2r);
    op_field(ops, "smpsVcDecayLevel", d1l);
    op_field(ops, "smpsVcReleaseRate", rr);
    op_field(ops, "smpsVcTotalLevel", tl);

    const int tl_mask[4] = {0x80, (alg >= 5) ? 0x80 : 0, (alg >= 4) ? 0x80 : 0, (alg == 7) ? 0x80 : 0};

    for (int k = 0; k < 4; k++) {
        int pos = kOpWriteOrder[k];
        em_byte(em, ((dt[pos] & 7) << 4) | (mul[pos] & 0xF));
    }
    for (int k = 0; k < 4; k++) {
        int pos = kOpWriteOrder[k];
        em_byte(em, ((rs[pos] & 3) << 6) | (ar[pos] & 0x1F));
    }
    for (int k = 0; k < 4; k++) {
        int pos = kOpWriteOrder[k];
        em_byte(em, ((am[pos] & 1) << 7) | (d1r[pos] & 0x1F));
    }
    for (int k = 0; k < 4; k++) {
        int pos = kOpWriteOrder[k];
        em_byte(em, d2r[pos] & 0x1F);
    }
    for (int k = 0; k < 4; k++) {
        int pos = kOpWriteOrder[k];
        em_byte(em, ((d1l[pos] & 0xF) << 4) | (rr[pos] & 0xF));
    }
    for (int k = 0; k < 4; k++) {
        int pos = kOpWriteOrder[k];
        em_byte(em, (tl[pos] & 0x7F) | tl_mask[pos]);
    }
}

// ---------------------------------------------------------------------------
// compile_song -- json_to_header.py:388-456
// ---------------------------------------------------------------------------

static void compile_song_internal(Emitter *em, const PJValue *data) {
    const PJValue *header = pj_object_get(data, "header");
    const char *voice_bank_name = "";
    for (size_t i = 0; i < pj_array_size(header); i++) {
        const PJValue *entry = pj_array_get(header, i);
        const PJValue *voice_entry = pj_object_get(entry, "smpsHeaderVoice");
        if (voice_entry)
            voice_bank_name = pj_get_string(pj_array_get(voice_entry, 0), "");
    }

    em_word_placeholder(em, voice_bank_name, 1); // abs

    for (size_t i = 0; i < pj_array_size(header); i++) {
        const PJValue *entry = pj_array_get(header, i);
        const char *macro = pj_object_first_key(entry);
        if (!macro)
            continue;
        const PJValue *args = pj_object_get(entry, macro);

        if (strcmp(macro, "smpsHeaderStartSong") == 0 || strcmp(macro, "smpsHeaderVoice") == 0)
            continue;

        if (strcmp(macro, "smpsHeaderSFXChannel") == 0) {
            em_byte(em, 0x80);
            em_byte(em, unhex_int(pj_array_get(args, 0), 0) & 0xFF); // chanid
            em_word_placeholder(em, as_block_name(pj_array_get(args, 1)), 1); // loc, abs
            em_byte(em, unhex_int(pj_array_get(args, 2), 0) & 0xFF); // pitch
            em_byte(em, unhex_int(pj_array_get(args, 3), 0) & 0xFF); // vol
            continue;
        }
        if (strcmp(macro, "smpsHeaderDAC") == 0) {
            em_word_placeholder(em, as_block_name(pj_array_get(args, 0)), 1); // abs
            if (pj_array_size(args) > 1) {
                em_byte(em, unhex_int(pj_array_get(args, 1), 0) & 0xFF); // pitch
                em_byte(em, (pj_array_size(args) > 2 ? unhex_int(pj_array_get(args, 2), 0) : 0) & 0xFF); // vol
            } else {
                em_word_abs(em, 0);
            }
            continue;
        }

        int ptr_field = -1;
        if (strcmp(macro, "smpsHeaderVoice") == 0 || strcmp(macro, "smpsHeaderDAC") == 0 ||
            strcmp(macro, "smpsHeaderFM") == 0 || strcmp(macro, "smpsHeaderPSG") == 0)
            ptr_field = 0;

        for (size_t a = 0; a < pj_array_size(args); a++) {
            if ((int)a == ptr_field)
                em_word_placeholder(em, as_block_name(pj_array_get(args, a)), 1); // abs
            else
                em_byte(em, unhex_int(pj_array_get(args, a), 0) & 0xFF);
        }
    }

    // Playlist block order comes directly from the object's own insertion
    // order -- this library's PJValue preserves it exactly (see json.h's
    // own comment), so unlike the retired Qt/C++ port, there is no separate
    // order list to pass in or keep in sync.
    const PJValue *playlist = pj_object_get(data, "SMPSplaylist");

    // asm_to_json.py deliberately drops explicit smpsReturn instructions
    // ("block end IS the implicit return -- no event emitted, per schema"),
    // but this compiler has no such implicit-return concept -- a smpsCall
    // target block that ends without an explicit terminator just falls
    // through into whatever bytes come next in the compiled stream (real
    // hardware has no bounds checking), landing on unrelated data. Found
    // via the JSON-engine cross-verification effort: Mus82_LZ's own
    // Call02 (called from both FM3 and FM4) has no terminating event, so
    // the old byte-VM was falling through into the NEXT playlist block
    // (FM4) and re-triggering things that were never meant to run there.
    // Fix: scan every block for smpsCall targets first, then auto-append
    // an explicit smpsReturn ($E3) after any such block whose own last
    // event isn't already a real terminator (stop/return/unconditional
    // jump -- smpsLoop doesn't count, since it can fall through once
    // exhausted and already gets its own real $F7 either way).
    CallTargetSet call_targets = {0};
    for (size_t i = 0; i < pj_object_size(playlist); i++)
        scan_events_for_calls(pj_object_value_at(playlist, i), &call_targets);

    for (size_t i = 0; i < pj_object_size(playlist); i++) {
        const char *name = pj_object_key_at(playlist, i);
        const PJValue *events = pj_object_value_at(playlist, i);
        em_mark_block(em, name);
        for (size_t e = 0; e < pj_array_size(events); e++)
            emit_event(em, pj_array_get(events, e));
        size_t n = pj_array_size(events);
        int last_is_terminal = n > 0 && event_is_terminal(pj_array_get(events, n - 1));
        if (call_target_has(&call_targets, name) && !last_is_terminal)
            em_byte(em, 0xE3); // smpsReturn
    }

    if (voice_bank_name[0] != '\0') {
        em_mark_block(em, voice_bank_name);
        const PJValue *voices = pj_object_get(data, "voices");
        for (size_t i = 0; i < pj_array_size(voices); i++)
            emit_voice(em, pj_array_get(voices, i));
    }

    em_apply_patches(em);
}

PSCompileResult ps_compile_song(const PJValue *song) {
    PSCompileResult result;
    memset(&result, 0, sizeof(result));

    ErrorContext err;
    memset(err.message, 0, sizeof(err.message));
    Emitter em;
    em_init(&em, &err);
    em.driver_version = unhex_int(pj_object_get(song, "driverVersion"), 1);

    if (setjmp(err.jmp) == 0) {
        compile_song_internal(&em, song);
        result.success = 1;
        result.byte_count = em.len;
        result.driver_version = em.driver_version;
        result.bytes = (uint8_t *)malloc(em.len ? em.len : 1);
        memcpy(result.bytes, em.buf, em.len);
    } else {
        result.success = 0;
        result.bytes = NULL;
        result.byte_count = 0;
        strncpy(result.error_message, err.message, sizeof(result.error_message) - 1);
    }

    em_free(&em);
    return result;
}

void ps_compile_result_free(PSCompileResult *result) {
    free(result->bytes);
    result->bytes = NULL;
    result->byte_count = 0;
}
