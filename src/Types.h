#pragma once

#include <stdint.h>

//Fixed point ints
typedef union {
	struct {
		unsigned b0 : 1;
		unsigned b1 : 1;
		unsigned b2 : 1;
		unsigned b3 : 1;
		unsigned b4 : 1;
		unsigned b5 : 1;
		unsigned b6 : 1;
		unsigned b7 : 1;
	} f;
	uint8_t v;
} byte_u;

typedef union {
	struct {
		unsigned b0 : 1;
		unsigned b1 : 1;
		unsigned b2 : 1;
		unsigned b3 : 1;
		unsigned b4 : 1;
		unsigned b5 : 1;
		unsigned b6 : 1;
		unsigned b7 : 1;
	} f;
	int8_t v;
} byte_s;

typedef union {
	struct {
		#ifdef SCP_BIG_ENDIAN
			int8_t u;
			uint8_t l;
		#else
			uint8_t l;
			int8_t u;
		#endif
	} f;
	struct {
		#ifdef SCP_BIG_ENDIAN
		unsigned b0 : 1;
		unsigned b1 : 1;
		unsigned b2 : 1;
		unsigned b3 : 1;
		unsigned b4 : 1;
		unsigned b5 : 1;
		unsigned b6 : 1;
		unsigned b7 : 1;
		unsigned b8 : 1;
		unsigned b9 : 1;
		unsigned b10 : 1;
		unsigned b11 : 1;
		unsigned b12 : 1;
		unsigned b13: 1;
		unsigned b14 : 1;
		unsigned b15 : 1;
		#else
		unsigned b8 : 1;
		unsigned b9 : 1;
		unsigned b10 : 1;
		unsigned b11 : 1;
		unsigned b12 : 1;
		unsigned b13: 1;
		unsigned b14 : 1;
		unsigned b15 : 1;
		unsigned b0 : 1;
		unsigned b1 : 1;
		unsigned b2 : 1;
		unsigned b3 : 1;
		unsigned b4 : 1;
		unsigned b5 : 1;
		unsigned b6 : 1;
		unsigned b7 : 1;
		#endif
	} b;
	int16_t v;
} word_s;

typedef union
{
	struct
	{
		#ifdef SCP_BIG_ENDIAN
			uint8_t u;
			uint8_t l;
		#else
			uint8_t l;
			uint8_t u;
		#endif
	} f;
	struct {
		#ifdef SCP_BIG_ENDIAN
		unsigned b0 : 1;
		unsigned b1 : 1;
		unsigned b2 : 1;
		unsigned b3 : 1;
		unsigned b4 : 1;
		unsigned b5 : 1;
		unsigned b6 : 1;
		unsigned b7 : 1;
		unsigned b8 : 1;
		unsigned b9 : 1;
		unsigned b10 : 1;
		unsigned b11 : 1;
		unsigned b12 : 1;
		unsigned b13: 1;
		unsigned b14 : 1;
		unsigned b15 : 1;
		#else
		unsigned b8 : 1;
		unsigned b9 : 1;
		unsigned b10 : 1;
		unsigned b11 : 1;
		unsigned b12 : 1;
		unsigned b13: 1;
		unsigned b14 : 1;
		unsigned b15 : 1;
		unsigned b0 : 1;
		unsigned b1 : 1;
		unsigned b2 : 1;
		unsigned b3 : 1;
		unsigned b4 : 1;
		unsigned b5 : 1;
		unsigned b6 : 1;
		unsigned b7 : 1;
		#endif
	} b;
	uint16_t v;
} word_u;

