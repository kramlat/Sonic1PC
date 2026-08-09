#define _POSIX_C_SOURCE 200809L

#include "converter.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ARR_N(a) (sizeof(a) / sizeof((a)[0]))

// ---------------------------------------------------------------------------
// Small string utilities.
// ---------------------------------------------------------------------------

static char *xstrdup(const char *s) { return s ? strdup(s) : NULL; }

static char *trim_dup(const char *s, size_t len) {
    while (len > 0 && isspace((unsigned char)s[0])) { s++; len--; }
    while (len > 0 && isspace((unsigned char)s[len - 1])) len--;
    char *out = (char *)malloc(len + 1);
    memcpy(out, s, len);
    out[len] = '\0';
    return out;
}
static char *trim_dup_cstr(const char *s) { return trim_dup(s, strlen(s)); }

// split_comment -- json_to_header.py-style ';' comment split (no
// strings/escapes to worry about in this format). Neither half is trimmed;
// callers trim as needed, matching asm_to_json.py's own split_comment().
static void split_comment(const char *text, char **code_out, char **comment_out) {
    const char *semi = strchr(text, ';');
    if (!semi) {
        *code_out = xstrdup(text);
        *comment_out = NULL;
        return;
    }
    size_t code_len = (size_t)(semi - text);
    *code_out = (char *)malloc(code_len + 1);
    memcpy(*code_out, text, code_len);
    (*code_out)[code_len] = '\0';
    *comment_out = trim_dup_cstr(semi + 1);
}

// split_args -- comma-separated, whitespace-trimmed. Empty (post-trim) text
// yields zero args.
static void split_args(const char *text, char ***out_args, size_t *out_count) {
    char *t = trim_dup_cstr(text);
    if (t[0] == '\0') {
        free(t);
        *out_args = NULL;
        *out_count = 0;
        return;
    }
    size_t cap = 4, n = 0;
    char **args = (char **)malloc(cap * sizeof(char *));
    char *start = t;
    for (char *p = t;; p++) {
        if (*p == ',' || *p == '\0') {
            char *piece = (char *)malloc((size_t)(p - start) + 1);
            memcpy(piece, start, (size_t)(p - start));
            piece[p - start] = '\0';
            char *trimmed = trim_dup_cstr(piece);
            free(piece);
            if (n == cap) { cap *= 2; args = (char **)realloc(args, cap * sizeof(char *)); }
            args[n++] = trimmed;
            start = p + 1;
            if (*p == '\0') break;
        }
    }
    free(t);
    *out_args = args;
    *out_count = n;
}

static char **split_lines(const char *text, size_t *out_count) {
    size_t cap = 256, n = 0;
    char **lines = (char **)malloc(cap * sizeof(char *));
    const char *p = text;
    while (*p) {
        const char *start = p;
        while (*p && *p != '\n') p++;
        size_t len = (size_t)(p - start);
        if (len > 0 && start[len - 1] == '\r') len--;
        char *line = (char *)malloc(len + 1);
        memcpy(line, start, len);
        line[len] = '\0';
        if (n == cap) { cap *= 2; lines = (char **)realloc(lines, cap * sizeof(char *)); }
        lines[n++] = line;
        if (*p == '\n') p++;
    }
    *out_count = n;
    return lines;
}

// ---------------------------------------------------------------------------
// Line / LineList -- json_to_header.py's Line class + tokenize().
// ---------------------------------------------------------------------------

typedef struct {
    char *label;    // owned or NULL
    char *mnemonic; // owned or NULL
    char **args;    // owned array of owned strings, or NULL
    size_t arg_count;
    char *comment; // owned or NULL (carried but unused downstream, matches Python)
} PSLine;

typedef struct {
    PSLine *items;
    size_t count, cap;
} PSLineList;

static void line_list_push(PSLineList *ll, PSLine line) {
    if (ll->count == ll->cap) {
        ll->cap = ll->cap ? ll->cap * 2 : 64;
        ll->items = (PSLine *)realloc(ll->items, ll->cap * sizeof(PSLine));
    }
    ll->items[ll->count++] = line;
}

static void free_line(PSLine *l) {
    free(l->label);
    free(l->mnemonic);
    free(l->comment);
    for (size_t i = 0; i < l->arg_count; i++) free(l->args[i]);
    free(l->args);
}
static void free_line_list(PSLineList *ll) {
    for (size_t i = 0; i < ll->count; i++) free_line(&ll->items[i]);
    free(ll->items);
}

// ---------------------------------------------------------------------------
// Conditional assembly -- asm_to_json.py:65-119. Only flag this project's
// songs actually reference (found via corpus grep); value matches
// CMakeLists.txt's own FIX_BUGS OFF default, so resolving conditionals
// against this produces the exact branch the shipped game builds with.
// ---------------------------------------------------------------------------

static int assembler_flag_value(const char *name) {
    if (strcmp(name, "FixMusicAndSFXDataBugs") == 0) return 0;
    return 0; // unknown flags default to 0, matching Python's dict.get(flag, 0)
}

static bool is_word_char(char c) { return isalnum((unsigned char)c) || c == '_'; }

