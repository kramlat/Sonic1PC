// Sonic 2's level data (a copy of Sonic1LevelData.c, with each zone slot's entries replaced by their Sonic 2 zone's as those come; slot 3 = Emerald Hill, imported from the Nick
// Arcade prototype by tools/import_s2na_zone.pl). Sonic 1's level data: which art, maps, layouts, collision, objects and rings each zone and act is made of, where the player starts, how far the level reaches and how the
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

// The zones' tilesets (Kosinski, as Sonic 1's: Level.c decompresses the header's at level start, so the PLC lists leave them out)
#include "Resource/S2Art/CPZ.h"
#include "Resource/S2Art/EHZ.h"
#include "Resource/S2Art/HPZ.h"
#include "Resource/S2Art/HTZ.h"

// Chemical Plant (slot 2: Sonic 1's Marble slot), Hidden Palace (slot 4: Spring Yard's) and Hill Top (slot 5: Scrap Brain's, whose third act is the Final Zone's)
#include "Resource/S2Layout/CPZ1.h"
#include "Resource/S2Map128/CPZ.h"
#include "Resource/S2Map16/CPZ.h"
#include "Resource/S2Collision/CPZ1.h"
#include "Resource/S2Collision/CPZ2.h"
#include "Resource/S2Objects/CPZ1.h"
#include "Resource/S2Rings/CPZ1.h"
#include "Resource/S2Layout/HPZ1.h"
#include "Resource/S2Map128/HPZ.h"
#include "Resource/S2Map16/HPZ.h"
#include "Resource/S2Collision/HPZ1.h"
#include "Resource/S2Collision/HPZ2.h"
#include "Resource/S2Objects/HPZ1.h"
#include "Resource/S2Rings/HPZ1.h"
#include "Resource/S2Layout/HTZ1.h"
#include "Resource/S2Layout/HTZ2.h"
#include "Resource/S2Map16/HTZ.h"
#include "Resource/S2Objects/HTZ1.h"
#include "Resource/S2Rings/HTZ1.h"
#include "Resource/S2Rings/HTZ2.h"

// Emerald Hill (slot 3)
#include "Resource/S2Layout/EHZ1.h"
#include "Resource/S2Layout/EHZ2.h"
#include "Resource/S2Map128/EHZ.h"
#include "Resource/S2Map16/EHZ.h"
#include "Resource/S2Collision/EHZ1.h"
#include "Resource/S2Collision/EHZ2.h"
#include "Resource/S2Objects/EHZ1.h"
#include "Resource/S2Objects/EHZ2.h"
#include "Resource/S2Rings/EHZ1.h"
#include "Resource/S2Rings/EHZ2.h"

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
        { S2Layout_CPZ1 }, { S2Layout_CPZ1 }, { S2Layout_CPZ1 }, { S2Layout_CPZ1 }, // (Nick Arcade has one layout for all four acts)
    },
    {
        // ZoneId_SLZ
        { S2Layout_EHZ1 }, { S2Layout_EHZ2 }, { S2Layout_EHZ1 }, { S2Layout_EHZ2 }, // (as Nick Arcade's acts 3 and 4: acts 1 and 2 again)
    },
    {
        // ZoneId_SYZ
        { S2Layout_HPZ1 }, { S2Layout_HPZ1 }, { S2Layout_HPZ1 }, { S2Layout_HPZ1 },
    },
    {
        // ZoneId_SBZ: Hill Top (its third act, where Sonic 1 has the Final Zone, is Nick Arcade's HTZ3 on the first layout again)
        { S2Layout_HTZ1 }, { S2Layout_HTZ2 }, { S2Layout_HTZ1 }, { S2Layout_HTZ2 },
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
            { 0x0004, 0x0000, 0x3FFF + SCREEN_WIDEADD2, 0x0000, 0x0720 + SCREEN_TALLADD, 96 + SCREEN_TALLADD2 },
            { 0x0004, 0x0000, 0x3FFF + SCREEN_WIDEADD2, 0x0000, 0x0720 + SCREEN_TALLADD, 96 + SCREEN_TALLADD2 },
            { 0x0004, 0x0000, 0x3FFF + SCREEN_WIDEADD2, 0x0000, 0x0720 + SCREEN_TALLADD, 96 + SCREEN_TALLADD2 },
            { 0x0004, 0x0000, 0x3FFF + SCREEN_WIDEADD2, 0x0000, 0x0720 + SCREEN_TALLADD, 96 + SCREEN_TALLADD2 },
        },
        {
            // ZoneId_SLZ (Emerald Hill)
            { 0x0004, 0x0000, 0x29A0 + SCREEN_WIDEADD2, 0x0000, 0x0320 + SCREEN_TALLADD, 96 + SCREEN_TALLADD2 },
            { 0x0004, 0x0000, 0x2940 + SCREEN_WIDEADD2, 0x0000, 0x0420 + SCREEN_TALLADD, 96 + SCREEN_TALLADD2 },
            { 0x0004, 0x0000, 0x2940 + SCREEN_WIDEADD2, 0x0000, 0x0420 + SCREEN_TALLADD, 96 + SCREEN_TALLADD2 },
            { 0x0004, 0x0000, 0x2940 + SCREEN_WIDEADD2, 0x0000, 0x0420 + SCREEN_TALLADD, 96 + SCREEN_TALLADD2 },
        },
        {
            // ZoneId_SYZ
            { 0x0004, 0x0000, 0x3FFF + SCREEN_WIDEADD2, 0x0000, 0x0720 + SCREEN_TALLADD, 96 + SCREEN_TALLADD2 },
            { 0x0004, 0x0000, 0x3FFF + SCREEN_WIDEADD2, 0x0000, 0x0720 + SCREEN_TALLADD, 96 + SCREEN_TALLADD2 },
            { 0x0004, 0x0000, 0x3FFF + SCREEN_WIDEADD2, 0x0000, 0x0720 + SCREEN_TALLADD, 96 + SCREEN_TALLADD2 },
            { 0x0004, 0x0000, 0x3FFF + SCREEN_WIDEADD2, 0x0000, 0x0720 + SCREEN_TALLADD, 96 + SCREEN_TALLADD2 },
        },
        {
            // ZoneId_SBZ
            { 0x0004, 0x0000, 0x3FFF + SCREEN_WIDEADD2, 0x0000, 0x0720 + SCREEN_TALLADD, 96 + SCREEN_TALLADD2 },
            { 0x0004, 0x0000, 0x3FFF + SCREEN_WIDEADD2, -0x0100, 0x0720 + SCREEN_TALLADD, 96 + SCREEN_TALLADD2 },
            { 0x0004, 0x2080, 0x3FFF + SCREEN_WIDEADD2, 0x0510, 0x0720 + SCREEN_TALLADD, 96 + SCREEN_TALLADD2 },
            { 0x0004, 0x0000, 0x3FFF + SCREEN_WIDEADD2, 0x0000, 0x0720 + SCREEN_TALLADD, 96 + SCREEN_TALLADD2 },
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
    { { 0x0030, 0x01EC }, { 0x0030, 0x0266 }, { 0x0030, 0x0166 }, { 0x0080, 0x00A8 }, }, // ZoneId_MZ (Chemical Plant)
    { { 0x0060, 0x028F }, { 0x0040, 0x02AF }, { 0x0040, 0x02AF }, { 0x0040, 0x02AF }, }, // ZoneId_SLZ (Emerald Hill)
    { { 0x0230, 0x01AC }, { 0x0030, 0x01BD }, { 0x0030, 0x00EC }, { 0x0080, 0x00A8 }, }, // ZoneId_SYZ (Hidden Palace)
    { { 0x0040, 0x036F }, { 0x0060, 0x0690 }, { 0x2140, 0x05AC }, { 0x0080, 0x00A8 }, }, // ZoneId_SBZ (Hill Top; its third act starts where the Final Zone does)
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
    { PlcId_MZ, S2Art_CPZ, 0, S2Map16_CPZ, PalId_MZ, S2Map128_CPZ }, // Chemical Plant
    { PlcId_SLZ, S2Art_EHZ, 0, S2Map16_EHZ, PalId_SLZ, S2Map128_EHZ }, // Emerald Hill
    { PlcId_SYZ, S2Art_HPZ, 0, S2Map16_HPZ, PalId_SYZ, S2Map128_HPZ }, // Hidden Palace
    { PlcId_SBZ, S2Art_HTZ, 0, S2Map16_HTZ, PalId_SBZ1, S2Map128_EHZ }, // Hill Top (Emerald Hill's chunks; its tileset is Emerald Hill's with its own over it from tile $1FC)
    { 0, Art_GHZ2, 0, Map16_GHZ, PalId_Ending, Map128_GHZ },
};

