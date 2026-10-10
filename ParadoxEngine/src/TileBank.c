#include "TileBank.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Backend/VDP.h"

unsigned tilebank_stale_count;
bool tilebank_stale_fatal;

static tilebank_t banks[TILEBANKS];
static bool banks_ready;

static void Init(void) {
	if (banks_ready)
		return;
	banks_ready = true;
	banks[0] = (tilebank_t){ VDP_TileSpace(), VRAM_SIZE / 32, 0, 0, true }; // (generation 0 for good: a cleared entry names tile 0 of this bank)
}

tilebank_t *TileBank_Main(void) {
	Init();
	return &banks[0];
}

tilebank_t *TileBank_Create(size_t tiles) {
	Init();
	for (int i = 1; i < TILEBANKS; i++) {
		if (banks[i].live)
			continue;
		uint8_t generation = banks[i].generation; // (what the last use of this slot left: never 0, and not the same as then)
		uint8_t *patterns = calloc(tiles, 32);
		if (patterns == NULL)
			return NULL;
		banks[i] = (tilebank_t){ patterns, tiles, (uint8_t)i, generation == 0 ? 1 : generation, true };
		return &banks[i];
	}
	return NULL;
}

void TileBank_Free(tilebank_t *bank) {
	if (bank == NULL || bank->id == 0 || !bank->live)
		return;
	free(bank->patterns);
	bank->patterns = NULL;
	bank->tiles = 0;
	bank->live = false;
	bank->generation = (uint8_t)(bank->generation + 1 == 0 ? 1 : bank->generation + 1); // (entries made before this no longer match)
}

void TileBank_Write(tilebank_t *bank, size_t tile, const void *data, size_t bytes) {
	if (bank == NULL || !bank->live || tile >= bank->tiles)
		return;
	const size_t room = (bank->tiles - tile) * 32;
	if (bytes > room)
		bytes = room;
	memcpy(bank->patterns + tile * 32, data, bytes);
}

const tilebank_t *TileBank_Get(uint8_t id, uint8_t generation) {
	Init();
	const tilebank_t *bank = &banks[id % TILEBANKS];
	if (bank->live && bank->generation == generation)
		return bank;
	tilebank_stale_count++;
	if (tilebank_stale_fatal) {
		fprintf(stderr, "TileBank: a reference to a bank that is not there (bank %u, generation %u)\n", id, generation);
		abort();
	}
	return NULL;
}

const uint8_t *TileBank_Pattern(const tile_entry_t *entry) {
	Init();
	const tilebank_t *bank = &banks[entry->bank % TILEBANKS];
	if (bank->live && bank->generation == entry->generation && entry->pattern < bank->tiles)
		return bank->patterns + (size_t)entry->pattern * 32;
	tilebank_stale_count++;
	if (tilebank_stale_fatal) {
		fprintf(stderr, "TileBank: an entry names a tile that is not there (bank %u, generation %u, pattern %u)\n", entry->bank, entry->generation, entry->pattern);
		abort();
	}
	return NULL;
}