static bool eval_condition(const char *expr_raw) {
    // Real conditions are always a bare flag name (truthy check) or
    // `flag=N` -- no more complex expressions appear anywhere in the
    // corpus, so this deliberately doesn't implement a general evaluator.
    char *expr = trim_dup_cstr(expr_raw);
    size_t len = strlen(expr);
    size_t i = 0;
    while (i < len && is_word_char(expr[i])) i++;
    bool matched = false, result = false;
    if (i > 0) {
        size_t name_end = i;
        size_t j = i;
        while (j < len && isspace((unsigned char)expr[j])) j++;
        if (j < len && expr[j] == '=') {
            j++;
            while (j < len && isspace((unsigned char)expr[j])) j++;
            size_t digits_start = j;
            if (j < len && expr[j] == '-') j++;
            size_t after_sign = j;
            while (j < len && isdigit((unsigned char)expr[j])) j++;
            if (j > after_sign && j == len) {
                matched = true;
                char *name = (char *)malloc(name_end + 1);
                memcpy(name, expr, name_end);
                name[name_end] = '\0';
                long value = strtol(expr + digits_start, NULL, 10);
                result = (assembler_flag_value(name) == value);
                free(name);
            }
        }
    }
    if (!matched) result = assembler_flag_value(expr) != 0;
    free(expr);
    return result;
}

static bool match_label(const char *stripped, char **label_out) {
    size_t len = strlen(stripped);
    if (len < 2 || stripped[len - 1] != ':') return false;
    for (size_t i = 0; i < len - 1; i++)
        if (!is_word_char(stripped[i])) return false;
    *label_out = (char *)malloc(len);
    memcpy(*label_out, stripped, len - 1);
    (*label_out)[len - 1] = '\0';
    return true;
}

static bool match_if(const char *stripped, char **cond_out) {
    size_t len = strlen(stripped);
    if (len < 3 || stripped[0] != 'i' || stripped[1] != 'f' || !isspace((unsigned char)stripped[2]))
        return false;
    const char *rest = stripped + 3;
    while (*rest && isspace((unsigned char)*rest)) rest++;
    if (*rest == '\0') return false;
    *cond_out = trim_dup_cstr(rest);
    return true;
}
static bool match_else(const char *stripped) { return strcmp(stripped, "else") == 0; }
static bool match_endif(const char *stripped) { return strcmp(stripped, "endif") == 0; }

static PSLineList tokenize(const char *text) {
    PSLineList ll = {0};
    size_t line_count;
    char **raw_lines = split_lines(text, &line_count);

    bool *active_stack = NULL;
    size_t stack_n = 0, stack_cap = 0;

    for (size_t li = 0; li < line_count; li++) {
        char *code = NULL, *comment = NULL;
        split_comment(raw_lines[li], &code, &comment);
        char *stripped = trim_dup_cstr(code);
        free(code);

        char *cond;
        if (match_if(stripped, &cond)) {
            bool val = eval_condition(cond);
            free(cond);
            if (stack_n == stack_cap) {
                stack_cap = stack_cap ? stack_cap * 2 : 8;
                active_stack = (bool *)realloc(active_stack, stack_cap * sizeof(bool));
            }
            active_stack[stack_n++] = val;
            free(stripped);
            free(comment);
            continue;
        }
        if (match_else(stripped)) {
            if (stack_n > 0) active_stack[stack_n - 1] = !active_stack[stack_n - 1];
            free(stripped);
            free(comment);
            continue;
        }
        if (match_endif(stripped)) {
            if (stack_n > 0) stack_n--;
            free(stripped);
            free(comment);
            continue;
        }
        bool currently_active = true;
        for (size_t k = 0; k < stack_n; k++)
            if (!active_stack[k]) { currently_active = false; break; }
        if (!currently_active) {
            free(stripped);
            free(comment);
            continue;
        }

        if (stripped[0] == '\0') {
            if (comment != NULL) {
                PSLine line = {0};
                line.comment = comment;
                line_list_push(&ll, line);
            } else {
                free(comment);
            }
            free(stripped);
            continue;
        }

        char *label;
        if (match_label(stripped, &label)) {
            PSLine line = {0};
            line.label = label;
            line.comment = comment;
            line_list_push(&ll, line);
            free(stripped);
            continue;
        }

        size_t p = 0, len = strlen(stripped);
        while (p < len && !isspace((unsigned char)stripped[p])) p++;
        char *mnemonic = (char *)malloc(p + 1);
        memcpy(mnemonic, stripped, p);
        mnemonic[p] = '\0';
        char **args = NULL;
        size_t arg_count = 0;
        if (p < len) {
            size_t rest_start = p;
            while (rest_start < len && isspace((unsigned char)stripped[rest_start])) rest_start++;
            split_args(stripped + rest_start, &args, &arg_count);
        }
        PSLine line = {0};
        line.mnemonic = mnemonic;
        line.args = args;
        line.arg_count = arg_count;
        line.comment = comment;
        line_list_push(&ll, line);
        free(stripped);
    }

    free(active_stack);
    for (size_t i = 0; i < line_count; i++) free(raw_lines[i]);
    free(raw_lines);
    return ll;
}

// ---------------------------------------------------------------------------
// Numeric literal handling -- asm_to_json.py:150-178.
// ---------------------------------------------------------------------------

