// Sonic 1's level data: which art, maps, layouts, collision, objects and rings each zone and act is made of, where the player starts, how far the level reaches and how the
// background scrolls. Level.c (the loading and the level's state) reads them through Level.h; a game with other levels replaces this file (Sonic 2 has its own).
#include "Level.h"

#include "Constants.h"
#include "Game.h"
#include "PLC.h"
#include "Palette.h"

#include <string.h>

// Level layouts (interleaved FG+BG single-buffer blobs, Kosinski-compressed,
// real Sonic 2 format -- one combined resource per act, no separate BG file
// and no REV00/REV01 split at this layer; SonLVL-format-verified against
// s2disasm, which fixes Level_Layout at ds.b $1000 = 16 rows * 0x100 stride)
#include "Resource/Layout/GHZ1.h"
#include "Resource/Layout/GHZ2.h"
#include "Resource/Layout/GHZ3.h"

#include "Resource/Layout/LZ1.h"
#include "Resource/Layout/LZ2.h"
#include "Resource/Layout/LZ3.h"

#include "Resource/Layout/MZ1.h"
#include "Resource/Layout/MZ2.h"
#include "Resource/Layout/MZ3.h"

#include "Resource/Layout/SLZ1.h"
#include "Resource/Layout/SLZ2.h"
#include "Resource/Layout/SLZ3.h"

#include "Resource/Layout/SYZ1.h"
#include "Resource/Layout/SYZ2.h"
#include "Resource/Layout/SYZ3.h"

#include "Resource/Layout/SBZ1.h"
#include "Resource/Layout/SBZ2.h"
#include "Resource/Layout/SBZ3.h"

#include "Resource/Layout/Ending.h"

// 128x128 chunk tables (real Sonic 2 chunk size -- half of Sonic 1's
// original 256x256, so a level needs twice as many rows to cover the same
// physical height, see LEVEL_LAYOUT_ROWS)
#include "Resource/Map128/GHZ.h"
#include "Resource/Map128/LZ.h"
#include "Resource/Map128/MZREV01.h"
#include "Resource/Map128/SLZ.h"
#include "Resource/Map128/SYZ.h"
#include "Resource/Map128/SBZREV01.h"

// 16x16 mappings
#include "Resource/Map16/GHZ.h"
#include "Resource/Map16/LZ.h"
#include "Resource/Map16/MZ.h"
#include "Resource/Map16/SBZ.h"
#include "Resource/Map16/SLZ.h"
#include "Resource/Map16/SYZ.h"

// Collision indices, one file per path (1 = primary, 2 = secondary; both
// currently identical since no real content path-swaps yet)
#include "Resource/Collision/GHZ1.h"
#include "Resource/Collision/GHZ2.h"
#include "Resource/Collision/LZ1.h"
#include "Resource/Collision/LZ2.h"
#include "Resource/Collision/MZ1.h"
#include "Resource/Collision/MZ2.h"
#include "Resource/Collision/SBZ1.h"
#include "Resource/Collision/SBZ2.h"
#include "Resource/Collision/SLZ1.h"
#include "Resource/Collision/SLZ2.h"
#include "Resource/Collision/SYZ1.h"
#include "Resource/Collision/SYZ2.h"

// Object positions
#include "Resource/ObjectLayout/GHZ1.h"
#include "Resource/ObjectLayout/GHZ2.h"
#include "Resource/ObjectLayout/GHZ3REV01.h"

#include "Resource/ObjectLayout/LZ1REV01.h"
#include "Resource/ObjectLayout/LZ1PF1.h"
#include "Resource/ObjectLayout/LZ1PF2.h"
#include "Resource/ObjectLayout/LZ2.h"
#include "Resource/ObjectLayout/LZ2PF1.h"
#include "Resource/ObjectLayout/LZ2PF2.h"
#include "Resource/ObjectLayout/LZ3REV01.h"
#include "Resource/ObjectLayout/LZ3PF1.h"
#include "Resource/ObjectLayout/LZ3PF2.h"

#include "Resource/ObjectLayout/MZ1REV01.h"
#include "Resource/ObjectLayout/MZ2.h"
#include "Resource/ObjectLayout/MZ3.h"

#include "Resource/ObjectLayout/SLZ1.h"
#include "Resource/ObjectLayout/SLZ2.h"
#include "Resource/ObjectLayout/SLZ3.h"

#include "Resource/ObjectLayout/SYZ1.h"
#include "Resource/ObjectLayout/SYZ2.h"
#include "Resource/ObjectLayout/SYZ3REV01.h"

#include "Resource/ObjectLayout/SBZ1REV01.h"
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

