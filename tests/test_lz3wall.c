#include "test.h"

#include <string.h>

#include "Level.h"
#include "LevelScroll.h"
#include "Object.h"
#include "Macros.h"
#include "Object/LZWaterfall.h"
#include "LZWaterFeatures.h"
#include "Object/FloatingBlock.h"
#include "Game.h"

// Regression test for the LZ3 wall switch (2026-09). Switch $F makes
// DLE_LZ3 swap row 5, columns 12-13 from the closed wall ($F8/$F9) to
// $17/$18, and the hidden waterfall splash (Object 65, subtype $A9) inside
// that chunk goes onto the high plane only once the wall is open (MJ: P128
// compares the layout word against $1718). The port compared column 12
// against $F8 instead -- the closed wall -- so the splash drew in front of
// the wall before the switch and vanished once it opened.

static Object *SpawnHiddenSplash(void) {
    Object *splash = &level_objects[0];
    memset(splash, 0, sizeof(Object));
    splash->type = ObjId_LZWaterfall;
    splash->scratch.u8[0] = 0xA9;
    splash->pos.l.x.f.u = 0x658;
    splash->pos.l.y.f.u = 0x2F0;
    splash->respawn_index = 0; // no respawn entry to update
    return splash;
}

static void LZ3Wall_SplashFollowsSwitch(void) {
    level_id = LEVEL_ID(ZoneId_LZ, 2);
    LoadLevelLayout();
    memset(f_switch, 0, sizeof(f_switch));
    scrpos_x.f.u = 0x5C0; // camera over the wall so RememberState keeps the splash

    // Closed wall as shipped in the layout.
    CHECK_EQ(LEVEL_LAYOUT_FG(5)[12], 0xF8);
    CHECK_EQ(LEVEL_LAYOUT_FG(5)[13], 0xF9);

    Object *splash = SpawnHiddenSplash();
    Obj_LZWaterfall(splash); // WFall_Main
    CHECK_EQ(splash->routine, 8);
    Obj_LZWaterfall(splash); // WFall_Priority
    CHECK_EQ(splash->tile & 0x8000, 0); // hidden behind the closed wall

    // No switch yet: DLE leaves the layout alone.
    DynamicLevelEvents();
    CHECK_EQ(LEVEL_LAYOUT_FG(5)[12], 0xF8);

    // Press switch $F.
    f_switch[0xF] = 1;
    DynamicLevelEvents();
    CHECK_EQ(LEVEL_LAYOUT_FG(5)[12], 0x17);
    CHECK_EQ(LEVEL_LAYOUT_FG(5)[13], 0x18);

    Obj_LZWaterfall(splash);
    CHECK_EQ(splash->tile & 0x8000, 0x8000); // visible through the opened wall
}

// The original sets high priority with "bset #7,obGfx(a0)" -- a byte op on the
// high byte of a big-endian word, i.e. bit 15. The port OR-ed 0x80 into the
// tile word instead, shifting every high-priority waterfall piece $80 tiles
// into unrelated art.
static void LZWaterfall_HighPrioritySetsBit15(void) {
    scrpos_x.f.u = 0x5C0;
    Object *wf = SpawnHiddenSplash();
    wf->scratch.u8[0] = 0x80; // high-priority narrow vertical piece
    Obj_LZWaterfall(wf);
    CHECK_EQ(wf->tile, TILE_MAP(0, 2, 0, 0, ArtTile_LZ_Splash) | 0x8000);
}

// Object 61's generic block frame is a single piece with tile word $FDFA
// ($5FA, x/y flip, palette 3, high priority) -- the port's mapping data had
// $FFFA, which drew unrelated tiles instead of the 32x32 block.
extern const uint8_t Mappings_LZBlocks[];
static void LZBlocks_BlockFrameMatchesROM(void) {
    const uint8_t *m = Mappings_LZBlocks;
    const uint8_t *frame = m + ((m[6] << 8) | m[7]);
    CHECK_EQ(frame[0], 1);
    CHECK_EQ((frame[3] << 8) | frame[4], 0xFDFA);
}

