#include "DebugList.h"

#include <stdbool.h>

#include "Level.h"
#include "Game.h"

#include "Backend/VDP.h"

// Plain extern declarations rather than pulling in each object's own
// header -- several of those headers directly #include their
// Resource/Mappings/*.h (an actual array *definition*, not just a
// declaration, see e.g. Object/Ring.h), so including them a second time
// here would give this translation unit its own duplicate definition and
// fail to link. A bare `extern const uint8_t X[];` is all a data-only
// reference like this needs.
extern const uint8_t Mappings_RingREV01[];
extern const uint8_t Mappings_Monitor[];
extern const uint8_t Mappings_Crabmeat[];
extern const uint8_t Mappings_BuzzBomber[];
extern const uint8_t Mappings_Chopper[];
extern const uint8_t Mappings_Spikes[];
extern const uint8_t Mappings_GHZRock[];
extern const uint8_t Mappings_Motobug[];
extern const uint8_t Mappings_Spring[];
extern const uint8_t Mappings_Newtron[];
extern const uint8_t Mappings_GHZEdge[];
extern const uint8_t Mappings_Checkpoint[];
extern const uint8_t Mappings_Bubbles[];       // Object/AirBubbles.c
extern const uint8_t Mappings_SwingGHZ[];      // Object/SwingingPlatform.c
extern const uint8_t Mappings_BigSpikedBall[]; // Object/SwingingPlatform.c (SBZ's big spiked ball variant) -- Mappings_Monitor above is reused for InvisibleBarrier's debug-only placeholder sprite
extern const uint8_t Mappings_GHZPlatforms[];  // Object/BasicPlatform.c
extern const uint8_t Mappings_GiantRing[];     // Object/GiantRing.c
extern const uint8_t Mappings_HiddenBonuses[]; // Object/HiddenBonus.c
extern const uint8_t Mappings_SLZPlatforms[];     // Object/BasicPlatform.c
extern const uint8_t Mappings_SYZPlatforms[];     // Object/BasicPlatform.c
extern const uint8_t Mappings_CollapsingFloors[]; // Object/CollapseFloor.c
extern const uint8_t Mappings_Scenery[];          // Object/Scenery.c (SLZ fireball launcher variant)
extern const uint8_t Mappings_MZBricks[];             // Object/MarbleBrick.c
extern const uint8_t Mappings_SmashableGreenBlock[];  // Object/SmashBlock.c
extern const uint8_t Mappings_MovingBlocks[];         // Object/MovingBlock.c
extern const uint8_t Mappings_Caterkiller[];          // Object/Caterkiller.c
extern const uint8_t Mappings_Fireballs[];            // Object/GrassFire.c
extern const uint8_t Mappings_Basaran[];              // Object/Basaran.c
extern const uint8_t Mappings_LavaGeyser[];           // Object/LavaGeyser.c
extern const uint8_t Mappings_PushableBlocks[];       // Object/PushBlock.c
extern const uint8_t Mappings_Yadrin[];               // Object/Yadrin.c
extern const int8_t ring_pos[16][2]; // Object/Ring.c -- ring row/column formation offsets

#define NULL_ENTRY {ObjId_Null, NULL, 0, 0, 0, NULL, 0, 0, 0}

// Spikes' subtype top nibble (0-5) selects both its orientation and its
// mapping frame directly (Object/Spikes.c: `obj->frame = *set++;` off
// spike_set[subtype>>4], where each entry's frame just equals its own
// index) -- single-layer stack, no flip/palette change needed.
static const DebugFramePiece DebugStack_Spikes[6][1] = {
    {{0, 0, 0}}, {{1, 0, 0}}, {{2, 0, 0}}, {{3, 0, 0}}, {{4, 0, 0}}, {{5, 0, 0}},
};
static const DebugSubtypeVariant DebugVariants_Spikes[] = {
    {0, DebugStack_Spikes[0], 1, 0, 0, 0}, {1, DebugStack_Spikes[1], 1, 0, 0, 0},
    {2, DebugStack_Spikes[2], 1, 0, 0, 0}, {3, DebugStack_Spikes[3], 1, 0, 0, 0},
    {4, DebugStack_Spikes[4], 1, 0, 0, 0}, {5, DebugStack_Spikes[5], 1, 0, 0, 0},
};
#define SPIKES_VARIANTS DebugVariants_Spikes, 6, 4, 0xF

