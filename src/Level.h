#pragma once

#include "Object.h"
#include "PLC.h"

//Level macros
#define LEVEL_ID(zone, level) (((zone) << 8) | (level))
#define LEVEL_ZONE(id)        ((id) >> 8)
#define LEVEL_ACT(id)         ((id) & 0x3)
#define LEVEL_INDEX(id)       ((LEVEL_ZONE(id) << 2) | LEVEL_ACT(id))

//Level bitfield structures
//Dual collision-path block word format (matches s1disasm's ProjectSonic1TwoEight
//branch / Sonic 2 & 3K convention): bits 0-9 = tile index (0-1023), bit10 =
//x-flip, bit11 = y-flip, bit12 = path-1 top-solid, bit13 = path-1 LRB-solid,
//bit14 = path-2 top-solid, bit15 = path-2 LRB-solid. Path selection (which
//pair of solid bits to test, and which of the two heightmap arrays below to
//consult) is runtime state set by the path-swapper object (Obj03), not baked
//into the block data itself.
#define META_SOLID_TOP_1 0x1000
#define META_SOLID_LRB_1 0x2000
#define META_SOLID_TOP_2 0x4000
#define META_SOLID_LRB_2 0x8000
#define META_Y_FLIP       0x0800
#define META_X_FLIP       0x0400
#define META_TILE         0x03FF
//Bare names used by every existing FindFloor/FindWall call site: alias to
//path 1 so those callers keep testing the exact same bit they always have.
//The actual bit tested at runtime is shifted by collision_path*2 inside
//LevelCollision.c, which is what makes path 2 apply without touching every
//call site.
#define META_SOLID_TOP META_SOLID_TOP_1
#define META_SOLID_LRB META_SOLID_LRB_1

//Level types
typedef enum {
	ZoneId_GHZ,
	ZoneId_LZ,
	ZoneId_MZ,
	ZoneId_SLZ,
	ZoneId_SYZ,
	ZoneId_SBZ,
	ZoneId_EndZ,
	ZoneId_SS,
	ZoneId_Num,
} ZoneId;

typedef struct {
	uint8_t frame;
	int8_t time;
} LevelAnim;

typedef struct {
	uint16_t direction;
	uint16_t state[16][2];
} Oscillatory;

// 3 longs per zone, matching s2disasm's LevelArtPointers (12 bytes/zone,
// zone_id*12): each long's upper byte is a small metadata value, the lower
// 3 bytes are (conceptually) a pointer to the actual data. Kept as plain
// separate C fields rather than literally bit-packed -- there's no ROM-space
// benefit to packing a real pointer with metadata on a PC target, only the
// conceptual 3-pair grouping is preserved. (music/pad/pal_dup from the old
// 9-field struct were dead fields, never read anywhere -- dropped.)
typedef struct {
	uint8_t plc1;
	const uint8_t *art;
	uint8_t plc2;
	const uint8_t *map16;
	uint8_t pal;
	const uint8_t *map128;
} LevelHeader;

typedef struct {
	uint8_t pad, min, sec, frame;
} LevelTime;

typedef struct {
	struct {
		uint16_t x;
		uint16_t y;
	} spawn;
	uint16_t rings;
	LevelTime time;
	uint8_t dle;
	uint8_t pad0;
	uint16_t limitbtm;
	struct {
		uint16_t x;
		uint16_t y;
	} foreground;
	struct {
		uint16_t x;
		uint16_t y;
	} background;
	struct {
		uint16_t x;
		uint16_t y;
	} background2;
	struct {
		uint16_t x;
		uint16_t y;
	} background3;
	struct {
	uint16_t pos;
	uint8_t routine;
	uint8_t state;
	} water_level;
	uint8_t lives;
} CheckpointState;

//Level headers
extern const LevelHeader level_header[ZoneId_Num];

//Level globals
extern uint16_t level_id;

extern uint8_t dle_routine;

extern uint16_t limit_left1, limit_right1, limit_top1, limit_btm1;
extern uint16_t limit_left2, limit_right2, limit_top2, limit_btm2;
extern uint16_t limit_left3;
extern uint16_t limit_top_db, limit_btm_db;

extern LevelAnim level_anim[6];

extern uint8_t last_lamp;
extern uint8_t prev_lamp;
extern CheckpointState lamp_state;

extern uint16_t restart;
extern uint16_t pause_state;
extern uint8_t time_over;

extern uint16_t frame_count;

extern uint32_t score;
extern LevelTime level_time;
extern uint16_t rings;
extern uint8_t lives;
extern uint8_t continues;

extern uint32_t score_life;

extern uint16_t air;
extern uint8_t last_special;
extern uint8_t big_ring_collected; // set by Obj_RingFlash (GiantRing.c) -- matches the real f_bigring flag; not yet consumed anywhere (level-exit-to-Special-Stage transition isn't wired up yet)

extern uint8_t life_num;
extern uint8_t life_count;
extern uint8_t ring_count;
extern uint8_t time_count;
extern uint8_t score_count;

