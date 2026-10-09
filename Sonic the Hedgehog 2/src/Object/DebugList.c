// Sonic 2's debug (object placement) lists: the Simon Wai prototype's Debug_* tables (JmpTbl_DbgObjLists, loc_23DBE on), a list for each zone in the order the prototype has them, the same entries (the mapping, the art, the
// subtype and the frame each shows) in the same order. Sonic 1's DebugList.c (a copy of which this is, with the lists of Sonic 2's zones in place of its own) has the preview machinery that Sonic.c uses: the ring's subtype
// previews as the formation it makes, and the monitor's as the box with its item. An object this port has not built yet is a placeholder that is counted and cannot be placed (type null), as the list's length and order
// are the prototype's. Zones with no list of their own (the prototype's Debug_Null) have a ring and a monitor.
#include "Object/DebugList.h"
#include "Constants.h"

#include <stdbool.h>

#include "Game.h"
#include "Level.h"

#include "Backend/VDP.h"

extern const uint8_t Mappings_RingREV01[];
extern const uint8_t Mappings_Monitor[];
extern const uint8_t Mappings_Checkpoint[];
extern const uint8_t Mappings_PathSwapper[];
extern const uint8_t Mappings_WaterfallEHZ[];
extern const uint8_t Mappings_PlatformEHZ[];
extern const uint8_t Mappings_PlatformNGHZ[];
extern const uint8_t Mappings_Spikes[];
extern const uint8_t Mappings_Spring[];
extern const uint8_t Mappings_DiagSpring[];
extern const uint8_t Mappings_Buzzer[];
extern const uint8_t Mappings_Snail[];
extern const uint8_t Mappings_Masher[];
extern const uint8_t Mappings_MTZSteamVent[];
extern const uint8_t Mappings_MTZPiston[];
extern const uint8_t Mappings_MTZPlatformA[];
extern const uint8_t Mappings_MTZSpringWall[];
extern const uint8_t Mappings_MTZBlockArrow[];
extern const uint8_t Mappings_MTZScrewNut[];
extern const uint8_t Mappings_MTZMachine[];
extern const uint8_t Mappings_MTZElevator[];
extern const uint8_t Mappings_MTZGear[];
extern const uint8_t Mappings_MTZLavaBubble[];
extern const uint8_t Mappings_SceneryA[];
extern const uint8_t Mappings_SceneryD[];
extern const uint8_t Mappings_CPZBarrier[];
extern const uint8_t Mappings_Switch[];
extern const uint8_t Mappings_HTZSeesaw[];
extern const uint8_t Mappings_HTZBreakFloor[];
extern const uint8_t Mappings_HTZFireball[];
extern const uint8_t Mappings_HTZLift[];
extern const uint8_t Mappings_HTZRock[];
extern const uint8_t Mappings_HPZOrb[];
extern const uint8_t Mappings_HPZWaterfall[];
extern const uint8_t Mappings_LedgeHPZ[];
extern const uint8_t Mappings_Redz[];
extern const uint8_t Mappings_BBat[];
extern const uint8_t Mappings_OOZLauncher[];
extern const uint8_t Mappings_OOZSpikeball[];
extern const uint8_t Mappings_CPZElevator[];
extern const uint8_t Mappings_OOZPushSpring[];
extern const uint8_t Mappings_OOZSpringBall[];
extern const uint8_t Mappings_OOZSwing[];
extern const uint8_t Mappings_OOZCannon[];
extern const uint8_t Mappings_CollapsePlatformOOZ[];
extern const uint8_t Mappings_CollapsePlatformDHZ[];
extern const uint8_t Mappings_DHZSwing[];
extern const uint8_t Mappings_NGHZSwing[];
extern const uint8_t Mappings_RotatingBoxes[];
extern const uint8_t Mappings_DHZStomper[];
extern const uint8_t Mappings_CPZInvisibleBlock[];
extern const uint8_t Mappings_SpikeballChain[];
extern const uint8_t Mappings_PlatformSpikes[];
extern const uint8_t Mappings_DHZGate[];
extern const uint8_t Mappings_PipeTipper[];
extern const uint8_t Mappings_Booster[];
extern const uint8_t Mappings_CPZWorm[];
extern const uint8_t Mappings_TubeCover[];
extern const uint8_t Mappings_CPZBlock[];
extern const uint8_t Mappings_CPZSlider[];
extern const uint8_t Mappings_TubeSpring[];
extern const uint8_t Mappings_ArrowShooter[];
extern const int8_t ring_pos[16][2]; // Object/Ring.c -- ring row/column formation offsets