// Monitor's valid subtype range is 0-8 (Object/Monitor.c's own
// obj->anim=subtype, AnimateSprite(Animation_Monitor) idle-flicker between
// the plain box frame (0) and this subtype's own content-icon frame
// (subtype+2, frames 3-10) -- see that file's Ani_Monitor-equivalent
// script). The debug preview composites box+icon together as a 2-layer
// stack (per your direction) rather than trying to pick just one frame or
// replicate the real flicker timing -- subtype 0 (Ring) has no distinct
// icon of its own, so it's just the box alone. 0=Ring/1=Eggman/2=Sonic(1-Up)/
// 3=Shoes/4=Shield/5=Invincibility/6=Rings/7=S(extra life)/8=Goggles.
static const DebugFramePiece DebugStack_Monitor_Static[] =  {{0, 0, 0}            }; // does nothing
static const DebugFramePiece DebugStack_Monitor_Eggman[] =  {{3, 0, 0},  {0, 0, 0}}; // does nothing, often coded to hurt sonic (or kill him if he has no rongs)
static const DebugFramePiece DebugStack_Monitor_1Up[] =     {{4, 0, 0},  {0, 0, 0}}; // extra life
static const DebugFramePiece DebugStack_Monitor_Shoes[] =   {{5, 0, 0},  {0, 0, 0}}; // briefly makes sonic faster
static const DebugFramePiece DebugStack_Monitor_Shield[] =  {{6, 0, 0},  {0, 0, 0}}; // gives a temporary shield to sonic that takes damage for him
static const DebugFramePiece DebugStack_Monitor_Invinc[] =  {{7, 0, 0},  {0, 0, 0}}; // makes sonic impervious to damage for a while
static const DebugFramePiece DebugStack_Monitor_SRing[] =   {{8, 0, 0},  {0, 0, 0}}; // super ring, gives 10 rings
static const DebugFramePiece DebugStack_Monitor_SUnused[] = {{9, 0, 0},  {0, 0, 0}}; // "S" icon, unised, does nothing
static const DebugFramePiece DebugStack_Monitor_Goggles[] = {{10, 0, 0}, {0, 0, 0}}; // googles, unused, does nothing
static const DebugFramePiece DebugStack_Broken_Monitor[] =  {{11, 0, 0}           }; // already collected monitor
static const DebugSubtypeVariant DebugVariants_Monitor[] =  {
    {0, DebugStack_Monitor_Static,  1, 0, 0, 0},
    {1, DebugStack_Monitor_Eggman,  2, 0, 0, 0},
    {2, DebugStack_Monitor_1Up,     2, 0, 0, 0},
    {3, DebugStack_Monitor_Shoes,   2, 0, 0, 0},
    {4, DebugStack_Monitor_Shield,  2, 0, 0, 0},
    {5, DebugStack_Monitor_Invinc,  2, 0, 0, 0},
    {6, DebugStack_Monitor_SRing,   2, 0, 0, 0},
    {7, DebugStack_Monitor_SUnused, 2, 0, 0, 0},
    {8, DebugStack_Monitor_Goggles, 2, 0, 0, 0},
    {9, DebugStack_Broken_Monitor,  1, 0, 0, 0},
};
#define MONITOR_VARIANTS DebugVariants_Monitor, 10, 0, 0xFF

