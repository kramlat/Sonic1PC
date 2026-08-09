//This file was given by Clownacy

#include "Kosinski.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

static uint16_t descriptor_field;
static uint32_t descriptor_bits_remaining;
const uint8_t *source;

static void RefreshDescriptorField(void) {
	descriptor_field = source[0] | (source[1] << 8);
	source += 2;

	descriptor_bits_remaining = 16;
}

static bool GetDescriptorBit(void) {
	bool bit = descriptor_field & 1;

	descriptor_field >>= 1;

	if (--descriptor_bits_remaining == 0)
		RefreshDescriptorField();
	return bit;
}

uint8_t* KosDec(const uint8_t *_source, void *_destination) {
	source = _source;
	uint8_t *destination = _destination;

	RefreshDescriptorField();

	for (;;) {
		if (GetDescriptorBit())
			*destination++ = *source++;
		else {
			uint32_t length = 0;
			int32_t offset;

			if (!GetDescriptorBit()) {
				if (GetDescriptorBit())
					length += 2;

				if (GetDescriptorBit())
					++length;

				++length;

				offset = -0x100 + *source++;
			} else {
				uint8_t d0 = *source++;
				uint8_t d1 = *source++;

				offset = -0x2000 + (((d1 & 0xF8) << 5) | d0);
				length = d1 & 7;

				if (length != 0)
					++length;
				else {
					length = *source++;

					if (length == 0)
						break;

					if (length == 1)
						continue;
				}
			}

			do
			{
				*destination = destination[offset];
				++destination;
			} while (length-- != 0);
		}
	}
	return destination;
}

//---------------------------------------------------------------------------
// Encoder (KensSharp/Clownacy-format-compatible Kosinski compressor)
//
// Not part of the original "given by Clownacy" decoder file -- added to
// support producing new Kosinski-compressed resources (e.g. the interleaved
// level layout / chunk table conversion) without needing external tooling.
// Uses a straightforward greedy LZ77 match search (not optimal-parse, just
// correct) -- match encoding mirrors KosDec's token format exactly:
//   - literal byte                              (descriptor bit 1)
//   - inline match: length 1-4, offset -1..-256  (descriptor bits 00)
//   - full match:   length 2-8, offset -1..-8192 (descriptor bits 01, length!=0 case)
//   - full match:   length 2-255 via extra byte, offset -1..-8192 (descriptor bits 01, length==0 case)
// Terminates with a full-match token carrying an explicit length byte of 0.
//---------------------------------------------------------------------------

typedef struct {
	uint8_t *buffer;
	size_t size, capacity;
} ByteBuffer;

static void BufPush(ByteBuffer *buf, uint8_t byte) {
	if (buf->size >= buf->capacity) {
		buf->capacity = buf->capacity ? buf->capacity * 2 : 256;
		buf->buffer = realloc(buf->buffer, buf->capacity);
	}
	buf->buffer[buf->size++] = byte;
}

// Descriptor fields are written 2 bytes at a time, little-endian, with bits
// consumed LSB-first -- same layout RefreshDescriptorField/GetDescriptorBit
// read. We buffer pending descriptor bits and flush a 16-bit field (with its
// data bytes already emitted after it) once full.
typedef struct {
	ByteBuffer out;
	uint16_t pending_field;
	uint8_t pending_bits;
	bool field_reserved; // true once FlushField has reserved field_patch_pos and it's awaiting its 16 bits
	size_t field_patch_pos; // position in out.buffer of the not-yet-written field
} KosEncState;

static void FlushField(KosEncState *state) {
	// Reserve 2 bytes now; filled in once the field is complete. Matches the
	// decoder's own model: a field is read whole, 2 bytes, before any of the
	// bits in it are consumed relative to the bytes that follow.
	state->field_patch_pos = state->out.size;
	BufPush(&state->out, 0);
	BufPush(&state->out, 0);
	state->pending_field = 0;
	state->pending_bits = 0;
	state->field_reserved = true;
}

static void EmitBit(KosEncState *state, bool bit) {
	if (!state->field_reserved)
		FlushField(state);

	if (bit)
		state->pending_field |= (uint16_t)(1u << state->pending_bits);
	state->pending_bits++;

	if (state->pending_bits == 16) {
		size_t patch_pos = state->field_patch_pos;
		uint16_t field = state->pending_field;
		// Reserve the NEXT field's 2 bytes right now, before returning --
		// matching RefreshDescriptorField's own side-effecting behaviour in
		// the decoder, which refetches the next field the instant the 16th
		// bit of the current one is consumed, even if the token whose bit
		// just completed the field still has its own data byte(s) left to
		// emit after this call returns. Deferring this reservation to the
		// next EmitBit call (i.e. the next token) places those bytes too
		// late whenever a field boundary falls mid-token. field_reserved
		// stays true across this, so the *next* EmitBit call (for whatever
		// bit comes after this one, possibly still within the same token)
		// does NOT reserve yet another field on top of this one.
		FlushField(state);
		state->out.buffer[patch_pos] = field & 0xFF;
		state->out.buffer[patch_pos + 1] = (field >> 8) & 0xFF;
	}
}

static void EmitByte(KosEncState *state, uint8_t byte) {
	BufPush(&state->out, byte);
}