// The prototype's Obj31_MapUnc_15612: three empty frames (the lava boxes and the leaves are never drawn)
static const uint8_t Mappings_EmptyBox[] = { 0, 6, 0, 6, 0, 6, 0, 0 };

#define NULL_ENTRY {ObjId_Null, NULL, 0, 0, 0, NULL, 0, 0, 0}
#define OBJ(id) ((ObjectId)(id))
#define PLAIN(id, map, tile, sub, frame) {OBJ(id), map, tile, sub, frame, NULL, 0, 0, 0}

// The ring's subtype is how many more there are (bits 0-2, 7 is 6) and which way they go (bits 4-7, ring_pos): the preview is the formation. Built when first asked for, from Ring.c's own table, so that it cannot
// drift from the placement.
#define RING_MAX_EXTRA 7
static DebugFramePiece DebugStack_Ring[16][RING_MAX_EXTRA];
static DebugSubtypeVariant DebugVariants_Ring[16 * 8];
static bool debug_ring_variants_ready = false;

static void DebugList_InitRingVariants(void) {
    if (debug_ring_variants_ready)
        return;
    debug_ring_variants_ready = true;

    for (int dir = 0; dir < 16; dir++) {
        for (int i = 0; i < RING_MAX_EXTRA; i++) {
            DebugStack_Ring[dir][i].frame = 0;
            DebugStack_Ring[dir][i].x_off = (int16_t)(ring_pos[dir][0] * i);
            DebugStack_Ring[dir][i].y_off = (int16_t)(ring_pos[dir][1] * i);
        }
        for (int num = 0; num < 8; num++) {
            int eff = (num == 7) ? 6 : num;
            DebugSubtypeVariant *v = &DebugVariants_Ring[dir * 8 + num];
            v->subtype_key = (uint8_t)((dir << 4) | num);
            v->stack = DebugStack_Ring[dir];
            v->stack_count = (uint8_t)(eff + 1);
            v->flip = 0;
            v->pal_add = 0;
            v->tile_override = 0;
        }
    }
}
#define RING_VARIANTS DebugVariants_Ring, 128, 0, 0xFF

// The monitor's subtype is the item and its icon is the mapping's frame subtype + 1 (Monitor.c): 1 Sonic, 2 Tails, 4 rings, 5 shoes, 6 shield, 7 invincibility do something; the box is drawn with its icon
static const DebugFramePiece DebugStack_Monitor[8][2] = {
    {{1, 0, 0}, {0, 0, 0}}, {{2, 0, 0}, {0, 0, 0}}, {{3, 0, 0}, {0, 0, 0}}, {{4, 0, 0}, {0, 0, 0}},
    {{5, 0, 0}, {0, 0, 0}}, {{6, 0, 0}, {0, 0, 0}}, {{7, 0, 0}, {0, 0, 0}}, {{8, 0, 0}, {0, 0, 0}},
};
static const DebugSubtypeVariant DebugVariants_Monitor[] = {
    {1, DebugStack_Monitor[1], 2, 0, 0, 0}, {2, DebugStack_Monitor[2], 2, 0, 0, 0},
    {4, DebugStack_Monitor[4], 2, 0, 0, 0}, {5, DebugStack_Monitor[5], 2, 0, 0, 0},
    {6, DebugStack_Monitor[6], 2, 0, 0, 0}, {7, DebugStack_Monitor[7], 2, 0, 0, 0},
};
#define MONITOR_VARIANTS DebugVariants_Monitor, 6, 0, 0xFF

