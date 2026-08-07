#pragma once

// smps.h -- a C dialect of smps2asm.asm (Flamewing's SMPS2ASM, based on
// S1SMPS2ASM 1.1 by Marc "Cinossu" Gordon), specialized to
// SonicDriverVer=1/SourceDriver=1 (the real Sonic 1 sound driver -- no
// cross-driver conversion logic needed, since that's the only
// configuration this project's songs ever use). Each macro here reproduces
// exactly the bytes the real assembly macro of the same name would emit in
// that configuration.
//
// Translated song files (see smps2asmc) are small standalone C programs:
// they #include this header, call these functions in the same order and
// with the same arguments as the original .asm file's macro invocations,
// mark label positions with SMPS_Label(), and finish with SMPS_End() to
// resolve every forward/backward reference and write out a resource .h
// (the same "const uint8_t name[] = {...};" shape as every other resource
// in this project, e.g. bin2h's output).

#include <stdint.h>

// ---------------------------------------------------------------------
// Byte stream / label API
// ---------------------------------------------------------------------

// Starts a new song encode -- call first, before anything else.
void SMPS_Begin(void);

// Marks the current write position under `name`, resolving it against any
// earlier forward references and making it available to later ones.
void SMPS_Label(const char *name);

// Raw byte/word emission (big-endian, matching every other resource in
// this project). Used directly for `dc.b`/`dc.w` lines in the original
// (note lists, literal bytes) that don't correspond to a named macro.
void SMPS_Byte(uint8_t value);
void SMPS_Word(uint16_t value);

// Resolves every deferred label reference and writes the finished song as
// a C header resource: `const uint8_t <array_name>[] = { ... };`.
void SMPS_End(const char *out_path, const char *array_name);

// ---------------------------------------------------------------------
// Header macros (music) -- smpsHeaderStartSong through smpsHeaderPSG
// ---------------------------------------------------------------------

void smpsHeaderStartSong(int driver_version); // Just a version assertion (must be 1) -- emits nothing
void smpsHeaderStartSong2(int driver_version, int source_driver); // 2-arg form -- both must be 1
void smpsHeaderVoice(const char *loc);        // dc.w loc-songStart
void smpsHeaderVoiceNull(void);               // dc.w $0000
void smpsHeaderVoiceUVB(void);                // Not valid for driver 1 -- fatal error if called
void smpsHeaderChan(uint8_t fm, uint8_t psg);
void smpsHeaderTempo(uint8_t div, uint8_t mod);
void smpsHeaderDAC(const char *loc);                                    // pitch/vol omitted (every real usage does this)
void smpsHeaderDAC_Full(const char *loc, uint8_t pitch, uint8_t vol);   // pitch/vol given
void smpsHeaderFM(const char *loc, uint8_t pitch, uint8_t vol);
void smpsHeaderPSG(const char *loc, uint8_t pitch, uint8_t vol, uint8_t mod, uint8_t voice);

// ---------------------------------------------------------------------
// Header macros (SFX) -- smpsHeaderTempoSFX through smpsHeaderSFXChannel
// ---------------------------------------------------------------------

void smpsHeaderTempoSFX(uint8_t div);
void smpsHeaderChanSFX(uint8_t chan);
void smpsHeaderSFXChannel(uint8_t chanid, const char *loc, uint8_t pitch, uint8_t vol);

// SFX channel IDs, for the chanid argument above
#define cPSG1  0x80
#define cPSG2  0xA0
#define cPSG3  0xC0
#define cNoise 0xE0
#define cFM3   0x02
#define cFM4   0x04
#define cFM5   0x05
#define cFM6   0x06 // Only in S3/S&K/S3D, overrides DAC -- unused by driver 1
// channels in this project's songs.

// ---------------------------------------------------------------------
// Coordination flag macros ($E0-$F9 -- the $FA-$FF S3/S&K/S3D-only flags
// are intentionally not implemented, since driver 1 songs never use them)
// ---------------------------------------------------------------------

#define panNone   0x00
#define panRight  0x40
#define panLeft   0x80
#define panCentre 0xC0
#define panCenter 0xC0

// Highest PSG tone-channel pitch (SonicDriverVer<=2: nA5) -- used by songs
// to cap frequency envelope sweeps.
#define nMaxPSG nA5

// DAC sample IDs (driver-1 "DAC Equates" case). These are the drum/sample
// values a DAC track's note bytes hold (0x81+), distinct from the PSG/FM
// note-name enum above.
enum {
    dKick = 0x81, dSnare, dTimpani,
    dHiTimpani = 0x88, dMidTimpani, dLowTimpani, dVLowTimpani,
};

