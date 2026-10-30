#include "LevelData.h"

uint8_t* const level_map128 = &buffer0000[0x0000];
ALIGNED2 uint8_t level_map16[0x1800];
LevelLayoutPlane level_layout_fg, level_layout_bg;
unsigned level_layout_generation;
uint8_t coll_index[2][0x400];
uint8_t collision_path;

ALIGNED4 uint8_t buffer0000[0xA400];