// Spring's subtype (Object/Spring.c's routine 0) packs direction into bits
// 4-5 (0x10 = left/right, 0x20 = down, neither = up) and color into bit 1
// (0x02 = yellow/strong, unset = red/weak) -- masking on 0x32 with shift 0
// isolates exactly those bits regardless of the power bits elsewhere in the
// byte. Left/right isn't just a different frame -- it's different art
// entirely (tile 0x533 vs the default 0x523), hence tile_override. Setting
// both direction bits at once is real hardware undefined behavior (see
// Spring.c's back-to-back ifs), so that combination is deliberately left
// out of this table, same as Monitor's out-of-range subtype 9+.
static const DebugFramePiece DebugStack_Spring_Up[] = {{0, 0, 0}};
static const DebugFramePiece DebugStack_Spring_LR[] = {{3, 0, 0}};
#define SPRING_TILE_LR TILE_MAP(0, 0, 0, 0, 0x533)
static const DebugSubtypeVariant DebugVariants_Spring[] = {
    {0x00, DebugStack_Spring_Up, 1, 0, 0, 0},             // up, red
    {0x02, DebugStack_Spring_Up, 1, 0, 1, 0},             // up, yellow
    {0x20, DebugStack_Spring_Up, 1, 2, 0, 0},             // down, red (y-flipped upright art)
    {0x22, DebugStack_Spring_Up, 1, 2, 1, 0},             // down, yellow
    {0x10, DebugStack_Spring_LR, 1, 0, 0, SPRING_TILE_LR}, // left/right, red
    {0x12, DebugStack_Spring_LR, 1, 0, 1, SPRING_TILE_LR}, // left/right, yellow
};
#define SPRING_VARIANTS DebugVariants_Spring, 6, 0, 0x32

// Ring's subtype (Object/Ring.c's routine 0) packs the extra-ring count into
// bits 0-2 (0-7, with 7 folded down to 6 -- see Obj_Ring) and the formation
// direction/spacing into bits 4-7, indexing the real ring_pos[] table. This
// doesn't change what a single ring looks like, only how many copies get
// placed and where -- so unlike Spikes/Monitor/Spring above, this stack
// can't be written as a fixed literal table; it's built once at startup
// (DebugList_InitRingVariants, called lazily from DebugList_Get) directly
// from Ring.c's own ring_pos, so the preview can never drift from the real
// placement math. Mutable (non-const) for exactly that reason.
#define RING_MAX_EXTRA 7 // ring_pos[16] entries * up to 7 total rings (subtype&7, 7 folds to 6, +1 for the base ring)
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

// Newtron's subtype (Object/Newtron.c's routine_sec 0) is a green/blue flag:
// 0 = green, crawls/flies toward Sonic; nonzero = blue, stands still and
// fires a BuzzMissile. Both start on the exact same rendered frame (traced
// through Animation_Newtron's own script: anim 1 and anim 4 are both
// overwritten to anim 4 on the same tick the subtype check runs, and anim
// 4's first frame command is frame 0, same as anim 1's) -- so unlike
// Spikes/Monitor/Spring, no frame swap is needed here, just the palette bit
// (tile |= TILE_MAP(0,1,0,0,0) in the real code) pal_add already covers.
static const DebugFramePiece DebugStack_Newtron[] = {{0, 0, 0}};
static const DebugSubtypeVariant DebugVariants_Newtron[] = {
    {0, DebugStack_Newtron, 1, 0, 0, 0}, // green
    {1, DebugStack_Newtron, 1, 0, 1, 0}, // blue
};
#define NEWTRON_VARIANTS DebugVariants_Newtron, 2, 0, 0x1

