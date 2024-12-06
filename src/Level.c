#include "Level.h"

#include "Constants.h"
#include "Game.h"
#include "Kosinski.h"
#include "LevelDraw.h"
#include "LevelScroll.h"
#include "PLC.h"
#include "Palette.h"

#include "Backend/VDP.h"

#include <string.h>

// Level layouts
#include "Resource/Layout/GHZ1.h"
#include "Resource/Layout/GHZ2.h"
#include "Resource/Layout/GHZ3.h"
#include "Resource/Layout/GHZBG.h"

#include "Resource/Layout/LZ1.h"
#include "Resource/Layout/LZ2.h"
#include "Resource/Layout/LZ3.h"
#include "Resource/Layout/LZBG.h"

#include "Resource/Layout/MZ1.h"
#include "Resource/Layout/MZ1BG.h"
#include "Resource/Layout/MZ2.h"
#include "Resource/Layout/MZ2BG.h"
#include "Resource/Layout/MZ3.h"
#include "Resource/Layout/MZ3BG.h"

#include "Resource/Layout/SLZ1.h"
#include "Resource/Layout/SLZ2.h"
#include "Resource/Layout/SLZ3.h"
#include "Resource/Layout/SLZBG.h"

#include "Resource/Layout/SYZ1.h"
#include "Resource/Layout/SYZ2.h"
#include "Resource/Layout/SYZ3.h"
#ifdef SCP_REV00
#include "Resource/Layout/SYZBGREV00.h"
#else
#include "Resource/Layout/SYZBGREV01.h"
#endif
#include "Resource/Layout/SBZ1.h"
#include "Resource/Layout/SBZ1BG.h"
#include "Resource/Layout/SBZ2.h"
#include "Resource/Layout/SBZ2BG.h"
#include "Resource/Layout/SBZ3.h"

#include "Resource/Layout/Ending.h"

// 256x256 mappings
#include "Resource/Map256/GHZ.h"
#include "Resource/Map256/LZ.h"
#ifdef SCP_REV00
#include "Resource/Map256/MZREV00.h"
#else
#include "Resource/Map256/MZREV01.h"
#endif
#include "Resource/Map256/SLZ.h"
#include "Resource/Map256/SYZ.h"
#ifdef SCP_REV00
#include "Resource/Map256/SBZREV00.h"
#else
#include "Resource/Map256/SBZREV01.h"
#endif

// 16x16 mappings
#include "Resource/Map16/GHZ.h"
#include "Resource/Map16/LZ.h"
#include "Resource/Map16/MZ.h"
#include "Resource/Map16/SBZ.h"
#include "Resource/Map16/SLZ.h"
#include "Resource/Map16/SYZ.h"

// Collision indices
#include "Resource/CollisionIndex/GHZ.h"
#include "Resource/CollisionIndex/LZ.h"
#include "Resource/CollisionIndex/MZ.h"
#include "Resource/CollisionIndex/SBZ.h"
#include "Resource/CollisionIndex/SLZ.h"
#include "Resource/CollisionIndex/SYZ.h"

// Object positions
#include "Resource/ObjectLayout/GHZ1.h"
#include "Resource/ObjectLayout/GHZ2.h"
#ifdef SCP_REV00
#include "Resource/ObjectLayout/GHZ3REV00.h"
#else
#include "Resource/ObjectLayout/GHZ3REV01.h"
#endif

#ifdef SCP_REV00
#include "Resource/ObjectLayout/LZ1REV00.h"
#else
#include "Resource/ObjectLayout/LZ1REV01.h"
#endif
#include "Resource/ObjectLayout/LZ1PF1.h"
#include "Resource/ObjectLayout/LZ1PF2.h"
#include "Resource/ObjectLayout/LZ2.h"
#include "Resource/ObjectLayout/LZ2PF1.h"
#include "Resource/ObjectLayout/LZ2PF2.h"
#ifdef SCP_REV00
#include "Resource/ObjectLayout/LZ3REV00.h"
#else
#include "Resource/ObjectLayout/LZ3REV01.h"
#endif
#include "Resource/ObjectLayout/LZ3PF1.h"
#include "Resource/ObjectLayout/LZ3PF2.h"

#ifdef SCP_REV00
#include "Resource/ObjectLayout/MZ1REV00.h"
#else
#include "Resource/ObjectLayout/MZ1REV01.h"
#endif
#include "Resource/ObjectLayout/MZ2.h"
#include "Resource/ObjectLayout/MZ3.h"

#include "Resource/ObjectLayout/SLZ1.h"
#include "Resource/ObjectLayout/SLZ2.h"
#include "Resource/ObjectLayout/SLZ3.h"

#include "Resource/ObjectLayout/SYZ1.h"
#include "Resource/ObjectLayout/SYZ2.h"
#ifdef SCP_REV00
#include "Resource/ObjectLayout/SYZ3REV00.h"
#else
#include "Resource/ObjectLayout/SYZ3REV01.h"
#endif

