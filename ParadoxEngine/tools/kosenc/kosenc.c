// kosenc IN OUT: Kosinski-compresses a file with the engine's encoder (Kosinski.c). Used to bring uncompressed data (the prototypes' tables) in as resources.
// Build: cc -O2 -I../../src kosenc.c ../../src/Kosinski.c -o kosenc
#include "Kosinski.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
	if (argc != 3) {
		fprintf(stderr, "usage: kosenc IN OUT\n");
		return 1;
	}
	FILE *f = fopen(argv[1], "rb");
	if (!f) {
		perror(argv[1]);
		return 1;
	}
	fseek(f, 0, SEEK_END);
	long n = ftell(f);
	fseek(f, 0, SEEK_SET);
	uint8_t *data = malloc(n ? n : 1);
	if (fread(data, 1, n, f) != (size_t)n) {
		perror("read");
		return 1;
	}
	fclose(f);
	size_t out_size;
	uint8_t *out = KosEnc(data, n, &out_size);
	f = fopen(argv[2], "wb");
	if (!f || fwrite(out, 1, out_size, f) != out_size) {
		perror(argv[2]);
		return 1;
	}
	fclose(f);
	return 0;
}