// GHZEdge's subtype (Object/GHZEdge.c's routine 0) IS the raw frame index
// (`obj->frame = obj->scratch.u8[0]`) -- bit 0x10 only toggles solid vs
// decorative collision, it doesn't change which frame shows, so masking it
// off (mask 0xF) makes subtypes 0x10-0x12 preview identically to 0-2.
// Mappings_GHZEdge only has 3 real frames (its own header is 3 words), so
// that's the whole valid range -- higher values would read past the table.
static const DebugFramePiece DebugStack_GHZEdge0[] = {{0, 0, 0}};
static const DebugFramePiece DebugStack_GHZEdge1[] = {{1, 0, 0}};
static const DebugFramePiece DebugStack_GHZEdge2[] = {{2, 0, 0}};
static const DebugSubtypeVariant DebugVariants_GHZEdge[] = {
    {0, DebugStack_GHZEdge0, 1, 0, 0, 0},
    {1, DebugStack_GHZEdge1, 1, 0, 0, 0},
    {2, DebugStack_GHZEdge2, 1, 0, 0, 0},
};
#define GHZEDGE_VARIANTS DebugVariants_GHZEdge, 3, 0, 0xF

// ---------------------------------------------------------------------------
// GHZ
// ---------------------------------------------------------------------------
static const DebugListEntry DebugList_GHZ[] = {
    {ObjId_Ring, Mappings_RingREV01, TILE_MAP(0, 1, 0, 0, 0x7B2), 0, 0, RING_VARIANTS}, // Ring
    {ObjId_Monitor, Mappings_Monitor, TILE_MAP(0, 0, 0, 0, 0x680), 0, 0, MONITOR_VARIANTS},
    {ObjId_Crabmeat, Mappings_Crabmeat, TILE_MAP(0, 0, 0, 0, 0x400), 0, 0, NULL, 0, 0, 0},
    {ObjId_BuzzBomber, Mappings_BuzzBomber, TILE_MAP(0, 0, 0, 0, 0x444), 0, 0, NULL, 0, 0, 0},
    {ObjId_Chopper, Mappings_Chopper, TILE_MAP(0, 0, 0, 0, 0x47B), 0, 0, NULL, 0, 0, 0},
    {ObjId_Spikes, Mappings_Spikes, TILE_MAP(0, 0, 0, 0, 0x51B), 0, 0, SPIKES_VARIANTS},
    {ObjId_BasicPlatform, Mappings_GHZPlatforms, TILE_MAP(0, 2, 0, 0, 0), 0, 0, NULL, 0, 0, 0}, // ArtTile_Level | Tile_Pal3
    {ObjId_GHZRock, Mappings_GHZRock, TILE_MAP(0, 3, 0, 0, 0x3D0), 0, 0, NULL, 0, 0, 0}, // Purple Rock
    {ObjId_Motobug, Mappings_Motobug, TILE_MAP(0, 0, 0, 0, 0x4F0), 0, 0, NULL, 0, 0, 0},
    {ObjId_Spring, Mappings_Spring, TILE_MAP(0, 0, 0, 0, 0x523), 0, 0, SPRING_VARIANTS},
    {ObjId_Newtron, Mappings_Newtron, TILE_MAP(0, 0, 0, 0, 0x49B), 0, 0, NEWTRON_VARIANTS},
    {ObjId_GHZEdge, Mappings_GHZEdge, TILE_MAP(0, 2, 0, 0, 0x34C), 0, 0, GHZEDGE_VARIANTS},
    NULL_ENTRY, // GHZ Giant Ball (Obj19) -- not ported
    {ObjId_Checkpoint, Mappings_Checkpoint, TILE_MAP(0, 0, 0, 0, 0x6C0), 1, 0, NULL, 0, 0, 0}, // Lamppost
    {ObjId_GiantRing, Mappings_GiantRing, TILE_MAP(0, 1, 0, 0, 0x400), 0, 0, NULL, 0, 0, 0}, // ArtTile_Giant_Ring | Tile_Pal2
    {ObjId_HiddenBonus, Mappings_HiddenBonuses, TILE_MAP(1, 0, 0, 0, 0x4B6), 1, 1, NULL, 0, 0, 0}, // ArtTile_Hidden_Points | Tile_Prio
};