#ifdef SCP_REV00
#include "Resource/ObjectLayout/SBZ1REV00.h"
#else
#include "Resource/ObjectLayout/SBZ1REV01.h"
#endif
#include "Resource/ObjectLayout/Ending.h"
#include "Resource/ObjectLayout/FZ.h"
#include "Resource/ObjectLayout/SBZ1PF1.h"
#include "Resource/ObjectLayout/SBZ1PF2.h"
#include "Resource/ObjectLayout/SBZ1PF3.h"
#include "Resource/ObjectLayout/SBZ1PF4.h"
#include "Resource/ObjectLayout/SBZ1PF5.h"
#include "Resource/ObjectLayout/SBZ1PF6.h"
#include "Resource/ObjectLayout/SBZ2.h"
#include "Resource/ObjectLayout/SBZ3.h"

void Obj_Checkpoint_LoadInfo() {
    last_lamp = prev_lamp;
    player->pos.l.x.v = lamp_state.spawn.x;
    player->pos.l.y.v = lamp_state.spawn.y;
    // rings = lamp_state.rings; // restore rings if you want
    rings = 0; // normal behavior
    life_count = lamp_state.lives;
    time.pad = lamp_state.time.pad;
    time.min = lamp_state.time.min;
    time.sec = lamp_state.time.sec;
    time.frame = lamp_state.time.frame;
    time.frame = 59;
    time.sec--;
    dle_routine = lamp_state.dle;
    wtr_routine = lamp_state.water_level.routine;
    limit_btm2 = lamp_state.limitbtm;
    limit_btm1 = lamp_state.limitbtm;
    scrpos_x.v = lamp_state.foreground.x;
    scrpos_y.v = lamp_state.foreground.y;
    bg_scrpos_x.v = lamp_state.background.x;
    bg_scrpos_y.v = lamp_state.background.y;
    bg2_scrpos_x.v = lamp_state.background2.x;
    bg2_scrpos_y.v = lamp_state.background2.y;
    bg3_scrpos_x.v = lamp_state.background3.x;
    bg3_scrpos_y.v = lamp_state.background3.y;

    if (LEVEL_ZONE(level_id) == ZoneId_LZ) {  // Is this Labyrinth Zone?
        wtr_pos2 = lamp_state.water_level.pos;
        wtr_routine = lamp_state.water_level.routine;
        wtr_state = lamp_state.water_level.state;
    }

    if ((int8_t)last_lamp >= 0) {
        return;
    }

    uint16_t ds = lamp_state.spawn.x - 0xA0;
    limit_left2 = ds;
}

// Level definitions
static const struct
{
    const uint8_t* layout_fg;
    const uint8_t* layout_bg;
    const uint8_t* layout_3;
} level_layouts[ZoneId_Num][4] = {
    {
        // ZoneId_GHZ
        { Layout_GHZ1, Layout_GHZBG, NULL },
        { Layout_GHZ2, Layout_GHZBG, NULL },
        { Layout_GHZ3, Layout_GHZBG, NULL },
        { NULL, NULL, NULL },
    },
    {
        // ZoneId_LZ
        { Layout_LZ1, Layout_LZBG, NULL },
        { Layout_LZ2, Layout_LZBG, NULL },
        { Layout_LZ3, Layout_LZBG, NULL },
        { Layout_SBZ3, Layout_LZBG, NULL },
    },
    {
        // ZoneId_MZ
        { Layout_MZ1, Layout_MZ1BG, Layout_MZ1 },
        { Layout_MZ2, Layout_MZ2BG, NULL },
        { Layout_MZ3, Layout_MZ3BG, NULL },
        { NULL, NULL, NULL },
    },
    {
        // ZoneId_SLZ
        { Layout_SLZ1, Layout_SLZBG, NULL },
        { Layout_SLZ2, Layout_SLZBG, NULL },
        { Layout_SLZ3, Layout_SLZBG, NULL },
        { NULL, NULL, NULL },
    },
    {
// ZoneId_SYZ
#ifdef SCP_REV00
        { Layout_SYZ1, Layout_SYZBGREV00, NULL },
        { Layout_SYZ2, Layout_SYZBGREV00, NULL },
        { Layout_SYZ3, Layout_SYZBGREV00, NULL },
#else
        { Layout_SYZ1, Layout_SYZBGREV01, NULL },
        { Layout_SYZ2, Layout_SYZBGREV01, NULL },
        { Layout_SYZ3, Layout_SYZBGREV01, NULL },
#endif
        { NULL, NULL, NULL },
    },
    {
        // ZoneId_SBZ
        { Layout_SBZ1, Layout_SBZ1BG, Layout_SBZ1BG },
        { Layout_SBZ2, Layout_SBZ2BG, Layout_SBZ2BG },
        { Layout_SBZ2, Layout_SBZ2BG, NULL },
        { NULL, NULL, NULL },
    },
    {
        // ZoneId_EndZ
        { Layout_Ending, Layout_GHZBG, NULL },
        { Layout_Ending, Layout_GHZBG, NULL },
        { NULL, NULL, NULL },
        { NULL, NULL, NULL },
    },
};

