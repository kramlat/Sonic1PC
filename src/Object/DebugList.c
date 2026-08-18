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
extern const uint8_t Mappings_BigSpikedBall[]; // Object/SwingingPlatform.c (SBZ's big spiked ball variant)
extern const uint8_t Mappings_InvisibleBarrier[]; // Object/InvisibleBarrier.c
extern const uint8_t Mappings_GHZBall[];       // Object/SwingingPlatform.c (subtype $1X "wrecking ball" variant, unused in real levels but a real preview stand-in for the never-ported Obj19 "GHZ Giant Ball")
extern const uint8_t Mappings_GHZPlatforms[];  // Object/BasicPlatform.c
extern const uint8_t Mappings_GiantRing[];     // Object/GiantRing.c
extern const uint8_t Mappings_HiddenBonuses[]; // Object/HiddenBonus.c
extern const uint8_t Mappings_SLZPlatforms[];     // Object/BasicPlatform.c
extern const uint8_t Mappings_SYZPlatforms[];     // Object/BasicPlatform.c
extern const uint8_t Mappings_CollapsingFloors[]; // Object/CollapseFloor.c
extern const uint8_t Mappings_VanishingPlatforms[]; // Object/VanishingPlatform.c
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
extern const uint8_t Mappings_Jaws[];                 // Object/Jaws.c
extern const uint8_t Mappings_Burrobot[];             // Object/Burrobot.c
extern const uint8_t Mappings_Harpoon[];              // Object/Harpoon.c
extern const uint8_t Mappings_LZBlocks[];             // Object/LZBlocks.c
extern const uint8_t Mappings_LZConveyor[];           // Object/LZConveyor.c
extern const uint8_t Mappings_LZWaterfalls[];         // Object/LZWaterfall.c
extern const uint8_t Mappings_Button[];               // Object/Button.c
extern const uint8_t Mappings_LZMovingBlocks[];       // Object/MovingBlock.c
extern const uint8_t Mappings_Gargoyle[];             // Object/Gargoyle.c
extern const uint8_t Mappings_Orbinaut[];             // Object/Orbinaut.c
extern const uint8_t Mappings_PoleThatBreaks[];       // Object/Pole.c
extern const uint8_t Mappings_FlappingDoor[];         // Object/FlapDoor.c
extern const uint8_t Mappings_LavaWall[];             // Object/LavaWall.c
extern const uint8_t Mappings_Elevator[];             // Object/Elevator.c
extern const uint8_t Mappings_CirclingPlatform[];     // Object/CirclingPlatform.c
extern const uint8_t Mappings_Staircase[];            // Object/Staircase.c
extern const uint8_t Mappings_Fan[];                  // Object/Fan.c
extern const uint8_t Mappings_Seesaw[];               // Object/Seesaw.c
extern const uint8_t Mappings_Bomb[];                 // Object/Bomb.c
extern const uint8_t Mappings_Roller[];               // Object/Roller.c
extern const uint8_t Mappings_Light[];                // Object/SpinningLight.c
extern const uint8_t Mappings_Bumper[];               // Object/Bumper.c
extern const uint8_t Mappings_FloatingBlock[];        // Object/FloatingBlock.c
extern const uint8_t Mappings_RunningDisc[];          // Object/RunningDisc.c
extern const uint8_t Mappings_SpinningPlatforms[];    // Object/SpinPlatform.c
extern const uint8_t Mappings_Trapdoor[];             // Object/SpinPlatform.c
extern const uint8_t Mappings_SawsPizzaCutters[];     // Object/Saw.c
extern const uint8_t Mappings_StomperDoor[];          // Object/ScrapStomp.c
extern const uint8_t Mappings_SmallDoor[];            // Object/SmallDoor.c
extern const uint8_t Mappings_Flamethrower[];         // Object/Flamethrower.c
extern const uint8_t Mappings_Electrocuter[];         // Object/Electrocuter.c
extern const uint8_t Mappings_GirderBlock[];          // Object/GirderBlock.c
extern const uint8_t Mappings_BallHog[];              // Object/BallHog.c
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
#define SPRING_TILE_LR TILE_MAP(0, 0, 0, 0, ArtTile_Spring_Vertical)
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

// MovingBlock's subtype upper nibble selects both size and frame via its
// own mblock_var[5] table (Object/MovingBlock.c) -- frame equals the
// table index directly, so this mirrors GHZEdge's own "subtype bits ARE
// the frame index" shape. Shared across MZ/LZ/SBZ (each zone's own
// DebugListEntry supplies the correct base tile/mappings already); this
// table only needs to track the frame.
static const DebugFramePiece DebugStack_MovingBlock0[] = {{0, 0, 0}};
static const DebugFramePiece DebugStack_MovingBlock1[] = {{1, 0, 0}};
static const DebugFramePiece DebugStack_MovingBlock2[] = {{2, 0, 0}};
static const DebugFramePiece DebugStack_MovingBlock3[] = {{3, 0, 0}};
static const DebugFramePiece DebugStack_MovingBlock4[] = {{4, 0, 0}};
static const DebugSubtypeVariant DebugVariants_MovingBlock[] = {
    {0, DebugStack_MovingBlock0, 1, 0, 0, 0},
    {1, DebugStack_MovingBlock1, 1, 0, 0, 0},
    {2, DebugStack_MovingBlock2, 1, 0, 0, 0},
    {3, DebugStack_MovingBlock3, 1, 0, 0, 0},
    {4, DebugStack_MovingBlock4, 1, 0, 0, 0},
};
#define MOVINGBLOCK_VARIANTS DebugVariants_MovingBlock, 5, 4, 0x7

