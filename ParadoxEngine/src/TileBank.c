#include "TileBank.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Backend/VDP.h"

unsigned tilebank_stale_count;
bool tilebank_stale_fatal;

static tilebank_t banks[TILEBANKS];
static uint8_t main_depth[VRAM_SIZE / 32]; // the main bank's allocation table
static bool banks_ready;
#define PURGERS 8
static void (*purgers[PURGERS])(uint8_t id);
static int purger_count;

void TileBank_OnFree(void (*purge)(uint8_t id)) {
	for (int i = 0; i < purger_count; i++)
		if (purgers[i] == purge)
			return;
	if (purger_count < PURGERS)
		purgers[purger_count++] = purge;
}

static void Init(void) {
	if (banks_ready)
		return;
	banks_ready = true;
	banks[0] = (tilebank_t){ VDP_TileSpace(), main_depth, VRAM_SIZE / 32, 0, 0, true }; // (generation 0 for good: a cleared entry names tile 0 of this bank)
}

tilebank_t *TileBank_Main(void) {
	Init();
	return &banks[0];
}

tilebank_t *TileBank_Create(size_t tiles) {
	Init();
	for (int i = 1; i < TILEBANK_NONE; i++) {
		if (banks[i].live)
			continue;
		uint16_t generation = banks[i].generation; // (what the last use of this slot left: never 0, and not the same as then)
		uint8_t *patterns = calloc(tiles, 32);
		uint8_t *depth = calloc(tiles, 1);
		if (patterns == NULL || depth == NULL) {
			free(patterns);
			free(depth);
			return NULL;
		}
		banks[i] = (tilebank_t){ patterns, depth, tiles, (uint8_t)i, generation == 0 ? 1 : generation, true };
		return &banks[i];
	}
	return NULL;
}

void TileBank_Free(tilebank_t *bank) {
	if (bank == NULL || bank->id == 0 || !bank->live)
		return;
	for (int i = 0; i < purger_count; i++)
		purgers[i](bank->id); // (before the bank goes: nothing names it after)
	free(bank->patterns);
	free(bank->depth);
	bank->patterns = NULL;
	bank->depth = NULL;
	bank->tiles = 0;
	bank->live = false;
	bank->generation = (uint16_t)(bank->generation == 0xFFFF ? 1 : bank->generation + 1); // (entries made before this no longer match: it takes 65,535 frees of the slot to come round)
}

void TileBank_SetDepth(tilebank_t *bank, size_t first, size_t slots, bool eight_bits) {
	if (bank == NULL || !bank->live || first >= bank->tiles)
		return;
	if (first + slots > bank->tiles)
		slots = bank->tiles - first;
	if (!eight_bits) {
		memset(bank->depth + first, TILE_SLOT_4BPP, slots);
		return;
	}
	for (size_t s = 0; s + 2 <= slots; s += 2) {
		bank->depth[first + s] = TILE_SLOT_8BPP_HEAD;
		bank->depth[first + s + 1] = TILE_SLOT_8BPP_TAIL;
	}
}

bool TileBank_IsDeep(const tilebank_t *bank, size_t pattern) {
	return bank != NULL && bank->live && pattern + 1 < bank->tiles && bank->depth[pattern] == TILE_SLOT_8BPP_HEAD;
}

void TileBank_Write(tilebank_t *bank, size_t tile, const void *data, size_t bytes) {
	if (bank == NULL || !bank->live || tile >= bank->tiles)
		return;
	const size_t room = (bank->tiles - tile) * 32;
	if (bytes > room)
		bytes = room;
	memcpy(bank->patterns + tile * 32, data, bytes);
}

const tilebank_t *TileBank_Get(uint8_t id, uint16_t generation) {
	Init();
	if (id == TILEBANK_NONE)
		return NULL; // (an entry that shows nothing: not a stale one)
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

const uint8_t *TileBank_PatternOf(uint16_t attr, uint16_t generation, size_t pattern, bool *deep) {
	*deep = false;
	const tilebank_t *bank = TileBank_Get(TileAttr_Bank(attr), generation);
	if (bank == NULL)
		return NULL;
	if (pattern >= bank->tiles) {
		tilebank_stale_count++;
		return NULL;
	}
	*deep = bank->depth[pattern] == TILE_SLOT_8BPP_HEAD && pattern + 1 < bank->tiles;
	return bank->patterns + pattern * 32;
}
