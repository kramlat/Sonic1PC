// kosdec IN OUT: decompresses a Kosinski file with the engine's decoder (Kosinski.c), for the importers that have to read a prototype's compressed data (its layouts, its blocks).
// Build: cc -O2 -I../../src kosdec.c ../../src/Kosinski.c -o kosdec
#include "Kosinski.h"

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
	if (argc != 3) {
		fprintf(stderr, "usage: kosdec IN OUT\n");
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
	uint8_t *out = malloc(1 << 20); // (a level's data is far under a megabyte: a layout is 4 KB, the tiles 64 KB)
	uint8_t *end = KosDec(data, out);
	f = fopen(argv[2], "wb");
	if (!f || fwrite(out, 1, (size_t)(end - out), f) != (size_t)(end - out)) {
		perror(argv[2]);
		return 1;
	}
	fclose(f);
	return 0;
}
