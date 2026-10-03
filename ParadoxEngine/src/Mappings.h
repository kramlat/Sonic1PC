#pragma once

#include <stdint.h>

// Sprite mappings, in Sonic 2's format -- the one format every game on the engine uses (Sonic 1's art mappings are converted to it; see asm/Mappings/_MapMacros.asm).
//
//   A mappings table starts with one big-endian word per frame: the offset of that frame from the start of the table.
//   A frame is a big-endian word piece count, then that many pieces of SPRITE_PIECE_SIZE bytes:
//       byte 0       y offset (signed)
//       byte 1       size: width - 1 in bits 2-3, height - 1 in bits 0-1 (in tiles)
//       bytes 2-3    tile attributes: priority, palette, y flip, x flip, tile number (added to the object's own tile base)
//       bytes 4-5    the same for the 2-player split-screen mode (its tile number is halved: that mode draws interlaced double-height tiles)
//       bytes 6-7    x offset (signed word)
//
// A "raw" object (ObjectRender::static_mappings) has `mappings` pointing straight at one piece instead of at a table.

#define SPRITE_PIECE_SIZE 8
#define SPRITE_FRAME_HEADER_SIZE 2 // the piece count

// The frame's piece count, and (through *pieces) where its first piece is
static inline uint16_t Mappings_FramePieces(const uint8_t *mappings, unsigned frame, const uint8_t **pieces) {
    const uint8_t *entry = mappings + (frame << 1);
    const uint8_t *header = mappings + ((entry[0] << 8) | entry[1]);
    *pieces = header + SPRITE_FRAME_HEADER_SIZE;
    return (uint16_t)((header[0] << 8) | header[1]);
}

// One piece's fields
static inline int8_t Mappings_PieceY(const uint8_t *piece) { return (int8_t)piece[0]; }
static inline uint8_t Mappings_PieceSize(const uint8_t *piece) { return piece[1]; }
static inline uint16_t Mappings_PieceTile(const uint8_t *piece) { return (uint16_t)((piece[2] << 8) | piece[3]); }
static inline uint16_t Mappings_PieceTile2P(const uint8_t *piece) { return (uint16_t)((piece[4] << 8) | piece[5]); }
static inline int16_t Mappings_PieceX(const uint8_t *piece) { return (int16_t)((piece[6] << 8) | piece[7]); }
