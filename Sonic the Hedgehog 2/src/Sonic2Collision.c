#include <stdint.h>

// Sonic 2's collision maps (the Nick Arcade prototype's: Sonic 2's solids replace Sonic 1's in this project, Green Hill's included): the angle of each collision curve, and the height of
// every column / width of every row of each one. The engine's collision code (LevelCollision.c) reads them under the names the engine knows.
#define S2Collision_Angle Collision_Angle
#define S2Collision_HeightMap Collision_HeightMap
#define S2Collision_WidthMap Collision_WidthMap
#include "Resource/S2Collision/Angle.h"
#include "Resource/S2Collision/HeightMap.h"
#include "Resource/S2Collision/WidthMap.h"
