#include "test.h"

#include "PLC.h"
#include "Constants.h"
#include "Level.h"
#include "Object.h"
#include "Object/Animal.h"
#include "Object/PrisonCapsule.h"
#include "Object/BossGreenHill.h"
#include "SpecialStage.h"
#include "Video.h"

#include <stdbool.h>
#include <string.h>
#include "Backend/VDP.h"

// Regression tests for the two PLC.c bugs found while debugging the
// titlecard hang/crash (2026-08): the queue that GM_Level pushes for a GHZ
// level start (title card + GHZ level art + Main2) fills plc_buffer to
// *exactly* 16/16 entries, leaving zero slack for a NULL sentinel. That
// exposed two separate bugs:
//   1. The pop/shift in ProcessDPLC_Main didn't clear the vacated tail
//      slot, so once the queue drained down to it, the same stale entry
//      kept getting re-copied into slot 0 forever (infinite hang).
//   2. AddPLC's "find an empty slot" scan had no bound, so any call made
//      while the buffer was completely full walked off the end of the
//      16-slot array into adjacent memory (crash / corruption).

static void PLC_DrainsExactlyFullQueue(void) {
    ClearPLC();
    NewPLC(PlcId_TitleCard);
    AddPLC(PlcId_GHZ);
    AddPLC(PlcId_Main2);
    // TitleCard(1) + GHZ(10) + Main2(3) = 14 since PLC_GHZ dropped its
    // Art_GHZ1/Art_GHZ2 entries (now loaded separately via KosDec, not the
    // Nemesis-only PLC pipeline -- see LevelDataLoad). Top up to the real
    // 16-slot edge case with an unrelated small list.
    AddPLC(PlcId_LZAnimals);

    CHECK(plc_buffer[0].art != NULL);

    int iterations = 0;
    const int max_iterations = 2000; // real drain takes ~150-200 at 9 tiles/frame
    while (plc_buffer[0].art != NULL && iterations < max_iterations) {
        RunPLC();
        ProcessDPLC();
        iterations++;
    }

    CHECK(plc_buffer[0].art == NULL);
    CHECK(iterations < max_iterations);
}

static void PLC_AddPLCDoesNotOverflowWhenFull(void) {
    ClearPLC();
    NewPLC(PlcId_TitleCard);
    AddPLC(PlcId_GHZ);
    AddPLC(PlcId_Main2);
    AddPLC(PlcId_LZAnimals); // top up to 16, see PLC_DrainsExactlyFullQueue

    for (int i = 0; i < 16; i++)
        CHECK(plc_buffer[i].art != NULL);

    // Buffer is completely full; these must be safely dropped, not corrupt
    // adjacent memory (this is exactly the scenario ASan caught -- run this
    // suite under -DSANITIZE=ON to get that coverage).
    AddPLC(PlcId_Explode);
    AddPLC(PlcId_GHZAnimals);

    for (int i = 0; i < 16; i++)
        CHECK(plc_buffer[i].art != NULL);
}

// The P128 migration (d414074) dropped Art_Splash from PLC_LZ along with the
// level-art entries, but it is object art (waterfalls and splashes), not part
// of the Kosinski level art -- every waterfall drew level tiles instead.
extern const uint8_t Art_Splash[];

static void PLC_LZLoadsWaterfallSplashArt(void) {
    ClearPLC();
    NewPLC(PlcId_LZ);
    bool found = false;
    for (int i = 0; i < 16 && plc_buffer[i].art != NULL; i++)
        if (plc_buffer[i].art == Art_Splash && plc_buffer[i].off == ART_VRAM(ArtTile_LZ_Splash))
            found = true;
    CHECK(found);
    ClearPLC();
}