static void EmitLiteral(KosEncState *state, uint8_t byte) {
	EmitBit(state, true);
	EmitByte(state, byte);
}

// IMPORTANT: KosDec's shared copy loop is `do { copy; } while (length-- != 0);`
// -- a do-while whose *condition* does the decrement, so it always copies one
// MORE byte than the "length" value the decoder computed would suggest (the
// loop body still runs on the final length==0 check before stopping). Every
// length encoded below is therefore (desired byte count - 1), not the byte
// count itself, to compensate -- verified by round-tripping through the real
// KosDec, not just derived on paper.

static void EmitInlineMatch(KosEncState *state, uint32_t byte_count, int32_t offset) {
	// byte_count 2-5, offset -1..-256
	EmitBit(state, false);
	EmitBit(state, false);
	uint32_t l = byte_count - 2; // 0-3 (2 bits: +2 bit, +1 bit)
	EmitBit(state, (l & 2) != 0);
	EmitBit(state, (l & 1) != 0);
	EmitByte(state, (uint8_t)(offset + 0x100));
}

static void EmitFullMatch(KosEncState *state, uint32_t byte_count, int32_t offset) {
	// offset -1..-8192
	EmitBit(state, false);
	EmitBit(state, true);
	uint16_t rel = (uint16_t)(offset + 0x2000); // 13-bit value
	uint8_t d0 = rel & 0xFF;
	uint8_t d1 = (uint8_t)((rel >> 5) & 0xF8);
	if (byte_count >= 3 && byte_count <= 9) {
		d1 |= (uint8_t)(byte_count - 2); // low 3 bits 1-7 (0 is reserved for the extended-length escape)
		EmitByte(state, d0);
		EmitByte(state, d1);
	} else {
		// d1's low 3 bits stay 0 -> extended length byte follows
		EmitByte(state, d0);
		EmitByte(state, d1);
		EmitByte(state, (uint8_t)(byte_count - 1)); // caller guarantees 3 <= byte_count <= 256; byte value 0 is the terminator, 1 is "skip", so this is never 0 or 1
	}
}

static void EmitTerminator(KosEncState *state) {
	EmitBit(state, false);
	EmitBit(state, true);
	EmitByte(state, 0);
	EmitByte(state, 0); // low 3 bits 0 -> extended length byte follows
	EmitByte(state, 0); // length 0 == stop
}

// Finds the longest match for source[pos..] within the already-encoded
// window source[0..pos), respecting Kosinski's two offset/length regimes.
// Greedy (not lazy/optimal) -- always takes the longest match found,
// preferring the smallest offset on ties (marginally better for the inline
// short-match encoding, and irrelevant to correctness either way).
static void FindBestMatch(const uint8_t *source, size_t pos, size_t size, uint32_t *out_length, int32_t *out_offset) {
	uint32_t best_length = 0;
	int32_t best_offset = 0;

	size_t max_offset = pos < 0x2000 ? pos : 0x2000;
	size_t max_length = size - pos;
	if (max_length > 256)
		max_length = 256; // no encodable match is ever longer than this in practice for level data; keeps search bounded

	for (size_t off = 1; off <= max_offset; off++) {
		size_t start = pos - off;
		size_t len = 0;
		while (len < max_length && source[start + len] == source[pos + len])
			len++;

		// Only offsets <= 256 can use the cheap inline-match encoding (byte
		// count 2-5); offsets beyond that require the full match encoding
		// (byte count 3+, since a 2-byte full match isn't representable --
		// see EmitFullMatch's comment on the do-while's extra iteration).
		uint32_t min_useful_length = (off <= 0x100) ? 2u : 3u;
		if (len < min_useful_length)
			continue;

		if (len > best_length) {
			best_length = (uint32_t)len;
			best_offset = -(int32_t)off;
		}
	}

	*out_length = best_length;
	*out_offset = best_offset;
}

uint8_t *KosEnc(const uint8_t *src, size_t size, size_t *out_size) {
	KosEncState state = {0};

	size_t pos = 0;
	while (pos < size) {
		uint32_t length;
		int32_t offset;
		FindBestMatch(src, pos, size, &length, &offset);

		// No match encoding can represent fewer than 2 bytes (see
		// EmitFullMatch's comment), so anything shorter than that falls back
		// to a literal.
		if (length < 2) {
			EmitLiteral(&state, src[pos]);
			pos += 1;
			continue;
		}

		if (-offset <= 0x100 && length <= 5) {
			EmitInlineMatch(&state, length, offset);
		} else {
			if (length > 256)
				length = 256;
			if (length < 3)
				length = 3; // FindBestMatch already guarantees this for offset > 0x100, but stay safe
			EmitFullMatch(&state, length, offset);
		}
		pos += length;
	}

	EmitTerminator(&state);

	// Flush a partially-filled trailing descriptor field (if the last
	// EmitBit call didn't complete one) -- the field bytes were already
	// reserved by FlushField, just need the accumulated bits written out.
	if (state.pending_bits != 0) {
		state.out.buffer[state.field_patch_pos] = state.pending_field & 0xFF;
		state.out.buffer[state.field_patch_pos + 1] = (state.pending_field >> 8) & 0xFF;
	}

	*out_size = state.out.size;
	return state.out.buffer;
}
