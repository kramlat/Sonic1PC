#include "Palette.h"

// Sonic 1's palettes: which colours each PalId_* loads, and where in the palette lines they go.
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
    /* PalId_SegaBG    */ { (const uint16_t*)Palette_SegaBG, &dry_palette[0][0], 0x40, PAL_FORMAT_GENESIS },
    /* PalId_Title     */ { (const uint16_t*)Palette_Title, &dry_palette[0][0], 0x40, PAL_FORMAT_GENESIS },
    /* PalId_LevelSel  */ { (const uint16_t*)Palette_LevelSel, &dry_palette[0][0], 0x40, PAL_FORMAT_GENESIS },
    /* PalId_Sonic     */ { (const uint16_t*)Palette_Sonic, &dry_palette[0][0], 0x10, PAL_FORMAT_GENESIS },
    /* PalId_GHZ       */ { (const uint16_t*)Palette_GHZ, &dry_palette[1][0], 0x30, PAL_FORMAT_GENESIS },
    /* PalId_LZ        */ { (const uint16_t*)Palette_LZ, &dry_palette[1][0], 0x30, PAL_FORMAT_GENESIS },
    /* PalId_MZ        */ { (const uint16_t*)Palette_MZ, &dry_palette[1][0], 0x30, PAL_FORMAT_GENESIS },
    /* PalId_SYZ       */ { (const uint16_t*)Palette_SYZ, &dry_palette[1][0], 0x30, PAL_FORMAT_GENESIS },
    /* PalId_SLZ       */ { (const uint16_t*)Palette_SLZ, &dry_palette[1][0], 0x30, PAL_FORMAT_GENESIS },
    /* PalId_SBZ1      */ { (const uint16_t*)Palette_SBZ1, &dry_palette[1][0], 0x30, PAL_FORMAT_GENESIS },
    /* PalId_Special   */ { (const uint16_t*)Palette_Special, &dry_palette[0][0], 0x40, PAL_FORMAT_GENESIS },
    /* PalId_LZWater   */ { (const uint16_t*)Palette_LZWater, &dry_palette[0][0], 0x40, PAL_FORMAT_GENESIS },
    /* PalId_SBZ3      */ { (const uint16_t*)Palette_SBZ3, &dry_palette[1][0], 0x30, PAL_FORMAT_GENESIS },
    /* PalId_SBZ3Water */ { (const uint16_t*)Palette_SBZ3Water, &dry_palette[0][0], 0x40, PAL_FORMAT_GENESIS },
    /* PalId_SBZ2      */ { (const uint16_t*)Palette_SBZ2, &dry_palette[1][0], 0x30, PAL_FORMAT_GENESIS },
    /* PalId_SonicLZ   */ { (const uint16_t*)Palette_SonicLZ, &dry_palette[0][0], 0x10, PAL_FORMAT_GENESIS },
    /* PalId_SonicSBZ  */ { (const uint16_t*)Palette_SonicSBZ, &dry_palette[0][0], 0x10, PAL_FORMAT_GENESIS },
    /* PalId_SSResults */ { (const uint16_t*)Palette_SSResults, &dry_palette[0][0], 0x40, PAL_FORMAT_GENESIS },
    /* PalId_Continue  */ { (const uint16_t*)Palette_Continue, &dry_palette[0][0], 0x20, PAL_FORMAT_GENESIS },
    /* PalId_Ending    */ { (const uint16_t*)Palette_Ending, &dry_palette[0][0], 0x40, PAL_FORMAT_GENESIS },
};