static bool parse_number(const char *tok, long *out) {
    size_t len = strlen(tok);
    if (len >= 1 && tok[0] == '$') {
        char *end;
        long v = strtol(tok + 1, &end, 16);
        if (end != tok + 1 && *end == '\0') { *out = v; return true; }
        return false;
    }
    size_t i = 0;
    if (len > 0 && tok[0] == '-') i = 1;
    if (i >= len) return false;
    for (size_t j = i; j < len; j++)
        if (!isdigit((unsigned char)tok[j])) return false;
    *out = strtol(tok, NULL, 10);
    return true;
}

// hx() -- marks a numeric token for 0x.. string rendering (schema convention:
// C-style hex as a quoted JSON string, since JSON has no hex literal syntax).
// Non-numeric tokens (named constants/block references) pass through as-is.
static PJValue *hx(const char *tok) {
    long n;
    if (!parse_number(tok, &n)) return pj_new_string(tok);
    char buf[64];
    if (n >= 0)
        snprintf(buf, sizeof(buf), "0x%lX", n);
    else
        snprintf(buf, sizeof(buf), "%ld", n);
    return pj_new_string(buf);
}

// ---------------------------------------------------------------------------
// Macro catalog -- asm_to_json.py:188-243.
// ---------------------------------------------------------------------------

static const char *ZERO_ARG[] = {
    "smpsStop", "smpsClearPush", "smpsStopSpecial", "smpsWeirdD1LRR", "smpsFade", "smpsReturn", "smpsNoAttack",
};
static const char *SINGLE_ARG[] = {
    "smpsChanTempoDiv", "smpsSetTempoDiv", "smpsSetTempoMod", "smpsAlterPitch", "smpsNop",
    "smpsAlterVol",     "smpsPSGAlterVol", "smpsAlterNote",   "smpsNoteFill",   "smpsPSGform",
    "smpsPSGvoice",     "smpsSetvoice",
};
static const char *MULTI_ARG[] = {"smpsPan", "smpsModSet"};
static const char *HEADER_MACROS[] = {
    "smpsHeaderStartSong", "smpsHeaderVoice",    "smpsHeaderChan",       "smpsHeaderTempo",
    "smpsHeaderDAC",       "smpsHeaderFM",       "smpsHeaderPSG",        "smpsHeaderTempoSFX",
    "smpsHeaderChanSFX",   "smpsHeaderSFXChannel",
};

static bool str_in(const char *s, const char **set, size_t n) {
    for (size_t i = 0; i < n; i++)
        if (strcmp(s, set[i]) == 0) return true;
    return false;
}

static PJValue *args_to_json(char **args, size_t count) {
    PJValue *arr = pj_new_array();
    for (size_t i = 0; i < count; i++) pj_array_append(arr, hx(args[i]));
    return arr;
}

// convert_instruction -- asm_to_json.py:289-305.
static PJValue *convert_instruction(const char *mnemonic, char **args, size_t arg_count) {
    if (str_in(mnemonic, ZERO_ARG, ARR_N(ZERO_ARG))) return pj_new_string(mnemonic);
    if (str_in(mnemonic, SINGLE_ARG, ARR_N(SINGLE_ARG))) {
        PJValue *obj = pj_new_object();
        pj_object_set(obj, mnemonic, arg_count > 0 ? hx(args[0]) : pj_new_null());
        return obj;
    }
    if (str_in(mnemonic, MULTI_ARG, ARR_N(MULTI_ARG))) {
        PJValue *obj = pj_new_object();
        pj_object_set(obj, mnemonic, args_to_json(args, arg_count));
        return obj;
    }
    if (strcmp(mnemonic, "smpsModOn") == 0) {
        PJValue *obj = pj_new_object();
        pj_object_set(obj, "smpsMod", pj_new_bool(true));
        return obj;
    }
    if (strcmp(mnemonic, "smpsModOff") == 0) {
        PJValue *obj = pj_new_object();
        pj_object_set(obj, "smpsMod", pj_new_bool(false));
        return obj;
    }
    // Unknown mnemonic -- pass through raw rather than silently dropping data.
    PJValue *obj = pj_new_object();
    pj_object_set(obj, "_unrecognized_macro", pj_new_string(mnemonic));
    pj_object_set(obj, "_args", args_to_json(args, arg_count));
    return obj;
}

// ---------------------------------------------------------------------------
// StrSet -- small ordered set of owned strings (label sets throughout).
// ---------------------------------------------------------------------------

typedef struct {
    char **items;
    size_t count, cap;
} StrSet;

static bool strset_contains(const StrSet *s, const char *v) {
    for (size_t i = 0; i < s->count; i++)
        if (strcmp(s->items[i], v) == 0) return true;
    return false;
}
static void strset_add(StrSet *s, const char *v) {
    if (strset_contains(s, v)) return;
    if (s->count == s->cap) {
        s->cap = s->cap ? s->cap * 2 : 16;
        s->items = (char **)realloc(s->items, s->cap * sizeof(char *));
    }
    s->items[s->count++] = xstrdup(v);
}
static void strset_free(StrSet *s) {
    for (size_t i = 0; i < s->count; i++) free(s->items[i]);
    free(s->items);
    s->items = NULL;
    s->count = s->cap = 0;
}

// ---------------------------------------------------------------------------
// Control-flow folding -- asm_to_json.py:308-364. See that function's own
// (extensive) correctness comment on the persistent-position-table
// discipline this replicates exactly.
// ---------------------------------------------------------------------------