// Fan's subtype bit 0 (Object/Fan.c's Fan_Action) selects between two
// animation sets (blowing forward vs backward), whose first frames are 0
// and 2 respectively -- bit 1 (always-on) doesn't affect appearance.
static const DebugFramePiece DebugStack_Fan0[] = {{0, 0, 0}};
static const DebugFramePiece DebugStack_Fan1[] = {{2, 0, 0}};
static const DebugSubtypeVariant DebugVariants_Fan[] = {
    {0, DebugStack_Fan0, 1, 0, 0, 0},
    {1, DebugStack_Fan1, 1, 0, 0, 0},
};
#define FAN_VARIANTS DebugVariants_Fan, 2, 0, 0x1

// FloatingBlock's subtype upper nibble (Object/FloatingBlock.c) IS the raw
// frame index directly (`obj->frame = index`), same shape as GHZEdge/
// MovingBlock above -- 8 valid sizes (index 0-7).
static const DebugFramePiece DebugStack_FloatingBlock[8][1] = {
    {{0, 0, 0}}, {{1, 0, 0}}, {{2, 0, 0}}, {{3, 0, 0}},
    {{4, 0, 0}}, {{5, 0, 0}}, {{6, 0, 0}}, {{7, 0, 0}},
};
static const DebugSubtypeVariant DebugVariants_FloatingBlock[] = {
    {0, DebugStack_FloatingBlock[0], 1, 0, 0, 0}, {1, DebugStack_FloatingBlock[1], 1, 0, 0, 0},
    {2, DebugStack_FloatingBlock[2], 1, 0, 0, 0}, {3, DebugStack_FloatingBlock[3], 1, 0, 0, 0},
    {4, DebugStack_FloatingBlock[4], 1, 0, 0, 0}, {5, DebugStack_FloatingBlock[5], 1, 0, 0, 0},
    {6, DebugStack_FloatingBlock[6], 1, 0, 0, 0}, {7, DebugStack_FloatingBlock[7], 1, 0, 0, 0},
};
#define FLOATINGBLOCK_VARIANTS DebugVariants_FloatingBlock, 8, 4, 0x7

// ScrapStomp's subtype upper nibble (Object/ScrapStomp.c) IS the raw frame
// index directly (`obj->frame = idx`) into its own Sto_Var[5] preset table
// -- same shape as MovingBlock/FloatingBlock/GHZEdge above.
static const DebugFramePiece DebugStack_ScrapStomp[5][1] = {
    {{0, 0, 0}}, {{1, 0, 0}}, {{2, 0, 0}}, {{3, 0, 0}}, {{4, 0, 0}},
};
static const DebugSubtypeVariant DebugVariants_ScrapStomp[] = {
    {0, DebugStack_ScrapStomp[0], 1, 0, 0, 0}, {1, DebugStack_ScrapStomp[1], 1, 0, 0, 0},
    {2, DebugStack_ScrapStomp[2], 1, 0, 0, 0}, {3, DebugStack_ScrapStomp[3], 1, 0, 0, 0},
    {4, DebugStack_ScrapStomp[4], 1, 0, 0, 0},
};
#define SCRAPSTOMP_VARIANTS DebugVariants_ScrapStomp, 5, 4, 0x7

// LabyrinthBlock's subtype upper nibble (Object/LZBlocks.c) IS the raw
// frame index directly (`obj->frame = index`) -- same shape as
// MovingBlock/FloatingBlock/ScrapStomp/GHZEdge above. Mappings_LZBlocks
// only has 4 real frames (its own header is 4 words), matching real
// hardware's own 4 placement examples (subtypes 1, $13, $27, $30) exactly
// -- merged into one live-cycling entry instead of 4 static ones.
static const DebugFramePiece DebugStack_LabyrinthBlock[4][1] = {
    {{0, 0, 0}}, {{1, 0, 0}}, {{2, 0, 0}}, {{3, 0, 0}},
};
static const DebugSubtypeVariant DebugVariants_LabyrinthBlock[] = {
    {0, DebugStack_LabyrinthBlock[0], 1, 0, 0, 0}, {1, DebugStack_LabyrinthBlock[1], 1, 0, 0, 0},
    {2, DebugStack_LabyrinthBlock[2], 1, 0, 0, 0}, {3, DebugStack_LabyrinthBlock[3], 1, 0, 0, 0},
};
#define LABYRINTHBLOCK_VARIANTS DebugVariants_LabyrinthBlock, 4, 4, 0x7

