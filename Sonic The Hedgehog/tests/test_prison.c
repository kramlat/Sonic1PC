#include "test.h"

#include <string.h>

#include "Level.h"
#include "LevelScroll.h"
#include "Object.h"
#include "Object/Animal.h"
#include "Object/PrisonCapsule.h"
#include "SstLoad.h"

// Regression test for the SYZ3 prison capsule releasing an animal that drew
// as a bouncing Eggman cockpit (2026-09). Two bugs combined:
//   1. Scratch_Animals was laid out 2 bytes off the original offsets, so the
//      subtype was read from 0x29 instead of 0x28.
//   2. The capsule took free slots without clearing them, so
//      a stale nonzero byte there sent the animal down the ending-sequence
//      path (AnimalEndVram/AnimalEndMap) with the wrong art.
// Poison every free slot, open the capsule, and check that every animal
// comes out as a normal zone animal.

static int CountOf(uint8_t id) {
    int n = 0;
    for (int i = 0; i < SST_COUNT; i++)
        if (sst[i].code != NULL && sst[i].code == sst_object_codes[id])
            n++;
    return n;
}

static int SpawnPrisonAnimals(Sst *prison) {
    for (int i = SST_FIRST_DYNAMIC; i < SST_COUNT; i++) {
        memset(&sst[i], 0xAB, sizeof(Sst));
        sst[i].code = NULL; // free, but full of stale bytes
    }

    prison->code = sst_object_codes[ObjId_PrisonCapsule];
    prison->routine = 0xA;        // Pri_Explosion
    ((Own_PrisonCapsule *)prison->own.u8)->timer = 1; // spawn the 8 caged animals this frame
    prison->x_pos.f.u = 0x100;
    prison->y_pos.f.u = 0x100;
    scrpos_x.f.u = 0x80;
    static const SstObjectIndex index_ = {sst_object_codes, 0x100};
    SstLoadCamera camera = {0x80, 0, false, 0xFFF};
    SstLoad_Init(&index_, NULL, false, &camera);
    frame_count = 1;              // skip the every-8-frames explosion spawn
    boss_status = 1;
    level_id = LEVEL_ID(ZoneId_SYZ, 2);

    Obj_PrisonCapsule(prison);
    return CountOf(ObjId_Animal);
}

static void Prison_AnimalsIgnoreStaleSlots(void) {
    Sst_Reset();
    Sst *prison = Sst_Allocate();
    CHECK_EQ(SpawnPrisonAnimals(prison), 8);
    CHECK_EQ(boss_status, 2);

    for (int i = 0; i < SST_COUNT; i++) {
        Sst *animal = &sst[i];
        if (animal->code == NULL || animal->code != sst_object_codes[ObjId_Animal])
            continue;
        Own_Animals *own = (Own_Animals *)animal->own.u8;
        CHECK_EQ(animal->subtype, 0);
        CHECK(own->timer != 0);

        Obj_Animals(animal); // construct

        CHECK_EQ(animal->routine, 0x12); // waiting inside the prison
        CHECK(animal->art_tile == TILE_MAP(0, 0, 0, 0, ArtTile_Animal_1) ||
              animal->art_tile == TILE_MAP(0, 0, 0, 0, ArtTile_Animal_2));
        uint8_t var = own->routine;
        CHECK(var == AnimalVarIndex[ZoneId_SYZ][0] || var == AnimalVarIndex[ZoneId_SYZ][1]);
        CHECK(animal->mappings == AnimalVariables[var].mappings);
    }
}

static void Prison_ScratchMatchesLevelLoaderSubtype(void) {
    // Explosion copies points to the word at 0x3E.
    Sst obj;
    memset(&obj, 0, sizeof(obj));
    obj.own.u16[0xB] = 0x1234;
    CHECK_EQ(((Own_Animals *)obj.own.u8)->points, 0x1234);
}

void RegisterPrisonTests(void) {
    RUN_TEST(Prison_ScratchMatchesLevelLoaderSubtype);
    RUN_TEST(Prison_AnimalsIgnoreStaleSlots);
}
