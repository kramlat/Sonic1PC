#include "LevelData.h"

uint8_t* const level_map128 = &buffer0000[0x0000];
ALIGNED2 uint8_t level_map16[0x1800];
uint8_t level_layout[LEVEL_LAYOUT_ROWS * LEVEL_LAYOUT_ROW_STRIDE];
uint8_t coll_index[2][0x400];
uint8_t collision_path;

ALIGNED4 uint8_t buffer0000[0xA400];
