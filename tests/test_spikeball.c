#include "test.h"

#include <string.h>

#include "Level.h"
#include "LevelScroll.h"
#include "Object.h"
#include "Object/SpikeBall.h"

// Object 57 (spiked ball on a chain) spawns its chain links with
// FindNextFreeObj, which hands back slots that may still hold stale bytes. Every
// link must come out as a plain chain link (frame 0), except the LZ stem
// (frame 2, radius 0) -- a stale frame made links invisible or wrong.

static void SpikeBall_LZChainLinksAreCleanInDirtySlots(void) {
    static const uint8_t subtypes[] = { 0x26, 0x65, 0xC4, 0xD5, 0x54, 0x2E };
    for (unsigned s = 0; s < sizeof(subtypes); s++) {
        for (int i = 0; i < LEVEL_OBJECTS; i++) {
            memset(&level_objects[i], 0xAB, sizeof(Object));
            level_objects[i].type = ObjId_Null;
        }
        level_id = LEVEL_ID(ZoneId_LZ, 2);
        memset(player, 0, sizeof(Object));
        scrpos_x.f.u = 0;

        Object *ball = &level_objects[3];
        memset(ball, 0, sizeof(Object));
        ball->type = ObjId_SpikeBall;
        ball->pos.l.x.f.u = 0x100;
        ball->pos.l.y.f.u = 0x100;
        ball->scratch.u8[0] = subtypes[s];
        Obj_SpikeBall(ball);

        Scratch_SpikeBall *sc = (Scratch_SpikeBall *)&ball->scratch;
        int expect = subtypes[s] & 7;
        if (expect > 0 && (subtypes[s] & 8)) expect--;
        CHECK_EQ(sc->children, expect);
        CHECK_EQ(ball->frame, 1);
        for (int i = 0; i < sc->children; i++) {
            Object *link = &objects[sc->child_idx[i]];
            Scratch_SpikeBall *ls = (Scratch_SpikeBall *)&link->scratch;
            CHECK_EQ(link->type, ObjId_SpikeBall);
            CHECK_EQ(link->routine, 4);
            CHECK_EQ(link->frame, ls->radius == 0 ? 2 : 0);
            CHECK_EQ(link->anim, 0);
            CHECK_EQ(link->col_type, 0);
        }
    }
}

void RegisterSpikeBallTests(void) {
    RUN_TEST(SpikeBall_LZChainLinksAreCleanInDirtySlots);
}
