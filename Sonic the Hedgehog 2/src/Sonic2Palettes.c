#include "Palette.h"

// Sonic 2's palettes: Sonic 1's (a copy of Sonic1Palettes.c) with the zone slots' palettes replaced by their Sonic 2 zones' as those come (slot 3 = Emerald Hill).
#include "Resource/S2Palette/EHZ.h"
#include "Resource/S2Palette/Sonic.h"
#include "Resource/S2Palette/CPZ.h"
#include "Resource/S2Palette/HPZ.h"
#include "Resource/S2Palette/HTZ.h"
#include "Resource/S2Palette/WZ.h"
#include "Resource/S2Palette/MTZ.h"
#include "Resource/S2Palette/OOZ.h"
#include "Resource/S2Palette/DHZ.h"
#include "Resource/S2Palette/CNZ.h"
#include "Resource/S2Palette/NGHZ.h"
#include "Resource/S2Palette/LevelSel.h" // the prototypes' level select palette
#include "Resource/S2Palette/HPZWater.h"
#include "Resource/S2Palette/CPZWater.h"
#include "Resource/S2Palette/SonicWater.h"
#include "Resource/S2Palette/SonicWater4.h"
// Palettes
#include "Resource/Palette/Continue.h"
#include "Resource/Palette/Ending.h"
#include "Resource/Palette/GHZ.h"
#include "Resource/Palette/LZ.h"
#include "Resource/Palette/LZWater.h"
#include "Resource/Palette/LevelSel.h"
#include "Resource/Palette/MZ.h"
#include "Resource/Palette/SBZ1.h"
#include "Resource/Palette/SBZ2.h"
#include "Resource/Palette/SBZ3.h"
#include "Resource/Palette/SBZ3Water.h"
#include "Resource/Palette/SLZ.h"
#include "Resource/Palette/SSResults.h"
#include "Resource/Palette/SYZ.h"
#include "Resource/Palette/SegaBG.h"
#include "Resource/Palette/Sonic.h"
#include "Resource/Palette/SonicLZ.h"
#include "Resource/Palette/SonicSBZ.h"
#include "Resource/Palette/Special.h"
#include "Resource/Palette/Title.h"

PalettePointer palette_pointers[] = {
    /* PalId_SegaBG    */ { (const uint16_t*)Palette_SegaBG, &dry_palette[0][0], 0x40 },
    /* PalId_Title     */ { (const uint16_t*)Palette_Title, &dry_palette[0][0], 0x40 },
    /* PalId_LevelSel  */ { (const uint16_t*)S2Palette_LevelSel, &dry_palette[0][0], 0x40 },
    /* PalId_Sonic     */ { (const uint16_t*)S2Palette_Sonic, &dry_palette[0][0], 0x10 },
    /* PalId_GHZ       */ { (const uint16_t*)Palette_GHZ, &dry_palette[1][0], 0x30 },
    /* PalId_LZ        */ { (const uint16_t*)S2Palette_CPZ, &dry_palette[1][0], 0x30 },
    /* PalId_MZ        */ { (const uint16_t*)S2Palette_CPZ, &dry_palette[1][0], 0x30 },
    /* PalId_SYZ       */ { (const uint16_t*)S2Palette_HPZ, &dry_palette[1][0], 0x30 },
    /* PalId_SLZ       */ { (const uint16_t*)S2Palette_EHZ, &dry_palette[1][0], 0x30 },
    /* PalId_SBZ1      */ { (const uint16_t*)S2Palette_HTZ, &dry_palette[1][0], 0x30 },
    /* PalId_Special   */ { (const uint16_t*)Palette_Special, &dry_palette[0][0], 0x40 },
    /* PalId_LZWater   */ { (const uint16_t*)S2Palette_HPZWater, &dry_palette[0][0], 0x40 }, // (the zone slot that Sonic 1 has Labyrinth in has Hidden Palace's water here),
    /* PalId_SBZ3      */ { (const uint16_t*)S2Palette_HTZ, &dry_palette[1][0], 0x30 },
    /* PalId_SBZ3Water */ { (const uint16_t*)Palette_SBZ3Water, &dry_palette[0][0], 0x40 },
    /* PalId_SBZ2      */ { (const uint16_t*)S2Palette_HTZ, &dry_palette[1][0], 0x30 },
    /* PalId_SonicLZ   */ { (const uint16_t*)S2Palette_SonicWater, &dry_palette[0][0], 0x10 },
    /* PalId_SonicSBZ  */ { (const uint16_t*)S2Palette_SonicWater4, &dry_palette[0][0], 0x10 },
    /* PalId_SSResults */ { (const uint16_t*)Palette_SSResults, &dry_palette[0][0], 0x40 },
    /* PalId_Continue  */ { (const uint16_t*)Palette_Continue, &dry_palette[0][0], 0x20 },
    /* PalId_Ending    */ { (const uint16_t*)Palette_Ending, &dry_palette[0][0], 0x40 },
    /* PalId_WZ        */ { (const uint16_t*)S2Palette_WZ, &dry_palette[1][0], 0x30 },
    /* PalId_MTZ       */ { (const uint16_t*)S2Palette_MTZ, &dry_palette[1][0], 0x30 },
    /* PalId_OOZ       */ { (const uint16_t*)S2Palette_OOZ, &dry_palette[1][0], 0x30 },
    /* PalId_DHZ       */ { (const uint16_t*)S2Palette_DHZ, &dry_palette[1][0], 0x30 },
    /* PalId_CNZ       */ { (const uint16_t*)S2Palette_CNZ, &dry_palette[1][0], 0x30 },
    /* PalId_NGHZ      */ { (const uint16_t*)S2Palette_NGHZ, &dry_palette[1][0], 0x30 },
    /* PalId_CPZWater  */ { (const uint16_t*)S2Palette_CPZWater, &dry_palette[0][0], 0x40 },
};

