// layout2s3: converts the engine's old level layout files (one Kosinski-compressed blob: 16 rows of $100 bytes, the foreground in the first $80 of each and the background in the
// second) to the Sonic 3 & Knuckles layout format (src/LevelLayout.h), checking that reading the new file back gives the old buffer byte for byte.
//   layout2s3 [--kos] IN OUT   IN is the old file; --kos Kosinski compresses the new one (how the games keep them)
//   layout2s3 --check IN   only checks a file that is already in the new format (reads it back and packs it again)
// Build: cc -O2 -DLEVELLAYOUT_NO_LEVEL -I../../src layout2s3.c ../../src/LevelLayout.c ../../src/Kosinski.c -o layout2s3
#include "Kosinski.h"
#include "LevelLayout.h"

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

int main(int argc, char **argv) {
	int kos = 0;
	if (argc == 4 && strcmp(argv[1], "--kos") == 0) {
		kos = 1;
		argv++;
		argc--;
	}
	if (argc == 3 && strcmp(argv[1], "--check") == 0) {
		size_t size;
		uint8_t *file = ReadFile(argv[2], &size);
		static uint8_t buffer[16 * 0x100];
		LevelLayoutSize s;
		if (!LevelLayout_Unpack(file, size, buffer, 16, &s)) {
			fprintf(stderr, "%s: a row lies outside the file\n", argv[2]);
			return 1;
		}
		printf("%s: foreground %u x %u, background %u x %u chunks\n", argv[2], s.fg_width, s.fg_rows, s.bg_width, s.bg_rows);
		return 0;
	}
	if (argc != 3) {
		fprintf(stderr, "usage: layout2s3 IN OUT | layout2s3 --check FILE\n");
		return 1;
	}
	size_t size;
	uint8_t *old = ReadFile(argv[1], &size);
	static uint8_t buffer[16 * 0x100], again[16 * 0x100];
	memset(buffer, 0, sizeof(buffer));
	KosDec(old, buffer);
	size_t packed_size;
	LevelLayoutSize s;
	uint8_t *packed = LevelLayout_Pack(buffer, 16, &packed_size, &s);
	LevelLayoutSize back;
	if (!LevelLayout_Unpack(packed, packed_size, again, 16, &back) || memcmp(buffer, again, sizeof(buffer)) != 0) {
		fprintf(stderr, "%s: the new file does not read back as the old one\n", argv[1]);
		return 1;
	}
	size_t stored = packed_size;
	if (kos) {
		uint8_t *compressed = KosEnc(packed, packed_size, &stored);
		packed = compressed;
	}
	FILE *f = fopen(argv[2], "wb");
	if (!f || fwrite(packed, 1, stored, f) != stored) {
		perror(argv[2]);
		return 1;
	}
	fclose(f);
	printf("%s -> %s: foreground %u x %u, background %u x %u chunks, %zu bytes (%zu before)\n", argv[1], argv[2], s.fg_width, s.fg_rows, s.bg_width, s.bg_rows, stored, size);
	return 0;
}