// ---------------------------------------------------------------------------
// LZ
// ---------------------------------------------------------------------------
static const DebugListEntry DebugList_LZ[] = {
    {ObjId_Ring, Mappings_RingREV01, TILE_MAP(0, 1, 0, 0, 0x7B2), 0, 0, RING_VARIANTS}, // Ring
    {ObjId_Monitor, Mappings_Monitor, TILE_MAP(0, 0, 0, 0, 0x680), 0, 0, MONITOR_VARIANTS},
    {ObjId_Spring, Mappings_Spring, TILE_MAP(0, 0, 0, 0, 0x523), 0, 0, SPRING_VARIANTS},
    NULL_ENTRY, // Jaws -- not ported
    NULL_ENTRY, // Burrobot -- not ported
    NULL_ENTRY, // Harpoon -- not ported
    NULL_ENTRY, // Harpoon (2nd entry) -- not ported
    {ObjId_PushBlock, Mappings_PushableBlocks, TILE_MAP(0, 2, 0, 0, 0x3DE), 0, 0, NULL, 0, 0, 0}, // ArtTile_LZ_Push_Block | Tile_Pal3
    NULL_ENTRY, // Button -- not ported
    {ObjId_Spikes, Mappings_Spikes, TILE_MAP(0, 0, 0, 0, 0x51B), 0, 0, SPIKES_VARIANTS},
    NULL_ENTRY, // Moving Block (LZ) -- not ported
    NULL_ENTRY, // Labyrinth Block -- not ported
    NULL_ENTRY, // Labyrinth Block -- not ported
    NULL_ENTRY, // Labyrinth Block -- not ported
    NULL_ENTRY, // Gargoyle -- not ported
    NULL_ENTRY, // Labyrinth Block -- not ported
    NULL_ENTRY, // Labyrinth Block -- not ported
    NULL_ENTRY, // Labyrinth Conveyor -- not ported
    NULL_ENTRY, // Orbinaut -- not ported
    {ObjId_Bubble, Mappings_Bubbles, TILE_MAP(1, 0, 0, 0, 0x348), 0x84, 0x13, NULL, 0, 0, 0}, // Bubble maker
    NULL_ENTRY, // Waterfall (visible LZ splash) -- Object/Waterfall.c only ports GHZ's invisible sound-trigger version, not this one
    NULL_ENTRY, // Waterfall (2nd entry)
    NULL_ENTRY, // Pole -- not ported
    NULL_ENTRY, // Flapping Door -- not ported
    {ObjId_Checkpoint, Mappings_Checkpoint, TILE_MAP(0, 0, 0, 0, 0x6C0), 1, 0, NULL, 0, 0, 0}, // Lamppost
};