void smpsPan(uint8_t direction, uint8_t amsfms);        // $E0
void smpsDetune(uint8_t val);                            // $E1
#define smpsAlterNote smpsDetune
void smpsNop(uint8_t val);                                // $E2
void smpsReturn(void);                                    // $E3
void smpsFade(void);                                       // $E4
void smpsChanTempoDiv(uint8_t val);                        // $E5
void smpsAlterVol(uint8_t val);                             // $E6
#define smpsNoAttack 0xE7                                    // Plain data byte, not a macro call
void smpsNoteFill(uint8_t val);                              // $E8
void smpsChangeTransposition(uint8_t val);                    // $E9
#define smpsAlterPitch smpsChangeTransposition
void smpsSetTempoMod(uint8_t mod);                             // $EA
void smpsSetTempoDiv(uint8_t val);                              // $EB
// smpsSetVol ($EC on driver>=3 only) intentionally omitted -- fatal on driver 1
void smpsPSGAlterVol(uint8_t vol);                               // $EC
#define smpsPSGAlterVolS2 smpsPSGAlterVol
void smpsClearPush(void);                                         // $ED
void smpsStopSpecial(void);                                        // $EE
void smpsFMvoice(uint8_t voice);                                    // $EF
#define smpsSetvoice smpsFMvoice
void smpsModSet(uint8_t wait, uint8_t speed, uint8_t change, uint8_t step); // $F0
void smpsModOn(void);                                                        // $F1
void smpsStop(void);                                                          // $F2
void smpsPSGform(uint8_t form);                                                 // $F3
void smpsModOff(void);                                                          // $F4
void smpsPSGvoice(uint8_t voice);                                                // $F5
void smpsJump(const char *loc);                                                  // $F6
void smpsLoop(uint8_t index, uint8_t loops, const char *loc);                     // $F7
void smpsCall(const char *loc);                                                    // $F8
void smpsMaxRelRate(void);                                                          // $F9
#define smpsWeirdD1LRR smpsMaxRelRate

// PSG volume envelope indices (smpsHeaderPSG's `voice` argument), matching
// the driver-1 case of the "PSG volume envelope equates" switch.
enum {
    fTone_01 = 1, fTone_02, fTone_03, fTone_04, fTone_05,
    fTone_06, fTone_07, fTone_08, fTone_09,
};

// ---------------------------------------------------------------------
// FM voice bank macros. The first eleven just record operator parameters;
// smpsVcTotalLevel is the one that actually emits the finished 25-byte
// voice (matching the real macro package's design -- see its comment
// about deciding this makes more sense as a trigger on the last-supplied
// field rather than a separate "flush" call).
// ---------------------------------------------------------------------

void smpsVcFeedback(int val);
void smpsVcAlgorithm(int val);
void smpsVcUnusedBits(int val, int d1r1, int d1r2, int d1r3, int d1r4);
void smpsVcDetune(int op1, int op2, int op3, int op4);
void smpsVcCoarseFreq(int op1, int op2, int op3, int op4);
void smpsVcRateScale(int op1, int op2, int op3, int op4);
void smpsVcAttackRate(int op1, int op2, int op3, int op4);
void smpsVcAmpMod(int op1, int op2, int op3, int op4);
void smpsVcDecayRate1(int op1, int op2, int op3, int op4);
void smpsVcDecayRate2(int op1, int op2, int op3, int op4);
void smpsVcDecayLevel(int op1, int op2, int op3, int op4);
void smpsVcReleaseRate(int op1, int op2, int op3, int op4);
void smpsVcTotalLevel(int op1, int op2, int op3, int op4); // Emits the voice

// ---------------------------------------------------------------------
// Note names -- mechanically derived from smps2asm.asm's note enum (see
// the "Note Equates" section), not hand-transcribed, to rule out a
// transcription slip silently shifting every note past some point.
// nRst = rest. Enharmonic spellings (nCs0/nDb0, etc.) intentionally share
// a value, same as the original.
// ---------------------------------------------------------------------