typedef enum { FI_LABEL, FI_EVENT, FI_LOOP, FI_JUMP } FlatKind;
typedef struct {
    FlatKind kind;
    char *label;    // LABEL name, or LOOP/JUMP target
    PJValue *event; // EVENT payload (owned)
    PJValue *idx;   // LOOP index (owned, already hx()'d)
    PJValue *cnt;   // LOOP count (owned, already hx()'d)
} FlatItem;
typedef struct {
    FlatItem *items;
    size_t count, cap;
} FlatList;

static void flat_push(FlatList *fl, FlatItem it) {
    if (fl->count == fl->cap) {
        fl->cap = fl->cap ? fl->cap * 2 : 64;
        fl->items = (FlatItem *)realloc(fl->items, fl->cap * sizeof(FlatItem));
    }
    fl->items[fl->count++] = it;
}
static void free_flat_list(FlatList *fl) {
    for (size_t i = 0; i < fl->count; i++) {
        FlatItem *fi = &fl->items[i];
        free(fi->label);
        if (fi->event) pj_free(fi->event);
        if (fi->idx) pj_free(fi->idx);
        if (fi->cnt) pj_free(fi->cnt);
    }
    free(fl->items);
}

typedef struct {
    char *name;
    size_t pos;
} LabelPos;
typedef struct {
    LabelPos *items;
    size_t count, cap;
} LabelPosTable;
static bool labelpos_find(LabelPosTable *t, const char *name, size_t *pos_out) {
    for (size_t i = 0; i < t->count; i++)
        if (strcmp(t->items[i].name, name) == 0) { *pos_out = t->items[i].pos; return true; }
    return false;
}
static void labelpos_set(LabelPosTable *t, const char *name, size_t pos) {
    for (size_t i = 0; i < t->count; i++)
        if (strcmp(t->items[i].name, name) == 0) { t->items[i].pos = pos; return; }
    if (t->count == t->cap) {
        t->cap = t->cap ? t->cap * 2 : 32;
        t->items = (LabelPos *)realloc(t->items, t->cap * sizeof(LabelPos));
    }
    t->items[t->count].name = xstrdup(name);
    t->items[t->count].pos = pos;
    t->count++;
}
static void labelpos_free(LabelPosTable *t) {
    for (size_t i = 0; i < t->count; i++) free(t->items[i].name);
    free(t->items);
    t->items = NULL;
    t->count = t->cap = 0;
}

static PJValue *fold_control_flow(FlatList *flat, const char *block_name) {
    PJValue **items = NULL;
    size_t items_len = 0, items_cap = 0;
#define ITEMS_PUSH(v)                                                                                                \
    do {                                                                                                             \
        if (items_len == items_cap) {                                                                               \
            items_cap = items_cap ? items_cap * 2 : 64;                                                             \
            items = (PJValue **)realloc(items, items_cap * sizeof(PJValue *));                                      \
        }                                                                                                            \
        items[items_len++] = (v);                                                                                   \
    } while (0)

    LabelPosTable table = {0};

    for (size_t i = 0; i < flat->count; i++) {
        FlatItem *fi = &flat->items[i];
        if (fi->kind == FI_LABEL) {
            labelpos_set(&table, fi->label, items_len);
        } else if (fi->kind == FI_EVENT) {
            ITEMS_PUSH(fi->event);
            fi->event = NULL;
        } else if (fi->kind == FI_LOOP) {
            size_t start;
            bool found = labelpos_find(&table, fi->label, &start);
            if (!found || start > items_len) {
                fprintf(stderr,
                        "WARNING: %s: smpsLoop target '%s' has no matching label in this block "
                        "-- emitting flat jumpTo fallback, needs manual review\n",
                        block_name, fi->label);
                PJValue *jt = pj_new_object();
                pj_object_set(jt, "jumpTo", pj_new_string(fi->label));
                PJValue *loopArgs = pj_new_array();
                pj_array_append(loopArgs, fi->idx);
                fi->idx = NULL;
                pj_array_append(loopArgs, fi->cnt);
                fi->cnt = NULL;
                pj_object_set(jt, "smpsLoopArgs", loopArgs);
                ITEMS_PUSH(jt);
            } else {
                PJValue *loopArr = pj_new_array();
                pj_array_append(loopArr, fi->idx);
                fi->idx = NULL;
                pj_array_append(loopArr, fi->cnt);
                fi->cnt = NULL;
                for (size_t k = start; k < items_len; k++) pj_array_append(loopArr, items[k]);
                items_len = start;
                PJValue *loopObj = pj_new_object();
                pj_object_set(loopObj, "smpsLoop", loopArr);
                ITEMS_PUSH(loopObj);
                // table entry for fi->label intentionally left pointing at
                // `start` -- a later re-reference should scoop up this fold
                // result too (see the function-level correctness comment).
            }
        } else if (fi->kind == FI_JUMP) {
            size_t start;
            bool found = labelpos_find(&table, fi->label, &start);
            if (!found || start > items_len) {
                fprintf(stderr,
                        "WARNING: %s: smpsJump target '%s' has no matching label in this block "
                        "-- emitting flat jumpTo fallback, needs manual review\n",
                        block_name, fi->label);
                PJValue *jt = pj_new_object();
                pj_object_set(jt, "jumpTo", pj_new_string(fi->label));
                ITEMS_PUSH(jt);
            } else {
                PJValue *jumpArr = pj_new_array();
                for (size_t k = start; k < items_len; k++) pj_array_append(jumpArr, items[k]);
                items_len = start;
                PJValue *jumpObj = pj_new_object();
                pj_object_set(jumpObj, "smpsJump", jumpArr);
                ITEMS_PUSH(jumpObj);
            }
        }
    }

    PJValue *result = pj_new_array();
    for (size_t i = 0; i < items_len; i++) pj_array_append(result, items[i]);
    free(items);
    labelpos_free(&table);
#undef ITEMS_PUSH
    return result;
}