// ---------------------------------------------------------------------------
// MZ
// ---------------------------------------------------------------------------
static const DebugListEntry DebugList_MZ[] = {
    {ObjId_Ring, Mappings_RingREV01, TILE_MAP(0, 1, 0, 0, 0x7B2), 0, 0, RING_VARIANTS}, // Ring
    {ObjId_Monitor, Mappings_Monitor, TILE_MAP(0, 0, 0, 0, 0x680), 0, 0, MONITOR_VARIANTS},
    {ObjId_BuzzBomber, Mappings_BuzzBomber, TILE_MAP(0, 0, 0, 0, 0x444), 0, 0, NULL, 0, 0, 0},
    {ObjId_Spikes, Mappings_Spikes, TILE_MAP(0, 0, 0, 0, 0x51B), 0, 0, SPIKES_VARIANTS},
    {ObjId_Spring, Mappings_Spring, TILE_MAP(0, 0, 0, 0, 0x523), 0, 0, SPRING_VARIANTS},
    {ObjId_LavaMaker, Mappings_Fireballs, TILE_MAP(0, 0, 0, 0, 0x345), 0, 0, NULL, 0, 0, 0}, // ArtTile_MZ_Fireball -- LavaMaker itself is invisible, real hardware substitutes the fireball's own art for placement visibility too
    {ObjId_MarbleBrick, Mappings_MZBricks, TILE_MAP(0, 2, 0, 0, 0), 0, 0, NULL, 0, 0, 0}, // ArtTile_Level | Tile_Pal3
    {ObjId_GeyserMaker, Mappings_LavaGeyser, TILE_MAP(0, 3, 0, 0, 0x3A8), 0, 0, NULL, 0, 0, 0}, // ArtTile_MZ_Lava | Tile_Pal4
    NULL_ENTRY, // Lava Wall -- not ported
    {ObjId_PushBlock, Mappings_PushableBlocks, TILE_MAP(0, 2, 0, 0, 0x2B8), 0, 0, NULL, 0, 0, 0}, // ArtTile_MZ_Block | Tile_Pal3
    {ObjId_Yadrin, Mappings_Yadrin, TILE_MAP(0, 1, 0, 0, 0x47B), 0, 0, NULL, 0, 0, 0}, // ArtTile_Yadrin | Tile_Pal2
    {ObjId_SmashBlock, Mappings_SmashableGreenBlock, TILE_MAP(0, 2, 0, 0, 0x2B8), 0, 0, NULL, 0, 0, 0}, // ArtTile_MZ_Block | Tile_Pal3
    {ObjId_MovingBlock, Mappings_MovingBlocks, TILE_MAP(0, 2, 0, 0, 0x2B8), 0, 0, NULL, 0, 0, 0}, // ArtTile_MZ_Block | Tile_Pal3
    {ObjId_CollapseFloor, Mappings_CollapsingFloors, TILE_MAP(0, 2, 0, 0, 0x2B8), 0, 0, NULL, 0, 0, 0}, // ArtTile_MZ_Block | Tile_Pal3
    {ObjId_LavaTag, Mappings_Monitor, TILE_MAP(1, 0, 0, 0, 0x680), 0, 0, NULL, 0, 0, 0}, // ArtTile_Monitor|Tile_Prio -- LavaTag's own mappings are blank, real hardware substitutes the Monitor art for placement visibility too
    {ObjId_Basaran, Mappings_Basaran, TILE_MAP(1, 0, 0, 0, 0x4B8), 0, 0, NULL, 0, 0, 0}, // ArtTile_Basaran | Tile_Prio
    {ObjId_Caterkiller, Mappings_Caterkiller, TILE_MAP(0, 1, 0, 0, 0x4FF), 0, 0, NULL, 0, 0, 0}, // ArtTile_MZ_SYZ_Caterkiller | Tile_Pal2
    {ObjId_Checkpoint, Mappings_Checkpoint, TILE_MAP(0, 0, 0, 0, 0x6C0), 1, 0, NULL, 0, 0, 0}, // Lamppost
};

// ---------------------------------------------------------------------------
// SLZ
// ---------------------------------------------------------------------------
static const DebugListEntry DebugList_SLZ[] = {
    {ObjId_Ring, Mappings_RingREV01, TILE_MAP(0, 1, 0, 0, 0x7B2), 0, 0, RING_VARIANTS}, // Ring
    {ObjId_Monitor, Mappings_Monitor, TILE_MAP(0, 0, 0, 0, 0x680), 0, 0, MONITOR_VARIANTS},
    NULL_ENTRY, // Elevator -- not ported
    {ObjId_CollapseFloor, Mappings_CollapsingFloors, TILE_MAP(0, 2, 0, 0, 0x4E0), 0, 2, NULL, 0, 0, 0}, // ArtTile_SLZ_Collapsing_Floor | Tile_Pal3
    {ObjId_BasicPlatform, Mappings_SLZPlatforms, TILE_MAP(0, 2, 0, 0, 0), 0, 0, NULL, 0, 0, 0}, // ArtTile_Level | Tile_Pal3
    NULL_ENTRY, // Circling Platform -- not ported
    NULL_ENTRY, // Staircase -- not ported
    NULL_ENTRY, // Fan -- not ported
    NULL_ENTRY, // Seesaw -- not ported
    {ObjId_Spring, Mappings_Spring, TILE_MAP(0, 0, 0, 0, 0x523), 0, 0, SPRING_VARIANTS},
    {ObjId_LavaMaker, Mappings_Fireballs, TILE_MAP(0, 0, 0, 0, 0x480), 0, 0, NULL, 0, 0, 0}, // ArtTile_SLZ_Fireball
    {ObjId_Scenery, Mappings_Scenery, TILE_MAP(0, 2, 0, 0, 0x4D8), 0, 0, NULL, 0, 0, 0}, // ArtTile_SLZ_Fireball_Launcher | Tile_Pal3
    NULL_ENTRY, // Bomb -- not ported
    NULL_ENTRY, // Orbinaut -- not ported
    {ObjId_Checkpoint, Mappings_Checkpoint, TILE_MAP(0, 0, 0, 0, 0x6C0), 1, 0, NULL, 0, 0, 0}, // Lamppost
};