typedef union
{
	struct
	{
		#ifdef SCP_BIG_ENDIAN
			int16_t u;
			uint16_t l;
		#else
			uint16_t l;
			int16_t u;
		#endif
	} f;
	struct {
		#ifdef SCP_BIG_ENDIAN
			signed c0: 8;
			unsigned c1: 8;
			unsigned c2: 8;
			unsigned c3: 8;
		#else
			unsigned c3: 8;
			unsigned c2: 8;
			unsigned c1: 8;
			signed c0: 8;
		#endif
	} c;
	struct {
		#ifdef SCP_BIG_ENDIAN
		unsigned b0 : 1;
		unsigned b1 : 1;
		unsigned b2 : 1;
		unsigned b3 : 1;
		unsigned b4 : 1;
		unsigned b5 : 1;
		unsigned b6 : 1;
		unsigned b7 : 1;
		unsigned b8 : 1;
		unsigned b9 : 1;
		unsigned b10 : 1;
		unsigned b11 : 1;
		unsigned b12 : 1;
		unsigned b13: 1;
		unsigned b14 : 1;
		unsigned b15 : 1;
		unsigned b16 : 1;
		unsigned b17 : 1;
		unsigned b18 : 1;
		unsigned b19 : 1;
		unsigned b20 : 1;
		unsigned b21 : 1;
		unsigned b22 : 1;
		unsigned b23 : 1;
		unsigned b24 : 1;
		unsigned b25 : 1;
		unsigned b26 : 1;
		unsigned b27 : 1;
		unsigned b28 : 1;
		unsigned b29 : 1;
		unsigned b30 : 1;
		unsigned b31 : 1;
		#else
		unsigned b24 : 1;
		unsigned b25 : 1;
		unsigned b26 : 1;
		unsigned b27 : 1;
		unsigned b28 : 1;
		unsigned b29 : 1;
		unsigned b30 : 1;
		unsigned b31 : 1;
		unsigned b16 : 1;
		unsigned b17 : 1;
		unsigned b18 : 1;
		unsigned b19 : 1;
		unsigned b20 : 1;
		unsigned b21 : 1;
		unsigned b22 : 1;
		unsigned b23 : 1;
		unsigned b8 : 1;
		unsigned b9 : 1;
		unsigned b10 : 1;
		unsigned b11 : 1;
		unsigned b12 : 1;
		unsigned b13: 1;
		unsigned b14 : 1;
		unsigned b15 : 1;
		unsigned b0 : 1;
		unsigned b1 : 1;
		unsigned b2 : 1;
		unsigned b3 : 1;
		unsigned b4 : 1;
		unsigned b5 : 1;
		unsigned b6 : 1;
		unsigned b7 : 1;
		#endif
	} b;
	int32_t v;
} dword_s;

typedef union
{
	struct
	{
		#ifdef SCP_BIG_ENDIAN
			uint16_t u;
			uint16_t l;
		#else
			uint16_t l;
			uint16_t u;
		#endif
	} f;
	struct {
		#ifdef SCP_BIG_ENDIAN
			unsigned c0: 8;
			unsigned c1: 8;
			unsigned c2: 8;
			unsigned c3: 8;
		#else
			unsigned c3: 8;
			unsigned c2: 8;
			unsigned c1: 8;
			unsigned c0: 8;
		#endif
	} c;
	struct {
		#ifdef SCP_BIG_ENDIAN
		unsigned b0 : 1;
		unsigned b1 : 1;
		unsigned b2 : 1;
		unsigned b3 : 1;
		unsigned b4 : 1;
		unsigned b5 : 1;
		unsigned b6 : 1;
		unsigned b7 : 1;
		unsigned b8 : 1;
		unsigned b9 : 1;
		unsigned b10 : 1;
		unsigned b11 : 1;
		unsigned b12 : 1;
		unsigned b13: 1;
		unsigned b14 : 1;
		unsigned b15 : 1;
		unsigned b16 : 1;
		unsigned b17 : 1;
		unsigned b18 : 1;
		unsigned b19 : 1;
		unsigned b20 : 1;
		unsigned b21 : 1;
		unsigned b22 : 1;
		unsigned b23 : 1;
		unsigned b24 : 1;
		unsigned b25 : 1;
		unsigned b26 : 1;
		unsigned b27 : 1;
		unsigned b28 : 1;
		unsigned b29 : 1;
		unsigned b30 : 1;
		unsigned b31 : 1;
		#else
		unsigned b24 : 1;
		unsigned b25 : 1;
		unsigned b26 : 1;
		unsigned b27 : 1;
		unsigned b28 : 1;
		unsigned b29 : 1;
		unsigned b30 : 1;
		unsigned b31 : 1;
		unsigned b16 : 1;
		unsigned b17 : 1;
		unsigned b18 : 1;
		unsigned b19 : 1;
		unsigned b20 : 1;
		unsigned b21 : 1;
		unsigned b22 : 1;
		unsigned b23 : 1;
		unsigned b8 : 1;
		unsigned b9 : 1;
		unsigned b10 : 1;
		unsigned b11 : 1;
		unsigned b12 : 1;
		unsigned b13: 1;
		unsigned b14 : 1;
		unsigned b15 : 1;
		unsigned b0 : 1;
		unsigned b1 : 1;
		unsigned b2 : 1;
		unsigned b3 : 1;
		unsigned b4 : 1;
		unsigned b5 : 1;
		unsigned b6 : 1;
		unsigned b7 : 1;
		#endif
	} b;
	uint32_t v;
} dword_u;
