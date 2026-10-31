#include "test.h"

#include <string.h>

#include "Level.h"
#include "LevelScroll.h"
#include "Object.h"
#include "Object/SpikeBall.h"
#include "SstLoad.h"

// Object 57 (spiked ball on a chain) takes its chain links from free slots, which may still hold stale bytes. Every
// link must come out as a plain chain link (frame 0), except the LZ stem (frame 2, radius 0) -- a stale frame made
// links invisible or wrong.

static void SpikeBall_LZChainLinksAreCleanInDirtySlots(void) {
    static const uint8_t subtypes[] = { 0x26, 0x65, 0xC4, 0xD5, 0x54, 0x2E };
    for (unsigned s = 0; s < sizeof(subtypes); s++) {
        Sst_Reset();
        for (int i = SST_FIRST_DYNAMIC; i < SST_COUNT; i++) {
            memset(&sst[i], 0xAB, sizeof(Sst));
            sst[i].code = NULL;
        }
        level_id = LEVEL_ID(ZoneId_LZ, 2);
        memset(player, 0, sizeof(Sst));
        scrpos_x.f.u = 0;
        SstLoadCamera camera = {0, 0, false, 0xFFF};
        static const SstObjectIndex index_ = {sst_object_codes, 0x100};
        SstLoad_Init(&index_, NULL, false, &camera);

        Sst *ball = &sst[SST_FIRST_LEVEL + 3];
        memset(ball, 0, sizeof(Sst));
        ball->code = sst_object_codes[ObjId_SpikeBall];
        ball->x_pos.f.u = 0x100;
        ball->y_pos.f.u = 0x100;
        ball->subtype = subtypes[s];
        Obj_SpikeBall(ball);

        Own_SpikeBall *sc = (Own_SpikeBall *)ball->own.u8;
        int expect = subtypes[s] & 7;
        if (expect > 0 && (subtypes[s] & 8)) expect--;
        CHECK_EQ(sc->children, expect);
        CHECK_EQ(ball->mapping_frame, 1);
        for (int i = 0; i < sc->children; i++) {
            Sst *link = &sst[sc->child_idx[i]];
            Own_SpikeBall *ls = (Own_SpikeBall *)link->own.u8;
            CHECK(link->code == ball->code);
            CHECK_EQ(link->routine, 4);
            CHECK_EQ(link->mapping_frame, ls->radius == 0 ? 2 : 0);
            CHECK_EQ(link->anim, 0);
            CHECK_EQ(link->collision_flags, 0);
        }
    }
}

// The chain is made from the free slots wherever they are, also when the ball sits after all of them.
static void SpikeBall_ChainSpawnsWhenNoSlotFreeAfterParent(void) {
    Sst_Reset();
    for (int i = SST_FIRST_DYNAMIC; i < SST_COUNT; i++)
        sst[i].code = i < SST_FIRST_DYNAMIC + 10 ? NULL : sst_object_codes[ObjId_Points]; // only the first ten are free
    level_id = LEVEL_ID(ZoneId_LZ, 0);
    memset(player, 0, sizeof(Sst));
    scrpos_x.f.u = 0;
    SstLoadCamera camera = {0, 0, false, 0xFFF};
    static const SstObjectIndex index_ = {sst_object_codes, 0x100};
    SstLoad_Init(&index_, NULL, false, &camera);

    Sst *ball = &sst[SST_COUNT - 1];
    memset(ball, 0, sizeof(Sst));
    ball->code = sst_object_codes[ObjId_SpikeBall];
    ball->x_pos.f.u = 0x100;
    ball->y_pos.f.u = 0x100;
    ball->subtype = 0xD4; // an LZ1 placement: 4 links
    Obj_SpikeBall(ball);

    Own_SpikeBall *sc = (Own_SpikeBall *)ball->own.u8;
    CHECK_EQ(sc->children, 4);
}

void RegisterSpikeBallTests(void) {
    RUN_TEST(SpikeBall_ChainSpawnsWhenNoSlotFreeAfterParent);
    RUN_TEST(SpikeBall_LZChainLinksAreCleanInDirtySlots);
}