static const int16_t ldef_size[ZoneId_Num][4][6] = {
    {
        // ZoneId_GHZ
        { 0x0004, 0x0000, 0x24BF + SCREEN_WIDEADD2, 0x0000, 0x0300 + SCREEN_TALLADD, 96 + SCREEN_TALLADD2 },
        { 0x0004, 0x0000, 0x1EBF + SCREEN_WIDEADD2, 0x0000, 0x0300 + SCREEN_TALLADD, 96 + SCREEN_TALLADD2 },
        { 0x0004, 0x0000, 0x2960 + SCREEN_WIDEADD2, 0x0000, 0x0300 + SCREEN_TALLADD, 96 + SCREEN_TALLADD2 },
        { 0x0004, 0x0000, 0x2ABF + SCREEN_WIDEADD2, 0x0000, 0x0300 + SCREEN_TALLADD, 96 + SCREEN_TALLADD2 },
    },
    {
        // ZoneId_LZ
        { 0x0004, 0x0000, 0x19BF + SCREEN_WIDEADD2, 0x0000, 0x0530 + SCREEN_TALLADD, 96 + SCREEN_TALLADD2 },
        { 0x0004, 0x0000, 0x10AF + SCREEN_WIDEADD2, 0x0000, 0x0720 + SCREEN_TALLADD, 96 + SCREEN_TALLADD2 },
        { 0x0004, 0x0000, 0x202F + SCREEN_WIDEADD2, -0x0100, 0x0800 + SCREEN_TALLADD, 96 + SCREEN_TALLADD2 },
        { 0x0004, 0x0000, 0x20BF, 0x0000, 0x0720 + SCREEN_TALLADD, 96 + SCREEN_TALLADD2 },
    },
    {
        // ZoneId_MZ
        { 0x0004, 0x0000, 0x17BF + SCREEN_WIDEADD2, 0x0000, 0x01D0 + SCREEN_TALLADD, 96 + SCREEN_TALLADD2 },
        { 0x0004, 0x0000, 0x17BF + SCREEN_WIDEADD2, 0x0000, 0x0520 + SCREEN_TALLADD, 96 + SCREEN_TALLADD2 },
        { 0x0004, 0x0000, 0x1800 + SCREEN_WIDEADD2, 0x0000, 0x0720 + SCREEN_TALLADD, 96 + SCREEN_TALLADD2 },
        { 0x0004, 0x0000, 0x16BF + SCREEN_WIDEADD2, 0x0000, 0x0720 + SCREEN_TALLADD, 96 + SCREEN_TALLADD2 },
    },
    {
        // ZoneId_SLZ
        { 0x0004, 0x0000, 0x1FBF + SCREEN_WIDEADD2, 0x0000, 0x0640 + SCREEN_TALLADD, 96 + SCREEN_TALLADD2 },
        { 0x0004, 0x0000, 0x1FBF + SCREEN_WIDEADD2, 0x0000, 0x0640 + SCREEN_TALLADD, 96 + SCREEN_TALLADD2 },
        { 0x0004, 0x0000, 0x2000 + SCREEN_WIDEADD2, 0x0000, 0x06C0 + SCREEN_TALLADD, 96 + SCREEN_TALLADD2 },
        { 0x0004, 0x0000, 0x3EC0 + SCREEN_WIDEADD2, 0x0000, 0x0720 + SCREEN_TALLADD, 96 + SCREEN_TALLADD2 },
    },
    {
        // ZoneId_SYZ
        { 0x0004, 0x0000, 0x22C0 + SCREEN_WIDEADD2, 0x0000, 0x0420 + SCREEN_TALLADD, 96 + SCREEN_TALLADD2 },
        { 0x0004, 0x0000, 0x28C0 + SCREEN_WIDEADD2, 0x0000, 0x0520 + SCREEN_TALLADD, 96 + SCREEN_TALLADD2 },
        { 0x0004, 0x0000, 0x2C00 + SCREEN_WIDEADD2, 0x0000, 0x0620 + SCREEN_TALLADD, 96 + SCREEN_TALLADD2 },
        { 0x0004, 0x0000, 0x2EC0 + SCREEN_WIDEADD2, 0x0000, 0x0620 + SCREEN_TALLADD, 96 + SCREEN_TALLADD2 },
    },
    {
        // ZoneId_SBZ
        { 0x0004, 0x0000, 0x21C0 + SCREEN_WIDEADD2, 0x0000, 0x0720 + SCREEN_TALLADD, 96 + SCREEN_TALLADD2 },
        { 0x0004, 0x0000, 0x1E40 + SCREEN_WIDEADD2, -0x0100, 0x0800 + SCREEN_TALLADD, 96 + SCREEN_TALLADD2 },
        { 0x0004, 0x2080, 0x2460 + SCREEN_WIDEADD2, 0x0510, 0x0510 + SCREEN_TALLADD, 96 + SCREEN_TALLADD2 },
        { 0x0004, 0x0000, 0x3EC0 + SCREEN_WIDEADD2, 0x0000, 0x0720 + SCREEN_TALLADD, 96 + SCREEN_TALLADD2 },
    },
    {
        // ZoneId_EndZ
        { 0x0004, 0x0000, 0x0500 + SCREEN_WIDEADD2, 0x0110, 0x0110 + SCREEN_TALLADD, 96 + SCREEN_TALLADD2 },
        { 0x0004, 0x0000, 0x0DC0 + SCREEN_WIDEADD2, 0x0110, 0x0110 + SCREEN_TALLADD, 96 + SCREEN_TALLADD2 },
        { 0x0004, 0x0000, 0x2FFF + SCREEN_WIDEADD2, 0x0000, 0x0320 + SCREEN_TALLADD, 96 + SCREEN_TALLADD2 },
        { 0x0004, 0x0000, 0x2FFF + SCREEN_WIDEADD2, 0x0000, 0x0320 + SCREEN_TALLADD, 96 + SCREEN_TALLADD2 },
    }
};