#define RING_ENTRY {OBJ(0x25), Mappings_RingREV01, TILE_MAP(0, 1, 0, 0, ArtTile_Ring), 0, 0, RING_VARIANTS}
#define MONITOR_ENTRY(sub) {OBJ(0x26), Mappings_Monitor, 0x680, sub, 0, MONITOR_VARIANTS}
#define CHECKPOINT_ENTRY PLAIN(0x79, Mappings_Checkpoint, TILE_MAP(0, 0, 0, 0, ArtTile_Lamppost), 1, 0)
#define PATHSWAPPER_ENTRY PLAIN(0x03, Mappings_PathSwapper, TILE_MAP(0, 1, 0, 0, ArtTile_Ring), 9, 1)
#define SPIKES_ENTRY(sub, frame) PLAIN(0x36, Mappings_Spikes, TILE_MAP(0, 1, 0, 0, ArtTile_Spikes), sub, frame)
#define SPRING_UP_RED PLAIN(0x41, Mappings_Spring, TILE_MAP(0, 0, 0, 0, ArtTile_SpringUp), 0x81, 0)
#define SPRING_SIDE_RED PLAIN(0x41, Mappings_Spring, TILE_MAP(0, 0, 0, 0, ArtTile_SpringSide), 0x90, 3)
#define SPRING_DOWN_RED PLAIN(0x41, Mappings_Spring, TILE_MAP(0, 0, 0, 0, ArtTile_SpringUp), 0xA0, 6)
#define SPRING_DIAG_UP PLAIN(0x41, Mappings_Spring, TILE_MAP(0, 0, 0, 0, ArtTile_SpringDiag), 0x30, 7)
#define SPRING_DIAG_DOWN PLAIN(0x41, Mappings_Spring, TILE_MAP(0, 0, 0, 0, ArtTile_SpringDiag), 0x40, 0xA)
#define DIAG_SPRING_ENTRY PLAIN(0x40, Mappings_DiagSpring, 0x440, 1, 0)

// Debug_Null
static const DebugListEntry DebugList_Null[] = {
    RING_ENTRY,
    MONITOR_ENTRY(0),
};

// Debug_GHZ (Emerald Hill)
static const DebugListEntry DebugList_EHZ[] = {
    RING_ENTRY,
    MONITOR_ENTRY(7),
    CHECKPOINT_ENTRY,
    PATHSWAPPER_ENTRY,
    PLAIN(0x49, Mappings_WaterfallEHZ, 0x23AE, 0, 0),
    PLAIN(0x49, Mappings_WaterfallEHZ, 0x23AE, 2, 3),
    PLAIN(0x49, Mappings_WaterfallEHZ, 0x23AE, 4, 5),
    PLAIN(0x18, Mappings_PlatformEHZ, 0x4000, 1, 0),
    PLAIN(0x18, Mappings_PlatformEHZ, 0x4000, 0x9A, 1),
    SPIKES_ENTRY(0, 0),
    SPRING_UP_RED,
    SPRING_SIDE_RED,
    SPRING_DOWN_RED,
    SPRING_DIAG_UP,
    SPRING_DIAG_DOWN,
    PLAIN(0x4B, Mappings_Buzzer, 0x3E6, 0, 0),
    PLAIN(0x54, Mappings_Snail, 0x402, 0, 0),
    PLAIN(0x53, Mappings_Masher, 0x41C, 0, 0),
};