// LZWaterfall's subtype IS the raw frame index directly, low nibble
// (Object/LZWaterfall.c: `obj->frame = subtype & 0xF`). Real hardware's
// own debug list only ever showed 2 of these (subtypes 2 and 9) despite
// Mappings_LZWaterfalls actually having 12 real frames (its own header is
// 12 words) -- exploiting the rest here instead of leaving them
// undiscoverable.
static const DebugFramePiece DebugStack_LZWaterfall[12][1] = {
    {{0, 0, 0}}, {{1, 0, 0}}, {{2, 0, 0}}, {{3, 0, 0}},
    {{4, 0, 0}}, {{5, 0, 0}}, {{6, 0, 0}}, {{7, 0, 0}},
    {{8, 0, 0}}, {{9, 0, 0}}, {{10, 0, 0}}, {{11, 0, 0}},
};
static const DebugSubtypeVariant DebugVariants_LZWaterfall[] = {
    {0, DebugStack_LZWaterfall[0], 1, 0, 0, 0}, {1, DebugStack_LZWaterfall[1], 1, 0, 0, 0},
    {2, DebugStack_LZWaterfall[2], 1, 0, 0, 0}, {3, DebugStack_LZWaterfall[3], 1, 0, 0, 0},
    {4, DebugStack_LZWaterfall[4], 1, 0, 0, 0}, {5, DebugStack_LZWaterfall[5], 1, 0, 0, 0},
    {6, DebugStack_LZWaterfall[6], 1, 0, 0, 0}, {7, DebugStack_LZWaterfall[7], 1, 0, 0, 0},
    {8, DebugStack_LZWaterfall[8], 1, 0, 0, 0}, {9, DebugStack_LZWaterfall[9], 1, 0, 0, 0},
    {10, DebugStack_LZWaterfall[10], 1, 0, 0, 0}, {11, DebugStack_LZWaterfall[11], 1, 0, 0, 0},
};
#define LZWATERFALL_VARIANTS DebugVariants_LZWaterfall, 12, 0, 0xF

// Harpoon's subtype (Object/Harpoon.c) selects obj->anim directly (0 =
// sideways, frames 0-2 via Harp_ColTypes; 2 = upright, frames 3-5) -- only
// those two values are meaningful, so this table just picks each anim's
// first frame as its preview.
static const DebugFramePiece DebugStack_Harpoon0[] = {{0, 0, 0}};
static const DebugFramePiece DebugStack_Harpoon1[] = {{3, 0, 0}};
static const DebugSubtypeVariant DebugVariants_Harpoon[] = {
    {0, DebugStack_Harpoon0, 1, 0, 0, 0},
    {1, DebugStack_Harpoon1, 1, 0, 0, 0},
};
#define HARPOON_VARIANTS DebugVariants_Harpoon, 2, 1, 0x1

// SBZ's two MovingBlock placements (short vs long) are otherwise identical
// to MOVINGBLOCK_VARIANTS above, except the "long" size (index 3) lives at
// an entirely different tile+palette (ArtTile_SBZ_Moving_Block_Long,
// Tile_Pal3) rather than the entry's own base tile -- tile_override
// carries that swap so both SBZ placements can merge into one entry.
static const DebugSubtypeVariant DebugVariants_MovingBlock_SBZ[] = {
    {0, DebugStack_MovingBlock0, 1, 0, 0, 0},
    {1, DebugStack_MovingBlock1, 1, 0, 0, 0},
    {2, DebugStack_MovingBlock2, 1, 0, 0, 0},
    {3, DebugStack_MovingBlock3, 1, 0, 0, TILE_MAP(0, 3, 0, 0, ArtTile_SBZ_Moving_Block_Long)},
    {4, DebugStack_MovingBlock4, 1, 0, 0, 0},
};
#define MOVINGBLOCK_SBZ_VARIANTS DebugVariants_MovingBlock_SBZ, 5, 4, 0x7

// Saw's subtype (Object/Saw.c) doesn't set obj->frame directly, but its own
// Saw_Main branches on `subtype < 3` (pizza cutter, frame stays default 0)
// vs "speeding saw" (frame forced to 2 once triggered) -- real hardware's
// own two debug entries (subtypes 1 and 4) are exactly one example of
// each, so this table just keys off those same two representative values.
static const DebugFramePiece DebugStack_Saw0[] = {{0, 0, 0}};
static const DebugFramePiece DebugStack_Saw1[] = {{2, 0, 0}};
static const DebugSubtypeVariant DebugVariants_Saw[] = {
    {1, DebugStack_Saw0, 1, 0, 0, 0},
    {4, DebugStack_Saw1, 1, 0, 0, 0},
};
#define SAW_VARIANTS DebugVariants_Saw, 2, 0, 0x7

