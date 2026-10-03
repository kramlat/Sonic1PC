#include "test.h"

#include <string.h>

#include "Level.h"
#include "LevelScroll.h"
#include "Object.h"
#include "Object/Animal.h"
#include "Object/PrisonCapsule.h"

// Regression test for the SYZ3 prison capsule releasing an animal that drew
// as a bouncing Eggman cockpit (2026-09). Two bugs combined:
//   1. Scratch_Animals was laid out 2 bytes off the original offsets, so the
//      subtype was read from 0x29 instead of 0x28.
//   2. The capsule took slots from FindFreeObj() without clearing them, so
//      a stale nonzero byte there sent the animal down the ending-sequence
//      path (AnimalEndVram/AnimalEndMap) with the wrong art.
// Poison every free slot, open the capsule, and check that every animal
// comes out as a normal zone animal.

static int SpawnPrisonAnimals(Object *prison) {
    for (int i = 0; i < LEVEL_OBJECTS; i++) {
        memset(&level_objects[i], 0xAB, sizeof(Object));
        level_objects[i].type = ObjId_Null; // free, but full of stale bytes
    }

    memset(prison, 0, sizeof(Object));
    prison->type = ObjId_PrisonCapsule;
    prison->routine = 0xA;        // Pri_Explosion
    prison->frame_time.w = 1;     // spawn the 8 caged animals this frame
    prison->pos.l.x.f.u = 0x100;
    prison->pos.l.y.f.u = 0x100;
    scrpos_x.f.u = 0x80;
    frame_count = 1;              // skip the every-8-frames explosion spawn
    boss_status = 1;
    level_id = LEVEL_ID(ZoneId_SYZ, 2);

    Obj_PrisonCapsule(prison);

    int count = 0;
    for (int i = 0; i < LEVEL_OBJECTS; i++)
        if (level_objects[i].type == ObjId_Animal)
            count++;
    return count;
}

static void Prison_AnimalsIgnoreStaleSlots(void) {
    Object *prison = &level_objects[0];
    CHECK_EQ(SpawnPrisonAnimals(prison), 8);
    CHECK_EQ(boss_status, 2);

    for (int i = 1; i < LEVEL_OBJECTS; i++) {
        Object *animal = &level_objects[i];
        if (animal->type != ObjId_Animal)
            continue;
        Scratch_Animals *scratch = (Scratch_Animals *)&animal->scratch;
        CHECK_EQ(scratch->subtype, 0);
        CHECK(scratch->timer != 0);

        Obj_Animals(animal); // construct

        CHECK_EQ(animal->routine, 0x12); // waiting inside the prison
        CHECK(animal->tile == TILE_MAP(0, 0, 0, 0, ArtTile_Animal_1) ||
              animal->tile == TILE_MAP(0, 0, 0, 0, ArtTile_Animal_2));
        uint8_t var = scratch->routine;
        CHECK(var == AnimalVarIndex[ZoneId_SYZ][0] || var == AnimalVarIndex[ZoneId_SYZ][1]);
        CHECK(animal->mappings == AnimalVariables[var].mappings);
    }
}

static void Prison_ScratchMatchesLevelLoaderSubtype(void) {
    // The level loader writes the subtype to scratch.u8[0] (0x28).
    Object obj;
    memset(&obj, 0, sizeof(obj));
    obj.scratch.u8[0] = 0x0A;
    CHECK_EQ(((Scratch_Animals *)&obj.scratch)->subtype, 0x0A);
    // Explosion copies points to scratch.u16[0xB] (0x3E).
    obj.scratch.u16[0xB] = 0x1234;
    CHECK_EQ(((Scratch_Animals *)&obj.scratch)->points, 0x1234);
}

void RegisterPrisonTests(void) {
    RUN_TEST(Prison_ScratchMatchesLevelLoaderSubtype);
    RUN_TEST(Prison_AnimalsIgnoreStaleSlots);
}