// The rings, in their own layouts (RingsManager.h)
#include "Resource/RingLayout/Ending.h"
#include "Resource/RingLayout/FZ.h"
#include "Resource/RingLayout/GHZ1.h"
#include "Resource/RingLayout/GHZ2.h"
#include "Resource/RingLayout/GHZ3REV01.h"
#include "Resource/RingLayout/LZ1REV01.h"
#include "Resource/RingLayout/LZ2.h"
#include "Resource/RingLayout/LZ3REV01.h"
#include "Resource/RingLayout/MZ1REV01.h"
#include "Resource/RingLayout/MZ2.h"
#include "Resource/RingLayout/MZ3.h"
#include "Resource/RingLayout/SLZ1.h"
#include "Resource/RingLayout/SLZ2.h"
#include "Resource/RingLayout/SLZ3.h"
#include "Resource/RingLayout/SYZ1.h"
#include "Resource/RingLayout/SYZ2.h"
#include "Resource/RingLayout/SYZ3REV01.h"
#include "Resource/RingLayout/SBZ1REV01.h"
#include "Resource/RingLayout/SBZ2.h"
#include "Resource/RingLayout/SBZ3.h"

// Level definitions -- one combined (interleaved FG+BG, Kosinski) blob per
// act now, not a separate FG/BG pair (layout_3 was a dead 3rd field here
// that was never actually read anywhere -- same dead-field pattern as
// LevelHeader's old pad/music/pal_dup -- dropped along with layout_bg).
const struct LevelLayout level_layouts[ZoneId_Num][4] = {
    {
        // ZoneId_GHZ
        { Layout_GHZ1 }, { Layout_GHZ2 }, { Layout_GHZ3 }, { NULL },
    },
    {
        // ZoneId_LZ
        { Layout_LZ1 }, { Layout_LZ2 }, { Layout_LZ3 }, { Layout_SBZ3 },
    },
    {
        // ZoneId_MZ
        { Layout_MZ1 }, { Layout_MZ2 }, { Layout_MZ3 }, { NULL },
    },
    {
        // ZoneId_SLZ
        { Layout_SLZ1 }, { Layout_SLZ2 }, { Layout_SLZ3 }, { NULL },
    },
    {
        // ZoneId_SYZ
        { Layout_SYZ1 }, { Layout_SYZ2 }, { Layout_SYZ3 }, { NULL },
    },
    {
        // ZoneId_SBZ (3rd slot is FZ -- the Final Zone boss act reuses SBZ
        // act 2's own chunk layout, just with a different start position;
        // Layout_SBZ3 is real data but only used for LZ's 4th act, above)
        { Layout_SBZ1 }, { Layout_SBZ2 }, { Layout_SBZ2 }, { NULL },
    },
    {
        // ZoneId_EndZ
        { Layout_Ending }, { Layout_Ending }, { NULL }, { NULL },
    },
};