extern uint8_t shield;
extern uint8_t invincibility;
extern uint8_t shoes;
extern uint8_t debug_use;
extern uint8_t debug_item;         // currently selected index into the active zone's DebugList
extern uint8_t debug_speed;        // current free-move speed (ramps up while a D-Pad direction is held)
extern uint8_t debug_speed_timer;  // frames left before debug_speed ramps up again
extern uint8_t debug_subtype;      // subtype the selected item will spawn with -- resets to the DebugList entry's own default whenever the item changes, adjustable via right stick / ,. (see JPAD_EXT_SUBTYPE_*)

extern int16_t wtr_pos1, wtr_pos2, wtr_pos3;
extern uint8_t water;
extern uint8_t wtr_routine;
extern uint8_t wtr_state;
extern bool hblank_pal;         //Set every VBlank; tells HBlank() to swap CRAM to the water palette
extern bool doupdatesinhblank;  //Set when VBlank ran out of time; defers standard transfers to HBlank

extern uint8_t *const level_map128;
extern uint8_t level_map16[0x1800];
//Interleaved single-buffer level layout (matches s1disasm's ProjectSonic1TwoEight
//branch / Sonic 2 convention): each row is a fixed 0x100-byte stride, with
//foreground chunk IDs in the first 0x80 bytes and background chunk IDs in the
//second 0x80 bytes of that same row. Use level_layout[row]/level_layout[row]+0x80
//(or the LEVEL_LAYOUT_FG/LEVEL_LAYOUT_BG helpers) rather than indexing a
//separate plane dimension.
#define LEVEL_LAYOUT_ROWS 16
#define LEVEL_LAYOUT_ROW_STRIDE 0x100
#define LEVEL_LAYOUT_COLS 0x80
#define LEVEL_LAYOUT_FG(row) (&level_layout[(row) * LEVEL_LAYOUT_ROW_STRIDE])
#define LEVEL_LAYOUT_BG(row) (&level_layout[(row) * LEVEL_LAYOUT_ROW_STRIDE + LEVEL_LAYOUT_COLS])
extern uint8_t level_layout[LEVEL_LAYOUT_ROWS * LEVEL_LAYOUT_ROW_STRIDE];
extern uint8_t level_schunks[2][2]; //TODO: retired once Obj03/path-swapper fully replaces Sonic_Loops' special-chunk check
//Dual collision heightmap arrays (collision curve ID -> 16 height bytes),
//selected at runtime by the active path (see META_SOLID_TOP_1/2,
//META_SOLID_LRB_1/2). coll_index[0] = path 1 (primary), coll_index[1] = path
//2 (secondary). 0x400 bytes/path is headroom over the largest existing zone's
//real data (SBZ at 608 bytes) -- these are decompressed via KosDec at level
//load time, not indexed directly against ROM like the old single pointer was.
extern uint8_t coll_index[2][0x400];
//Active collision path (0 = path 1/primary, 1 = path 2/secondary). Set by
//the path-swapper object (Obj03) once it's ported; defaults to 0, which
//reproduces Sonic 1's original single-path behaviour exactly.
extern uint8_t collision_path;

extern Object objects[OBJECTS];
extern Object *const player;
extern Object *const level_objects;

extern uint16_t opl_routine;
extern int16_t opl_screen;
extern const uint8_t *opl_ptr0;
extern const uint8_t *opl_ptr4;
extern const uint8_t *opl_ptr8;
extern const uint8_t *opl_ptrC;

extern uint8_t objstate_left;
extern uint8_t objstate_right;
extern uint8_t objstate[0x100];

extern int16_t obj31_ypos;
extern uint8_t boss_status;
extern uint8_t lock_screen;
extern uint16_t gfx_big_ring;
extern uint8_t convey_rev;
extern uint8_t obj63[6];
extern uint8_t tunnel_mode;
extern uint8_t lock_multi;
extern uint8_t tunnel_allow;
extern uint8_t jump_only;
extern uint8_t obj6B;
extern uint8_t lock_ctrl;
extern uint8_t big_ring;
extern uint16_t item_bonus;
extern uint16_t time_bonus;
extern uint16_t ring_bonus;
extern uint8_t endact_bonus;
extern uint8_t sonicend;
extern uint16_t lz_deform;
extern uint8_t f_switch[0x10];
extern bool f_wtunneldisallow; // LZ wind tunnels aren't ported yet -- coordinated with by FloatingBlock.c and FlapDoor.c
extern uint8_t obj63_loaded[0x80]; // LZConveyor.c -- per-group "platform group already spawned" flags
extern bool f_slidemode; // LZWaterFeatures.c -- set while Sonic is on a water slide

extern Oscillatory oscillatory;

extern LevelAnim sprite_anim[4];
extern uint16_t sprite_anim_3buf;

//Game functions
void AddPoints(uint16_t points);

//Level functions
void Obj_Checkpoint_LoadInfo(void);
void LoadLevelMaps(void);
void LoadLevelLayout(void);
void LoadMap16(ZoneId zone);
void LoadMap256(ZoneId zone);
void LevelSizeLoad(void);
void LevelDataLoad(void);
void ColIndexLoad(void);
void DynamicLevelEvents(void);
void SynchroAnimate(void);
void SignpostArtLoad(void);
void ObjPosLoad(void);
