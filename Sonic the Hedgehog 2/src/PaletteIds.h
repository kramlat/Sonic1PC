#ifndef _PALETTEIDS_H
#define _PALETTEIDS_H

// Sonic 2's palette ids (the order of palette_pointers in Sonic2Palettes.c): Sonic 1's, whose names the code shared with it uses (the zone ones are what its zone slots' palettes were, for the zones that were
// in these slots first), then the palettes of the zones of the Simon Wai prototype that have none of those. The zones' names are the ones to use.
enum {
	PalId_SegaBG,
	PalId_Title,
	PalId_LevelSel,
	PalId_Sonic,
	PalId_GHZ,   // (not a zone of this game's)
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
	PalId_WZ,
	PalId_MTZ,
	PalId_OOZ,
	PalId_DHZ,
	PalId_CNZ,
	PalId_NGHZ,
	PalId_CPZWater,
	PalId_NGHZWater,
	PalId_CPZWaterSonic,
	PalId_NGHZWaterSonic,

	PalId_EHZ = PalId_SLZ,
	PalId_CPZ = PalId_MZ,
	PalId_HPZ = PalId_SYZ,
	PalId_HTZ = PalId_SBZ1,
};

#endif //_PALETTEIDS_H