// Player start positions
static const int16_t ldef_start[ZoneId_Num][4][2] = {
    {
        // ZoneId_GHZ
        { 0x0050, 0x03B0 },
        { 0x0050, 0x00FC },
        { 0x0050, 0x03B0 },
        { 0x0080, 0x00A8 },
    },
    {
        // ZoneId_LZ
        { 0x0060, 0x006C },
        { 0x0050, 0x00EC },
        { 0x0050, 0x02EC },
        { 0x0B80, 0x0000 },
    },
    {
        // ZoneId_MZ
        { 0x0030, 0x0266 },
        { 0x0030, 0x0266 },
        { 0x0030, 0x0166 },
        { 0x0080, 0x00A8 },
    },
    {
        // ZoneId_SLZ
        { 0x0040, 0x02CC },
        { 0x0040, 0x014C },
        { 0x0040, 0x014C },
        { 0x0080, 0x00A8 },
    },
    {
        // ZoneId_SYZ
        { 0x0030, 0x03BD },
        { 0x0030, 0x01BD },
        { 0x0030, 0x00EC },
        { 0x0080, 0x00A8 },
    },
    {
        // ZoneId_SBZ
        { 0x0030, 0x048C },
        { 0x0030, 0x074C },
        { 0x2140, 0x05AC },
        { 0x0080, 0x00A8 },
    },
    {
        // ZoneId_EndZ
        { 0x0620, 0x016B },
        { 0x0EE0, 0x016C },
        { 0x0080, 0x00A8 },
        { 0x0080, 0x00A8 },
    },
};

// Level loop (and S-tube) chunks
static const uint8_t ldef_schunks[ZoneId_Num][2][2] = {
    { { 0xB5, 0x7F }, { 0x1F, 0x20 } }, // ZoneId_GHZ
    { { 0x7F, 0x7F }, { 0x7F, 0x7F } }, // ZoneId_LZ
    { { 0x7F, 0x7F }, { 0x7F, 0x7F } }, // ZoneId_MZ
    { { 0xAA, 0xB4 }, { 0x7F, 0x7F } }, // ZoneId_SLZ
    { { 0x7F, 0x7F }, { 0x7F, 0x7F } }, // ZoneId_SYZ
    { { 0x7F, 0x7F }, { 0x7F, 0x7F } }, // ZoneId_SBZ
    { { 0x7F, 0x7F }, { 0x7F, 0x7F } }, // ZoneId_EndZ
};

// Level scroll block sizes
static const int16_t ldef_scrollsize[ZoneId_Num][4] = {
    { 0x70, 0x100, 0x100, 0x100 },
    { 0x800, 0x100, 0x100, 0 },
    { 0x800, 0x100, 0x100, 0 },
    { 0x800, 0x100, 0x100, 0 },
    { 0x800, 0x100, 0x100, 0 },
    { 0x800, 0x100, 0x100, 0 },
    { 0x70, 0x100, 0x100, 0x100 },
};

// Level headers
const LevelHeader level_header[ZoneId_Num] = {
    { PlcId_GHZ, Art_GHZ2, PlcId_GHZ2, Map16_GHZ, Map256_GHZ, 0, 0, PalId_GHZ, PalId_GHZ, sizeof(Map16_GHZ) },
    { PlcId_LZ, Art_LZ, PlcId_LZ2, Map16_LZ, Map256_LZ, 0, 0, PalId_LZ, PalId_LZ, sizeof(Map16_LZ) },
#ifdef SCP_REV00
    { PlcId_MZ, Art_MZ, PlcId_MZ2, Map16_MZ, Map256_MZREV00, 0, 0, PalId_MZ, PalId_MZ, sizeof(Map16_MZ) },
#else
    { PlcId_MZ, Art_MZ, PlcId_MZ2, Map16_MZ, Map256_MZREV01, 0, 0, PalId_MZ, PalId_MZ, sizeof(Map16_MZ) },
#endif
    { PlcId_SLZ, Art_SLZ, PlcId_SLZ2, Map16_SLZ, Map256_SLZ, 0, 0, PalId_SLZ, PalId_SLZ, sizeof(Map16_SLZ) },
    { PlcId_SYZ, Art_SYZ, PlcId_SYZ2, Map16_SYZ, Map256_SYZ, 0, 0, PalId_SYZ, PalId_SYZ, sizeof(Map16_SYZ) },
#ifdef SCP_REV00
    { PlcId_SBZ, Art_SBZ, PlcId_SBZ2, Map16_SBZ, Map256_SBZREV00, 0, 0, PalId_SBZ1, PalId_SBZ1, sizeof(Map16_SBZ) },
#else
    { PlcId_SBZ, Art_SBZ, PlcId_SBZ2, Map16_SBZ, Map256_SBZREV01, 0, 0, PalId_SBZ1, PalId_SBZ1, sizeof(Map16_SBZ) },
#endif
    { 0, Art_GHZ2, 0, Map16_GHZ, Map256_GHZ, 0, 0, PalId_GHZ, PalId_GHZ, sizeof(Map16_GHZ) },
};

