#include <stdint.h>

// Sonic 1's collision maps: the angle of each collision curve, and the height of every column / width of every row of each one. The engine's collision
// code (LevelCollision.c) reads them.
#include "Resource/Collision/Angle.h"
#include "Resource/Collision/HeightMap.h"
#include "Resource/Collision/WidthMap.h"