// Debug_MTZ (Metropolis)
static const DebugListEntry DebugList_MTZ[] = {
    RING_ENTRY,
    MONITOR_ENTRY(7),
    CHECKPOINT_ENTRY,
    PATHSWAPPER_ENTRY,
    PLAIN(0x42, Mappings_MTZSteamVent, 0x6000, 1, 7),
    PLAIN(0x64, Mappings_MTZPiston, 0x2000, 1, 0),
    PLAIN(0x64, Mappings_MTZPiston, 0x2000, 0x11, 1),
    PLAIN(0x65, Mappings_MTZPlatformA, 0x6000, 0x80, 0),
    PLAIN(0x65, Mappings_MTZPlatformA, 0x6000, 0x13, 1),
    PLAIN(0x47, Mappings_Switch, 0x424, 0, 2),
    PLAIN(0x2D, Mappings_CPZBarrier, 0x6000, 1, 1),
    PLAIN(0x66, Mappings_MTZSpringWall, 0x8680, 1, 0),
    PLAIN(0x66, Mappings_MTZSpringWall, 0x8680, 0x11, 1),
    PLAIN(0x68, Mappings_MTZBlockArrow, 0x6414, 0, 4),
    PLAIN(0x69, Mappings_MTZScrewNut, 0x2500, 4, 0),
    PLAIN(0x6A, Mappings_MTZPlatformA, 0x6000, 0, 1),
    PLAIN(0x6B, Mappings_MTZPlatformA, 0x6000, 1, 1),
    PLAIN(0x6D, Mappings_MTZBlockArrow, 0x241C, 0, 0),
    PLAIN(0x6E, Mappings_MTZMachine, 0x6000, 0, 0),
    PLAIN(0x6E, Mappings_MTZMachine, 0x6000, 0x10, 1),
    PLAIN(0x6E, Mappings_MTZMachine, 0x6000, 0x20, 2),
    PLAIN(0x6F, Mappings_MTZElevator, 0x653F, 0, 0),
    PLAIN(0x70, Mappings_MTZGear, 0xE378, 0x10, 0),
    PLAIN(0x71, Mappings_MTZLavaBubble, 0x4536, 0x22, 5),
    PLAIN(0x1C, Mappings_SceneryD, 0x43FD, 0, 0),
    PLAIN(0x1C, Mappings_SceneryD, 0x43FD, 1, 1),
    PLAIN(0x1C, Mappings_SceneryD, 0x23FD, 3, 2),
    PLAIN(0x65, Mappings_MTZPlatformA, 0x6000, 0xB0, 0),
};

// Debug_HTZ (Hill Top)
static const DebugListEntry DebugList_HTZ[] = {
    RING_ENTRY,
    MONITOR_ENTRY(7),
    CHECKPOINT_ENTRY,
    PATHSWAPPER_ENTRY,
    PLAIN(0x18, Mappings_PlatformEHZ, 0x4000, 1, 0),
    PLAIN(0x18, Mappings_PlatformEHZ, 0x4000, 0x9A, 1),
    SPIKES_ENTRY(0, 0),
    PLAIN(0x14, Mappings_HTZSeesaw, 0x3C6, 0, 0),
    PLAIN(0x2D, Mappings_CPZBarrier, 0x2426, 0, 0),
    PLAIN(0x2F, Mappings_HTZBreakFloor, 0xC000, 0, 0),
    PLAIN(0x20, Mappings_HTZFireball, 0x8416, 0x44, 2),
    SPRING_UP_RED,
    SPRING_SIDE_RED,
    SPRING_DOWN_RED,
    SPRING_DIAG_UP,
    SPRING_DIAG_DOWN,
    PLAIN(0x16, Mappings_HTZLift, 0x43E6, 0, 0),
    PLAIN(0x1C, Mappings_HTZLift, 0x43E6, 4, 3),
    PLAIN(0x1C, Mappings_HTZLift, 0x43E6, 5, 4),
    PLAIN(0x1C, Mappings_SceneryA, 0x4000, 7, 0),
    PLAIN(0x1C, Mappings_SceneryA, 0x4000, 8, 1),
    PLAIN(0x32, Mappings_HTZRock, 0x43B2, 0, 0),
    PLAIN(0x31, Mappings_EmptyBox, 0x8680, 0, 0),
    PLAIN(0x31, Mappings_EmptyBox, 0x8680, 1, 1),
    PLAIN(0x31, Mappings_EmptyBox, 0x8680, 2, 2),
};

