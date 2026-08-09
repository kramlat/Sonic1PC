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
} Emitter;

static void em_init(Emitter *em, ErrorContext *err) {
    memset(em, 0, sizeof(*em));
    em->err = err;
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

static void emit_event(Emitter *em, const PJValue *event) {
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
    for (size_t i = 0; i < pj_object_size(playlist); i++) {
        const char *name = pj_object_key_at(playlist, i);
        const PJValue *events = pj_object_value_at(playlist, i);
        em_mark_block(em, name);
        for (size_t e = 0; e < pj_array_size(events); e++)
            emit_event(em, pj_array_get(events, e));
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

    if (setjmp(err.jmp) == 0) {
        compile_song_internal(&em, song);
        result.success = 1;
        result.byte_count = em.len;
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