// Level collision indices
const uint8_t* level_coli[ZoneId_Num - 1] = {
    CollisionIndex_GHZ,
    CollisionIndex_LZ,
    CollisionIndex_MZ,
    CollisionIndex_SLZ,
    CollisionIndex_SYZ,
    CollisionIndex_SBZ,
};

// Level object layouts
static const uint8_t obj_null[] = { 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00 };

const uint8_t* level_obj[ZoneId_Num][4][2] = {
    {
        // ZoneId_GHZ
        { ObjectLayout_GHZ1, obj_null },
        { ObjectLayout_GHZ2, obj_null },
#ifdef SCP_REV00
        { ObjectLayout_GHZ3REV00, obj_null },
#else
        { ObjectLayout_GHZ3REV01, obj_null },
#endif
        { ObjectLayout_GHZ1, obj_null },
    },
    {
// ZoneId_LZ
#ifdef SCP_REV00
        { ObjectLayout_LZ1REV00, obj_null },
        { ObjectLayout_LZ2, obj_null },
        { ObjectLayout_LZ3REV00, obj_null },
#else
        { ObjectLayout_LZ1REV01, obj_null },
        { ObjectLayout_LZ2, obj_null },
        { ObjectLayout_LZ3REV01, obj_null },
#endif
        { ObjectLayout_SBZ3, obj_null },
    },
    {
// ZoneId_MZ
#ifdef SCP_REV00
        { ObjectLayout_MZ1REV00, obj_null },
        { ObjectLayout_MZ2, obj_null },
        { ObjectLayout_MZ3, obj_null },
        { ObjectLayout_MZ1REV00, obj_null },
#else
        { ObjectLayout_MZ1REV01, obj_null },
        { ObjectLayout_MZ2, obj_null },
        { ObjectLayout_MZ3, obj_null },
        { ObjectLayout_MZ1REV01, obj_null },
#endif
    },
    {
        // ZoneId_SLZ
        { ObjectLayout_SLZ1, obj_null },
        { ObjectLayout_SLZ2, obj_null },
        { ObjectLayout_SLZ3, obj_null },
        { ObjectLayout_SLZ1, obj_null },
    },
    {
        // ZoneId_SYZ
        { ObjectLayout_SYZ1, obj_null },
        { ObjectLayout_SYZ2, obj_null },
#ifdef SCP_REV00
        { ObjectLayout_SYZ3REV00, obj_null },
#else
        { ObjectLayout_SYZ3REV01, obj_null },
#endif
        { ObjectLayout_SYZ1, obj_null },
    },
    {
// ZoneId_SBZ
#ifdef SCP_REV00
        { ObjectLayout_SBZ1REV00, obj_null },
        { ObjectLayout_SBZ2, obj_null },
        { ObjectLayout_FZ, obj_null },
        { ObjectLayout_SBZ1REV00, obj_null },
#else
        { ObjectLayout_SBZ1REV01, obj_null },
        { ObjectLayout_SBZ2, obj_null },
        { ObjectLayout_FZ, obj_null },
        { ObjectLayout_SBZ1REV01, obj_null },
#endif
    },
    {
        // ZoneId_EndZ
        { ObjectLayout_Ending, obj_null },
        { ObjectLayout_Ending, obj_null },
        { ObjectLayout_Ending, obj_null },
        { ObjectLayout_Ending, obj_null },
    }
};

// Level state
uint16_t level_id;

uint8_t dle_routine;

uint16_t limit_left1, limit_right1, limit_top1, limit_btm1;
uint16_t limit_left2, limit_right2, limit_top2, limit_btm2;
uint16_t limit_left3;
uint16_t limit_top_db, limit_btm_db;

LevelAnim level_anim[6];

uint8_t last_lamp;
uint8_t prev_lamp;
CheckpointState lamp_state;

uint16_t restart;
uint16_t pause;
uint8_t time_over;

uint16_t frame_count;

// Player state
uint32_t score;
LevelTime time;
uint16_t rings;
uint8_t lives;
uint8_t continues;

uint32_t score_life;

uint16_t air;
uint8_t last_special;

uint8_t life_num;
uint8_t life_count;
uint8_t ring_count;
uint8_t time_count;
uint8_t score_count;

uint8_t shield;
uint8_t invincibility;
uint8_t shoes;
uint8_t debug_use;

// Water state
int16_t wtr_pos1, wtr_pos2, wtr_pos3;
uint8_t water;
uint8_t wtr_routine;
uint8_t wtr_state;

// Loaded level data
uint8_t* const level_map256 = &buffer0000[0x0000];
ALIGNED2 uint8_t level_map16[0x1800];
uint8_t level_layout[8][2][0x40];
uint8_t level_schunks[2][2];
const uint8_t* coll_index;

// Object state
Object objects[OBJECTS];
Object* const player = objects;
Object* const level_objects = objects + RESERVED_OBJECTS;

uint16_t opl_routine;
int16_t opl_screen;
const uint8_t* opl_ptr0;
const uint8_t* opl_ptr4;
const uint8_t* opl_ptr8;
const uint8_t* opl_ptrC;
const uint8_t* opl_layout;

