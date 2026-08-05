#include "smps.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---------------------------------------------------------------------
// Byte buffer
// ---------------------------------------------------------------------

static uint8_t *buffer;
static size_t buffer_len, buffer_cap;

static void EnsureCap(size_t extra) {
    if (buffer_len + extra <= buffer_cap)
        return;
    buffer_cap = (buffer_cap == 0) ? 4096 : buffer_cap * 2;
    while (buffer_cap < buffer_len + extra)
        buffer_cap *= 2;
    buffer = (uint8_t *)realloc(buffer, buffer_cap);
}

void SMPS_Byte(uint8_t value) {
    EnsureCap(1);
    buffer[buffer_len++] = value;
}

void SMPS_Word(uint16_t value) {
    SMPS_Byte((uint8_t)(value >> 8));
    SMPS_Byte((uint8_t)value);
}

// ---------------------------------------------------------------------
// Labels and deferred patches
// ---------------------------------------------------------------------

typedef struct {
    char name[128];
    size_t address;
} Label;

static Label labels[4096];
static size_t label_count;

typedef enum { PATCH_SONG_RELATIVE, PATCH_PC_RELATIVE } PatchKind;

typedef struct {
    char name[128];
    size_t word_offset; // Where in `buffer` the 2-byte placeholder lives
    PatchKind kind;
} Patch;

static Patch patches[4096];
static size_t patch_count;

void SMPS_Begin(void) {
    free(buffer);
    buffer = NULL;
    buffer_len = buffer_cap = 0;
    label_count = 0;
    patch_count = 0;
}

void SMPS_Label(const char *name) {
    if (label_count >= sizeof(labels) / sizeof(labels[0])) {
        fprintf(stderr, "smps.c: too many labels\n");
        exit(1);
    }
    strncpy(labels[label_count].name, name, sizeof(labels[0].name) - 1);
    labels[label_count].address = buffer_len;
    label_count++;
}

static size_t FindLabel(const char *name) {
    for (size_t i = 0; i < label_count; i++)
        if (strcmp(labels[i].name, name) == 0)
            return labels[i].address;
    fprintf(stderr, "smps.c: undefined label \"%s\"\n", name);
    exit(1);
}

// Emits a placeholder word and records it for resolution in SMPS_End.
static void RefPatch(const char *loc, PatchKind kind) {
    if (patch_count >= sizeof(patches) / sizeof(patches[0])) {
        fprintf(stderr, "smps.c: too many pending references\n");
        exit(1);
    }
    strncpy(patches[patch_count].name, loc, sizeof(patches[0].name) - 1);
    patches[patch_count].word_offset = buffer_len;
    patches[patch_count].kind = kind;
    patch_count++;
    SMPS_Word(0); // Placeholder, patched in SMPS_End
}

void SMPS_End(const char *out_path, const char *array_name) {
    for (size_t i = 0; i < patch_count; i++) {
        size_t target = FindLabel(patches[i].name);
        size_t word_offset = patches[i].word_offset;
        uint16_t value;

        if (patches[i].kind == PATCH_SONG_RELATIVE) {
            // Matches CheckedChannelPointer for SonicDriverVer==1: dc.w loc-songStart
            value = (uint16_t)(target - 0);
        } else {
            // Matches smpsJump/smpsLoop/smpsCall for SonicDriverVer==1: dc.w loc-*-1
            value = (uint16_t)((int32_t)target - (int32_t)word_offset - 1);
        }

        buffer[word_offset] = (uint8_t)(value >> 8);
        buffer[word_offset + 1] = (uint8_t)value;
    }

    FILE *f = fopen(out_path, "w");
    if (!f) {
        perror(out_path);
        exit(1);
    }

    fprintf(f, "#pragma once\n\n#include <stdint.h>\n\nconst uint8_t %s[] = {\n", array_name);
    for (size_t i = 0; i < buffer_len; i++) {
        fprintf(f, "%s0x%02X,%s", (i % 16 == 0) ? "    " : "", buffer[i], (i % 16 == 15) ? "\n" : " ");
    }
    if (buffer_len % 16 != 0)
        fprintf(f, "\n");
    fprintf(f, "};\n");

    fclose(f);
}

// ---------------------------------------------------------------------
// Header macros (music)
// ---------------------------------------------------------------------

void smpsHeaderStartSong(int driver_version) {
    if (driver_version != 1) {
        fprintf(stderr, "smps.c: only SonicDriverVer=1 is supported\n");
        exit(1);
    }
}

void smpsHeaderVoice(const char *loc) { RefPatch(loc, PATCH_SONG_RELATIVE); }
void smpsHeaderVoiceNull(void) { SMPS_Word(0x0000); }