// ---------------------------------------------------------------------------
// Header parsing -- asm_to_json.py:372-385.
// ---------------------------------------------------------------------------

static PJValue *parse_header(const PSLineList *lines, size_t *idx_inout) {
    PJValue *header = pj_new_array();
    size_t idx = *idx_inout;
    while (idx < lines->count) {
        const PSLine *line = &lines->items[idx];
        if (line->label != NULL) break;
        if (line->mnemonic == NULL) { idx++; continue; }
        if (!str_in(line->mnemonic, HEADER_MACROS, ARR_N(HEADER_MACROS))) break;
        PJValue *entry = pj_new_object();
        pj_object_set(entry, line->mnemonic, args_to_json(line->args, line->arg_count));
        pj_array_append(header, entry);
        idx++;
    }
    *idx_inout = idx;
    return header;
}

// ---------------------------------------------------------------------------
// Voice bank parsing -- asm_to_json.py:388-426.
// ---------------------------------------------------------------------------

static const char *VOICE_ORDER[] = {
    "smpsVcAlgorithm",  "smpsVcFeedback",   "smpsVcUnusedBits",  "smpsVcDetune",     "smpsVcCoarseFreq",
    "smpsVcRateScale",  "smpsVcAttackRate", "smpsVcAmpMod",      "smpsVcDecayRate1", "smpsVcDecayRate2",
    "smpsVcDecayLevel", "smpsVcReleaseRate", "smpsVcTotalLevel",
};

static size_t skip_blank(const PSLineList *lines, size_t idx) {
    while (idx < lines->count && lines->items[idx].mnemonic == NULL && lines->items[idx].label == NULL) idx++;
    return idx;
}

typedef struct {
    const char *key;
    const PSLine *line;
} VoiceGroupEntry;

// lines[start_idx] should be the voice bank's own label; voices follow as
// repeated 13-macro groups until a non-voice mnemonic or EOF. Real data
// always writes exactly 4 comma-separated args (one per operator) on each
// VOICE_MULTI line, so per-operator extraction is direct indexing here
// (Python would raise IndexError on malformed data at the same point; this
// defaults to "$0" instead -- a stricter-than-Python divergence that never
// triggers on any file in the real corpus, verified by byte/text diff).
static PJValue *parse_voice_bank(const PSLineList *lines, size_t start_idx, size_t *idx_out) {
    size_t idx = skip_blank(lines, start_idx + 1);
    PJValue *voices = pj_new_array();
    while (true) {
        idx = skip_blank(lines, idx);
        if (idx >= lines->count || lines->items[idx].mnemonic == NULL ||
            strcmp(lines->items[idx].mnemonic, "smpsVcAlgorithm") != 0)
            break;

        VoiceGroupEntry group[ARR_N(VOICE_ORDER)];
        size_t group_n = 0;
        for (size_t oi = 0; oi < ARR_N(VOICE_ORDER); oi++) {
            if (idx >= lines->count || lines->items[idx].mnemonic == NULL ||
                strcmp(lines->items[idx].mnemonic, VOICE_ORDER[oi]) != 0)
                break;
            group[group_n].key = VOICE_ORDER[oi];
            group[group_n].line = &lines->items[idx];
            group_n++;
            idx++;
        }

        const char *alg_tok = "$0", *fb_tok = "$0", *ub_tok = "$0";
        PJValue *ops[4];
        for (int i = 0; i < 4; i++) ops[i] = pj_new_object();
        for (size_t gi = 0; gi < group_n; gi++) {
            const char *key = group[gi].key;
            const PSLine *line = group[gi].line;
            if (strcmp(key, "smpsVcAlgorithm") == 0) { if (line->arg_count > 0) alg_tok = line->args[0]; continue; }
            if (strcmp(key, "smpsVcFeedback") == 0) { if (line->arg_count > 0) fb_tok = line->args[0]; continue; }
            if (strcmp(key, "smpsVcUnusedBits") == 0) { if (line->arg_count > 0) ub_tok = line->args[0]; continue; }
            for (int op_i = 0; op_i < 4; op_i++) {
                const char *tok = ((size_t)op_i < line->arg_count) ? line->args[op_i] : "$0";
                pj_object_set(ops[op_i], key, hx(tok));
            }
        }

        PJValue *voice = pj_new_object();
        pj_object_set(voice, "smpsVcAlgorithm", hx(alg_tok));
        pj_object_set(voice, "smpsVcFeedback", hx(fb_tok));
        pj_object_set(voice, "smpsVcUnusedBits", hx(ub_tok));
        PJValue *opsArr = pj_new_array();
        for (int i = 0; i < 4; i++) pj_array_append(opsArr, ops[i]);
        pj_object_set(voice, "operators", opsArr);
        pj_array_append(voices, voice);
    }
    *idx_out = idx;
    return voices;
}

