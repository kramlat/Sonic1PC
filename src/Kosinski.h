#pragma once

#include <stddef.h>
#include <stdint.h>

uint8_t *KosDec(const uint8_t *source, void *destination);

// Compresses `size` bytes at `src` into a newly malloc'd Kosinski buffer,
// writing its length to `*out_size`. Caller owns the returned buffer (free()
// it). Round-trips byte-exact through KosDec, but is not a byte-exact
// reproduction of any particular official/real-ROM Kosinski compressor's
// output for the same input (different match-finding heuristics produce
// different, still-valid, encodings of the same data).
uint8_t *KosEnc(const uint8_t *src, size_t size, size_t *out_size);