// SBZ's collapsing floor art (Nem_SbzFloor) is 4 tiles, loaded at $3F5 in SBZ1 and
// $3F9 in SBZ2. res/Art/SBZFloor used to be a copy of the 15-tile sliding-floor
// art, so the floors drew wrong graphics and, in SBZ2, the extra tiles spilled
// over the art after tile $3F9.
extern const uint8_t Art_SBZFloor[];
extern const uint8_t Art_SlideFloor[];
static void PLC_SBZCollapsingFloorArtIsFourTiles(void) {
    CHECK_EQ(((Art_SBZFloor[0] << 8) | Art_SBZFloor[1]) & 0x7FFF, 4);
    CHECK(((Art_SlideFloor[0] << 8) | Art_SlideFloor[1]) != ((Art_SBZFloor[0] << 8) | Art_SBZFloor[1]));
}

// SLZ loaded the bomb enemy art into the Orbinaut slot ($429), where the Orbinaut art
// overwrote it, but the object reads it from ArtTile_Bomb ($400).
extern const uint8_t Art_Bomb[];
static void PLC_SLZLoadsBombArtAtBombTile(void) {
    ClearPLC();
    NewPLC(PlcId_SLZ);
    bool found = false;
    for (int i = 0; i < 16 && plc_buffer[i].art != NULL; i++)
        if (plc_buffer[i].art == Art_Bomb && plc_buffer[i].off == ART_VRAM(ArtTile_Bomb))
            found = true;
    CHECK(found);
    ClearPLC();
}

// Diagnostic: the tile ranges that the pattern load cues of one level load into, and which of them overlap (Nemesis art: the first word's low 15 bits are the tile count)
// Green Hill's act 3: what the animals' art in the video memory (tiles $580-$59F) looks like once the boss's lists have been loaded over the level's own: it must be what it was
static void Drain(void) {
    for (int i = 0; i < 4000 && plc_buffer[0].art != NULL; i++) {
        RunPLC();
        ProcessDPLC();
    }
}

static void PLC_TheBossListsDoNotTouchTheAnimalsArt(void) {
    uint8_t *tiles = VDP_TileSpace();
    ClearPLC();
    NewPLC(PlcId_Explode);
    AddPLC(PlcId_GHZAnimals);
    Drain();
    static uint8_t before[0x20 * 32];
    memcpy(before, tiles + 0x580 * 32, sizeof(before));
    AddPLC(PlcId_Boss);
    Drain();
    CHECK(memcmp(before, tiles + 0x580 * 32, sizeof(before)) == 0);
    AddPLC(PlcId_Signpost);
    Drain();
    CHECK(memcmp(before, tiles + 0x580 * 32, sizeof(before)) == 0);
}

// The animals that Green Hill's prison capsule lets out are ordinary animals: the zone's rabbit and flicky art, never the ending's
static void PLC_PrisonAnimalsUseTheZonesAnimalArt(void) {
    level_id = LEVEL_ID(ZoneId_GHZ, 2);
    memset(level_objects, 0, sizeof(Object) * LEVEL_OBJECTS);
    Object *prison = &level_objects[0];
    prison->type = ObjId_PrisonCapsule;
    prison->routine = 0xA;
    prison->frame_time.w = 1;
    prison->pos.l.x.f.u = 0x100;
    prison->pos.l.y.f.u = 0x100;
    Obj_PrisonCapsule(prison);
    int animals = 0;
    for (int i = 1; i < LEVEL_OBJECTS; i++) {
        Object *a = &level_objects[i];
        if (a->type != ObjId_Animal)
            continue;
        animals++;
        Obj_Animals(a);
        const int tile = a->tile & 0x7FF;
        CHECK(tile == ArtTile_Animal_1 || tile == ArtTile_Animal_2);
        CHECK(a->mappings != NULL);
    }
    CHECK(animals > 0);
}