uint8_t objstate_left;
uint8_t objstate_right;
uint8_t objstate[0x100];

int16_t obj31_ypos;
uint8_t boss_status;
uint8_t lock_screen;
uint16_t gfx_big_ring;
uint8_t convey_rev;
uint8_t obj63[6];
uint8_t tunnel_mode;
uint8_t lock_multi;
uint8_t tunnel_allow;
uint8_t jump_only;
uint8_t obj6B;
uint8_t lock_ctrl;
uint8_t big_ring;
uint16_t item_bonus;
uint16_t time_bonus;
uint16_t ring_bonus;
uint8_t endact_bonus;
uint8_t sonicend;
uint16_t lz_deform;
uint8_t f_switch[0x10];

Oscillatory oscillatory;

LevelAnim sprite_anim[4];
uint16_t sprite_anim_3buf;

// Game functions
void AddPoints(uint16_t points)
{
    // Update HUD
    score_count = 1;

#ifdef SCP_REV00
    // TODO
#else
    // Increase score
    if ((score += points) >= 999999)
        score = 999999;

    // Check if we should be rewarded an extra life
    if (score >= score_life) {
        score_life += 5000;
#ifndef SCP_JP
        lives++;
        life_count++;
        // music	bgm_ExtraLife,1,0,0 //TODO
#endif
    }
#endif
}

// Level loading
void LoadLevelMaps()
{
    // Get header
    const LevelHeader* header = &level_header[LEVEL_ZONE(level_id)];

    // Load chunk maps and tile map
    KosDec(header->map256, level_map256);
    memcpy(level_map16, header->map16, header->map16_size);
}

void LoadLayout(const uint8_t* from, uint8_t* to)
{
    // Read layout header (dimensions - 1)
    uint8_t width = *from++;
    uint8_t height = *from++;

    // Read layout data
    do {
        for (size_t i = 0; i <= width; i++)
            *to++ = *from++;
        to += 0x80 - (width + 1);
    } while (height-- > 0);
}

void LoadLevelLayout()
{
    // Load foreground and background layers
    memset(level_layout, 0, sizeof(level_layout));
    LoadLayout(
        level_layouts[LEVEL_ZONE(level_id)][LEVEL_ACT(level_id)].layout_fg,
        level_layout[0][0]);
    LoadLayout(
        level_layouts[LEVEL_ZONE(level_id)][LEVEL_ACT(level_id)].layout_bg,
        level_layout[0][1]);
}

void LevelSizeLoad()
{
    // Reset level state
    dle_routine = 0;

    // Get sizes to load
    const int16_t* sizes = ldef_size[LEVEL_ZONE(level_id)][LEVEL_ACT(level_id)];

    // Load sizes and other stuff
    /* FFFFF730 = */ sizes++;
    limit_left2 = *sizes;
    limit_left1 = *sizes++;
    limit_right2 = *sizes;
    limit_right1 = *sizes++;
    limit_top2 = *sizes;
    limit_top1 = *sizes++;
    limit_btm2 = *sizes;
    limit_btm1 = *sizes++;
    limit_left3 = limit_left2 + 0x240;
    look_shift = *sizes++;

    // Load player start
    int16_t x, y;
    if (last_lamp) {
        Obj_Checkpoint_LoadInfo();
        x = player->pos.l.x.f.u;
        y = player->pos.l.y.f.u;
    } else {
        if (demo < 0) {
            // TODO - in an ending demo
            x = 0x80;
            y = 0xA8;
        } else {
            // Level
            x = ldef_start[LEVEL_ZONE(level_id)][LEVEL_ACT(level_id)][0];
            y = ldef_start[LEVEL_ZONE(level_id)][LEVEL_ACT(level_id)][1];
        }

        player->pos.l.x.f.u = x;
        player->pos.l.y.f.u = y;
    }

    // Clip camera position against left and right
    if ((x -= (SCREEN_WIDTH / 2)) < 0) // 0 instead of limit_left
        x = 0;
    if (x >= limit_right2)
        x = limit_right2;
    scrpos_x.f.u = x;

    // Clip camera position against top and bottom
    if ((y -= (96 + SCREEN_TALLADD2)) < 0) // 0 instead of limit_top
        y = 0;
    if (y >= limit_btm2)
        y = limit_btm2;
    scrpos_y.f.u = y;

    // Load other level stuff
    BgScrollSpeed(x, y);
    memcpy(&level_schunks[0][0], &ldef_schunks[LEVEL_ZONE(level_id)][0][0], 4);

    const int16_t* scroll_size = ldef_scrollsize[LEVEL_ZONE(level_id)];
    scroll_block1_size = *scroll_size++;
    scroll_block2_size = *scroll_size++;
    scroll_block3_size = *scroll_size++;
    scroll_block4_size = *scroll_size++;
}