// ---------------------------------------------------------------------------
// find_header_start_labels -- asm_to_json.py:434-442.
// ---------------------------------------------------------------------------

static StrSet find_header_start_labels(const PJValue *header) {
    StrSet starts = {0};
    for (size_t i = 0; i < pj_array_size(header); i++) {
        const PJValue *entry = pj_array_get(header, i);
        const char *macro = pj_object_first_key(entry);
        if (!macro) continue;
        const PJValue *args = pj_object_get(entry, macro);
        if ((strcmp(macro, "smpsHeaderFM") == 0 || strcmp(macro, "smpsHeaderPSG") == 0 ||
             strcmp(macro, "smpsHeaderDAC") == 0) &&
            pj_array_size(args) > 0) {
            strset_add(&starts, pj_get_string(pj_array_get(args, 0), ""));
        } else if (strcmp(macro, "smpsHeaderSFXChannel") == 0 && pj_array_size(args) > 1) {
            strset_add(&starts, pj_get_string(pj_array_get(args, 1), ""));
        }
    }
    return starts;
}

// ---------------------------------------------------------------------------
// NoteStreamState -- asm_to_json.py:445-503.
// ---------------------------------------------------------------------------

typedef struct {
    char *last_note; // owned or NULL; persists across dc.b/dc.w lines within one block
} NoteStreamState;

static NoteStreamState note_state_new(void) {
    NoteStreamState s = {0};
    return s;
}
static void note_state_free(NoteStreamState *s) {
    free(s->last_note);
    s->last_note = NULL;
}

static bool is_note_token(const char *tok) {
    if (tok[0] != 'n' && tok[0] != 'd') return false;
    return isalnum((unsigned char)tok[1]) != 0;
}
static bool is_number_token(const char *tok) {
    long dummy;
    return parse_number(tok, &dummy);
}

// consume() -- note/duration/tie folding for one dc.b/dc.w line's tokens.
// `local_last` tracks only events produced by THIS call (a fresh local list
// in Python, re-created per call) -- matching that scoping exactly matters:
// the smpsNoAttack-then-bare-duration "tie" case only fires when both
// tokens are on the SAME source line, not across separate dc.b lines.
static void note_state_consume(NoteStreamState *st, char **tokens, size_t count, FlatList *out) {
    PJValue *local_last = NULL;
    size_t i = 0;
    while (i < count) {
        const char *tok = tokens[i];
        if (is_note_token(tok)) {
            free(st->last_note);
            st->last_note = xstrdup(tok);
            PJValue *ev = pj_new_object();
            pj_object_set(ev, "note", pj_new_string(tok + 1));
            if (i + 1 < count && is_number_token(tokens[i + 1])) {
                pj_object_set(ev, "duration", hx(tokens[i + 1]));
                i += 2;
            } else {
                i += 1;
            }
            FlatItem fi = {0};
            fi.kind = FI_EVENT;
            fi.event = ev;
            flat_push(out, fi);
            local_last = ev;
        } else if (is_number_token(tok)) {
            bool prev_is_noattack =
                local_last && pj_type(local_last) == PJ_STRING && strcmp(pj_get_string(local_last, ""), "smpsNoAttack") == 0;
            PJValue *ev = pj_new_object();
            if (prev_is_noattack) {
                // NoAttack suppresses the retrigger and there's no note
                // byte at all -- real hardware extends whatever note is
                // ALREADY sounding by this many more ticks.
                pj_object_set(ev, "tie", pj_new_bool(true));
                pj_object_set(ev, "duration", hx(tok));
            } else if (st->last_note == NULL) {
                // Bare duration with no local note anywhere in this block --
                // this block is a smpsCall target and inherits its pitch
                // from the calling context at playback time.
                pj_object_set(ev, "inheritedNote", pj_new_bool(true));
                pj_object_set(ev, "duration", hx(tok));
            } else {
                pj_object_set(ev, "note", pj_new_string(st->last_note + 1));
                pj_object_set(ev, "duration", hx(tok));
            }
            FlatItem fi = {0};
            fi.kind = FI_EVENT;
            fi.event = ev;
            flat_push(out, fi);
            local_last = ev;
            i += 1;
        } else if (strcmp(tok, "smpsNoAttack") == 0) {
            PJValue *ev = pj_new_string("smpsNoAttack");
            FlatItem fi = {0};
            fi.kind = FI_EVENT;
            fi.event = ev;
            flat_push(out, fi);
            local_last = ev;
            i += 1;
        } else {
            PJValue *ev = pj_new_object();
            pj_object_set(ev, "_unrecognized_token", pj_new_string(tok));
            FlatItem fi = {0};
            fi.kind = FI_EVENT;
            fi.event = ev;
            flat_push(out, fi);
            local_last = ev;
            i += 1;
        }
    }
}

// ---------------------------------------------------------------------------
// Cross-segment jump-target promotion -- asm_to_json.py:506-568. See that
// function's own comment for why this fixpoint iteration is needed (real
// SMPS tracks sometimes jump straight into a label physically inside a
// DIFFERENT channel's already-written data).
// ---------------------------------------------------------------------------

typedef void (*OnLabelFn)(void *ud, const char *name, const char *current_segment);
typedef void (*OnJumpFn)(void *ud, const char *current_segment, const char *target);