// DynWater_LZ3 routine 2: at the lamppost (camera < $1860) the water stays
// shallow at $508 and the routine must not advance. The port advanced on the
// first frame and routine 3 snapped the water to $188.
static void LZ3Water_StaysShallowAtLamppost(void) {
    level_id = LEVEL_ID(ZoneId_LZ, 2);
    memset(player, 0, sizeof(Object));
    player->routine = 2;
    player->pos.l.y.f.u = 0x4EC;
    nobgscroll = 0;
    wtr_routine = 2;
    wtr_pos2 = wtr_pos3 = 0x508;
    scrpos_x.f.u = 0x1790;
    for (int i = 0; i < 60; i++)
        LZWaterFeatures();
    CHECK_EQ(wtr_routine, 2);
    CHECK_EQ(wtr_pos2, 0x508);

    // Reaching the first cork: water rises 1px/frame towards $188, and
    // routine 3 only takes over once it gets there.
    scrpos_x.f.u = 0x1870;
    LZWaterFeatures();
    CHECK_EQ(wtr_routine, 2);
    CHECK_EQ(wtr_pos3, 0x188);
    CHECK_EQ(wtr_pos2, 0x507);
    for (int i = 0; i < 0x508 - 0x188; i++)
        LZWaterFeatures();
    CHECK_EQ(wtr_pos2, 0x188);
    LZWaterFeatures();
    CHECK_EQ(wtr_routine, 3);
}

// Port change (2026-09): LZ1's first wind tunnel (underwater lower route)
// stays completely off while the switch-3 door across it is shut or still
// sliding open -- the tunnel used to push Sonic into the door and kill him.
static void LZ1Tunnel_OffUntilDoorFullyOpen(void) {
    level_id = LEVEL_ID(ZoneId_LZ, 0);
    memset(level_objects, 0, sizeof(Object) * LEVEL_OBJECTS);
    memset(f_switch, 0, sizeof(f_switch));
    f_wtunneldisallow = false;
    f_lz1tunnel_open = false;
    tunnel_mode = 0;
    debug_use = false;
    nobgscroll = 0;
    scrpos_x.f.u = 0xA40;

    Object *door = &level_objects[0];
    door->type = ObjId_FloatingBlock;
    door->scratch.u8[0] = 0xE3; // small LZ door, switch 3
    door->pos.l.x.f.u = 0xB08;
    door->pos.l.y.f.u = 0x2E0;

    memset(player, 0, sizeof(Object));
    player->type = ObjId_Sonic;
    player->routine = 2;

    // Sonic in the tunnel box but RIGHT of the shut door: the original
    // position check would have let the tunnel run here.
    for (int f = 0; f < 5; f++) {
        player->pos.l.x.f.u = 0xB40;
        player->pos.l.y.f.u = 0x340;
        Obj_FloatingBlock(door);
        LZWaterFeatures();
        CHECK_EQ(tunnel_mode, 0);
    }

    // Press the switch: door slides open 2px/frame, tunnel stays off meanwhile.
    f_switch[3] = 1;
    int frames = 0;
    for (; frames < 200 && !f_lz1tunnel_open; frames++) {
        player->pos.l.x.f.u = 0xAE0;
        player->pos.l.y.f.u = 0x340;
        Obj_FloatingBlock(door);
        LZWaterFeatures();
        if (!f_lz1tunnel_open)
            CHECK_EQ(tunnel_mode, 0);
    }
    CHECK(frames > 1);
    CHECK(f_lz1tunnel_open);

    // Fully open: the tunnel carries Sonic.
    player->pos.l.x.f.u = 0xAE0;
    player->pos.l.y.f.u = 0x340;
    Obj_FloatingBlock(door);
    LZWaterFeatures();
    CHECK_EQ(tunnel_mode, 1);
    CHECK_EQ(player->pos.l.x.f.u, 0xAE4);
}

void RegisterLZ3WallTests(void) {
    RUN_TEST(LZ3Wall_SplashFollowsSwitch);
    RUN_TEST(LZ1Tunnel_OffUntilDoorFullyOpen);
    RUN_TEST(LZ3Water_StaysShallowAtLamppost);
    RUN_TEST(LZWaterfall_HighPrioritySetsBit15);
    RUN_TEST(LZBlocks_BlockFrameMatchesROM);
}