// ---------------------------------------------------------------------------
// GHZ
// ---------------------------------------------------------------------------
static const DebugListEntry DebugList_GHZ[] = {
    {ObjId_Ring, Mappings_RingREV01, TILE_MAP(0, 1, 0, 0, ArtTile_Ring), 0, 0, RING_VARIANTS}, // Ring
    {ObjId_Monitor, Mappings_Monitor, TILE_MAP(0, 0, 0, 0, ArtTile_Monitor), 0, 0, MONITOR_VARIANTS},
    {ObjId_Crabmeat, Mappings_Crabmeat, TILE_MAP(0, 0, 0, 0, ArtTile_Crabmeat), 0, 0, NULL, 0, 0, 0},
    {ObjId_BuzzBomber, Mappings_BuzzBomber, TILE_MAP(0, 0, 0, 0, ArtTile_Buzz_Bomber), 0, 0, NULL, 0, 0, 0},
    {ObjId_Chopper, Mappings_Chopper, TILE_MAP(0, 0, 0, 0, ArtTile_Chopper), 0, 0, NULL, 0, 0, 0},
    {ObjId_Spikes, Mappings_Spikes, TILE_MAP(0, 0, 0, 0, ArtTile_Spikes), 0, 0, SPIKES_VARIANTS},
    {ObjId_BasicPlatform, Mappings_GHZPlatforms, TILE_MAP(0, 2, 0, 0, ArtTile_Level), 0, 0, NULL, 0, 0, 0}, // ArtTile_Level | Tile_Pal3
    {ObjId_GHZRock, Mappings_GHZRock, TILE_MAP(0, 3, 0, 0, ArtTile_GHZ_Purple_Rock), 0, 0, NULL, 0, 0, 0}, // Purple Rock
    {ObjId_Motobug, Mappings_Motobug, TILE_MAP(0, 0, 0, 0, ArtTile_Moto_Bug), 0, 0, NULL, 0, 0, 0},
    {ObjId_Spring, Mappings_Spring, TILE_MAP(0, 0, 0, 0, ArtTile_Spring_Horizontal), 0, 0, SPRING_VARIANTS},
    {ObjId_Newtron, Mappings_Newtron, TILE_MAP(0, 0, 0, 0, ArtTile_Newtron), 0, 0, NEWTRON_VARIANTS},
    {ObjId_GHZEdge, Mappings_GHZEdge, TILE_MAP(0, 2, 0, 0, ArtTile_GHZ_Edge_Wall), 0, 0, GHZEDGE_VARIANTS},
    {ObjId_SwingingPlatform, Mappings_GHZBall, TILE_MAP(0, 2, 0, 0, ArtTile_GHZ_Giant_Ball), 0x10, 1, NULL, 0, 0, 0}, // GHZ Giant Ball (Obj19) -- not its own object here; SwingingPlatform's own subtype $1X "wrecking ball" branch already implements this exact visual, so it stands in rather than staying blank
    {ObjId_Checkpoint, Mappings_Checkpoint, TILE_MAP(0, 0, 0, 0, ArtTile_Lamppost), 1, 0, NULL, 0, 0, 0}, // Lamppost
    {ObjId_GiantRing, Mappings_GiantRing, TILE_MAP(0, 1, 0, 0, ArtTile_Giant_Ring), 0, 0, NULL, 0, 0, 0}, // | Tile_Pal2
    {ObjId_HiddenBonus, Mappings_HiddenBonuses, TILE_MAP(1, 0, 0, 0, ArtTile_Hidden_Points), 1, 1, NULL, 0, 0, 0}, // | Tile_Prio
};