static void walk_segments(const PSLineList *lines, size_t idx, size_t voice_bank_idx, bool has_vb,
                           const StrSet *top_level_labels, OnLabelFn on_label, OnJumpFn on_jump, void *ud) {
    const char *current = NULL;
    size_t i = idx;
    while (i < lines->count) {
        const PSLine *line = &lines->items[i];
        if (has_vb && i == voice_bank_idx) {
            size_t next_i;
            PJValue *discard = parse_voice_bank(lines, voice_bank_idx, &next_i);
            pj_free(discard);
            i = next_i;
            continue;
        }
        if (line->label != NULL) {
            if (strset_contains(top_level_labels, line->label)) current = line->label;
            on_label(ud, line->label, current);
            i++;
            continue;
        }
        if (line->mnemonic && strcmp(line->mnemonic, "smpsJump") == 0 && line->arg_count > 0) {
            on_jump(ud, current, line->args[0]);
        } else if (line->mnemonic && strcmp(line->mnemonic, "smpsLoop") == 0 && line->arg_count >= 3) {
            on_jump(ud, current, line->args[2]);
        }
        i++;
    }
}

typedef struct {
    char *label;
    char *segment; // NULL allowed (label defined before any top-level label seen)
} LabelSegEntry;
typedef struct {
    LabelSegEntry *items;
    size_t count, cap;
} LabelSegMap;
static void labelseg_set(LabelSegMap *m, const char *label, const char *segment) {
    for (size_t i = 0; i < m->count; i++)
        if (strcmp(m->items[i].label, label) == 0) {
            free(m->items[i].segment);
            m->items[i].segment = segment ? xstrdup(segment) : NULL;
            return;
        }
    if (m->count == m->cap) {
        m->cap = m->cap ? m->cap * 2 : 32;
        m->items = (LabelSegEntry *)realloc(m->items, m->cap * sizeof(LabelSegEntry));
    }
    m->items[m->count].label = xstrdup(label);
    m->items[m->count].segment = segment ? xstrdup(segment) : NULL;
    m->count++;
}
// Returns NULL both when `label` isn't a key at all AND when it is a key
// mapped to a NULL segment -- matching Python's `dict.get(target)` (None
// either way), which the one caller below treats identically in both cases.
static const char *labelseg_get(LabelSegMap *m, const char *label) {
    for (size_t i = 0; i < m->count; i++)
        if (strcmp(m->items[i].label, label) == 0) return m->items[i].segment;
    return NULL;
}
static void labelseg_free(LabelSegMap *m) {
    for (size_t i = 0; i < m->count; i++) { free(m->items[i].label); free(m->items[i].segment); }
    free(m->items);
    m->items = NULL;
    m->count = m->cap = 0;
}

static bool str_eq_or_null(const char *a, const char *b) {
    if (a == NULL && b == NULL) return true;
    if (a == NULL || b == NULL) return false;
    return strcmp(a, b) == 0;
}

static void build_map_on_label(void *ud, const char *name, const char *seg) { labelseg_set((LabelSegMap *)ud, name, seg); }
static void build_map_on_jump(void *ud, const char *seg, const char *target) { (void)ud; (void)seg; (void)target; }

typedef struct {
    LabelSegMap *map;
    StrSet *extra;
} CollectUD;
static void collect_on_label(void *ud, const char *name, const char *seg) { (void)ud; (void)name; (void)seg; }
static void collect_on_jump(void *ud, const char *seg, const char *target) {
    CollectUD *c = (CollectUD *)ud;
    const char *target_seg = labelseg_get(c->map, target);
    if (target_seg != NULL && !str_eq_or_null(target_seg, seg)) strset_add(c->extra, target);
}

static StrSet promote_cross_segment_targets(const PSLineList *lines, size_t idx, size_t voice_bank_idx, bool has_vb,
                                             const StrSet *initial_top_level) {
    StrSet top = {0};
    for (size_t i = 0; i < initial_top_level->count; i++) strset_add(&top, initial_top_level->items[i]);

    while (true) {
        LabelSegMap map = {0};
        walk_segments(lines, idx, voice_bank_idx, has_vb, &top, build_map_on_label, build_map_on_jump, &map);

        StrSet extra = {0};
        CollectUD cud = {&map, &extra};
        walk_segments(lines, idx, voice_bank_idx, has_vb, &top, collect_on_label, collect_on_jump, &cud);
        labelseg_free(&map);

        StrSet new_labels = {0};
        for (size_t i = 0; i < extra.count; i++)
            if (!strset_contains(&top, extra.items[i])) strset_add(&new_labels, extra.items[i]);
        strset_free(&extra);

        if (new_labels.count == 0) {
            strset_free(&new_labels);
            return top;
        }
        for (size_t i = 0; i < new_labels.count; i++) strset_add(&top, new_labels.items[i]);
        strset_free(&new_labels);
    }
}

// ---------------------------------------------------------------------------
// Main driver -- asm_to_json.py:571-676.
// ---------------------------------------------------------------------------