void smpsHeaderVoiceUVB(void) {
    fprintf(stderr, "smps.c: Universal Voice Bank does not exist in the Sonic 1 driver\n");
    exit(1);
}

void smpsHeaderChan(uint8_t fm, uint8_t psg) {
    SMPS_Byte(fm);
    SMPS_Byte(psg);
}

void smpsHeaderTempo(uint8_t div, uint8_t mod) {
    SMPS_Byte(div);
    SMPS_Byte(mod); // convertMainTempoMod: SonicDriverVer==SourceDriver, so mod is literal
}

void smpsHeaderDAC(const char *loc) {
    RefPatch(loc, PATCH_SONG_RELATIVE);
    SMPS_Word(0x0000); // pitch/vol omitted
}

void smpsHeaderDAC_Full(const char *loc, uint8_t pitch, uint8_t vol) {
    RefPatch(loc, PATCH_SONG_RELATIVE);
    SMPS_Byte(pitch);
    SMPS_Byte(vol);
}

void smpsHeaderFM(const char *loc, uint8_t pitch, uint8_t vol) {
    RefPatch(loc, PATCH_SONG_RELATIVE);
    SMPS_Byte(pitch);
    SMPS_Byte(vol);
}

void smpsHeaderPSG(const char *loc, uint8_t pitch, uint8_t vol, uint8_t mod, uint8_t voice) {
    RefPatch(loc, PATCH_SONG_RELATIVE);
    SMPS_Byte(pitch); // PSGPitchConvert: SonicDriverVer==SourceDriver, so literal
    SMPS_Byte(vol);
    SMPS_Byte(mod);
    SMPS_Byte(voice);
}

// ---------------------------------------------------------------------
// Header macros (SFX)
// ---------------------------------------------------------------------

void smpsHeaderTempoSFX(uint8_t div) { SMPS_Byte(div); }
void smpsHeaderChanSFX(uint8_t chan) { SMPS_Byte(chan); }

void smpsHeaderSFXChannel(uint8_t chanid, const char *loc, uint8_t pitch, uint8_t vol) {
    SMPS_Byte(0x80);
    SMPS_Byte(chanid);
    RefPatch(loc, PATCH_SONG_RELATIVE);
    SMPS_Byte(pitch); // PSGPitchConvert: literal, same driver
    SMPS_Byte(vol);
}

// ---------------------------------------------------------------------
// Coordination flags
// ---------------------------------------------------------------------

void smpsPan(uint8_t direction, uint8_t amsfms) {
    SMPS_Byte(0xE0);
    SMPS_Byte((uint8_t)(direction + amsfms));
}

void smpsDetune(uint8_t val) { SMPS_Byte(0xE1); SMPS_Byte(val); }
void smpsNop(uint8_t val) { SMPS_Byte(0xE2); SMPS_Byte(val); }
void smpsReturn(void) { SMPS_Byte(0xE3); }
void smpsFade(void) { SMPS_Byte(0xE4); }
void smpsChanTempoDiv(uint8_t val) { SMPS_Byte(0xE5); SMPS_Byte(val); }
void smpsAlterVol(uint8_t val) { SMPS_Byte(0xE6); SMPS_Byte(val); }
void smpsNoteFill(uint8_t val) { SMPS_Byte(0xE8); SMPS_Byte(val); }
void smpsChangeTransposition(uint8_t val) { SMPS_Byte(0xE9); SMPS_Byte(val); }

void smpsSetTempoMod(uint8_t mod) {
    SMPS_Byte(0xEA);
    SMPS_Byte(mod); // convertMainTempoMod: literal, same driver
}

void smpsSetTempoDiv(uint8_t val) { SMPS_Byte(0xEB); SMPS_Byte(val); }
void smpsPSGAlterVol(uint8_t vol) { SMPS_Byte(0xEC); SMPS_Byte(vol); }
void smpsClearPush(void) { SMPS_Byte(0xED); }
void smpsStopSpecial(void) { SMPS_Byte(0xEE); }
void smpsFMvoice(uint8_t voice) { SMPS_Byte(0xEF); SMPS_Byte(voice); }

void smpsModSet(uint8_t wait, uint8_t speed, uint8_t change, uint8_t step) {
    SMPS_Byte(0xF0);
    SMPS_Byte(wait);
    SMPS_Byte(speed);
    SMPS_Byte(change);
    SMPS_Byte(step);
}

void smpsModOn(void) { SMPS_Byte(0xF1); }
void smpsStop(void) { SMPS_Byte(0xF2); }
void smpsPSGform(uint8_t form) { SMPS_Byte(0xF3); SMPS_Byte(form); }
void smpsModOff(void) { SMPS_Byte(0xF4); }
void smpsPSGvoice(uint8_t voice) { SMPS_Byte(0xF5); SMPS_Byte(voice); }

