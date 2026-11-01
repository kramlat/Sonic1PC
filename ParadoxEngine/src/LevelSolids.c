#include "LevelSolids.h"

#include "Kosinski.h"
#include "LevelData.h"

size_t LevelSolids_Unpack(const uint8_t *file, size_t size, uint8_t *path1, uint8_t *path2) {
	const size_t blocks = size / 2;
	for (size_t i = 0; i < blocks; i++) {
		path1[i] = file[i * 2];
		path2[i] = file[i * 2 + 1];
	}
	return blocks;
}

void LevelSolids_Pack(const uint8_t *path1, const uint8_t *path2, size_t blocks, uint8_t *file) {
	for (size_t i = 0; i < blocks; i++) {
		file[i * 2] = path1[i];
		file[i * 2 + 1] = path2[i];
	}
}

size_t LevelSolids_LoadKosinski(const uint8_t *compressed) {
	static uint8_t file[sizeof(coll_index)]; // (both paths, as many bytes as the two arrays hold)
	size_t size = (size_t)(KosDec(compressed, file) - file);
	if (size > sizeof(file))
		size = sizeof(file);
	return LevelSolids_Unpack(file, size, coll_index[0], coll_index[1]);
}
