// solids2s3: makes a level's solid index in the Sonic 3 & Knuckles layout (LevelSolids.h) from the two path files the games kept: both Kosinski compressed (a level's primary and secondary index of
// 16 x 16 collision blocks), a byte a block; the output is one Kosinski file with the two interleaved.
//   solids2s3 OUT PATH1 PATH2
// Build: cc -O2 -I../../src solids2s3.c ../../src/Kosinski.c -o solids2s3
#include "Kosinski.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static size_t Load(const char *path, uint8_t *out) {
	FILE *f = fopen(path, "rb");
	if (!f) {
		perror(path);
		exit(1);
	}
	static uint8_t in[1 << 16];
	size_t n = fread(in, 1, sizeof in, f);
	fclose(f);
	return (size_t)(KosDec(in, out) - out);
}

int main(int argc, char **argv) {
	if (argc != 4) {
		fprintf(stderr, "usage: solids2s3 OUT PATH1 PATH2\n");
		return 1;
	}
	static uint8_t a[1 << 16], b[1 << 16], file[1 << 17];
	size_t na = Load(argv[2], a), nb = Load(argv[3], b);
	size_t blocks = na > nb ? na : nb; // (a path that is shorter is as if its other blocks had no collision)
	if (na < blocks)
		memset(a + na, 0, blocks - na);
	if (nb < blocks)
		memset(b + nb, 0, blocks - nb);
	for (size_t i = 0; i < blocks; i++) {
		file[i * 2] = a[i];
		file[i * 2 + 1] = b[i];
	}
	size_t out_size;
	uint8_t *kos = KosEnc(file, blocks * 2, &out_size);
	FILE *f = fopen(argv[1], "wb");
	if (!f || fwrite(kos, 1, out_size, f) != out_size) {
		perror(argv[1]);
		return 1;
	}
	fclose(f);
	printf("%s: %zu blocks a path (%zu and %zu in the files), %zu bytes compressed\n", argv[1], blocks, na, nb, out_size);
	return 0;
}