// ---------------------------------------------------------------------------
// LZ
// ---------------------------------------------------------------------------
static const DebugListEntry DebugList_LZ[] = {
    {ObjId_Ring, Mappings_RingREV01, TILE_MAP(0, 1, 0, 0, ArtTile_Ring), 0, 0, RING_VARIANTS}, // Ring
    {ObjId_Monitor, Mappings_Monitor, TILE_MAP(0, 0, 0, 0, ArtTile_Monitor), 0, 0, MONITOR_VARIANTS},
    {ObjId_Spring, Mappings_Spring, TILE_MAP(0, 0, 0, 0, ArtTile_Spring_Horizontal), 0, 0, SPRING_VARIANTS},
    {ObjId_Jaws, Mappings_Jaws, TILE_MAP(0, 1, 0, 0, ArtTile_Jaws), 8, 0, NULL, 0, 0, 0}, // | Tile_Pal2
    {ObjId_Burrobot, Mappings_Burrobot, TILE_MAP(1, 0, 0, 0, ArtTile_Burrobot), 0, 2, NULL, 0, 0, 0}, // | Tile_Prio
    {ObjId_Harpoon, Mappings_Harpoon, TILE_MAP(0, 0, 0, 0, ArtTile_LZ_Harpoon), 0, 0, HARPOON_VARIANTS},
    {ObjId_PushBlock, Mappings_PushableBlocks, TILE_MAP(0, 2, 0, 0, ArtTile_LZ_Push_Block), 0, 0, NULL, 0, 0, 0}, // | Tile_Pal3
    {ObjId_Button, Mappings_Button, TILE_MAP(0, 0, 0, 0, ArtTile_Button_Main), 0, 0, NULL, 0, 0, 0},
    {ObjId_Spikes, Mappings_Spikes, TILE_MAP(0, 0, 0, 0, ArtTile_Spikes), 0, 0, SPIKES_VARIANTS},
    {ObjId_MovingBlock, Mappings_LZMovingBlocks, TILE_MAP(0, 2, 0, 0, ArtTile_LZ_Moving_Block), 4, 0, MOVINGBLOCK_VARIANTS}, // | Tile_Pal3
    {ObjId_LabyrinthBlock, Mappings_LZBlocks, TILE_MAP(0, 2, 0, 0, ArtTile_LZ_Blocks), 1, 0, LABYRINTHBLOCK_VARIANTS}, // | Tile_Pal3
    {ObjId_Gargoyle, Mappings_Gargoyle, TILE_MAP(0, 2, 0, 0, ArtTile_LZ_Gargoyle), 0, 0, NULL, 0, 0, 0}, // | Tile_Pal3 (FixBugs: real hardware's own non-FixBugs build uses a broken VRAM address here)
    {ObjId_LabyrinthConvey, Mappings_LZConveyor, TILE_MAP(0, 0, 0, 0, ArtTile_LZ_Conveyor_Belt), 0x7F, 0, NULL, 0, 0, 0},
    {ObjId_Orbinaut, Mappings_Orbinaut, TILE_MAP(0, 0, 0, 0, ArtTile_LZ_Orbinaut), 0, 0, NULL, 0, 0, 0},
    {ObjId_Bubble, Mappings_Bubbles, TILE_MAP(1, 0, 0, 0, ArtTile_LZ_Bubbles), 0x84, 0x13, NULL, 0, 0, 0}, // Bubble maker
    {ObjId_LZWaterfall, Mappings_LZWaterfalls, TILE_MAP(0, 2, 0, 0, ArtTile_LZ_Splash), 2, 2, LZWATERFALL_VARIANTS}, // | Tile_Pal3 | Tile_Prio -- real ASM's Map_WFall/id_Waterfall here is this project's own LZWaterfall.c (Object 65), not Waterfall.c's GHZ-only invisible trigger (Object 49)
    {ObjId_Pole, Mappings_PoleThatBreaks, TILE_MAP(0, 2, 0, 0, ArtTile_LZ_Pole), 0, 0, NULL, 0, 0, 0}, // | Tile_Pal3
    {ObjId_FlapDoor, Mappings_FlappingDoor, TILE_MAP(0, 2, 0, 0, ArtTile_LZ_Flapping_Door), 2, 0, NULL, 0, 0, 0}, // | Tile_Pal3
    {ObjId_Checkpoint, Mappings_Checkpoint, TILE_MAP(0, 0, 0, 0, ArtTile_Lamppost), 1, 0, NULL, 0, 0, 0}, // Lamppost
};