void LevelDataLoad()
{
    // Get header
    const LevelHeader* header = &level_header[LEVEL_ZONE(level_id)];

    // Load chunk maps and tile map
    KosDec(header->map256, level_map256);
    memcpy(level_map16, header->map16, header->map16_size);

    // Load level layout
    LoadLevelLayout();

    // Load level palette
    PaletteId pal = header->pal;
    if (level_id == LEVEL_ID(ZoneId_LZ, 3))
        pal = PalId_SBZ3;
    if (level_id == LEVEL_ID(ZoneId_SBZ, 1) || level_id == LEVEL_ID(ZoneId_SBZ, 2))
        pal = PalId_SBZ2;
    PalLoad1(pal);

    // Load level art
    if (header->plc2 != 0)
        AddPLC(header->plc2);
}

void ColIndexLoad()
{
    // Use zone's collision indices
    coll_index = level_coli[LEVEL_ZONE(level_id)];
}

// Dynamic level events
void DynamicLevelEvents()
{
    // Update target scroll limits
    switch (LEVEL_ZONE(level_id)) {
    case ZoneId_GHZ:
        switch (LEVEL_ACT(level_id)) {
        case 0: // Act 1
            if ((uint16_t)scrpos_x.f.u >= (0x1780 - SCREEN_WIDEADD2))
                limit_btm1 = 0x400 - SCREEN_TALLADD;
            else
                limit_btm1 = 0x300 - SCREEN_TALLADD;
            break;
        case 1: // Act 2
            limit_btm1 = 0x300 - SCREEN_TALLADD;
            if ((uint16_t)scrpos_x.f.u < (0xED0 - SCREEN_WIDEADD2))
                break;
            limit_btm1 = 0x200 - SCREEN_TALLADD;
            if ((uint16_t)scrpos_x.f.u < (0x1600 - SCREEN_WIDEADD2))
                break;
            limit_btm1 = 0x400 - SCREEN_TALLADD;
            if ((uint16_t)scrpos_x.f.u < (0x1D60 - SCREEN_WIDEADD2))
                break;
            limit_btm1 = 0x300 - SCREEN_TALLADD;
            break;
        case 2: // Act 3
            switch (dle_routine) {
            case 0:
                limit_btm1 = 0x300 - SCREEN_TALLADD;
                if ((uint16_t)scrpos_x.f.u < (0x380 - SCREEN_WIDEADD2))
                    break;
                limit_btm1 = 0x310 - SCREEN_TALLADD;
                if ((uint16_t)scrpos_x.f.u < (0x960 - SCREEN_WIDEADD2))
                    break;
                if (scrpos_y.f.u >= 0x280 + SCREEN_TALLADD) {
                    limit_btm1 = 0x400 - SCREEN_TALLADD;
                    if ((uint16_t)scrpos_x.f.u < (0x1380 - SCREEN_WIDEADD2)) {
                        limit_btm1 = 0x4C0 - SCREEN_TALLADD;
                        limit_btm2 = 0x4C0 - SCREEN_TALLADD;
                    } else if ((uint16_t)scrpos_x.f.u >= (0x1700 - SCREEN_WIDEADD2)) {
                        limit_btm1 = 0x300 - SCREEN_TALLADD;
                        dle_routine += 2;
                    }
                } else {
                    limit_btm1 = 0x300 - SCREEN_TALLADD;
                    dle_routine += 2;
                }
                break;
            case 2:
                if ((uint16_t)scrpos_x.f.u < (0x960 - SCREEN_WIDEADD2))
                    dle_routine -= 2;
                if ((uint16_t)scrpos_x.f.u < (0x2960 - SCREEN_WIDEADD2))
                    break;
                // TODO spawn boss
                break;
            }
            break;
        }
        break;
    default:
        break;
    }

    // Update scroll limits
    int16_t scroll_diff = limit_btm1 - limit_btm2;
    int16_t spd = 2;

    if (scroll_diff < 0) {
        if ((uint16_t)scrpos_y.f.u > limit_btm1)
            limit_btm2 = scrpos_y.f.u & ~1;
        limit_btm2 -= spd;
        bgscrollvert = true;
    } else if (scroll_diff > 0) {
        if (((uint16_t)scrpos_y.f.u + 8) >= limit_btm2 && player->status.p.f.in_air)
            spd *= 4;
        limit_btm2 += spd;
        bgscrollvert = true;
    }
}

// Object animation
void SynchroAnimate()
{
    // Spiked log
    if (--sprite_anim[0].time < 0) {
        sprite_anim[0].time = 11;
        sprite_anim[0].frame = (sprite_anim[0].frame - 1) & 7;
    }

    // Rings
    if (--sprite_anim[1].time < 0) {
        sprite_anim[1].time = 7;
        sprite_anim[1].frame = (sprite_anim[1].frame + 1) & 3;
    }

    // Unused
    if (--sprite_anim[2].time < 0) {
        sprite_anim[2].time = 7;
        if (++sprite_anim[2].frame >= 6)
            sprite_anim[2].frame = 0;
    }

    // Bouncing rings
    if (sprite_anim[3].time != 0) {
        // WACKY!!
        sprite_anim_3buf += (uint8_t)sprite_anim[3].time;
        sprite_anim[3].frame = (sprite_anim_3buf >> 9) & 3;
        sprite_anim[3].time--;
    }
}