// ---------------------------------------------------------------------------
// SYZ
// ---------------------------------------------------------------------------
static const DebugListEntry DebugList_SYZ[] = {
    {ObjId_Ring, Mappings_RingREV01, TILE_MAP(0, 1, 0, 0, 0x7B2), 0, 0, RING_VARIANTS}, // Ring
    {ObjId_Monitor, Mappings_Monitor, TILE_MAP(0, 0, 0, 0, 0x680), 0, 0, MONITOR_VARIANTS},
    {ObjId_Spikes, Mappings_Spikes, TILE_MAP(0, 0, 0, 0, 0x51B), 0, 0, SPIKES_VARIANTS},
    {ObjId_Spring, Mappings_Spring, TILE_MAP(0, 0, 0, 0, 0x523), 0, 0, SPRING_VARIANTS},
    NULL_ENTRY, // Roller -- not ported
    NULL_ENTRY, // Spinning Light -- not ported
    NULL_ENTRY, // Bumper -- not ported
    {ObjId_Crabmeat, Mappings_Crabmeat, TILE_MAP(0, 0, 0, 0, 0x400), 0, 0, NULL, 0, 0, 0},
    {ObjId_BuzzBomber, Mappings_BuzzBomber, TILE_MAP(0, 0, 0, 0, 0x444), 0, 0, NULL, 0, 0, 0},
    NULL_ENTRY, // Yadrin -- not ported
    {ObjId_BasicPlatform, Mappings_SYZPlatforms, TILE_MAP(0, 2, 0, 0, 0), 0, 0, NULL, 0, 0, 0}, // ArtTile_Level | Tile_Pal3
    NULL_ENTRY, // Floating Block -- not ported
    NULL_ENTRY, // Button -- not ported
    NULL_ENTRY, // Caterkiller -- not ported
    {ObjId_Checkpoint, Mappings_Checkpoint, TILE_MAP(0, 0, 0, 0, 0x6C0), 1, 0, NULL, 0, 0, 0}, // Lamppost
};

