#include "LevelLoadBlock.h"

#include "Kosinski.h"

uint8_t *LevelLoadBlock_Unpack(const uint8_t *const group[2], uint8_t *dest) {
	uint8_t *end = KosDec(group[0], dest);
	if (group[1] != NULL && group[1] != group[0])
		end = KosDec(group[1], end);
	return end;
}