// Debug_HPZ (Hidden Palace)
static const DebugListEntry DebugList_HPZ[] = {
    RING_ENTRY,
    MONITOR_ENTRY(7),
    PLAIN(0x71, Mappings_HPZOrb, 0xE35A, 0x11, 3),
    PLAIN(0x13, Mappings_HPZWaterfall, 0xE315, 4, 4),
    PLAIN(0x1A, Mappings_LedgeHPZ, 0x434A, 0, 0),
    PATHSWAPPER_ENTRY,
    PLAIN(0x4F, Mappings_Redz, 0x500, 0, 0),
    PLAIN(0x4C, Mappings_BBat, 0x2530, 0, 0),
};

// Debug_OOZ (Oil Ocean)
static const DebugListEntry DebugList_OOZ[] = {
    RING_ENTRY,
    MONITOR_ENTRY(7),
    PLAIN(0x33, Mappings_OOZLauncher, 0x632C, 1, 0),
    PLAIN(0x43, Mappings_OOZSpikeball, 0xC30C, 0, 0),
    PLAIN(0x19, Mappings_CPZElevator, 0x6300, 0x23, 2),
    PLAIN(0x45, Mappings_OOZPushSpring, 0x43C5, 2, 0),
    PLAIN(0x45, Mappings_OOZPushSpring, 0x43C5, 0x12, 0xA),
    PLAIN(0x46, Mappings_OOZSpringBall, 0x6354, 0, 1),
    PLAIN(0x47, Mappings_Switch, 0x424, 0, 2),
    PLAIN(0x15, Mappings_OOZSwing, 0x43E3, 0x88, 1),
    NULL_ENTRY, // 3D: the prototype's object here is not ported yet
    PLAIN(0x48, Mappings_OOZCannon, 0x6368, 0x80, 0),
    PLAIN(0x48, Mappings_OOZCannon, 0x6368, 0x81, 1),
    PLAIN(0x48, Mappings_OOZCannon, 0x6368, 0x82, 2),
    PLAIN(0x48, Mappings_OOZCannon, 0x6368, 0x83, 3),
    PLAIN(0x1F, Mappings_CollapsePlatformOOZ, 0x639D, 0, 0),
};

// Debug_DHZ (Dust Hill)
static const DebugListEntry DebugList_DHZ[] = {
    RING_ENTRY,
    MONITOR_ENTRY(0),
    PLAIN(0x15, Mappings_DHZSwing, 0, 0x48, 2),
    PLAIN(0x1F, Mappings_CollapsePlatformDHZ, 0x63F4, 0, 0),
    NULL_ENTRY, // 73: the rotating rings are not ported yet
    PLAIN(0x6A, Mappings_RotatingBoxes, 0x63D4, 0x18, 0),
    PLAIN(0x2A, Mappings_DHZStomper, 0x4000, 0, 0),
    SPIKES_ENTRY(0, 0),
    SPIKES_ENTRY(0x40, 4),
    SPRING_UP_RED,
    SPRING_SIDE_RED,
    DIAG_SPRING_ENTRY,
    PLAIN(0x74, Mappings_CPZInvisibleBlock, 0x8680, 0x11, 0),
    PLAIN(0x75, Mappings_SpikeballChain, 0x2000, 0x18, 2),
    PLAIN(0x76, Mappings_PlatformSpikes, 0, 0, 0),
    PLAIN(0x77, Mappings_DHZGate, 0x643C, 1, 0),
};

// Debug_CNZ (Casino Night)
static const DebugListEntry DebugList_CNZ[] = {
    RING_ENTRY,
    MONITOR_ENTRY(0),
};

