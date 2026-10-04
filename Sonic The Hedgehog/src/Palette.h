#pragma once

#include "EnginePalette.h"

#ifdef PALETTE_IDS_CUSTOM // a game built on this one with palettes of its own (Sonic 2): it defines the whole enum
#include "PaletteIds.h"
#else
//Sonic 1 palette ids (the order of palette_pointers in Sonic1Palettes.c)
enum {
	PalId_SegaBG,
	PalId_Title,
	PalId_LevelSel,
	PalId_Sonic,
	PalId_GHZ,
	PalId_LZ,
	PalId_MZ,
	PalId_SYZ,
	PalId_SLZ,
	PalId_SBZ1,
	PalId_Special,
	PalId_LZWater,
	PalId_SBZ3,
	PalId_SBZ3Water,
	PalId_SBZ2,
	PalId_SonicLZ,
	PalId_SonicSBZ,
	PalId_SSResults,
	PalId_Continue,
	PalId_Ending,
};
#endif