// ---------------------------------------------------------------------------
// MZ
// ---------------------------------------------------------------------------
static const DebugListEntry DebugList_MZ[] = {
    {ObjId_Ring, Mappings_RingREV01, TILE_MAP(0, 1, 0, 0, ArtTile_Ring), 0, 0, RING_VARIANTS}, // Ring
    {ObjId_Monitor, Mappings_Monitor, TILE_MAP(0, 0, 0, 0, ArtTile_Monitor), 0, 0, MONITOR_VARIANTS},
    {ObjId_BuzzBomber, Mappings_BuzzBomber, TILE_MAP(0, 0, 0, 0, ArtTile_Buzz_Bomber), 0, 0, NULL, 0, 0, 0},
    {ObjId_Spikes, Mappings_Spikes, TILE_MAP(0, 0, 0, 0, ArtTile_Spikes), 0, 0, SPIKES_VARIANTS},
    {ObjId_Spring, Mappings_Spring, TILE_MAP(0, 0, 0, 0, ArtTile_Spring_Horizontal), 0, 0, SPRING_VARIANTS},
    {ObjId_LavaMaker, Mappings_Fireballs, TILE_MAP(0, 0, 0, 0, ArtTile_MZ_Fireball), 0, 0, NULL, 0, 0, 0}, // -- LavaMaker itself is invisible, real hardware substitutes the fireball's own art for placement visibility too
    {ObjId_MarbleBrick, Mappings_MZBricks, TILE_MAP(0, 2, 0, 0, ArtTile_Level), 0, 0, NULL, 0, 0, 0}, // ArtTile_Level | Tile_Pal3
    {ObjId_GeyserMaker, Mappings_LavaGeyser, TILE_MAP(0, 3, 0, 0, ArtTile_MZ_Lava), 0, 0, NULL, 0, 0, 0}, // | Tile_Pal4
    {ObjId_LavaWall, Mappings_LavaWall, TILE_MAP(0, 3, 0, 0, ArtTile_MZ_Lava), 0, 0, NULL, 0, 0, 0}, // | Tile_Pal4
    {ObjId_PushBlock, Mappings_PushableBlocks, TILE_MAP(0, 2, 0, 0, ArtTile_MZ_Block), 0, 0, NULL, 0, 0, 0}, // | Tile_Pal3
    {ObjId_Yadrin, Mappings_Yadrin, TILE_MAP(0, 1, 0, 0, ArtTile_Yadrin), 0, 0, NULL, 0, 0, 0}, // | Tile_Pal2
    {ObjId_SmashBlock, Mappings_SmashableGreenBlock, TILE_MAP(0, 2, 0, 0, ArtTile_MZ_Block), 0, 0, NULL, 0, 0, 0}, // | Tile_Pal3
    {ObjId_MovingBlock, Mappings_MovingBlocks, TILE_MAP(0, 2, 0, 0, ArtTile_MZ_Block), 0, 0, NULL, 0, 0, 0}, // | Tile_Pal3
    {ObjId_CollapseFloor, Mappings_CollapsingFloors, TILE_MAP(0, 2, 0, 0, ArtTile_MZ_Block), 0, 0, NULL, 0, 0, 0}, // | Tile_Pal3
    {ObjId_LavaTag, Mappings_Monitor, TILE_MAP(1, 0, 0, 0, ArtTile_Monitor), 0, 0, NULL, 0, 0, 0}, // |Tile_Prio -- LavaTag's own mappings are blank, real hardware substitutes the Monitor art for placement visibility too
    {ObjId_Basaran, Mappings_Basaran, TILE_MAP(1, 0, 0, 0, ArtTile_Basaran), 0, 0, NULL, 0, 0, 0}, // | Tile_Prio
    {ObjId_Caterkiller, Mappings_Caterkiller, TILE_MAP(0, 1, 0, 0, ArtTile_MZ_SYZ_Caterkiller), 0, 0, NULL, 0, 0, 0}, // | Tile_Pal2
    {ObjId_Checkpoint, Mappings_Checkpoint, TILE_MAP(0, 0, 0, 0, ArtTile_Lamppost), 1, 0, NULL, 0, 0, 0}, // Lamppost
};

// ---------------------------------------------------------------------------
// SLZ
// ---------------------------------------------------------------------------
static const DebugListEntry DebugList_SLZ[] = {
    {ObjId_Ring, Mappings_RingREV01, TILE_MAP(0, 1, 0, 0, ArtTile_Ring), 0, 0, RING_VARIANTS}, // Ring
    {ObjId_Monitor, Mappings_Monitor, TILE_MAP(0, 0, 0, 0, ArtTile_Monitor), 0, 0, MONITOR_VARIANTS},
    {ObjId_Elevator, Mappings_Elevator, TILE_MAP(0, 2, 0, 0, ArtTile_Level), 0, 0, NULL, 0, 0, 0}, // ArtTile_Level | Tile_Pal3
    {ObjId_CollapseFloor, Mappings_CollapsingFloors, TILE_MAP(0, 2, 0, 0, ArtTile_SLZ_Collapsing_Floor), 0, 2, NULL, 0, 0, 0}, // | Tile_Pal3
    {ObjId_BasicPlatform, Mappings_SLZPlatforms, TILE_MAP(0, 2, 0, 0, ArtTile_Level), 0, 0, NULL, 0, 0, 0}, // ArtTile_Level | Tile_Pal3
    {ObjId_CirclingPlatform, Mappings_CirclingPlatform, TILE_MAP(0, 2, 0, 0, ArtTile_Level), 0, 0, NULL, 0, 0, 0}, // ArtTile_Level | Tile_Pal3
    {ObjId_Staircase, Mappings_Staircase, TILE_MAP(0, 2, 0, 0, ArtTile_Level), 0, 0, NULL, 0, 0, 0}, // ArtTile_Level | Tile_Pal3
    {ObjId_Fan, Mappings_Fan, TILE_MAP(0, 2, 0, 0, ArtTile_SLZ_Fan), 0, 0, FAN_VARIANTS}, // | Tile_Pal3
    {ObjId_Seesaw, Mappings_Seesaw, TILE_MAP(0, 0, 0, 0, ArtTile_SLZ_Seesaw), 0, 0, NULL, 0, 0, 0},
    {ObjId_Spring, Mappings_Spring, TILE_MAP(0, 0, 0, 0, ArtTile_Spring_Horizontal), 0, 0, SPRING_VARIANTS},
    {ObjId_LavaMaker, Mappings_Fireballs, TILE_MAP(0, 0, 0, 0, ArtTile_SLZ_Fireball), 0, 0, NULL, 0, 0, 0},
    {ObjId_Scenery, Mappings_Scenery, TILE_MAP(0, 2, 0, 0, ArtTile_SLZ_Fireball_Launcher), 0, 0, NULL, 0, 0, 0}, // | Tile_Pal3
    {ObjId_Bomb, Mappings_Bomb, TILE_MAP(0, 0, 0, 0, ArtTile_Bomb), 0, 0, NULL, 0, 0, 0},
    {ObjId_Orbinaut, Mappings_Orbinaut, TILE_MAP(0, 1, 0, 0, ArtTile_SLZ_Orbinaut), 0, 0, NULL, 0, 0, 0}, // | Tile_Pal2
    {ObjId_Checkpoint, Mappings_Checkpoint, TILE_MAP(0, 0, 0, 0, ArtTile_Lamppost), 1, 0, NULL, 0, 0, 0}, // Lamppost
};