// Signpost loading
void SignpostArtLoad()
{
    // Check if signpost should load
    if (debug_use || (level_id & 0xFF) == 2)
        return;

    // Check if we've reached the end of the level
    int16_t end_x = limit_right2 - 0x100 - SCREEN_WIDEADD2;
    if (scrpos_x.f.u >= end_x && time_count && limit_left2 != end_x) {
        limit_left2 = end_x;
        NewPLC(PlcId_Signpost);
    }
}

// Level object loading
#define LOAD_WIDTH (((SCREEN_WIDTH + 0x80) & ~0x7F) + 0x100) // I dunno

static bool ChkLoadObj(uint8_t index, const uint8_t** entry)
{
    // Handle object state
    if ((*entry)[4] & 0x80) {
        if (objstate[index] & 0x80) {
            // Object already loaded
            *entry += 6;
            return false;
        } else {
            // Object loaded, set flag
            objstate[index] |= 0x80;
        }
    }

    // Load object
    Object* obj = FindFreeObj();
    if (obj == NULL)
        return true; // Result from FindFreeObj, not d0

    obj->pos.l.x.f.u = ((*entry)[0] << 8) | ((*entry)[1] << 0);
    *entry += 2;

    uint16_t w1 = ((*entry)[0] << 8) | ((*entry)[1] << 0);
    *entry += 2;
    obj->pos.l.y.f.u = w1 & 0xFFF;
    obj->render.b = 0;
    obj->status.o.b = 0;
    obj->render.f.x_flip = obj->status.o.f.x_flip = w1 >> 14;
    obj->render.f.y_flip = obj->status.o.f.y_flip = w1 >> 15;

    uint8_t b4 = *(*entry)++;
    if (b4 & 0x80)
        obj->respawn_index = index;
    obj->type = b4 & 0x7F;

    obj->scratch.u8[0] = *(*entry)++; // Subtype

    return false;
}

void ObjPosLoad()
{
    const uint8_t* entry;

    switch (opl_routine) {
    case 0: // Initialization
    {
        // Increment routine
        opl_routine += 2;

        // Initialize state
        opl_layout = level_obj[LEVEL_ZONE(level_id)][LEVEL_ACT(level_id)][0];
        opl_ptr0 = opl_layout;
        opl_ptr4 = opl_layout;
        opl_ptr8 = level_obj[LEVEL_ZONE(level_id)][LEVEL_ACT(level_id)][1];
        opl_ptrC = level_obj[LEVEL_ZONE(level_id)][LEVEL_ACT(level_id)][1];

        objstate_left = 1;
        objstate_right = 1;
        memset(objstate, 0, sizeof(objstate));

        // Load immediately on-screen objects
        int16_t load_x = (scrpos_x.f.u - 0x80) & ~0x7F;
        if (load_x < 0)
            load_x = 0;

        entry = opl_ptr0;
        while (load_x > ((entry[0] << 8) | (entry[1] << 0))) {
            if (entry[4] & 0x80)
                objstate_right++;
            entry += 6;
        }
        opl_ptr0 = entry;

        entry = opl_ptr4;
        if ((load_x -= 0x80) >= 0) {
            while (load_x > ((entry[0] << 8) | (entry[1] << 0))) {
                if (entry[4] & 0x80)
                    objstate_left++;
                entry += 6;
            }
        }
        opl_ptr4 = entry;

        opl_screen = -1;
    }
        // Fallthrough
    case 2: // Main
    {
        // Check if screen has scrolled and load objects
        uint8_t index = 0;

        int16_t load_x = scrpos_x.f.u & ~0x7F;

        if (load_x < opl_screen) {
            // Moving left
            opl_screen = load_x;

            // Load objects
            entry = opl_ptr4;
            if ((load_x -= 0x80) >= 0) {
                while (entry > opl_layout && load_x < (int16_t)((entry[-6] << 8) | (entry[-5] << 0))) {
                    entry -= 6;
                    if (entry[4] & 0x80)
                        index = --objstate_left;

                    // Load object
                    if (!ChkLoadObj(index, &entry)) {
                        entry -= 6;
                    } else {
                        if (entry[4] & 0x80)
                            objstate_left++;
                        entry += 6;
                        break;
                    }
                }
            }
            opl_ptr4 = entry;

            // Move right pointer
            entry = opl_ptr0;
            load_x += 0x80 + LOAD_WIDTH;
            while (entry > opl_layout && load_x <= ((entry[-6] << 8) | (entry[-5] << 0))) {
                if (entry[-2] & 0x80)
                    objstate_right--;
                entry -= 6;
            }
            opl_ptr0 = entry;
        } else if (load_x > opl_screen) {
            // Moving right
            opl_screen = load_x;

            // Load objects
            entry = opl_ptr0;
            load_x += LOAD_WIDTH;
            while (load_x > ((entry[0] << 8) | (entry[1] << 0))) {
                if (entry[4] & 0x80)
                    index = objstate_right++;

                // Load object
                if (ChkLoadObj(index, &entry))
                    break;
            }
            opl_ptr0 = entry;

            // Move left pointer
            entry = opl_ptr4;
            load_x -= 0x80 + LOAD_WIDTH;
            while (load_x > ((entry[0] << 8) | (entry[1] << 0))) {
                if (entry[4] & 0x80)
                    objstate_left++;
                entry += 6;
            }
            opl_ptr4 = entry;
        }
        break;
    }
    }
}
