// nem2kos OUT IN[@TILE]...: decodes Nemesis tilesets with the engine's decoder (Nemesis.c) and writes them as one Kosinski-compressed tileset, each IN at tile TILE (default 0),
// later ones over earlier ones where they overlap. Brings a prototype's level art in as the game's own kind of level art (Level.c decompresses it at level start).
// Build: cc -O2 -I../../src -I../../src/Backend nem2kos.c ../../src/Nemesis.c ../../src/Kosinski.c -o nem2kos
#include "Kosinski.h"
#include "Nemesis.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Nemesis.c writes to VRAM in its VRAM mode, which this tool never uses
void VDP_WriteVRAM(const uint8_t *data, size_t len) { (void)data; (void)len; }
void VDP_SeekVRAM(size_t offset) { (void)offset; }

int main(int argc, char **argv) {
	if (argc < 3) {
		fprintf(stderr, "usage: nem2kos OUT IN[@TILE]...\n");
		return 1;
	}
	static uint8_t art[0x10000];
	size_t size = 0;
	for (int i = 2; i < argc; i++) {
		char *at = strrchr(argv[i], '@');
		size_t tile = 0;
		if (at) {
			*at = 0;
			tile = strtoul(at + 1, NULL, 0);
		}
		FILE *f = fopen(argv[i], "rb");
		if (!f) {
			perror(argv[i]);
			return 1;
		}
		static uint8_t in[0x10000];
		size_t n = fread(in, 1, sizeof(in) - 16, f);
		fclose(f);
		memset(in + n, 0, 16); // (the decoder reads a little past the end)
		size_t tiles = (((size_t)in[0] << 8) | in[1]) & 0x7FFF;
		if ((tile + tiles) * 32 > sizeof(art)) {
			fprintf(stderr, "%s: too many tiles\n", argv[i]);
			return 1;
		}
		NemDecToRAM(in, art + tile * 32);
		if ((tile + tiles) * 32 > size)
			size = (tile + tiles) * 32;
	}
	size_t out_size;
	uint8_t *out = KosEnc(art, size, &out_size);
	FILE *o = fopen(argv[1], "wb");
	if (!o || fwrite(out, 1, out_size, o) != out_size) {
		perror(argv[1]);
		return 1;
	}
	fclose(o);
	fprintf(stderr, "%s: %zu tiles, %zu bytes\n", argv[1], size / 32, out_size);
	return 0;
}
