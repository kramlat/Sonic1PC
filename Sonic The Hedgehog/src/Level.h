#pragma once

#include "Oscillator.h"
#include "Object.h"
#include "LevelData.h"
#include "ObjectsManager.h" // the layout, chunk and collision data, and the collision flags: the engine's
#include "PLC.h"
#include "Video.h" // wtr_state, hblank_pal, doupdatesinhblank

//Level macros
#define LEVEL_ID(zone, level) (((zone) << 8) | (level))
#define LEVEL_ZONE(id)        ((id) >> 8)
#define LEVEL_ACT(id)         ((id) & 0x3)
#define LEVEL_INDEX(id)       ((LEVEL_ZONE(id) << 2) | LEVEL_ACT(id))


//Level types
#ifdef ZONE_SLOTS // a game built on this one with its own zone slots (Sonic 2 has 17): every per-zone table is sized by ZoneId_Num
#include "ZoneIds.h"
#else
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
#endif

typedef struct {
	uint8_t frame;
	int8_t time;
} LevelAnim;

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

//The level data tables (Sonic1LevelData.c: the game's own; Level.c's loading reads them)
struct LevelLayout { const uint8_t *layout; }; // one combined (interleaved FG+BG, Kosinski) blob per act
extern const struct LevelLayout level_layouts[ZoneId_Num][4];
const int16_t *LevelSizes(int zone, int act);  // the level's boundaries and camera shifts; they depend on the picture size, so the table is built when it is read
extern const int16_t StartLocArray[ZoneId_Num][4][2];
extern const int16_t BGScrollBlockSizes[ZoneId_Num][4];
extern const uint8_t *level_coli[ZoneId_Num - 1][2];
extern const uint8_t *level_obj[ZoneId_Num][4];
extern const uint8_t *level_ring[ZoneId_Num][4];

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
extern uint8_t big_ring; // f_bigring: set by Obj_RingFlash (GiantRing.c) when Sonic jumps into a giant ring; the end-of-act card then goes to the special stage

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

// The level's water (Sonic 1: Labyrinth's; Sonic 2: Hidden Palace's): each game's own Level.c says whether the level has it, where it starts, what palettes it uses, and what is put on its surface
bool Level_HasWater(void);
int16_t Level_WaterStartHeight(void);
void Level_LoadWaterPalettes(bool sonic); // Sonic's underwater palette (sonic) or the level's
void Level_MakeWaterSurfaces(void);

// The music a level plays when it starts (and after a jingle or drowning), or 0 for none: each game's own Level.c has its zone table (Sonic 1's is zone by zone, with act 3 of Scrap Brain and the Final Zone stored under other zones' slots)
uint8_t Level_Music(uint16_t level);



extern uint16_t opl_routine;


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
// Set when Sonic destroys Eggman's ship as it flees the Final Zone (the escape phase): the ending is meant to show the
// unused exploding Eggmobile in its background (art and mappings: Map_FZDamaged). Cleared when a Final Zone fight starts.
extern uint8_t ending_eggmobile_exploding;
extern uint16_t item_bonus;
extern uint16_t time_bonus;
extern uint16_t ring_bonus;
extern uint8_t endact_bonus;
extern uint8_t sonicend;
extern uint16_t lz_deform;
extern uint8_t f_switch[0x10];
extern bool f_wtunneldisallow; // LZ wind tunnels are disallowed while set -- coordinated with by FloatingBlock.c and FlapDoor.c
// FloatingBlock.c -- LZ1's switch-3 door in front of the first (underwater, lower
// route) wind tunnel is fully open. Port change: that tunnel stays off
// until then, see LZWindTunnels.
extern bool f_lz1tunnel_open;
extern uint8_t obj63_loaded[0x80]; // LZConveyor.c -- per-group "platform group already spawned" flags
extern bool f_slidemode; // LZWaterFeatures.c -- set while Sonic is on a water slide


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
// The rings' manager (Object/Ring.c): started with the level, given the camera's X once a frame, by ObjPosLoad
void Rings_Init(const uint8_t *layout, int16_t camera_x);
void Rings_Update(int16_t camera_x);
extern ObjectsManager objects_manager; // Sonic 1's object loader (ObjPosLoad runs it)
