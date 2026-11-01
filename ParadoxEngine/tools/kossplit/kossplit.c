// kossplit: splits a Kosinski file into a primary and a secondary group, as the Sonic 3 & Knuckles level load block keeps a level's patterns, blocks and chunks (LevelLoadBlock.h): the first `offset` bytes of
// the decompressed data are the primary, the rest the secondary (it is decompressed straight after the primary). Each is made into a Kosinski file with the engine's own codec.
//   kossplit IN OFFSET PRIMARY SECONDARY     IN a Kosinski file, OFFSET in bytes (0x... for hex), PRIMARY and SECONDARY the files written
//   kossplit --cut IN OFFSET OUT             IN a Kosinski file: writes the data from OFFSET on as one Kosinski file (a secondary that follows another file's primary)
// Build: cc -O2 -I../../src kossplit.c ../../src/Kosinski.c -o kossplit
#include "Kosinski.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint8_t *ReadFile(const char *path, size_t *size) {
	FILE *f = fopen(path, "rb");
	if (!f) {
		perror(path);
		exit(1);
	}
	fseek(f, 0, SEEK_END);
	long n = ftell(f);
	fseek(f, 0, SEEK_SET);
	uint8_t *data = malloc(n ? (size_t)n : 1);
	if (fread(data, 1, (size_t)n, f) != (size_t)n) {
		perror("read");
		exit(1);
	}
	fclose(f);
	*size = (size_t)n;
	return data;
}

static void WriteKos(const char *path, const uint8_t *data, size_t size) {
	size_t out_size;
	uint8_t *kos = KosEnc(data, size, &out_size);
	FILE *f = fopen(path, "wb");
	if (!f || fwrite(kos, 1, out_size, f) != out_size) {
		perror(path);
		exit(1);
	}
	fclose(f);
	free(kos);
	printf("%s: %zu bytes (%zu compressed)\n", path, size, out_size);
}

int main(int argc, char **argv) {
	int cut = argc > 1 && !strcmp(argv[1], "--cut");
	if (argc != (cut ? 5 : 5)) {
		fprintf(stderr, "usage: kossplit IN OFFSET PRIMARY SECONDARY | kossplit --cut IN OFFSET OUT\n");
		return 1;
	}
	const char *in = argv[cut ? 2 : 1];
	size_t offset = (size_t)strtoul(argv[cut ? 3 : 2], NULL, 0);
	size_t size;
	uint8_t *kos = ReadFile(in, &size);
	static uint8_t data[1 << 20];
	size_t total = (size_t)(KosDec(kos, data) - data);
	if (offset > total) {
		fprintf(stderr, "%s: only %zu bytes\n", in, total);
		return 1;
	}
	if (cut) {
		WriteKos(argv[4], data + offset, total - offset);
	} else {
		WriteKos(argv[3], data, offset);
		WriteKos(argv[4], data + offset, total - offset);
	}
	return 0;
}
