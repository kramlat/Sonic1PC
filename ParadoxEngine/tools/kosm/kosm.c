// kosm: makes and unpacks Kosinski Moduled archives (see src/KosinskiModule.h), with the engine's own Kosinski codec.
//   kosm IN OUT            IN is raw data (up to $FFFF bytes): writes the archive
//   kosm --kos IN OUT      IN is a Kosinski file (a level's art as it is now): decompressed, then made into an archive
//   kosm --unpack IN OUT   IN is an archive: writes the raw data
// Build: cc -O2 -DKOSM_NO_VDP -I../../src kosm.c ../../src/Kosinski.c ../../src/KosinskiModule.c -o kosm
#include "Kosinski.h"
#include "KosinskiModule.h"

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

static void WriteFile(const char *path, const uint8_t *data, size_t size) {
	FILE *f = fopen(path, "wb");
	if (!f || fwrite(data, 1, size, f) != size) {
		perror(path);
		exit(1);
	}
	fclose(f);
}

int main(int argc, char **argv) {
	int from_kos = 0, unpack = 0, a = 1;
	if (a < argc && strcmp(argv[a], "--kos") == 0) from_kos = 1, a++;
	else if (a < argc && strcmp(argv[a], "--unpack") == 0) unpack = 1, a++;
	if (argc - a != 2) {
		fprintf(stderr, "usage: kosm [--kos | --unpack] IN OUT\n");
		return 1;
	}
	size_t size;
	uint8_t *in = ReadFile(argv[a], &size);
	if (unpack) {
		uint8_t *out = malloc(0x10000);
		size_t n = KosinskiModuleDecompress(in, out);
		WriteFile(argv[a + 1], out, n);
		return 0;
	}
	if (from_kos) {
		uint8_t *raw = malloc(0x20000);
		size = (size_t)(KosDec(in, raw) - raw);
		in = raw;
	}
	if (size > 0xFFFF) {
		fprintf(stderr, "kosm: %zu bytes: an archive holds up to $FFFF\n", size);
		return 1;
	}
	uint8_t *out = malloc(size + size / 4 + 0x1000);
	size_t used = 0;
	out[used++] = (uint8_t)(size >> 8);
	out[used++] = (uint8_t)size;
	for (size_t at = 0; at < size; at += KOSM_MODULE_SIZE) {
		size_t part = size - at < KOSM_MODULE_SIZE ? size - at : KOSM_MODULE_SIZE;
		size_t packed;
		uint8_t *module = KosEnc(in + at, part, &packed);
		memcpy(out + used, module, packed);
		free(module);
		size_t padded = (packed + 15) & ~(size_t)15;
		memset(out + used + packed, 0, padded - packed);
		used += padded;
	}
	WriteFile(argv[a + 1], out, used);
	return 0;
}