enum {
    nRst = 0x80, nC0 = 0x81, nCs0 = 0x82, nDb0 = 0x82,
    nD0 = 0x83, nDs0 = 0x84, nEb0 = 0x84, nE0 = 0x85,
    nFb0 = 0x85, nEs0 = 0x86, nF0 = 0x86, nFs0 = 0x87,
    nGb0 = 0x87, nG0 = 0x88, nGs0 = 0x89, nAb0 = 0x89,
    nA0 = 0x8A, nAs0 = 0x8B, nBb0 = 0x8B, nB0 = 0x8C,
    nCb1 = 0x8C, nBs0 = 0x8D, nC1 = 0x8D, nCs1 = 0x8E,
    nDb1 = 0x8E, nD1 = 0x8F, nDs1 = 0x90, nEb1 = 0x90,
    nE1 = 0x91, nFb1 = 0x91, nEs1 = 0x92, nF1 = 0x92,
    nFs1 = 0x93, nGb1 = 0x93, nG1 = 0x94, nGs1 = 0x95,
    nAb1 = 0x95, nA1 = 0x96, nAs1 = 0x97, nBb1 = 0x97,
    nB1 = 0x98, nCb2 = 0x98, nBs1 = 0x99, nC2 = 0x99,
    nCs2 = 0x9A, nDb2 = 0x9A, nD2 = 0x9B, nDs2 = 0x9C,
    nEb2 = 0x9C, nE2 = 0x9D, nFb2 = 0x9D, nEs2 = 0x9E,
    nF2 = 0x9E, nFs2 = 0x9F, nGb2 = 0x9F, nG2 = 0xA0,
    nGs2 = 0xA1, nAb2 = 0xA1, nA2 = 0xA2, nAs2 = 0xA3,
    nBb2 = 0xA3, nB2 = 0xA4, nCb3 = 0xA4, nBs2 = 0xA5,
    nC3 = 0xA5, nCs3 = 0xA6, nDb3 = 0xA6, nD3 = 0xA7,
    nDs3 = 0xA8, nEb3 = 0xA8, nE3 = 0xA9, nFb3 = 0xA9,
    nEs3 = 0xAA, nF3 = 0xAA, nFs3 = 0xAB, nGb3 = 0xAB,
    nG3 = 0xAC, nGs3 = 0xAD, nAb3 = 0xAD, nA3 = 0xAE,
    nAs3 = 0xAF, nBb3 = 0xAF, nB3 = 0xB0, nCb4 = 0xB0,
    nBs3 = 0xB1, nC4 = 0xB1, nCs4 = 0xB2, nDb4 = 0xB2,
    nD4 = 0xB3, nDs4 = 0xB4, nEb4 = 0xB4, nE4 = 0xB5,
    nFb4 = 0xB5, nEs4 = 0xB6, nF4 = 0xB6, nFs4 = 0xB7,
    nGb4 = 0xB7, nG4 = 0xB8, nGs4 = 0xB9, nAb4 = 0xB9,
    nA4 = 0xBA, nAs4 = 0xBB, nBb4 = 0xBB, nB4 = 0xBC,
    nCb5 = 0xBC, nBs4 = 0xBD, nC5 = 0xBD, nCs5 = 0xBE,
    nDb5 = 0xBE, nD5 = 0xBF, nDs5 = 0xC0, nEb5 = 0xC0,
    nE5 = 0xC1, nFb5 = 0xC1, nEs5 = 0xC2, nF5 = 0xC2,
    nFs5 = 0xC3, nGb5 = 0xC3, nG5 = 0xC4, nGs5 = 0xC5,
    nAb5 = 0xC5, nA5 = 0xC6, nAs5 = 0xC7, nBb5 = 0xC7,
    nB5 = 0xC8, nCb6 = 0xC8, nBs5 = 0xC9, nC6 = 0xC9,
    nCs6 = 0xCA, nDb6 = 0xCA, nD6 = 0xCB, nDs6 = 0xCC,
    nEb6 = 0xCC, nE6 = 0xCD, nFb6 = 0xCD, nEs6 = 0xCE,
    nF6 = 0xCE, nFs6 = 0xCF, nGb6 = 0xCF, nG6 = 0xD0,
    nGs6 = 0xD1, nAb6 = 0xD1, nA6 = 0xD2, nAs6 = 0xD3,
    nBb6 = 0xD3, nB6 = 0xD4, nCb7 = 0xD4, nBs6 = 0xD5,
    nC7 = 0xD5, nCs7 = 0xD6, nDb7 = 0xD6, nD7 = 0xD7,
    nDs7 = 0xD8, nEb7 = 0xD8, nE7 = 0xD9, nFb7 = 0xD9,
    nEs7 = 0xDA, nF7 = 0xDA, nFs7 = 0xDB, nGb7 = 0xDB,
    nG7 = 0xDC, nGs7 = 0xDD, nAb7 = 0xDD, nA7 = 0xDE,
    nAs7 = 0xDF, nBb7 = 0xDF,
};