// Debug_CPZ (Chemical Plant)
static const DebugListEntry DebugList_CPZ[] = {
    RING_ENTRY,
    MONITOR_ENTRY(7),
    PLAIN(0x0B, Mappings_PipeTipper, 0xE3B0, 0x70, 0),
    PLAIN(0x1B, Mappings_Booster, 0xE39C, 0, 0),
    PLAIN(0x1D, Mappings_CPZWorm, 0xE43C, 0x15, 0),
    PLAIN(0x19, Mappings_CPZElevator, 0x63A0, 6, 0),
    PLAIN(0x2D, Mappings_CPZBarrier, 0x2394, 2, 2),
    PLAIN(0x32, Mappings_TubeCover, 0x6430, 0, 0),
    PLAIN(0x6B, Mappings_CPZBlock, 0x6418, 0x10, 0),
    PLAIN(0x78, Mappings_CPZBlock, 0x6418, 0, 0),
    PLAIN(0x7A, Mappings_CPZSlider, 0xE418, 0, 0),
    PLAIN(0x7B, Mappings_TubeSpring, 0x3E0, 2, 0),
    PATHSWAPPER_ENTRY,
    {OBJ(0x03), Mappings_PathSwapper, TILE_MAP(0, 1, 0, 0, ArtTile_Ring), 0xD, 5, NULL, 0, 0, 0},
    SPIKES_ENTRY(0, 0),
    SPRING_UP_RED,
    SPRING_SIDE_RED,
    SPRING_DOWN_RED,
    DIAG_SPRING_ENTRY,
};

// Debug_NGHZ (Neo Green Hill)
static const DebugListEntry DebugList_NGHZ[] = {
    RING_ENTRY,
    MONITOR_ENTRY(0),
    PLAIN(0x15, Mappings_NGHZSwing, 0, 0x88, 2),
    PLAIN(0x18, Mappings_PlatformNGHZ, 0x4000, 1, 0),
    PLAIN(0x18, Mappings_PlatformNGHZ, 0x4000, 0x9A, 1),
    PLAIN(0x22, Mappings_ArrowShooter, 0x417, 0, 1),
    NULL_ENTRY, // 23: the pillar is not ported yet
    NULL_ENTRY, // 2B: the breakable pillar is not ported yet
    PLAIN(0x2C, Mappings_EmptyBox, 0x8680, 0, 0),
    PLAIN(0x2C, Mappings_EmptyBox, 0x8680, 1, 1),
    PLAIN(0x2C, Mappings_EmptyBox, 0x8680, 2, 2),
    DIAG_SPRING_ENTRY,
    SPRING_UP_RED,
    SPRING_SIDE_RED,
    SPRING_DOWN_RED,
    PATHSWAPPER_ENTRY,
    SPIKES_ENTRY(0, 0),
};

#define LIST(name) list = name; count = sizeof(name) / sizeof(name[0])

const DebugListEntry *DebugList_Get(int *count_out) {
    const DebugListEntry *list;
    int count;

    DebugList_InitRingVariants();

    switch (LEVEL_ZONE(level_id)) {
    case ZoneId_EHZ: LIST(DebugList_EHZ); break;
    case ZoneId_MTZ:
    case ZoneId_MTZ3: LIST(DebugList_MTZ); break;
    case ZoneId_HTZ: LIST(DebugList_HTZ); break;
    case ZoneId_HPZ: LIST(DebugList_HPZ); break;
    case ZoneId_OOZ: LIST(DebugList_OOZ); break;
    case ZoneId_MCZ: LIST(DebugList_DHZ); break;
    case ZoneId_CNZ: LIST(DebugList_CNZ); break;
    case ZoneId_CPZ: LIST(DebugList_CPZ); break;
    case ZoneId_ARZ: LIST(DebugList_NGHZ); break;
    default: LIST(DebugList_Null); break;
    }

    *count_out = count;
    return list;
}
