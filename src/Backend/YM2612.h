#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Opaque handle around fmcore's in-house YM2612 implementation
// (Backend/YM2612_FMCore.c, backed by fmcore/fm_voice.h).
typedef struct YM2612 YM2612;

YM2612 *YM2612_Create(void);
void YM2612_Destroy(YM2612 *chip);

// Matches a real write to the chip's 4-register I/O ports:
// offset 0 = address port 1 (channels 1-3), 1 = data port 1,
// offset 2 = address port 2 (channels 4-6), 3 = data port 2.
void YM2612_Write(YM2612 *chip, uint32_t offset, uint8_t data);

// Renders `count` stereo sample-pairs (interleaved L,R, so `out` needs
// 2*count entries) at `sample_rate`, additively into `out` (does not clear
// it first). `clock_rate` is the chip's input clock. Real L/R output --
// panning is set per-channel via register $B4+ch.
void YM2612_Generate(YM2612 *chip, int32_t *out, uint32_t count, uint32_t sample_rate, uint32_t clock_rate);

// Real YM2612 hardware (and ymfm, faithfully) is write-only -- there's no
// way to read a register back from the chip itself. This reads back a
// shadow copy YM2612_Write keeps internally instead, for debug tooling
// (Z80 Peek) that wants to show current register state. port is 0 or 1
// (matching offset>>1 from YM2612_Write -- port 0 = channels 1-3, port 1 =
// channels 4-6), reg is the full 8-bit register address (as latched by the
// preceding address-port write).
uint8_t YM2612_PeekReg(const YM2612 *chip, int port, uint8_t reg);

// Decoded, persistent per-channel key-on state (bit N = FM channel N+1,
// 0-5) -- $28 itself only ever encodes one channel's state per write (see
// YM2612_Write's own comment), so this isn't just another PeekReg lookup.
uint8_t YM2612_PeekKeyOn(const YM2612 *chip);

// Bulk voice load: writes a full voice's register set (the $B0+ch
// algorithm/feedback byte, plus each of the 4 operators' 6 registers --
// op_regs[op][0..5] in DT/MUL, RS/AR, AM/D1R, D2R, D1L/RR, TL order,
// matching FM_LoadVoice's/FM_LoadVoiceJSON's own FM_WriteReg call order in
// Sound.c) directly into the shadow and resyncs each operator exactly once
// (5 total: 1 algorithm + 4 operators), instead of the 25 individual
// register writes a real hardware-faithful load takes -- each of which
// would otherwise trigger its own full operator resync (SyncOperator
// reassembles ALL 6 of an operator's fields from the shadow on every
// single register write, so loading one operator's 6 registers one at a
// time redundantly resyncs it 6 times when only the last write matters).
// channel_index is this project's own flat 0-5 FM channel index (matches
// YM2612_Write's other callers' convention).
void YM2612_LoadVoice(YM2612 *chip, int channel_index, uint8_t alg_fb_byte, const uint8_t op_regs[4][6]);

// Toggles emulation of the real chip's DAC "ladder effect" -- a documented
// nonlinearity in the channel output stage (see YM2612_FMCore.c's own
// comment on ApplyLadderEffect for details/caveats). Off by default. Per
// chip, not global, so e.g. sound_music.fm can run clean while sound_sfx.fm
// stays authentic to the real hardware's characteristic distortion.
void YM2612_SetLadderEffect(YM2612 *chip, int enabled);

#ifdef __cplusplus
}
#endif