// Level collision indices, one file per path (real data, not a
// decode-the-same-blob-twice shim -- GHZ1/GHZ2 etc are currently identical
// since no content path-swaps yet, but are independent resources)
const uint8_t* level_coli[ZoneId_Num - 1][2] = {
    { Collision_GHZ1, Collision_GHZ2 },
    { Collision_LZ1, Collision_LZ2 },
    { S2Collision_CPZ1, S2Collision_CPZ2 },
    { S2Collision_EHZ1, S2Collision_EHZ2 },
    { S2Collision_HPZ1, S2Collision_HPZ2 },
    { S2Collision_EHZ1, S2Collision_EHZ2 }, // Hill Top uses Emerald Hill's
};

// Level object layouts (Sonic 2 format: ObjectsManager.h)
static const uint8_t obj_null[] = { 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00 };
static const uint8_t ring_null[] = { 0xFF, 0xFF, 0x00, 0x00 };

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
        S2Objects_CPZ1,
        obj_null, // act 3 is a copy of act 1 with no objects or rings (and goes in M2)
        obj_null,
        S2Objects_CPZ1,
    },
    {
        // ZoneId_SLZ
        S2Objects_EHZ1,
        S2Objects_EHZ2,
        obj_null, // act 3: no objects (and it goes in M2)
        S2Objects_EHZ1,
    },
    {
        // ZoneId_SYZ
        S2Objects_HPZ1,
        obj_null,
        obj_null,
        S2Objects_HPZ1,
    },
    {
// ZoneId_SBZ
        S2Objects_HTZ1,
        obj_null,
        obj_null,
        S2Objects_HTZ1,
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
        S2Rings_CPZ1,
        ring_null,
        ring_null,
        S2Rings_CPZ1,
    },
    {
        // ZoneId_SLZ
        S2Rings_EHZ1,
        S2Rings_EHZ2,
        ring_null,
        ring_null,
    },
    {
        // ZoneId_SYZ
        S2Rings_HPZ1,
        ring_null,
        ring_null,
        S2Rings_HPZ1,
    },
    {
// ZoneId_SBZ
        S2Rings_HTZ1,
        S2Rings_HTZ2,
        ring_null,
        S2Rings_HTZ1,
    },
    {
        // ZoneId_EndZ
        RingLayout_Ending,
        RingLayout_Ending,
        RingLayout_Ending,
        RingLayout_Ending,
    }
};