// ---------------------------------------------------------------------------
// SYZ
// ---------------------------------------------------------------------------
static const DebugListEntry DebugList_SYZ[] = {
    {ObjId_Ring, Mappings_RingREV01, TILE_MAP(0, 1, 0, 0, ArtTile_Ring), 0, 0, RING_VARIANTS}, // Ring
    {ObjId_Monitor, Mappings_Monitor, TILE_MAP(0, 0, 0, 0, ArtTile_Monitor), 0, 0, MONITOR_VARIANTS},
    {ObjId_Spikes, Mappings_Spikes, TILE_MAP(0, 0, 0, 0, ArtTile_Spikes), 0, 0, SPIKES_VARIANTS},
    {ObjId_Spring, Mappings_Spring, TILE_MAP(0, 0, 0, 0, ArtTile_Spring_Horizontal), 0, 0, SPRING_VARIANTS},
    {ObjId_Roller, Mappings_Roller, TILE_MAP(0, 0, 0, 0, ArtTile_Roller), 0, 0, NULL, 0, 0, 0},
    {ObjId_SpinningLight, Mappings_Light, TILE_MAP(0, 0, 0, 0, ArtTile_Level), 0, 0, NULL, 0, 0, 0},
    {ObjId_Bumper, Mappings_Bumper, TILE_MAP(0, 0, 0, 0, ArtTile_SYZ_Bumper), 0, 0, NULL, 0, 0, 0},
    {ObjId_Crabmeat, Mappings_Crabmeat, TILE_MAP(0, 0, 0, 0, ArtTile_Crabmeat), 0, 0, NULL, 0, 0, 0},
    {ObjId_BuzzBomber, Mappings_BuzzBomber, TILE_MAP(0, 0, 0, 0, ArtTile_Buzz_Bomber), 0, 0, NULL, 0, 0, 0},
    {ObjId_Yadrin, Mappings_Yadrin, TILE_MAP(0, 1, 0, 0, ArtTile_Yadrin), 0, 0, NULL, 0, 0, 0}, // | Tile_Pal2
    {ObjId_BasicPlatform, Mappings_SYZPlatforms, TILE_MAP(0, 2, 0, 0, ArtTile_Level), 0, 0, NULL, 0, 0, 0}, // ArtTile_Level | Tile_Pal3
    {ObjId_FloatingBlock, Mappings_FloatingBlock, TILE_MAP(0, 2, 0, 0, ArtTile_Level), 0, 0, FLOATINGBLOCK_VARIANTS}, // ArtTile_Level | Tile_Pal3
    {ObjId_Button, Mappings_Button, TILE_MAP(0, 0, 0, 0, ArtTile_Button_Main), 0, 0, NULL, 0, 0, 0},
    {ObjId_Caterkiller, Mappings_Caterkiller, TILE_MAP(0, 1, 0, 0, ArtTile_MZ_SYZ_Caterkiller), 0, 0, NULL, 0, 0, 0}, // | Tile_Pal2
    {ObjId_Checkpoint, Mappings_Checkpoint, TILE_MAP(0, 0, 0, 0, ArtTile_Lamppost), 1, 0, NULL, 0, 0, 0}, // Lamppost
};

