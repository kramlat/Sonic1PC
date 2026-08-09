#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Opaque handle around ymfm's ym2612 class (see /ymfm) -- kept behind a
// plain C interface so the rest of this C99 project never needs to know
// it's linking against C++.
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

#ifdef __cplusplus
}
#endif