// ---------------------------------------------------------------------------
// SBZ
// ---------------------------------------------------------------------------
static const DebugListEntry DebugList_SBZ[] = {
    {ObjId_Ring, Mappings_RingREV01, TILE_MAP(0, 1, 0, 0, 0x7B2), 0, 0, RING_VARIANTS}, // Ring
    {ObjId_Monitor, Mappings_Monitor, TILE_MAP(0, 0, 0, 0, 0x680), 0, 0, MONITOR_VARIANTS},
    NULL_ENTRY, // Bomb -- not ported
    NULL_ENTRY, // Orbinaut -- not ported
    NULL_ENTRY, // Caterkiller -- not ported
    {ObjId_SwingingPlatform, Mappings_BigSpikedBall, TILE_MAP(0, 0, 0, 0, 0x300), 7, 2, NULL, 0, 0, 0}, // SBZ's big spiked ball swing
    NULL_ENTRY, // Running Disc -- not ported
    NULL_ENTRY, // Moving Block -- not ported
    NULL_ENTRY, // Button -- not ported
    NULL_ENTRY, // Spin Platform -- not ported
    NULL_ENTRY, // Spin Platform -- not ported
    NULL_ENTRY, // Saws -- not ported
    NULL_ENTRY, // Collapsing Floor -- not ported
    NULL_ENTRY, // Moving Block -- not ported
    NULL_ENTRY, // Scrap Stomp -- not ported
    NULL_ENTRY, // Auto Door -- not ported
    NULL_ENTRY, // Scrap Stomp -- not ported
    NULL_ENTRY, // Saws -- not ported
    NULL_ENTRY, // Scrap Stomp -- not ported
    NULL_ENTRY, // Saws -- not ported
    NULL_ENTRY, // Scrap Stomp -- not ported
    NULL_ENTRY, // Vanishing Platform -- not ported
    NULL_ENTRY, // Flamethrower -- not ported
    NULL_ENTRY, // Flamethrower -- not ported
    NULL_ENTRY, // Electro -- not ported
    NULL_ENTRY, // Girder -- not ported
    {ObjId_InvisibleBarrier, Mappings_Monitor, TILE_MAP(1, 0, 0, 0, 0x680), 0x11, 0, NULL, 0, 0, 0}, // Invisible Barrier
    NULL_ENTRY, // Ball Hog -- not ported
    {ObjId_Checkpoint, Mappings_Checkpoint, TILE_MAP(0, 0, 0, 0, 0x6C0), 1, 0, NULL, 0, 0, 0}, // Lamppost
};

// ---------------------------------------------------------------------------
// Ending sequence / Special Stage (shared list) -- REV01 cleared this list
// down to just two Ring entries (the 2nd is a blank frame, matching the
// real disassembly's own "second one is blank" comment); this project has
// no SCP_REV00 build path for this list, so only the REV01 shape is here.
// ---------------------------------------------------------------------------
static const DebugListEntry DebugList_EndingSS[] = {
    {ObjId_Ring, Mappings_RingREV01, TILE_MAP(0, 1, 0, 0, 0x7B2), 0, 0, NULL, 0, 0, 0},
    {ObjId_Ring, Mappings_RingREV01, TILE_MAP(0, 1, 0, 0, 0x7B2), 0, 8, NULL, 0, 0, 0},
};

const DebugListEntry *DebugList_Get(int *count_out) {
    const DebugListEntry *list;
    int count;

    DebugList_InitRingVariants();

    if (gamemode == GameMode_Special) {
        list = DebugList_EndingSS;
        count = sizeof(DebugList_EndingSS) / sizeof(DebugList_EndingSS[0]);
    } else {
        switch (LEVEL_ZONE(level_id)) {
        case ZoneId_GHZ: list = DebugList_GHZ; count = sizeof(DebugList_GHZ) / sizeof(DebugList_GHZ[0]); break;
        case ZoneId_LZ:  list = DebugList_LZ;  count = sizeof(DebugList_LZ)  / sizeof(DebugList_LZ[0]);  break;
        case ZoneId_MZ:  list = DebugList_MZ;  count = sizeof(DebugList_MZ)  / sizeof(DebugList_MZ[0]);  break;
        case ZoneId_SLZ: list = DebugList_SLZ; count = sizeof(DebugList_SLZ) / sizeof(DebugList_SLZ[0]); break;
        case ZoneId_SYZ: list = DebugList_SYZ; count = sizeof(DebugList_SYZ) / sizeof(DebugList_SYZ[0]); break;
        case ZoneId_SBZ: list = DebugList_SBZ; count = sizeof(DebugList_SBZ) / sizeof(DebugList_SBZ[0]); break;
        default:
            list = DebugList_EndingSS;
            count = sizeof(DebugList_EndingSS) / sizeof(DebugList_EndingSS[0]);
            break;
        }
    }

    *count_out = count;
    return list;
}