// The level boundaries and camera shifts of each act. They depend on the size of the picture (the wider or taller it is, the
// further the camera can see past the edges), which is chosen at run time, so the table is built when it is read.
const int16_t *LevelSizes(int zone, int act) {
    const int16_t table[ZoneId_Num][4][6] = {
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
    static int16_t result[6];
    memcpy(result, table[zone][act], sizeof(result));
    return result;
}

// Player start positions
const int16_t StartLocArray[ZoneId_Num][4][2] = {
    { { 0x0050, 0x03B0 }, { 0x0050, 0x00FC }, { 0x0050, 0x03B0 }, { 0x0080, 0x00A8 }, }, // ZoneId_GHZ
    { { 0x0060, 0x006C }, { 0x0050, 0x00EC }, { 0x0050, 0x02EC }, { 0x0B80, 0x0000 }, }, // ZoneId_LZ
    { { 0x0030, 0x0266 }, { 0x0030, 0x0266 }, { 0x0030, 0x0166 }, { 0x0080, 0x00A8 }, }, // ZoneId_MZ
    { { 0x0040, 0x02CC }, { 0x0040, 0x014C }, { 0x0040, 0x014C }, { 0x0080, 0x00A8 }, }, // ZoneId_SLZ
    { { 0x0030, 0x03BD }, { 0x0030, 0x01BD }, { 0x0030, 0x00EC }, { 0x0080, 0x00A8 }, }, // ZoneId_SYZ
    { { 0x0030, 0x048C }, { 0x0030, 0x074C }, { 0x2140, 0x05AC }, { 0x0080, 0x00A8 }, }, // ZoneId_SBZ
    { { 0x0620, 0x016B }, { 0x0EE0, 0x016C }, { 0x0080, 0x00A8 }, { 0x0080, 0x00A8 }, }, // ZoneId_EndZ
};

// Level scroll block sizes
const int16_t BGScrollBlockSizes[ZoneId_Num][4] = {
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
    { PlcId_GHZ, Art_GHZ2, PlcId_GHZ2, Map16_GHZ, PalId_GHZ, Map128_GHZ },
    { PlcId_LZ, Art_LZ, PlcId_LZ2, Map16_LZ, PalId_LZ, Map128_LZ },
    { PlcId_MZ, Art_MZ, PlcId_MZ2, Map16_MZ, PalId_MZ, Map128_MZREV01 },
    { PlcId_SLZ, Art_SLZ, PlcId_SLZ2, Map16_SLZ, PalId_SLZ, Map128_SLZ },
    { PlcId_SYZ, Art_SYZ, PlcId_SYZ2, Map16_SYZ, PalId_SYZ, Map128_SYZ },
    { PlcId_SBZ, Art_SBZ, PlcId_SBZ2, Map16_SBZ, PalId_SBZ1, Map128_SBZREV01 },
    { 0, Art_GHZ2, 0, Map16_GHZ, PalId_Ending, Map128_GHZ },
};

// Level collision indices, one file per path (real data, not a
// decode-the-same-blob-twice shim -- GHZ1/GHZ2 etc are currently identical
// since no content path-swaps yet, but are independent resources)
const uint8_t* level_coli[ZoneId_Num - 1][2] = {
    { Collision_GHZ1, Collision_GHZ2 },
    { Collision_LZ1, Collision_LZ2 },
    { Collision_MZ1, Collision_MZ2 },
    { Collision_SLZ1, Collision_SLZ2 },
    { Collision_SYZ1, Collision_SYZ2 },
    { Collision_SBZ1, Collision_SBZ2 },
};

// Level object layouts (Sonic 2 format: ObjectsManager.h)
static const uint8_t obj_null[] = { 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00 };

const uint8_t* level_obj[ZoneId_Num][4] = {
    {
        // ZoneId_GHZ
        ObjectLayout_GHZ1,
        ObjectLayout_GHZ2,
        ObjectLayout_GHZ3REV01,
        ObjectLayout_GHZ1,
    },
    {
// ZoneId_LZ
        ObjectLayout_LZ1REV01,
        ObjectLayout_LZ2,
        ObjectLayout_LZ3REV01,
        ObjectLayout_SBZ3,
    },
    {
// ZoneId_MZ
        ObjectLayout_MZ1REV01,
        ObjectLayout_MZ2,
        ObjectLayout_MZ3,
        ObjectLayout_MZ1REV01,
    },
    {
        // ZoneId_SLZ
        ObjectLayout_SLZ1,
        ObjectLayout_SLZ2,
        ObjectLayout_SLZ3,
        ObjectLayout_SLZ1,
    },
    {
        // ZoneId_SYZ
        ObjectLayout_SYZ1,
        ObjectLayout_SYZ2,
        ObjectLayout_SYZ3REV01,
        ObjectLayout_SYZ1,
    },
    {
// ZoneId_SBZ
        ObjectLayout_SBZ1REV01,
        ObjectLayout_SBZ2,
        ObjectLayout_FZ,
        ObjectLayout_SBZ1REV01,
    },
    {
        // ZoneId_EndZ
        ObjectLayout_Ending,
        ObjectLayout_Ending,
        ObjectLayout_Ending,
        ObjectLayout_Ending,
    }
};

// ... and the ring layouts, in the same places
const uint8_t* level_ring[ZoneId_Num][4] = {
    {
        // ZoneId_GHZ
        RingLayout_GHZ1,
        RingLayout_GHZ2,
        RingLayout_GHZ3REV01,
        RingLayout_GHZ1,
    },
    {
// ZoneId_LZ
        RingLayout_LZ1REV01,
        RingLayout_LZ2,
        RingLayout_LZ3REV01,
        RingLayout_SBZ3,
    },
    {
// ZoneId_MZ
        RingLayout_MZ1REV01,
        RingLayout_MZ2,
        RingLayout_MZ3,
        RingLayout_MZ1REV01,
    },
    {
        // ZoneId_SLZ
        RingLayout_SLZ1,
        RingLayout_SLZ2,
        RingLayout_SLZ3,
        RingLayout_SLZ1,
    },
    {
        // ZoneId_SYZ
        RingLayout_SYZ1,
        RingLayout_SYZ2,
        RingLayout_SYZ3REV01,
        RingLayout_SYZ1,
    },
    {
// ZoneId_SBZ
        RingLayout_SBZ1REV01,
        RingLayout_SBZ2,
        RingLayout_FZ,
        RingLayout_SBZ1REV01,
    },
    {
        // ZoneId_EndZ
        RingLayout_Ending,
        RingLayout_Ending,
        RingLayout_Ending,
        RingLayout_Ending,
    }
};