// ---------------------------------------------------------------------------
// SBZ
// ---------------------------------------------------------------------------
static const DebugListEntry DebugList_SBZ[] = {
    {ObjId_Ring, Mappings_RingREV01, TILE_MAP(0, 1, 0, 0, ArtTile_Ring), 0, 0, RING_VARIANTS}, // Ring
    {ObjId_Monitor, Mappings_Monitor, TILE_MAP(0, 0, 0, 0, ArtTile_Monitor), 0, 0, MONITOR_VARIANTS},
    {ObjId_Bomb, Mappings_Bomb, TILE_MAP(0, 0, 0, 0, ArtTile_Bomb), 0, 0, NULL, 0, 0, 0},
    {ObjId_Orbinaut, Mappings_Orbinaut, TILE_MAP(0, 0, 0, 0, ArtTile_SBZ_Orbinaut), 0, 0, NULL, 0, 0, 0},
    {ObjId_Caterkiller, Mappings_Caterkiller, TILE_MAP(0, 1, 0, 0, ArtTile_SBZ_Caterkiller), 0, 0, NULL, 0, 0, 0}, // | Tile_Pal2
    {ObjId_SwingingPlatform, Mappings_BigSpikedBall, TILE_MAP(0, 2, 0, 0, ArtTile_SBZ_Swing), 7, 2, NULL, 0, 0, 0}, // SBZ's big spiked ball swing
    {ObjId_RunningDisc, Mappings_RunningDisc, TILE_MAP(1, 2, 0, 0, ArtTile_SBZ_Disc), 0xE0, 0, NULL, 0, 0, 0}, // | Tile_Pal3 | Tile_Prio
    {ObjId_MovingBlock, Mappings_MovingBlocks, TILE_MAP(0, 1, 0, 0, ArtTile_SBZ_Moving_Block_Short), 0x28, 2, MOVINGBLOCK_SBZ_VARIANTS}, // | Tile_Pal2
    {ObjId_Button, Mappings_Button, TILE_MAP(0, 0, 0, 0, ArtTile_Button_Main), 0, 0, NULL, 0, 0, 0},
    {ObjId_SpinPlatform, Mappings_Trapdoor, TILE_MAP(0, 2, 0, 0, ArtTile_SBZ_Trap_Door), 3, 0, NULL, 0, 0, 0}, // | Tile_Pal3
    {ObjId_SpinPlatform, Mappings_SpinningPlatforms, TILE_MAP(0, 0, 0, 0, ArtTile_SBZ_Spinning_Platform), 0x83, 0, NULL, 0, 0, 0},
    {ObjId_CollapseFloor, Mappings_CollapsingFloors, TILE_MAP(0, 2, 0, 0, ArtTile_SBZ_Collapsing_Floor), 0, 0, NULL, 0, 0, 0}, // | Tile_Pal3
    {ObjId_ScrapStomp, Mappings_StomperDoor, TILE_MAP(0, 1, 0, 0, ArtTile_SBZ_Moving_Block_Short), 0, 0, SCRAPSTOMP_VARIANTS}, // | Tile_Pal2
    {ObjId_SmallDoor, Mappings_SmallDoor, TILE_MAP(0, 2, 0, 0, ArtTile_SBZ_Door), 0, 0, NULL, 0, 0, 0}, // | Tile_Pal3
    {ObjId_Saw, Mappings_SawsPizzaCutters, TILE_MAP(0, 2, 0, 0, ArtTile_SBZ_Saw), 1, 0, SAW_VARIANTS}, // | Tile_Pal3
    {ObjId_VanishPlatform, Mappings_VanishingPlatforms, TILE_MAP(0, 2, 0, 0, ArtTile_SBZ_Vanishing_Block), 0, 0, NULL, 0, 0, 0}, // | Tile_Pal3
    {ObjId_Flamethrower, Mappings_Flamethrower, TILE_MAP(1, 0, 0, 0, ArtTile_SBZ_Flamethrower), 0x64, 0, NULL, 0, 0, 0}, // | Tile_Prio
    {ObjId_Flamethrower, Mappings_Flamethrower, TILE_MAP(1, 0, 0, 0, ArtTile_SBZ_Flamethrower), 0x64, 0xB, NULL, 0, 0, 0}, // | Tile_Prio
    {ObjId_Electrocuter, Mappings_Electrocuter, TILE_MAP(0, 0, 0, 0, ArtTile_SBZ_Electric_Orb), 4, 0, NULL, 0, 0, 0},
    {ObjId_GirderBlock, Mappings_GirderBlock, TILE_MAP(0, 2, 0, 0, ArtTile_SBZ_Girder), 0, 0, NULL, 0, 0, 0}, // | Tile_Pal3
    {ObjId_InvisibleBarrier, Mappings_InvisibleBarrier, TILE_MAP(1, 0, 0, 0, ArtTile_Monitor), 0x11, 0, NULL, 0, 0, 0}, // Invisible Barrier -- real hardware's own dedicated Map_Invis (4 tight Eggman-icon tiles, no monitor box)
    {ObjId_BallHog, Mappings_BallHog, TILE_MAP(0, 1, 0, 0, ArtTile_Ball_Hog), 4, 0, NULL, 0, 0, 0}, // | Tile_Pal2
    {ObjId_Checkpoint, Mappings_Checkpoint, TILE_MAP(0, 0, 0, 0, ArtTile_Lamppost), 1, 0, NULL, 0, 0, 0}, // Lamppost
};

// ---------------------------------------------------------------------------
// Ending sequence / Special Stage (shared list) -- REV01 cleared this list
// down to just two Ring entries (the 2nd is a blank frame, matching the
// real disassembly's own "second one is blank" comment); this project has
// no SCP_REV00 build path for this list, so only the REV01 shape is here.
// ---------------------------------------------------------------------------
static const DebugListEntry DebugList_EndingSS[] = {
    {ObjId_Ring, Mappings_RingREV01, TILE_MAP(0, 1, 0, 0, ArtTile_Ring), 0, 0, NULL, 0, 0, 0},
    {ObjId_Ring, Mappings_RingREV01, TILE_MAP(0, 1, 0, 0, ArtTile_Ring), 0, 8, NULL, 0, 0, 0},
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