PSConvertResult ps_convert_asm(const char *text) {
    PSConvertResult result;
    memset(&result, 0, sizeof(result));

    PSLineList lines = tokenize(text);

    size_t idx = 0;
    while (idx < lines.count && lines.items[idx].mnemonic == NULL && lines.items[idx].label == NULL) idx++;
    if (idx < lines.count && lines.items[idx].label != NULL) idx++;

    PJValue *header = parse_header(&lines, &idx);
    StrSet top_level = find_header_start_labels(header);

    char *voice_bank_label = NULL;
    for (size_t i = 0; i < pj_array_size(header); i++) {
        const PJValue *entry = pj_array_get(header, i);
        const PJValue *vb = pj_object_get(entry, "smpsHeaderVoice");
        if (vb && pj_array_size(vb) > 0) {
            free(voice_bank_label);
            voice_bank_label = xstrdup(pj_get_string(pj_array_get(vb, 0), ""));
        }
    }

    for (size_t i = idx; i < lines.count; i++) {
        const PSLine *line = &lines.items[i];
        if (line->mnemonic && strcmp(line->mnemonic, "smpsCall") == 0 && line->arg_count > 0)
            strset_add(&top_level, line->args[0]);
    }

    bool has_vb = false;
    size_t voice_bank_idx = 0;
    if (voice_bank_label) {
        for (size_t i = idx; i < lines.count; i++) {
            if (lines.items[i].label && strcmp(lines.items[i].label, voice_bank_label) == 0) {
                voice_bank_idx = i;
                has_vb = true;
                break;
            }
        }
    }

    StrSet promoted = promote_cross_segment_targets(&lines, idx, voice_bank_idx, has_vb, &top_level);
    strset_free(&top_level);
    top_level = promoted;

    PJValue *voices;
    if (has_vb) {
        size_t discard_idx;
        voices = parse_voice_bank(&lines, voice_bank_idx, &discard_idx);
    } else {
        voices = pj_new_array();
    }

    PJValue *playlist = pj_new_object();

    char *current_name = NULL; // borrowed pointer into `lines`' label storage
    FlatList current_flat = {0};
    NoteStreamState note_state = note_state_new();

#define FLUSH()                                                                                                      \
    do {                                                                                                              \
        if (current_name != NULL) {                                                                                   \
            PJValue *folded = fold_control_flow(&current_flat, current_name);                                         \
            pj_object_set(playlist, current_name, folded);                                                            \
        }                                                                                                              \
        free_flat_list(&current_flat);                                                                                 \
        current_flat = (FlatList){0};                                                                                  \
        note_state_free(&note_state);                                                                                  \
        note_state = note_state_new();                                                                                 \
    } while (0)

    size_t i = idx;
    while (i < lines.count) {
        const PSLine *line = &lines.items[i];
        if (has_vb && i == voice_bank_idx) {
            size_t next_i;
            PJValue *discard = parse_voice_bank(&lines, voice_bank_idx, &next_i);
            pj_free(discard);
            i = next_i;
            continue;
        }
        if (line->label != NULL) {
            if (strset_contains(&top_level, line->label)) {
                FLUSH();
                current_name = line->label;
            }
            FlatItem fi = {0};
            fi.kind = FI_LABEL;
            fi.label = xstrdup(line->label);
            flat_push(&current_flat, fi);
            i++;
            continue;
        }
        if (line->mnemonic == NULL) { i++; continue; }
        if (strcmp(line->mnemonic, "dc.b") == 0 || strcmp(line->mnemonic, "dc.w") == 0) {
            note_state_consume(&note_state, line->args, line->arg_count, &current_flat);
        } else if (strcmp(line->mnemonic, "smpsLoop") == 0) {
            if (line->arg_count >= 3) {
                FlatItem fi = {0};
                fi.kind = FI_LOOP;
                fi.idx = hx(line->args[0]);
                fi.cnt = hx(line->args[1]);
                fi.label = xstrdup(line->args[2]);
                flat_push(&current_flat, fi);
            }
        } else if (strcmp(line->mnemonic, "smpsJump") == 0) {
            if (line->arg_count >= 1) {
                FlatItem fi = {0};
                fi.kind = FI_JUMP;
                fi.label = xstrdup(line->args[0]);
                flat_push(&current_flat, fi);
            }
        } else if (strcmp(line->mnemonic, "smpsCall") == 0) {
            if (line->arg_count >= 1) {
                PJValue *ev = pj_new_object();
                pj_object_set(ev, "smpsCall", pj_new_string(line->args[0]));
                FlatItem fi = {0};
                fi.kind = FI_EVENT;
                fi.event = ev;
                flat_push(&current_flat, fi);
            }
        } else if (strcmp(line->mnemonic, "smpsReturn") == 0) {
            // block end is the implicit return -- no event emitted, per schema
        } else {
            PJValue *ev = convert_instruction(line->mnemonic, line->args, line->arg_count);
            FlatItem fi = {0};
            fi.kind = FI_EVENT;
            fi.event = ev;
            flat_push(&current_flat, fi);
        }
        i++;
    }
    FLUSH();
#undef FLUSH

    free(voice_bank_label);
    strset_free(&top_level);
    free_line_list(&lines);

    PJValue *root = pj_new_object();
    pj_object_set(root, "header", header);
    pj_object_set(root, "voices", voices);
    pj_object_set(root, "SMPSplaylist", playlist);

    result.success = 1;
    result.result = root;
    return result;
}

void ps_convert_result_free(PSConvertResult *result) {
    if (result->result) pj_free(result->result);
    result->result = NULL;
}
