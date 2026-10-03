// Enigma decompression algorithm.
// See http://www.segaretro.org/Enigma_compression for format description.
// Line-for-line translation of the disassembly's EniDec (Enigma
// Decompression.asm), matching its own register/state usage closely rather
// than restructuring it, since the bit-level tricks (the mid-word rotate
// when refilling with a bit deficit, the shared bit-buffer between the
// format-list reader and FetchInlineValue's per-flag bit tests) are easy
// to subtly break by "cleaning up" the control flow.

#include "Enigma.h"

#include <stdint.h>

static const uint8_t *source;   // a0
static uint8_t *destination;    // a1 (writes big-endian 16-bit words)
static uint16_t start_tile;     // a3
static uint16_t inline_bits;    // a5 -- bit width of an inline copy value
static uint8_t pccvh_flags;     // d4 -- which of Priority/palette x2/Yflip/Xflip vary per inline value, pre-shifted <<3
static uint16_t incremental_word; // a2
static uint16_t literal_word;     // a4
static uint16_t bit_buffer;     // d5
static int16_t bit_shift;       // d6 -- valid bit count remaining at the top of bit_buffer

static const uint16_t eni_masks[16] = {
    0x0001, 0x0003, 0x0007, 0x000F,
    0x001F, 0x003F, 0x007F, 0x00FF,
    0x01FF, 0x03FF, 0x07FF, 0x0FFF,
    0x1FFF, 0x3FFF, 0x7FFF, 0xFFFF,
};

static void WriteWord(uint16_t value) {
    *destination++ = (uint8_t)(value >> 8);
    *destination++ = (uint8_t)value;
}

// Matches EniDec_FetchByte: consumes `length` bits from bit_shift, and if
// that leaves fewer than 9 bits of lookahead, refills one more byte.
static void FetchByte(uint16_t length) {
    bit_shift = (int16_t)(bit_shift - length);
    if (bit_shift >= 9)
        return;
    bit_shift = (int16_t)(bit_shift + 8);
    bit_buffer = (uint16_t)((bit_buffer << 8) | *source++);
}

// Pops the top bit of *flags (matches "add.b d1,d1" / carry-out), MSB first.
static int ShiftFlag(uint8_t *flags) {
    int bit = (*flags & 0x80) != 0;
    *flags = (uint8_t)(*flags << 1);
    return bit;
}

// Matches EniDec_FetchInlineValue: builds one inline art-tile value from
// start_tile, up to 5 optional per-tile render flag bits (each only
// present if pccvh_flags says it varies), and an inline_bits-wide value
// pulled from the shared bitstream.
static uint16_t FetchInlineValue(void) {
    uint16_t tile = start_tile;
    uint8_t flags = pccvh_flags;

    if (ShiftFlag(&flags)) { bit_shift--; if (bit_buffer & (1 << bit_shift)) tile |= 0x8000; } // Priority
    if (ShiftFlag(&flags)) { bit_shift--; if (bit_buffer & (1 << bit_shift)) tile = (uint16_t)(tile + 0x4000); } // Palette line bit 1
    if (ShiftFlag(&flags)) { bit_shift--; if (bit_buffer & (1 << bit_shift)) tile = (uint16_t)(tile + 0x2000); } // Palette line bit 0
    if (ShiftFlag(&flags)) { bit_shift--; if (bit_buffer & (1 << bit_shift)) tile |= 0x1000; } // Y flip
    if (ShiftFlag(&flags)) { bit_shift--; if (bit_buffer & (1 << bit_shift)) tile |= 0x0800; } // X flip

    uint16_t d1 = bit_buffer;
    int16_t deficit = (int16_t)(inline_bits - bit_shift);
    uint16_t value;

    if (deficit > 0) {
        // Not enough bits left in bit_buffer -- peek the next byte (not yet
        // consumed) and rotate the needed high bits in.
        bit_shift = (int16_t)(16 - deficit);
        d1 = (uint16_t)(d1 << deficit);

        uint8_t peek = source[0];
        uint8_t rotated = (uint8_t)((peek << deficit) | (peek >> (8 - deficit)));
        d1 = (uint16_t)(d1 + (rotated & eni_masks[deficit - 1]));

        value = (uint16_t)((d1 & eni_masks[inline_bits - 1]) + tile);

        // Full refill -- re-reads the byte just peeked, plus one more.
        bit_buffer = (uint16_t)((*source++) << 8);
        bit_buffer |= *source++;
    } else if (deficit == 0) {
        bit_shift = 16;
        value = (uint16_t)((d1 & eni_masks[inline_bits - 1]) + tile);

        bit_buffer = (uint16_t)((*source++) << 8);
        bit_buffer |= *source++;
    } else {
        d1 = (uint16_t)(d1 >> (-deficit));
        value = (uint16_t)((d1 & eni_masks[inline_bits - 1]) + tile);

        FetchByte(inline_bits);
    }

    return value;
}

const uint8_t *EniDec(const uint8_t *src, uint8_t *dest, uint16_t tile_base) {
    source = src;
    destination = dest;
    start_tile = tile_base;

    inline_bits = *source++;
    pccvh_flags = (uint8_t)(*source++ << 3);

    incremental_word = (uint16_t)(((source[0] << 8) | source[1]) + start_tile);
    source += 2;
    literal_word = (uint16_t)(((source[0] << 8) | source[1]) + start_tile);
    source += 2;

    bit_buffer = (uint16_t)((*source++) << 8);
    bit_buffer |= *source++;
    bit_shift = 16;

    for (;;) {
        int16_t entry_len = 7;
        uint16_t raw = (uint16_t)((bit_buffer >> (bit_shift - entry_len)) & 0x7F);
        uint16_t repeat = raw;

        if (raw < 0x40) {
            entry_len = 6;
            repeat >>= 1;
        }

        FetchByte((uint16_t)entry_len);
        repeat &= 0xF;
        uint8_t code = (uint8_t)((raw >> 4) & 7);

        switch (code) {
        case 0:
        case 1:
            for (uint16_t i = 0; i <= repeat; i++) {
                WriteWord(incremental_word);
                incremental_word++;
            }
            break;

        case 2:
        case 3:
            for (uint16_t i = 0; i <= repeat; i++)
                WriteWord(literal_word);
            break;

        case 4: {
            uint16_t value = FetchInlineValue();
            for (uint16_t i = 0; i <= repeat; i++)
                WriteWord(value);
            break;
        }

        case 5: {
            uint16_t value = FetchInlineValue();
            for (uint16_t i = 0; i <= repeat; i++, value++)
                WriteWord(value);
            break;
        }

        case 6: {
            uint16_t value = FetchInlineValue();
            for (uint16_t i = 0; i <= repeat; i++, value--)
                WriteWord(value);
            break;
        }

        case 7:
            if (repeat == 0xF)
                goto done;
            for (uint16_t i = 0; i <= repeat; i++)
                WriteWord(FetchInlineValue());
            break;
        }
    }

done:
    // Rewind the source pointer to just past the compressed data, matching
    // whatever byte/word alignment the caller expects to continue from.
    source--;
    if (bit_shift == 16)
        source--;
    if ((uintptr_t)source & 1)
        source++;

    return source;
}