void smpsJump(const char *loc) {
    SMPS_Byte(0xF6);
    RefPatch(loc, PATCH_PC_RELATIVE);
}

void smpsLoop(uint8_t index, uint8_t loops, const char *loc) {
    SMPS_Byte(0xF7);
    SMPS_Byte(index);
    SMPS_Byte(loops);
    RefPatch(loc, PATCH_PC_RELATIVE);
}

void smpsCall(const char *loc) {
    SMPS_Byte(0xF8);
    RefPatch(loc, PATCH_PC_RELATIVE);
}

void smpsMaxRelRate(void) { SMPS_Byte(0xF9); }

// ---------------------------------------------------------------------
// FM voice bank
// ---------------------------------------------------------------------

static int vc_feedback, vc_algorithm, vc_unused_bits;
static int vc_d1r_unk[4];
static int vc_dt[4], vc_cf[4], vc_rs[4], vc_ar[4], vc_am[4];
static int vc_d1r[4], vc_d2r[4], vc_dl[4], vc_rr[4];

void smpsVcFeedback(int val) { vc_feedback = val; }
void smpsVcAlgorithm(int val) { vc_algorithm = val; }

void smpsVcUnusedBits(int val, int d1r1, int d1r2, int d1r3, int d1r4) {
    vc_unused_bits = val;
    vc_d1r_unk[0] = d1r1 << 5;
    vc_d1r_unk[1] = d1r2 << 5;
    vc_d1r_unk[2] = d1r3 << 5;
    vc_d1r_unk[3] = d1r4 << 5;
}

#define VC_SETTER4(name, field)                                    \
    void name(int op1, int op2, int op3, int op4) {                \
        field[0] = op1;                                            \
        field[1] = op2;                                            \
        field[2] = op3;                                            \
        field[3] = op4;                                             \
    }

VC_SETTER4(smpsVcDetune, vc_dt)
VC_SETTER4(smpsVcCoarseFreq, vc_cf)
VC_SETTER4(smpsVcRateScale, vc_rs)
VC_SETTER4(smpsVcAttackRate, vc_ar)
VC_SETTER4(smpsVcDecayRate1, vc_d1r)
VC_SETTER4(smpsVcDecayRate2, vc_d2r)
VC_SETTER4(smpsVcDecayLevel, vc_dl)
VC_SETTER4(smpsVcReleaseRate, vc_rr)

void smpsVcAmpMod(int op1, int op2, int op3, int op4) {
    // Matches SourceSMPS2ASM==0 (no source version given to
    // smpsHeaderStartSong, true for every song in this project): op<<5,
    // not the "corrected" op<<7 used when converting from a newer
    // SMPS2ASM release's voice data.
    vc_am[0] = op1 << 5;
    vc_am[1] = op2 << 5;
    vc_am[2] = op3 << 5;
    vc_am[3] = op4 << 5;
}

void smpsVcTotalLevel(int op1, int op2, int op3, int op4) {
    int tl[4] = {op1, op2, op3, op4};

    SMPS_Byte((uint8_t)((vc_unused_bits << 6) + (vc_feedback << 3) + vc_algorithm));

    // SourceSMPS2ASM==0 TL masks (see smps.h's comment on smpsVcAmpMod)
    int tl_mask[4] = {
        128,
        (vc_algorithm >= 5) ? 0x80 : 0,
        (vc_algorithm >= 4) ? 0x80 : 0,
        (vc_algorithm == 7) ? 0x80 : 0,
    };

    // Operator write order for SonicDriverVer != 2: 4,3,2,1
    static const int order[4] = {3, 2, 1, 0}; // 0-based index for op4,op3,op2,op1

    for (int row = 0; row < 3; row++) {
        for (int k = 0; k < 4; k++) {
            int i = order[k];
            uint8_t byte;
            switch (row) {
            case 0: byte = (uint8_t)((vc_dt[i] << 4) + vc_cf[i]); break;
            case 1: byte = (uint8_t)((vc_rs[i] << 6) + vc_ar[i]); break;
            default: byte = (uint8_t)(vc_am[i] | vc_d1r[i] | vc_d1r_unk[i]); break;
            }
            SMPS_Byte(byte);
        }
    }
    for (int k = 0; k < 4; k++)
        SMPS_Byte((uint8_t)vc_d2r[order[k]]);
    for (int k = 0; k < 4; k++) {
        int i = order[k];
        SMPS_Byte((uint8_t)((vc_dl[i] << 4) + vc_rr[i]));
    }
    for (int k = 0; k < 4; k++) {
        int i = order[k];
        SMPS_Byte((uint8_t)(tl[i] | tl_mask[i]));
    }
}
