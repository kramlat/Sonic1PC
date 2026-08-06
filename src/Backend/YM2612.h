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

#ifdef __cplusplus
}
#endif
