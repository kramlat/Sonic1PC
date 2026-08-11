#pragma once

#include "Object.h"

// Object 0x71 - invisible solid barrier: an invisible rectangular block
// Sonic collides with like platform/wall geometry, sized from its subtype
// byte (high nibble = width setting, low nibble = height setting, both
// 0-based in 16px units). Only visible (as a monitor-shaped placeholder
// sprite) in debug mode.
void Obj_InvisibleBarrier(Object *obj);
