#pragma once

#include <stdint.h>

// Decompresses Enigma-compressed data (used for 16x16 block mappings, and
// a handful of other tilemap-like assets) into `destination`, writing 16-bit
// big-endian words (matching every other tile/mapping buffer in this
// codebase). `start_tile` is added to every decompressed value (the base
// art tile these mappings are relative to -- 0 for 16x16 block mappings).
// Returns the source pointer just past the compressed data, for chaining
// further reads the way the original does.
const uint8_t *EniDec(const uint8_t *source, uint8_t *destination, uint16_t start_tile);
