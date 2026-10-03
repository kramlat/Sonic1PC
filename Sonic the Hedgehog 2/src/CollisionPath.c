// Which collision path a character walks on (Game_CollisionPath, GameInterface.h): Sonic and Tails each have their own, the solid bits that Nick Arcade's path swappers and springs set ($C/$D the first path,
// $E/$F the second); everything else collides with the first.
#include "Constants.h"

#include "Object/Sonic.h"
#include "Object/Tails.h"

uint8_t Game_CollisionPath(const void *object) {
    const Object *obj = (const Object *)object;
    if (obj != player && obj != TAILS_OBJ)
        return 0;
    return ((const Scratch_Sonic *)&obj->scratch)->top_solid_bit == 0xE;
}