// The special stage's background canvases live at the video memory addresses of the original's ($14000 on in this port's tile numbering): the scratch plane memory must reach them, or the planes are empty
static void SS_BackgroundPlanesAreNotEmpty(void) {
    Viewport_SetSize(SCREEN_WIDTH, SCREEN_HEIGHT, 64, 64);
    SS_BGLoad();
    for (int mode = 0; mode < 14; mode += 2) {
        SS_BGSetMode((uint16_t)mode, ArtTile_SS_Plane_5);
        int a = 0, b = 0;
        for (size_t i = 0; i < screen1p.plane_a.count; i++)
            a += TileEntry_Word(&screen1p.plane_a.entries[i]) != 0;
        for (size_t i = 0; i < screen1p.plane_b.count; i++)
            b += TileEntry_Word(&screen1p.plane_b.entries[i]) != 0;
        CHECK(a > 0);
        CHECK(b > 0);
    }
}

// Green Hill's ship pieces (the face and the flame) go when the ship does: they used to stay and follow whatever took the ship's slot, which is where the prison capsule's animals land
static void Boss_TheShipsPiecesGoWithTheShip(void) {
    level_id = LEVEL_ID(ZoneId_GHZ, 2);
    memset(level_objects, 0, sizeof(Object) * LEVEL_OBJECTS);
    Object *ship = &level_objects[0];
    ship->type = ObjId_BossGreenHill;
    ship->pos.l.x.f.u = 0x2A60;
    ship->pos.l.y.f.u = 0x200;
    Obj_BossGreenHill(ship); // (its first frame makes the pieces)
    int pieces = 0;
    for (int i = 1; i < LEVEL_OBJECTS; i++)
        pieces += level_objects[i].type == ObjId_BossGreenHill;
    CHECK(pieces >= 2);
    ObjectDelete(ship); // (the ship has left)
    Object *animal = &level_objects[0];
    animal->type = ObjId_Animal; // (and an animal takes its slot)
    for (int i = 1; i < LEVEL_OBJECTS; i++) {
        Object *o = &level_objects[i];
        if (o->type == ObjId_BossGreenHill)
            Obj_BossGreenHill(o);
    }
    for (int i = 1; i < LEVEL_OBJECTS; i++)
        CHECK(level_objects[i].type != ObjId_BossGreenHill);
}

// The prison capsule's explosions are the fiery ones of Object 3F (a destroyed boss's, and the Walking Bomb's: the bomb's sound), not the badnik explosion
static void Prison_ExplodesLikeABoss(void) {
    level_id = LEVEL_ID(ZoneId_GHZ, 2);
    memset(level_objects, 0, sizeof(Object) * LEVEL_OBJECTS);
    Object *prison = &level_objects[0];
    prison->type = ObjId_PrisonCapsule;
    prison->routine = 0xA;
    prison->frame_time.w = 100;
    prison->pos.l.x.f.u = 0x100;
    prison->pos.l.y.f.u = 0x100;
    frame_count = 0; // (it makes one every 8th frame)
    Obj_PrisonCapsule(prison);
    int bombs = 0, badniks = 0;
    for (int i = 1; i < LEVEL_OBJECTS; i++) {
        bombs += level_objects[i].type == ObjId_ExplosionBomb;
        badniks += level_objects[i].type == ObjId_Explosion;
    }
    CHECK_EQ(bombs, 1);
    CHECK_EQ(badniks, 0);
}

void RegisterPLCTests(void) {
    RUN_TEST(PLC_TheBossListsDoNotTouchTheAnimalsArt);
    RUN_TEST(SS_BackgroundPlanesAreNotEmpty);
    RUN_TEST(PLC_PrisonAnimalsUseTheZonesAnimalArt);
    RUN_TEST(Boss_TheShipsPiecesGoWithTheShip);
    RUN_TEST(Prison_ExplodesLikeABoss);
    RUN_TEST(PLC_DrainsExactlyFullQueue);
    RUN_TEST(PLC_AddPLCDoesNotOverflowWhenFull);
    RUN_TEST(PLC_LZLoadsWaterfallSplashArt);
    RUN_TEST(PLC_SBZCollapsingFloorArtIsFourTiles);
    RUN_TEST(PLC_SLZLoadsBombArtAtBombTile);
}
