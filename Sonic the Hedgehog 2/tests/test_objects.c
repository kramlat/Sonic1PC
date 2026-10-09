#include "test.h"

#include "Level.h"
#include "Object.h"

// The prototype's levels place objects by id; every id a built zone places has to run something (Obj_Null just deletes itself). The zones that are built are checked completely: the ids that are still not
// ported are listed here, so a zone that gains an object (or one that loses it) makes the test say so (remove the id from the list when it is ported).

typedef struct {
    int zone;
    int count;
    uint8_t ids[16];
} Pending;

// The object ids that the layouts of these zones place and that are still the null object (empty list: nothing missing)
static const Pending pending[] = {
    { ZoneId_EHZ, 0, { 0 } },
    { ZoneId_HTZ, 0, { 0 } },
    { ZoneId_HPZ, 0, { 0 } },
    { ZoneId_CPZ, 0, { 0 } },
    { ZoneId_ARZ, 0, { 0 } },
};

static bool IsPending(const Pending *p, uint8_t id) {
    for (int i = 0; i < p->count; i++)
        if (p->ids[i] == id)
            return true;
    return false;
}

static void ObjectCoverage_BuiltZonesPortEveryObjectTheyPlace(void) {
    for (size_t z = 0; z < sizeof(pending) / sizeof(pending[0]); z++) {
        const Pending *p = &pending[z];
        for (int act = 0; act < 3; act++) {
            for (const uint8_t *e = level_obj[p->zone][act]; e != NULL && !(e[0] == 0xFF && e[1] == 0xFF); e += 6) {
                const uint8_t id = e[4] & 0x7F;
                const bool ported = id < game_object_count && game_objects[id] != NULL && game_objects[id] != Obj_Null;
                if (!ported && !IsPending(p, id)) {
                    printf("\n    zone %d act %d places object %02X, which is not ported", p->zone, act, id);
                    CHECK(ported);
                    break; // (one report for each zone and act is enough)
                }
            }
        }
    }
}

static void ObjectCoverage_NoPlacedObjectIsFalselyPending(void) {
    for (size_t z = 0; z < sizeof(pending) / sizeof(pending[0]); z++) {
        const Pending *p = &pending[z];
        for (int i = 0; i < p->count; i++) {
            const uint8_t id = p->ids[i];
            CHECK(!(id < game_object_count && game_objects[id] != NULL && game_objects[id] != Obj_Null)); // (it was ported: take it off the list)
        }
    }
}

void RegisterObjectCoverageTests(void) {
    RUN_TEST(ObjectCoverage_BuiltZonesPortEveryObjectTheyPlace);
    RUN_TEST(ObjectCoverage_NoPlacedObjectIsFalselyPending);
}
